[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
    [switch] $FromCreation
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or
    $env:RUNNER_OS -ne 'Windows' -or -not $env:RUNNER_TEMP) {
    throw 'Startup tracing requires a disposable GitHub-hosted Windows runner.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($FromCreation -and ($Tag -cne 'md3-v190' -or $ExpectedCommit -cne '35d1074faea221fa4f289f1db1e0ee428a90d701')) {
    throw 'Creation tracing is restricted to the fixed diagnostic product.'
}
$out = Join-Path $root 'diagnostic\startup'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$verifier = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $verifier -cne $env:GITHUB_SHA) { throw 'Verifier checkout differs from the hosted run SHA.' }
$summary = [ordered]@{
    schema = 1
    status = 'started'
    run_id = $env:GITHUB_RUN_ID
    run_attempt = $env:GITHUB_RUN_ATTEMPT
    release_tag = $Tag
    source_commit = $ExpectedCommit.ToLowerInvariant()
    verification_commit = $verifier
    installed_exe_sha256 = $null
    cdb_sha256 = $null
    debugger_source = $null
    execution_class = 'instrumented_startup; separate from uninstrumented baseline'
    evidence_meaning = 'partial diagnostic transport only; no measured GUI geometry or behavior verdict'
    project_symbols = 'unavailable_for_md3_v125; no newer PDB substitution'
    evidence_status = 'not_started'
    failure_type = $null
}
if ($FromCreation) {
    $summary.execution_class = 'instrumented_from_creation; separate from uninstrumented baseline'
    $summary.project_symbols = 'matching_project_symbols_unavailable; module_offsets_only'
}

function Find-TrustedCdb {
    $candidate = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\cdb.exe'
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { return $null }
    $signature = Get-AuthenticodeSignature -LiteralPath $candidate
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft') {
        throw 'Installed CDB does not have a valid Microsoft signature.'
    }
    return $candidate
}

