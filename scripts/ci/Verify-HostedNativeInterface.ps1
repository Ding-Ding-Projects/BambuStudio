# One scope per disposable job. The envelope deliberately retains the existing
# bambu-automation-v2 protocol and dedicated recipient/reader.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedSourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidateSet('menus','vocabulary','vocabulary-persistence','slice-controls','combined-print','combined-send','cancellation','minimum-resize','minimum-observe','startup-diagnostic')][string] $Scope,
    [ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedVerifierCommit,
    [ValidateSet('en','yue_HK','bilingual_en_yue_HK')][string] $Language = 'en',
    [ValidateSet('light','dark')][string] $Theme = 'light',
    [ValidateSet('1','1.25','1.5','2')][string] $Scale = '1',
    [ValidateSet('1200x800','1000x600','measured-minimum')][string] $Viewport = '1200x800',
    [switch] $ProvisionDisplayScale,
    [switch] $ProvisionResolution
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'Automation verification requires a disposable GitHub-hosted Windows runner.'
}
$startupDiagnostic = $Scope -ceq 'startup-diagnostic'
if ($startupDiagnostic) {
    if (-not $ExpectedVerifierCommit -or $ProvisionDisplayScale -or $ProvisionResolution -or
        $Language -cne 'en' -or $Theme -cne 'light' -or $Scale -cne '1' -or $Viewport -cne '1200x800' -or
        $Tag -cne 'md3-v190' -or $ExpectedSourceCommit -cne '35d1074faea221fa4f289f1db1e0ee428a90d701') {
        throw 'Unsupported fixed startup diagnostic tuple.'
    }
} elseif ($ExpectedVerifierCommit) { throw 'Separate verifier identity is restricted to startup diagnostics.' }
$checkout = & git rev-parse HEAD
$requiredCheckout = if ($startupDiagnostic) { $ExpectedVerifierCommit } else { $ExpectedSourceCommit }
if ($LASTEXITCODE -ne 0 -or $checkout.Trim() -cne $requiredCheckout) { throw 'Verifier source SHA mismatch.' }
if ($ProvisionDisplayScale -and $Scale -eq '1') { throw 'The baseline 100% route does not use scale provisioning.' }
$minimumObserve = $Scope -ceq 'minimum-observe'
if ($minimumObserve -and (-not $ProvisionResolution -or -not $ProvisionDisplayScale -or $Scale -cne '2' -or $Viewport -cne 'measured-minimum' -or $Language -cne 'en' -or $Theme -cne 'light')) { throw 'Unsupported minimum observation tuple.' }
if ($ProvisionResolution -and -not $minimumObserve -and ($Scope -ne 'minimum-resize' -or $Scale -ne '1' -or
    $Viewport -ne 'measured-minimum' -or $ProvisionDisplayScale)) {
    throw 'Fixed resolution provisioning requires only the baseline minimum-resize tuple.'
}
if ($Scope -eq 'minimum-resize' -and ($Scale -ne '1' -or $Viewport -ne 'measured-minimum' -or $ProvisionDisplayScale)) {
    throw 'Interactive minimum proof currently requires baseline scale and measured-minimum viewport.'
}
if (-not (Test-Path -LiteralPath "$PSScriptRoot/../md3/hosted-automation-public-v1.pem" -PathType Leaf)) {
    throw 'Dedicated automation evidence recipient is missing; initialize and commit its public PEM before hosted verification.'
}
$tempRoot = [IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd('\') + '\'
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $output.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)) {
    throw 'Output must be a new child of RUNNER_TEMP.'
}
[void](New-Item -ItemType Directory -Path $output)
$raw = Join-Path $env:RUNNER_TEMP ('native-interface-restricted-' + $env:GITHUB_RUN_ID)
[void](New-Item -ItemType Directory -Path $raw)
$receipt = [ordered]@{schema=2; protocol='bambu-automation-v2'; source_commit=$ExpectedSourceCommit; release_tag=$Tag; run_id=$env:GITHUB_RUN_ID; status='failed'; hardware='unverified_no_printer_commands'; capture='not_started'; exe_sha256=$null; cli_sha256=$null}
if ($startupDiagnostic) { $receipt.diagnostic_only = $true }
$evidenceSafeToRead = $true
try {
    $installReceipt = Join-Path $raw 'install.json'
    & "$PSScriptRoot/Verify-HostedSquirrelInstall.ps1" -Tag $Tag -Repository $Repository -ExpectedCommit $ExpectedSourceCommit -OutputPath $installReceipt -CiExecutionApproved
    $install = Get-Content -LiteralPath $installReceipt -Raw | ConvertFrom-Json
    $versionRoot = Join-Path (Join-Path $env:LOCALAPPDATA 'BambuStudioMD3') "app-$($install.package_version)"
    $exe = Join-Path $versionRoot 'bambu-studio.exe'
    $cli = Join-Path $versionRoot 'automation/bambu-automation.exe'
    if (-not (Test-Path -LiteralPath $cli -PathType Leaf)) { throw 'Packaged automation executable is missing.' }
    # Bind the installed companion to the already digest-verified Squirrel archive.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $packageRoot = Join-Path $env:RUNNER_TEMP ('bambu-release-install-' + $env:GITHUB_RUN_ID)
    $packages = @(Get-ChildItem -LiteralPath $packageRoot -Filter '*-full.nupkg' -File)
    if ($packages.Count -ne 1) { throw 'No unique verified package exists.' }
    $zip = [IO.Compression.ZipFile]::OpenRead($packages[0].FullName)
    try {
        $entries = @($zip.Entries | Where-Object { $_.FullName.Replace('\','/') -ceq 'lib/net45/automation/bambu-automation.exe' })
        if ($entries.Count -ne 1) { throw 'No unique packaged companion exists.' }
        $stream = $entries[0].Open()
        try { $packageHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant() }
        finally { $stream.Dispose() }
    } finally { $zip.Dispose() }
    $receipt.cli_sha256 = (Get-FileHash -LiteralPath $cli -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.exe_sha256 = $install.installed_exe_sha256
    if ($receipt.cli_sha256 -cne $packageHash) { throw 'Installed companion differs from the verified package.' }
    # Same pinned bootstrap as Invoke-HostedReleaseVerification.ps1, with job-local tools only.
    $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    $toolRoot = Join-Path $env:RUNNER_TEMP ('automation-lowlevel-' + $env:GITHUB_RUN_ID)
    & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Pinned headless bootstrap fetch failed.' }
    & git -C $toolRoot checkout --quiet --detach $toolCommit
    if ($LASTEXITCODE -ne 0 -or (& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) { throw 'Headless tool SHA mismatch.' }
    $venv = Join-Path $env:RUNNER_TEMP ('automation-python-' + $env:GITHUB_RUN_ID)
    & python -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Job-local Python environment creation failed.' }
    $python = Join-Path $venv 'Scripts/python.exe'
    & $python -m pip install --disable-pip-version-check --quiet $toolRoot 'Pillow==11.3.0' 'comtypes==1.4.9'
    if ($LASTEXITCODE -ne 0) { throw 'Pinned headless dependencies could not be installed.' }
    $env:LLCU_CHEAP = Join-Path $venv 'Scripts/lowlevel-computer-use-cheap.exe'
    if (-not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) { throw 'Cheap headless executable missing.' }
    if ($ProvisionDisplayScale -or $ProvisionResolution) {
        # Installation and dependency bootstrap precede any display mutation.
        # Only this fixed driver request enters the contained scale interval.
        $scalePercent = @{ '1'=100; '1.25'=125; '1.5'=150; '2'=200 }[$Scale]
        $requestPath = Join-Path $env:RUNNER_TEMP ('native-scale-request-' + $env:GITHUB_RUN_ID + '.json')
        $adapterReceiptPath = Join-Path $env:RUNNER_TEMP ('native-scale-adapter-' + $env:GITHUB_RUN_ID + '.json')
        $scaleOutput = Join-Path $env:RUNNER_TEMP ('native-scale-' + $env:GITHUB_RUN_ID)
        foreach ($freshPath in @($requestPath,$adapterReceiptPath,$scaleOutput,(Join-Path $raw 'runtime.json'))) {
            if (Test-Path -LiteralPath $freshPath) { throw 'Scaled native invocation must be fresh.' }
        }
        $request = [ordered]@{schema=1; request_id=[Guid]::NewGuid().ToString('N'); source_commit=$ExpectedSourceCommit
            release_tag=$Tag; run_id=$env:GITHUB_RUN_ID; scope=$Scope; language=$Language; theme=$Theme
            viewport=$Viewport; scale_percent=$scalePercent; exe_sha256=$receipt.exe_sha256; cli_sha256=$receipt.cli_sha256
            refresh_page=$(if ($minimumObserve) {'acknowledged-roundtrip'} else {'none'})
            resolution=$(if ($minimumObserve) {'1600x1200'} elseif ($ProvisionResolution) {'1920x1080'} else {'unchanged'})
            job_name=('Local\BambuNativeScale-' + [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant())}
        $boundFiles = @{install=$installReceipt; driver="$PSScriptRoot/../md3/drive-native-interface.py"
            adapter="$PSScriptRoot/run-scaled-native-interface.py"; verifier=$PSCommandPath
            python=$python; cheap=$env:LLCU_CHEAP; helper="$PSScriptRoot/Invoke-HostedDisplayScale.ps1"
            containment="$PSScriptRoot/HostedScaleProcess.cs"; display_mode="$PSScriptRoot/HostedDisplayMode.cs"
            navigation="$PSScriptRoot/Invoke-HostedDisplayScaleNavigation.ps1"; minimum="$PSScriptRoot/../md3/minimum_resize.py"}
        foreach ($entry in $boundFiles.GetEnumerator()) {
            $request[$entry.Key + '_sha256'] = (Get-FileHash -LiteralPath $entry.Value -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        # The adapter rejects unknown/duplicate fields, unsafe paths and hashes
        # before Settings input, and repeats those checks before product launch.
        [IO.File]::WriteAllText($requestPath, ($request | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        $requestHash = (Get-FileHash -LiteralPath $requestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $evidenceSafeToRead = $false
        & "$PSScriptRoot/Invoke-HostedDisplayScale.ps1" -ScalePercent $scalePercent -OutputDirectory $scaleOutput -CheapExecutable $env:LLCU_CHEAP -InputRoute hosted-foreground -NativeRuntime -ProvisionResolution:$ProvisionResolution -ResolutionMode $(if ($minimumObserve) {'1600x1200'} else {'1920x1080'}) -RefreshSettingsPage:$minimumObserve
        $driverExit = $LASTEXITCODE
        $scaleSupervisor = Get-Content -LiteralPath (Join-Path $scaleOutput 'supervisor.json') -Raw | ConvertFrom-Json
        $evidenceSafeToRead = $scaleSupervisor.worker_termination_verified -eq $true -and
            $scaleSupervisor.recovery_termination_verified -eq $true -and $scaleSupervisor.child_termination_uncertain -eq $false
        $receipt.scale_provisioning = [ordered]@{requested_percent=$scalePercent; status=$scaleSupervisor.status
            resolution_requested=[bool]$ProvisionResolution; input_recovery_uncertain=$scaleSupervisor.input_recovery_uncertain
            worker_termination_verified=$scaleSupervisor.worker_termination_verified
            recovery_termination_verified=$scaleSupervisor.recovery_termination_verified
            restoration_verified=$scaleSupervisor.restoration_verified; disposal_required=$scaleSupervisor.disposal_required
            input_route=$scaleSupervisor.input_route; foreground_input_atomic=$false}
        if ($driverExit -ne 0 -or -not $evidenceSafeToRead -or
            $scaleSupervisor.status -cne 'verified_settings_scale_and_restoration' -or
            $scaleSupervisor.requested_scale -ne $scalePercent -or $scaleSupervisor.native_runtime_requested -ne $true -or
            $scaleSupervisor.resolution_provisioning_requested -ne [bool]$ProvisionResolution -or
            $scaleSupervisor.requested_resolution -cne $request.resolution -or
            $scaleSupervisor.page_refresh_requested -ne $minimumObserve -or
            $scaleSupervisor.navigation_recovery_uncertain -ne $false -or
            $scaleSupervisor.input_recovery_uncertain -ne $false -or
            $scaleSupervisor.restoration_verified -ne $true -or $scaleSupervisor.disposal_required -ne $false) {
            throw 'Native scale interval did not finish and restore successfully.'
        }
        $adapterFile = Get-Item -LiteralPath $adapterReceiptPath
        if ($adapterFile.Length -le 0 -or $adapterFile.Length -gt 8192 -or
            ($adapterFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Native adapter receipt unavailable.' }
        $adapter = Get-Content -LiteralPath $adapterReceiptPath -Raw | ConvertFrom-Json
        if ($adapter.status -cne 'runtime_and_membership_verified' -or
            $adapter.request_id -cne $request.request_id -or $adapter.request_sha256 -cne $requestHash -or
            $adapter.holder_membership_count -lt 1 -or $adapter.product_membership_count -lt 1 -or
            $adapter.runtime_sha256 -cne (Get-FileHash -LiteralPath (Join-Path $raw 'runtime.json') -Algorithm SHA256).Hash.ToLowerInvariant()) {
            throw 'Fresh native adapter evidence binding failed.'
        }
    } elseif ($Scope -eq 'minimum-resize') {
        $receipt.disposal_required = $true
        Add-Type -Path "$PSScriptRoot/HostedScaleProcess.cs"
        $minimumJob = 'Local\BambuNativeScale-' + [Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32)).ToLowerInvariant()
        $minimumArguments = @("$PSScriptRoot/../md3/drive-native-interface.py",'--exe',$exe,'--cli',$cli,
            '--install-receipt',$installReceipt,'--source-commit',$ExpectedSourceCommit,'--release-tag',$Tag,
            '--output',$raw,'--scope',$Scope,'--language',$Language,'--theme',$Theme,'--scale',$Scale,
            '--viewport',$Viewport,'--minimum-job-name',$minimumJob)
        $evidenceSafeToRead = $false
        $minimumRun = [HostedScaleProcess]::RunNamed($python,$minimumArguments,900,$false,$minimumJob)
        $evidenceSafeToRead = $minimumRun.Terminated
        $driverExit = $minimumRun.Code
        if (-not $evidenceSafeToRead -or $driverExit -ne 0) { throw 'Contained minimum proof unavailable.' }
    } elseif ($startupDiagnostic) {
        & $python "$PSScriptRoot/../md3/drive-native-interface.py" --exe $exe --cli $cli --install-receipt $installReceipt --source-commit $ExpectedSourceCommit --release-tag $Tag --output $raw --scope startup-diagnostic --verifier-commit $ExpectedVerifierCommit
        $driverExit = $LASTEXITCODE
    } else {
        & $python "$PSScriptRoot/../md3/drive-native-interface.py" --exe $exe --cli $cli --install-receipt $installReceipt --source-commit $ExpectedSourceCommit --release-tag $Tag --output $raw --scope $Scope --language $Language --theme $Theme --scale $Scale --viewport $Viewport
        $driverExit = $LASTEXITCODE
    }
    $driver = Get-Content -LiteralPath (Join-Path $raw 'runtime.json') -Raw | ConvertFrom-Json
    $receipt.scope = $Scope
    $receipt.requested_tuple = @{language=$Language; theme=$Theme; scale=$Scale; viewport=$Viewport}
    $receipt.operation_count = @($driver.operations).Count
    $receipt.capture_count = @($driver.captures).Count
    if ($Scope -eq 'minimum-resize') {
        $minimumEvidence = @($driver.operations | Where-Object operation -eq 'interactive-minimum-resize')
        if ($minimumEvidence.Count -ne 1 -or $minimumEvidence[0].status -ne 'interactive_clamp_observed' -or
            $minimumEvidence[0].frame_restored -ne $true -or $minimumEvidence[0].input_desktop_restored -ne $true -or
            $minimumEvidence[0].mouse_release_verified -ne $true -or $minimumEvidence[0].server_exit_verified -ne $true -or
            $minimumEvidence[0].disposal_required -ne $false -or $driver.teardown_verified -ne $true) {
            throw 'Interactive minimum restoration evidence unavailable.'
        }
        $receipt.disposal_required = $false
    }
    # Native observations contain labels, paths and window identities. They stay
    # inside the encrypted runtime.json, never in this public-safe receipt.
    $receipt.runtime = $driver.status
    $receipt.capture = 'encrypted_pending_pixel_review'
    if ($driverExit -ne 0) { throw 'Native interface runtime checks failed; restricted diagnostics retained.' }
    if ($startupDiagnostic) {
        if ($driver.diagnostic_only -ne $true -or $driver.status -cne 'diagnostic_completed' -or
            $driver.verifier_binding.source_commit -cne $ExpectedVerifierCommit -or
            @($driver.operations).Count -ne 0 -or @($driver.captures).Count -ne 0 -or $driver.teardown_verified -ne $true) {
            throw 'Startup diagnostic binding or teardown mismatch.'
        }
        $receipt.status = 'failed' # Diagnostic completion never satisfies a product verification gate.
    } else { $receipt.status = 'runtime_verified_capture_pending_review_hardware_unverified' }
} catch {
    # Never publish subprocess output, host paths, profile contents or native error messages.
    $receipt.status = 'failed'
    $receipt.failure = 'Install, bootstrap, packaged runtime or evidence verification failed; inspect restricted evidence.'
    throw 'Hosted native interface verification failed.'
} finally {
    if (-not $evidenceSafeToRead) {
        # A writer with unverified containment may still mutate evidence. Do not
        # read, zip or encrypt that directory, and do not accept an old receipt.
        $receipt.status = 'failed'
        $receipt.capture = 'withheld_unverified_containment'
        $receipt.disposal_required = $true
        $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
        throw 'Native evidence withheld because containment is unverified.'
    }
    # Encrypt only explicitly produced evidence, using the existing restricted-review recipient.
    $zipPath = Join-Path $env:RUNNER_TEMP ('native-interface-evidence-' + $env:GITHUB_RUN_ID + '.zip')
    $rsa = [Security.Cryptography.RSA]::Create()
    $key = $null
    $plain = $null
    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $manifest = @()
        $totalBytes = [long]0
        foreach ($file in @(Get-ChildItem -LiteralPath $raw -File | Sort-Object Name)) {
            if ($file.Name -cnotmatch '^(install\.json|runtime\.json|\d{3}-[a-z0-9-]+\.png)$' -or
                ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $file.Length -le 0 -or $file.Length -gt 33554432) {
                throw 'Restricted evidence inventory contains an unsupported file.'
            }
            $totalBytes += $file.Length
            $manifest += [ordered]@{path=$file.Name; bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
        if ($manifest.Count -lt 1 -or $manifest.Count -gt 32 -or $totalBytes -gt 67108864 -or
            @(Get-ChildItem -LiteralPath $raw -Directory).Count -ne 0) { throw 'Restricted evidence inventory exceeds its bounds.' }
        $receipt.manifest = $manifest
        $binding = @('bambu-automation-v2', $env:GITHUB_RUN_ID, $ExpectedSourceCommit, $Tag,
            [string]$receipt.exe_sha256, [string]$receipt.cli_sha256, [string]$receipt.status,
            [string]$receipt.hardware, [string]$receipt.capture)
        foreach ($row in $manifest) { $binding += "$($row.path)|$($row.bytes)|$($row.sha256)" }
        $aad = [Text.Encoding]::UTF8.GetBytes(($binding -join "`n") + "`n")
        [IO.Compression.ZipFile]::CreateFromDirectory($raw, $zipPath)
        if ((Get-Item -LiteralPath $zipPath).Length -gt 67108864) { throw 'Restricted evidence exceeds 64 MiB.' }
        $plain = [IO.File]::ReadAllBytes($zipPath)
        $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
        $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $tagBytes = [byte[]]::new(16)
        $cipher = [byte[]]::new($plain.Length)
        $rsa.ImportFromPem([IO.File]::ReadAllText("$PSScriptRoot/../md3/hosted-automation-public-v1.pem"))
        $keyId = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo())).ToLowerInvariant()
        $wrapped = $rsa.Encrypt($key, [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [Security.Cryptography.AesGcm]::new($key, 16)
        try { $aes.Encrypt($nonce, $plain, $cipher, $tagBytes, $aad) } finally { $aes.Dispose() }
        $cipherPath = Join-Path $output 'evidence.aesgcm'
        [IO.File]::WriteAllBytes($cipherPath, $cipher)
        $receipt.encrypted_bundle_sha256 = (Get-FileHash -LiteralPath $cipherPath -Algorithm SHA256).Hash.ToLowerInvariant()
        @{schema=2; protocol='bambu-automation-v2'; run_id=$env:GITHUB_RUN_ID; source_commit=$ExpectedSourceCommit; release_tag=$Tag; exe_sha256=$receipt.exe_sha256; cli_sha256=$receipt.cli_sha256; public_key_sha256=$keyId; aad_sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($aad)).ToLowerInvariant(); wrapped_key=[Convert]::ToBase64String($wrapped); nonce=[Convert]::ToBase64String($nonce); tag=[Convert]::ToBase64String($tagBytes); ciphertext_sha256=$receipt.encrypted_bundle_sha256} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'envelope.json') -Encoding utf8
    } catch { $receipt.capture = 'encryption_failed'; $receipt.status = 'failed'; throw 'Restricted evidence encryption failed.' }
    finally {
        $rsa.Dispose()
        foreach ($bytes in @($key, $plain)) { if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) } }
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath }
        $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding utf8
    }
}
