[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Join-Path $env:RUNNER_TEMP ('bambu-manifest-negative-' + [guid]::NewGuid().ToString('N'))
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or
    -not $env:RUNNER_TEMP) {
    throw 'Manifest fixture checks run only on a disposable GitHub-hosted runner.'
}
[void](New-Item -ItemType Directory -Path $root)
try {
    $source = 'a' * 40
    $verifier = 'b' * 40
    $exe = 'c' * 64
    $runId = '987654321'
    $bundlePath = Join-Path $root 'dummy.aesgcm'
    [System.IO.File]::WriteAllBytes($bundlePath, [byte[]]@(1))
    $cipherHash = (Get-FileHash -LiteralPath $bundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $rsa = [System.Security.Cryptography.RSA]::Create()
    try {
        $rsa.ImportFromPem([System.IO.File]::ReadAllText((Join-Path $PSScriptRoot 'hosted-gui-public.pem')))
        $publicHash = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData(
            $rsa.ExportSubjectPublicKeyInfo()))).ToLowerInvariant()
    }
    finally { $rsa.Dispose() }
    $baseReceipt = [ordered]@{
        schema = 2; status = 'encrypted_partial_capture_pending_restricted_review'
        image_availability = 'encrypted_bundle_only'; run_id = $runId
        source_commit = $source; verification_commit = $verifier; release_tag = 'md3-v9999'
        installed_exe_sha256 = $exe; encrypted_bundle_sha256 = $cipherHash
        captures = @(); manifest = @()
    }
    $envelope = [ordered]@{
        schema = 2; run_id = $runId; source_commit = $source
        verification_commit = $verifier; release_tag = 'md3-v9999'
        installed_exe_sha256 = $exe; public_key_sha256 = $publicHash
        ciphertext_sha256 = $cipherHash; aad_sha256 = ('0' * 64)
    }
    $envelopePath = Join-Path $root 'envelope.json'
    $envelope | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $envelopePath -Encoding utf8
    $validRow = [ordered]@{
        path = 'behavior/en-light-1-1200x800/behavior-report.json'
        kind = 'behavior_report'; tuple = 'en-light-1-1200x800'
        bytes = 1; sha256 = ('d' * 64)
    }
    $cases = @(
        [ordered]@{ name = 'traversal'; rows = @([ordered]@{
            path = 'behavior/../secret.txt'; kind = 'behavior_report'
            tuple = 'en-light-1-1200x800'; bytes = 1; sha256 = ('d' * 64)
        }); expected = 'invalid or traversing path' },
        [ordered]@{ name = 'duplicate'; rows = @($validRow, $validRow)
            expected = 'duplicate path' }
    )
    foreach ($case in $cases) {
        $receipt = [ordered]@{}
        foreach ($key in $baseReceipt.Keys) { $receipt[$key] = $baseReceipt[$key] }
        $receipt.manifest = $case.rows
        $receiptPath = Join-Path $root ($case.name + '-receipt.json')
        $receipt | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $receiptPath -Encoding utf8
        $outputPath = Join-Path $root ($case.name + '-opened')
        $failedAsExpected = $false
        try {
            & (Join-Path $PSScriptRoot 'Open-HostedReleaseGuiEvidence.ps1') `
                -ReceiptPath $receiptPath -EnvelopePath $envelopePath -BundlePath $bundlePath `
                -OutputDirectory $outputPath -ExpectedRunId $runId -ExpectedCommit $source `
                -ExpectedVerificationCommit $verifier -ExpectedTag 'md3-v9999' `
                -ExpectedExeSha256 $exe
        }
        catch { $failedAsExpected = $_.Exception.Message.Contains($case.expected) }
        if (-not $failedAsExpected -or (Test-Path -LiteralPath $outputPath)) {
            throw "Schema v2 $($case.name) fixture was not rejected before extraction."
        }
    }
    $originalLocalAppData = $env:LOCALAPPDATA
    try {
        $env:LOCALAPPDATA = Join-Path $root 'isolated-local-appdata'
        $fakeExe = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3\app-9.9.9\bambu-studio.exe'
        [void](New-Item -ItemType Directory -Path (Split-Path -Parent $fakeExe) -Force)
        [System.IO.File]::WriteAllBytes($fakeExe, [System.Text.Encoding]::UTF8.GetBytes('fixture executable identity'))
        $fixtureExeHash = (Get-FileHash -LiteralPath $fakeExe -Algorithm SHA256).Hash.ToLowerInvariant()
        $fixtureInstall = Join-Path $root 'install.json'
        [ordered]@{
            status = 'verified'; source_commit = $source; release_tag = 'md3-v9999'
            package_version = '9.9.9'; installed_exe_sha256 = $fixtureExeHash
            package_exe_sha256 = $fixtureExeHash
            asset_sha256 = [ordered]@{ 'Setup.exe' = ('e' * 64) }
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $fixtureInstall -Encoding utf8
        $behaviorRoot = Join-Path $root 'behavior'
        $tuple = Join-Path $behaviorRoot 'en-light-1-1200x800'
        [void](New-Item -ItemType Directory -Path $tuple -Force)
        [ordered]@{
            schema = 2; source_commit = $source; verification_commit = $verifier
            release_tag = 'md3-v9999'; hosted_run_id = $env:GITHUB_RUN_ID
            installed_exe_sha256 = $fixtureExeHash; scope = 'diagnostic'; verdict = 'blocked'
            requested_tuple = [ordered]@{
                language = 'en'; theme = 'light'; scale = 1.0; viewport = @(1200, 800)
            }
            images = @(); restricted_logs = @(); rows = @([ordered]@{
                name = 'launch-or-drive'; status = 'blocked'; reason = 'fixture launch failure'
            })
        } | ConvertTo-Json -Depth 8 |
            Set-Content -LiteralPath (Join-Path $tuple 'behavior-report.json') -Encoding utf8
        $fixtureOutput = Join-Path $root 'encrypted-partial'
        & (Join-Path $PSScriptRoot 'Capture-HostedReleaseGui.ps1') `
            -InstallReceipt $fixtureInstall -ExpectedCommit $source -VerificationCommit $verifier `
            -Tag 'md3-v9999' -BehaviorDirectory $behaviorRoot -CaptureScope diagnostic `
            -OutputDirectory $fixtureOutput
        $partial = Get-Content -LiteralPath (Join-Path $fixtureOutput 'receipt.json') -Raw | ConvertFrom-Json
        if ($partial.status -cne 'encrypted_partial_behavior_pending_restricted_review' -or
            @($partial.captures).Count -ne 0 -or @($partial.manifest).Count -ne 1 -or
            $partial.manifest[0].kind -cne 'behavior_report') {
            throw 'An empty failed diagnostic was not encrypted with an explicitly partial receipt.'
        }
        $fixtureEnvelope = Join-Path $fixtureOutput 'envelope.json'
        $fixtureBundle = Join-Path $fixtureOutput 'images.zip.aesgcm'
        if (-not (Test-Path -LiteralPath $fixtureEnvelope -PathType Leaf) -or
            -not (Test-Path -LiteralPath $fixtureBundle -PathType Leaf)) {
            throw 'The failed diagnostic has no complete encrypted transport.'
        }
        $ownerKeyNeeded = $false
        try {
            & (Join-Path $PSScriptRoot 'Open-HostedReleaseGuiEvidence.ps1') `
                -ReceiptPath (Join-Path $fixtureOutput 'receipt.json') `
                -EnvelopePath $fixtureEnvelope -BundlePath $fixtureBundle `
                -OutputDirectory (Join-Path $root 'partial-opened') `
                -ExpectedRunId $env:GITHUB_RUN_ID -ExpectedCommit $source `
                -ExpectedVerificationCommit $verifier -ExpectedTag 'md3-v9999' `
                -ExpectedExeSha256 $fixtureExeHash
        }
        catch { $ownerKeyNeeded = $_.Exception.Message.Contains('local DPAPI-protected key is unavailable') }
        if (-not $ownerKeyNeeded -or (Test-Path -LiteralPath (Join-Path $root 'partial-opened'))) {
            throw 'Partial evidence did not pass metadata and binding validation before owner-key access.'
        }
    }
    finally { $env:LOCALAPPDATA = $originalLocalAppData }
    Write-Host 'Schema v2 invalid manifests were rejected, and an empty failed diagnostic remained encrypted and partial.'
}
finally {
    if (Test-Path -LiteralPath $root -PathType Container) { Remove-Item -LiteralPath $root -Recurse -Force }
}