try {
    $installReceipt = Join-Path $out 'install-receipt.json'
    & (Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1') `
        -Tag $Tag -Repository 'Ding-Ding-Projects/BambuStudio' `
        -ExpectedCommit $ExpectedCommit -OutputPath $installReceipt -CiExecutionApproved
    $installed = Get-Content -LiteralPath $installReceipt -Raw | ConvertFrom-Json
    if ($installed.status -cne 'verified' -or $installed.source_commit -cne $summary.source_commit) {
        throw 'Installed release receipt failed its identity check.'
    }
    $exe = Join-Path (Join-Path (Join-Path $env:LOCALAPPDATA 'BambuStudioMD3') "app-$($installed.package_version)") 'bambu-studio.exe'
    $exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($exeHash -cne $installed.installed_exe_sha256) { throw 'Installed executable changed after package verification.' }
    $summary.installed_exe_sha256 = $exeHash

    $cdb = Find-TrustedCdb
    if (-not $cdb) {
        # Published by Microsoft as the Windows SDK installer. Install only
        # the debugger feature on this disposable runner, without a restart.
        $sdkSetup = Join-Path $env:RUNNER_TEMP 'winsdksetup.exe'
        Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/?linkid=2376217' -OutFile $sdkSetup
        $signature = Get-AuthenticodeSignature -LiteralPath $sdkSetup
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft') {
            throw 'Downloaded Windows SDK installer does not have a valid Microsoft signature.'
        }
        $setup = Start-Process -FilePath $sdkSetup `
            -ArgumentList '/features OptionId.WindowsDesktopDebuggers /quiet /norestart' `
            -PassThru -WindowStyle Hidden
        if (-not $setup.WaitForExit(300000)) {
            $setup.Kill($true)
            [void]$setup.WaitForExit(5000)
            throw 'Debugger bootstrap exceeded its bounded deadline.'
        }
        if ($setup.ExitCode -ne 0) { throw "Debugger-only SDK bootstrap exited $($setup.ExitCode); no restart was attempted." }
        $cdb = Find-TrustedCdb
        if (-not $cdb) { throw 'Microsoft debugger was absent after debugger-only SDK bootstrap.' }
        $summary.debugger_source = 'signed Microsoft SDK debugger-only bootstrap'
    } else {
        $summary.debugger_source = 'signed hosted Windows SDK'
    }
    $summary.cdb_sha256 = (Get-FileHash -LiteralPath $cdb -Algorithm SHA256).Hash.ToLowerInvariant()

    $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    $toolRoot = Join-Path $env:RUNNER_TEMP ("startup-lowlevel-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)")
    & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Pinned hidden-desktop tool fetch failed.' }
    & git -C $toolRoot checkout --quiet --detach $toolCommit
    if ($LASTEXITCODE -ne 0 -or (& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) {
        throw 'Pinned hidden-desktop tool source differs from the reviewed commit.'
    }
    $venv = Join-Path $env:RUNNER_TEMP ("startup-lowlevel-venv-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)")
    & py -3 -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Python could not create an isolated tool environment.' }
    $python = Join-Path $venv 'Scripts\python.exe'
    & $python -m pip install --disable-pip-version-check --quiet $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Pinned hidden-desktop tool bootstrap failed.' }
    & $python -m pip install --disable-pip-version-check --quiet 'Pillow==11.3.0'
    if ($LASTEXITCODE -ne 0) { throw 'Pinned image support for existing hosted identity checks is unavailable.' }
    & $python (Join-Path $root 'scripts\md3\test-trace-packaged-startup.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused hosted startup trace checks failed.' }
    $env:LLCU_CHEAP = Join-Path $venv 'Scripts\lowlevel-computer-use-cheap.exe'
    if (-not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) { throw 'Hidden-desktop executable is missing.' }

    $behavior = Join-Path $env:RUNNER_TEMP ("startup-trace-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)")
    $tuple = Join-Path $behavior 'en-light-1-1200x800'
    New-Item -ItemType Directory -Force -Path $tuple | Out-Null
    $driverArguments = @((Join-Path $root 'scripts\md3\trace-packaged-startup.py'),
        '--exe',$exe,'--cdb',$cdb,'--install-receipt',$installReceipt,
        '--source-commit',$ExpectedCommit,'--verification-commit',$verifier,'--tag',$Tag,'--output',$tuple)
    if ($FromCreation) {
        Add-Type -Path (Join-Path $root 'scripts/ci/HostedScaleProcess.cs')
        $desktop = "startup-loader-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)"
        $jobName = 'Local\BambuNativeScale-' + [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant()
        $created = $false
        $terminated = $false
        $closed = $false
        try {
            $response = & $env:LLCU_CHEAP create_headless_desktop --name $desktop 2>$null | ConvertFrom-Json
            if ($LASTEXITCODE -ne 0 -or $response.ok -ne $true) { throw 'Owned diagnostic desktop unavailable.' }
            $created = $true
            $driverArguments += @('--from-creation','--job-name',$jobName,'--desktop',$desktop)
            $result = [HostedScaleProcess]::RunNamedOnDesktop($python,[string[]]$driverArguments,120,$false,$jobName,"WinSta0\$desktop")
            $terminated = $result.Terminated
            $driverExit = $result.Code
        } finally {
            if ($created -and $terminated) {
                $response = & $env:LLCU_CHEAP close_headless_desktop --name $desktop 2>$null | ConvertFrom-Json
                if ($LASTEXITCODE -eq 0 -and $response.ok -eq $true) {
                    $check = & $env:LLCU_CHEAP list_headless_windows --name $desktop 2>$null | ConvertFrom-Json
                    $closed = $LASTEXITCODE -ne 0 -and ($check | ConvertTo-Json -Compress) -match [regex]::Escape("OpenDesktopW('$desktop')") -and
                        ($check | ConvertTo-Json -Compress) -match 'GetLastError=2(?!\d)'
                }
            }
            $summary.worker_tree_termination_verified = $terminated
            $summary.desktop_closed_verified = $closed
        }
        if (-not $terminated -or -not $closed) { throw 'Creation diagnostic teardown unverified; evidence withheld.' }
        $reportPath = Join-Path $tuple 'behavior-report.json'
        $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
        $report | Add-Member -NotePropertyName owned_process_cleanup -NotePropertyValue 'verified' -Force
        $report | Add-Member -NotePropertyName named_desktop_closed_verified -NotePropertyValue $true -Force
        $report | Add-Member -NotePropertyName verifier_files_sha256 -NotePropertyValue @{
            wrapper=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
            driver=(Get-FileHash -LiteralPath $driverArguments[0] -Algorithm SHA256).Hash.ToLowerInvariant()
            containment=(Get-FileHash -LiteralPath (Join-Path $root 'scripts/ci/HostedScaleProcess.cs') -Algorithm SHA256).Hash.ToLowerInvariant()
        } -Force
        $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding utf8
    } else {
        & $python @driverArguments
        $driverExit = $LASTEXITCODE
    }

    $encrypted = Join-Path $out 'encrypted'
    & (Join-Path $root 'scripts\md3\Capture-HostedReleaseGui.ps1') `
        -InstallReceipt $installReceipt -ExpectedCommit $ExpectedCommit `
        -VerificationCommit $verifier -Tag $Tag -BehaviorDirectory $behavior `
        -CaptureScope diagnostic -OutputDirectory $encrypted
    $capture = Get-Content -LiteralPath (Join-Path $encrypted 'receipt.json') -Raw | ConvertFrom-Json
    if (-not (Test-Path -LiteralPath (Join-Path $encrypted 'images.zip.aesgcm') -PathType Leaf) -or
        $capture.image_availability -cne 'encrypted_bundle_only' -or
        $capture.status -cne 'encrypted_partial_behavior_pending_restricted_review') {
        throw 'Restricted partial diagnostic transport was not encrypted with the expected verdict.'
    }
    $summary.evidence_status = $capture.status
    $summary.trace_driver_exit_code = $driverExit
    $summary.status = 'instrumented_trace_partial_encrypted_pending_review'
}
catch {
    $summary.status = 'failed'
    $summary.failure_type = $_.Exception.GetType().Name
    throw
}
finally {
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $out 'trace-summary.json') -Encoding utf8
}
