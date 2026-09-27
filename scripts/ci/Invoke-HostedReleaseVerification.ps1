[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedSourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $VerificationCommit,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [ValidateSet('diagnostic', 'behavior')][string] $VerificationScope = 'behavior',
    [ValidateSet('en', 'yue_HK', 'bilingual_en_yue_HK')][string] $Language = 'en',
    [ValidateSet('light', 'dark')][string] $Theme = 'light',
    [Parameter(Mandatory)][string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or
    -not $env:RUNNER_TEMP -or $env:RUNNER_OS -ne 'Windows') {
    throw 'Release behavior verification requires an isolated GitHub-hosted Windows runner.'
}
if ((& git rev-parse HEAD).Trim() -cne $VerificationCommit.ToLowerInvariant()) {
    throw 'The checked-out verifier commit does not match the requested verifier identity.'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Verification output directory already exists.' }
[void](New-Item -ItemType Directory -Path $OutputDirectory)
$receipt = [ordered]@{
    schema = 1
    status = 'failed'
    hosted_run_id = $env:GITHUB_RUN_ID
    source_commit = $ExpectedSourceCommit.ToLowerInvariant()
    verification_commit = $VerificationCommit.ToLowerInvariant()
    release_tag = $Tag
    runner = 'github-hosted-windows'
    install_status = 'not_started'
    behavior_status = 'not_started'
    capture_status = 'not_started'
    verification_scope = $VerificationScope
}
$behaviorOutput = Join-Path $env:RUNNER_TEMP ('bambu-behavior-' + $env:GITHUB_RUN_ID)
try {
    $installReceipt = Join-Path $OutputDirectory 'install-receipt.json'
    & (Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1') `
        -Tag $Tag -Repository $Repository -ExpectedCommit $ExpectedSourceCommit `
        -OutputPath $installReceipt -CiExecutionApproved
    $receipt.install_status = 'verified'
    $install = Get-Content -LiteralPath $installReceipt -Raw | ConvertFrom-Json
    $installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
    $versionRoot = Join-Path $installRoot "app-$($install.package_version)"
    $exe = Join-Path $versionRoot 'bambu-studio.exe'

    $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    $toolRoot = Join-Path $env:RUNNER_TEMP ('lowlevel-verify-' + $env:GITHUB_RUN_ID)
    & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the pinned headless tool.' }
    & git -C $toolRoot checkout --quiet --detach $toolCommit
    if ($LASTEXITCODE -ne 0 -or (& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) {
        throw 'The pinned headless tool could not be checked out exactly.'
    }
    $venv = Join-Path $env:RUNNER_TEMP ('lowlevel-verify-venv-' + $env:GITHUB_RUN_ID)
    & python -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Could not create a job-local Python environment.' }
    $python = Join-Path $venv 'Scripts\python.exe'
    & $python -m pip install --disable-pip-version-check --quiet $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not install the pinned headless tool.' }
    & $python -m pip install --disable-pip-version-check --quiet 'Pillow==11.3.0'
    if ($LASTEXITCODE -ne 0) { throw 'Could not install the pinned Pillow image inspector.' }
    $env:LLCU_CHEAP = Join-Path $venv 'Scripts\lowlevel-computer-use-cheap.exe'
    if (-not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) {
        throw 'The job-local headless tool executable is missing.'
    }

    $driver = Join-Path $PSScriptRoot '..\md3\drive-packaged-behavior.py'
    if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) {
        throw 'The behavior driver is absent from the selected verifier commit.'
    }
    [void](New-Item -ItemType Directory -Path $behaviorOutput)
    $tupleSpecs = @([ordered]@{ language = 'en'; theme = 'light'; scale = '1'; viewport = '1200x800'; scope = 'diagnostic' })
    if ($VerificationScope -eq 'behavior') {
        $tupleSpecs = @()
        foreach ($language in @($Language)) {
            foreach ($theme in @($Theme)) {
                foreach ($scale in @('1', '1.25', '1.5', '2')) {
                    foreach ($viewport in @('1200x800', '1000x600')) {
                        $scope = if ($language -eq 'en' -and $theme -eq 'light' -and
                            $scale -eq '1' -and $viewport -eq '1200x800') { 'behavior' } else { 'layout' }
                        $tupleSpecs += [ordered]@{
                            language = $language; theme = $theme; scale = $scale
                            viewport = $viewport; scope = $scope
                        }
                    }
                }
            }
        }
    }
    $driverFailures = @()
    foreach ($tuple in $tupleSpecs) {
        $name = "$($tuple.language)-$($tuple.theme)-$($tuple.scale)-$($tuple.viewport)"
        $tupleOutput = Join-Path $behaviorOutput $name
        & $python $driver --exe $exe --install-receipt $installReceipt `
            --source-commit $ExpectedSourceCommit --verification-commit $VerificationCommit `
            --release-tag $Tag --hosted-run-id $env:GITHUB_RUN_ID --output $tupleOutput `
            --language $tuple.language --theme $tuple.theme --scale $tuple.scale `
            --viewport $tuple.viewport --scope $tuple.scope
        if ($LASTEXITCODE -ne 0) { $driverFailures += "$name=$LASTEXITCODE" }
    }
    $receipt.behavior_status = if ($driverFailures.Count -eq 0) { 'report_written_pending_review' } else { 'blocked_or_partial' }
    $receipt.behavior_failed_tuples = $driverFailures

    $captureOutput = Join-Path $OutputDirectory 'evidence'
    & (Join-Path $PSScriptRoot '..\md3\Capture-HostedReleaseGui.ps1') `
        -InstallReceipt $installReceipt -ExpectedCommit $ExpectedSourceCommit `
        -VerificationCommit $VerificationCommit -Tag $Tag -BehaviorDirectory $behaviorOutput `
        -CaptureScope $VerificationScope `
        -OutputDirectory $captureOutput
    $receipt.capture_status = 'encrypted_pending_restricted_review'
    $captureReceipt = Get-Content -LiteralPath (Join-Path $captureOutput 'receipt.json') -Raw | ConvertFrom-Json
    $expectedEvidenceStatus = if ($VerificationScope -eq 'diagnostic') {
        'encrypted_diagnostic_pending_restricted_review'
    } else { 'encrypted_behavior_pending_restricted_review' }
    if ($captureReceipt.status -cne $expectedEvidenceStatus) {
        $receipt.capture_status = 'encrypted_partial_pending_restricted_review'
        throw 'Capture is partial; encrypted diagnostic evidence was retained, but GUI verification remains blocked.'
    }
    if ($driverFailures.Count -gt 0) { throw 'One or more behavior tuples failed; encrypted diagnostics were retained.' }
    $receipt.status = if ($VerificationScope -eq 'diagnostic') {
        'diagnostic_encrypted_pending_restricted_review'
    } else { 'matrix_encrypted_pending_restricted_review' }
}
catch {
    $receipt.failure = $_.Exception.Message
    throw
}
finally {
    $receipt | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $OutputDirectory 'verification-receipt.json') -Encoding utf8
}
