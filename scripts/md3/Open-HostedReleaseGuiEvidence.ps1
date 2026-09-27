[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $ReceiptPath,
    [Parameter(Mandatory)][string] $EnvelopePath,
    [Parameter(Mandatory)][string] $BundlePath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^\d+$')][string] $ExpectedRunId,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
    [ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedVerificationCommit,
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $ExpectedTag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string] $ExpectedExeSha256
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression

function Assert-True {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw $Message }
}

$privatePath = Join-Path $env:LOCALAPPDATA 'BambuStudio\HostedGuiEvidence\private-key.dpapi'
$publicPath = Join-Path $PSScriptRoot 'hosted-gui-public.pem'
$finalPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$parentPath = [System.IO.Path]::GetDirectoryName($finalPath)
Assert-True (-not [string]::IsNullOrWhiteSpace($parentPath) -and
    (Test-Path -LiteralPath $parentPath -PathType Container)) 'The output parent directory must already exist.'
Assert-True (-not (Test-Path -LiteralPath $finalPath)) 'The output directory already exists.'
$receipt = Get-Content -LiteralPath $ReceiptPath -Raw | ConvertFrom-Json
$envelope = Get-Content -LiteralPath $EnvelopePath -Raw | ConvertFrom-Json
$expectedSource = $ExpectedCommit.ToLowerInvariant()
$expectedExe = $ExpectedExeSha256.ToLowerInvariant()
Assert-True ($receipt.schema -in @(1, 2) -and $receipt.schema -eq $envelope.schema) 'Unsupported or mismatched evidence schema.'
if ($receipt.schema -eq 2) {
    Assert-True (-not [string]::IsNullOrWhiteSpace($ExpectedVerificationCommit)) 'Schema v2 requires the expected verifier commit.'
    $expectedVerifier = $ExpectedVerificationCommit.ToLowerInvariant()
    Assert-True ($receipt.verification_commit -ceq $expectedVerifier -and
        $envelope.verification_commit -ceq $expectedVerifier) 'The verifier commit does not match.'
}
Assert-True ($receipt.status -ceq 'encrypted_capture_pending_restricted_review' -or
    ($receipt.schema -eq 2 -and $receipt.status -ceq 'encrypted_partial_capture_pending_restricted_review')) 'The evidence receipt is not encrypted and reviewable.'
Assert-True ($receipt.image_availability -ceq 'encrypted_bundle_only') 'The receipt does not describe an encrypted image bundle.'
foreach ($row in @($receipt, $envelope)) {
    Assert-True ($row.run_id -ceq $ExpectedRunId) 'The run ID does not match.'
    Assert-True ($row.source_commit -ceq $expectedSource) 'The source commit does not match.'
    Assert-True ($row.release_tag -ceq $ExpectedTag) 'The release tag does not match.'
    Assert-True ($row.installed_exe_sha256 -ceq $expectedExe) 'The installed executable hash does not match.'
}
Assert-True ($receipt.encrypted_bundle_sha256 -ceq $envelope.ciphertext_sha256) 'The receipt and envelope name different bundles.'
$publicRsa = [System.Security.Cryptography.RSA]::Create()
try {
    $publicRsa.ImportFromPem([System.IO.File]::ReadAllText($publicPath))
    $publicKeyHash = ([Convert]::ToHexString(
        [System.Security.Cryptography.SHA256]::HashData($publicRsa.ExportSubjectPublicKeyInfo()))).ToLowerInvariant()
}
finally { $publicRsa.Dispose() }
Assert-True ($envelope.public_key_sha256 -ceq $publicKeyHash) 'The public key identity does not match.'
$bundle = Get-Item -LiteralPath $BundlePath
Assert-True ($bundle.Length -gt 0 -and $bundle.Length -le 268435456) 'The encrypted bundle exceeds the 256 MiB limit.'
Assert-True ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $envelope.ciphertext_sha256) 'The encrypted bundle hash does not match.'

$legacyNames = @('home', 'prepare', 'preview', 'device', 'project', 'ink',
    'preferences-appearance', 'preferences-general', 'preferences-user',
    'preferences-3d', 'preferences-other') |
    ForEach-Object { "${_}--en-light-comfortable--hosted.png" }
$legacyNames = @($legacyNames | Sort-Object)
$captures = @($receipt.captures | Sort-Object surface)
Assert-True ($captures.Count -le 11 -and
    ($receipt.schema -eq 2 -or $captures.Count -eq 11)) 'The receipt capture count is invalid.'
$captureByName = @{}
foreach ($capture in $captures) {
    $name = [string]$capture.surface + '.png'
    Assert-True ($legacyNames -ccontains $name) 'The receipt contains an unexpected image name.'
    Assert-True (-not $captureByName.ContainsKey($name)) 'The receipt contains a duplicate image name.'
    Assert-True ($capture.sha256 -cmatch '^[0-9a-f]{64}$') 'A capture hash is malformed.'
    Assert-True ($capture.bytes -gt 0 -and $capture.bytes -le 33554432) 'A capture exceeds the 32 MiB image limit.'
    $captureByName[$name] = $capture
}

$inventory = @{}
if ($receipt.schema -eq 1) {
    $expectedNames = $legacyNames
    $bindingLines = @('bambu-hosted-gui-v1', $ExpectedRunId, $expectedSource, $ExpectedTag, $expectedExe)
    foreach ($capture in $captures) {
        $bindingLines += "$($capture.surface)|$($capture.bytes)|$($capture.sha256)"
        $inventory[[string]$capture.surface + '.png'] = $capture
    }
}
else {
    $manifest = @($receipt.manifest | Sort-Object path)
    Assert-True ($manifest.Count -ge 1 -and $manifest.Count -le 280) 'Schema v2 inventory has an invalid entry count.'
    $bindingLines = @('bambu-hosted-gui-v2', $ExpectedRunId, $expectedSource,
        $expectedVerifier, $ExpectedTag, $expectedExe)
    $reportCount = 0
    $captureCount = 0
    foreach ($row in $manifest) {
        $path = [string]$row.path
        Assert-True ($path -cmatch '^(captures/[a-z0-9-]+--en-light-comfortable--hosted\.png|behavior/(en|yue_HK|bilingual_en_yue_HK)-(light|dark)-(1|1\.25|1\.5|2)-(1200x800|1000x600)/(behavior-report\.json|[a-zA-Z0-9-]+\.png|restricted-logs/[a-zA-Z0-9_.-]+))$' -and -not $path.Contains('..')) 'Schema v2 inventory contains an invalid or traversing path.'
        Assert-True (-not $inventory.ContainsKey($path)) 'Schema v2 inventory contains a duplicate path.'
        Assert-True ($row.sha256 -cmatch '^[0-9a-f]{64}$' -and $row.bytes -gt 0 -and
            $row.bytes -le 33554432) 'Schema v2 inventory hash or length is invalid.'
        if ($path.StartsWith('captures/')) {
            Assert-True ($row.kind -ceq 'capture' -and $row.tuple -ceq 'en-light-comfortable') 'Capture metadata is invalid.'
            $captureCount++
        } else {
            $tupleName = $path.Split('/')[1]
            Assert-True ($row.tuple -ceq $tupleName) 'Behavior tuple metadata is invalid.'
            if ($path.Contains('/restricted-logs/')) {
                Assert-True ($row.kind -ceq 'restricted_diagnostic') 'Behavior diagnostic kind is invalid.'
            } elseif ($path.EndsWith('/behavior-report.json')) {
                Assert-True ($row.kind -ceq 'behavior_report') 'Behavior report kind is invalid.'
                $reportCount++
            } else { Assert-True ($row.kind -ceq 'behavior_image') 'Behavior image kind is invalid.' }
        }
        $inventory[$path] = $row
        $bindingLines += "$($row.path)|$($row.kind)|$($row.tuple)|$($row.bytes)|$($row.sha256)"
    }
    Assert-True ($captureCount -eq $captures.Count -and $reportCount -ge 1 -and $reportCount -le 8) 'Schema v2 capture or report count is inconsistent.'
    foreach ($capture in $captures) {
        $entryPath = 'captures/' + [string]$capture.surface + '.png'
        Assert-True ($inventory.ContainsKey($entryPath) -and
            $inventory[$entryPath].bytes -eq $capture.bytes -and
            $inventory[$entryPath].sha256 -ceq $capture.sha256) 'Capture metadata differs from the authenticated inventory.'
    }
    if ($receipt.status -ceq 'encrypted_capture_pending_restricted_review') {
        Assert-True ($captureCount -eq 11) 'A complete receipt misses captures.'
        foreach ($legacy in $legacyNames) {
            Assert-True ($inventory.ContainsKey('captures/' + $legacy)) 'Schema v2 misses a required capture.'
        }
    }
    $expectedNames = @($inventory.Keys | Sort-Object)
}
$aad = [System.Text.Encoding]::UTF8.GetBytes(($bindingLines -join "`n") + "`n")
$aadHash = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($aad))).ToLowerInvariant()
Assert-True ($aadHash -ceq $envelope.aad_sha256) 'The envelope binding does not match the receipt.'

$privateBytes = $null
$key = $null
$plain = $null
$cipher = $null
$validated = @{}
$stagePath = $null
$stageCreated = $false
$stageId = $null
$rsa = [System.Security.Cryptography.RSA]::Create()
try {
    Assert-True (Test-Path -LiteralPath $privatePath -PathType Leaf) 'The local DPAPI-protected key is unavailable.'
    $protectedBytes = [System.IO.File]::ReadAllBytes($privatePath)
    $privateBytes = [System.Security.Cryptography.ProtectedData]::Unprotect(
        $protectedBytes, $null, [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
    $read = 0
    $rsa.ImportPkcs8PrivateKey($privateBytes, [ref]$read)
    Assert-True ($read -eq $privateBytes.Length) 'The local private key has trailing data.'
    $key = $rsa.Decrypt([Convert]::FromBase64String($envelope.wrapped_key),
        [System.Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    Assert-True ($key.Length -eq 32) 'The unwrapped AES key length is invalid.'
    $nonce = [Convert]::FromBase64String($envelope.nonce)
    $tagBytes = [Convert]::FromBase64String($envelope.tag)
    Assert-True ($nonce.Length -eq 12 -and $tagBytes.Length -eq 16) 'The AES-GCM envelope is malformed.'
    $cipher = [System.IO.File]::ReadAllBytes($BundlePath)
    $plain = [byte[]]::new($cipher.Length)
    $aes = [System.Security.Cryptography.AesGcm]::new($key, 16)
    try { $aes.Decrypt($nonce, $cipher, $tagBytes, $plain, $aad) }
    finally { $aes.Dispose() }

    $stream = [System.IO.MemoryStream]::new($plain, $false)
    $archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Read, $false)
    try {
        Assert-True ($archive.Entries.Count -eq $expectedNames.Count) 'The archive does not match the declared evidence inventory.'
        $totalBytes = [long]0
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName
            Assert-True ($expectedNames -ccontains $name -and -not $name.Contains('..') -and
                -not $name.Contains('\')) 'The archive contains an unexpected or traversing path.'
            Assert-True (-not $validated.ContainsKey($name)) 'The archive contains a duplicate path.'
            Assert-True ((($entry.ExternalAttributes -shr 16) -band 0xF000) -ne 0xA000) 'The archive contains a symbolic link.'
            Assert-True ($entry.Length -gt 0 -and $entry.Length -le 33554432) 'An image exceeds the 32 MiB extraction limit.'
            $totalBytes += $entry.Length
            Assert-True ($totalBytes -le 268435456) 'The archive exceeds the 256 MiB extraction limit.'
            Assert-True ($entry.Length -eq $inventory[$name].bytes) 'An entry length differs from the receipt.'
            $entryStream = $entry.Open()
            $imageBytes = [byte[]]::new([int]$entry.Length)
            try {
                $offset = 0
                while ($offset -lt $imageBytes.Length) {
                    $readCount = $entryStream.Read($imageBytes, $offset, $imageBytes.Length - $offset)
                    if ($readCount -le 0) { throw 'An image ended before its declared size.' }
                    $offset += $readCount
                }
                Assert-True ($entryStream.ReadByte() -eq -1) 'An image exceeds its declared size.'
            }
            finally { $entryStream.Dispose() }
            $hash = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($imageBytes))).ToLowerInvariant()
            Assert-True ($hash -ceq $inventory[$name].sha256) 'An entry hash differs from the receipt.'
            $validated[$name] = $imageBytes
        }
    }
    finally { $archive.Dispose(); $stream.Dispose() }

    $stageId = [guid]::NewGuid().ToString('N')
    $stagePath = Join-Path $parentPath ('.' + [System.IO.Path]::GetFileName($finalPath) + '.stage-' + $stageId)
    Assert-True (-not (Test-Path -LiteralPath $stagePath)) 'The generated staging directory already exists.'
    [void][System.IO.Directory]::CreateDirectory($stagePath)
    $stageCreated = $true
    $marker = Join-Path $stagePath '.bambu-stage-owner'
    [System.IO.File]::WriteAllText($marker, $stageId, [System.Text.Encoding]::ASCII)
    foreach ($name in $expectedNames) {
        $stagedFile = Join-Path $stagePath $name
        $stagedParent = Split-Path -Parent $stagedFile
        if (-not (Test-Path -LiteralPath $stagedParent)) { [void](New-Item -ItemType Directory -Path $stagedParent -Force) }
        [System.IO.File]::WriteAllBytes($stagedFile, $validated[$name])
        $staged = Get-Item -LiteralPath $stagedFile
        Assert-True ($staged.Length -eq $inventory[$name].bytes) 'A staged entry has the wrong size.'
        $stagedHash = (Get-FileHash -LiteralPath $stagedFile -Algorithm SHA256).Hash.ToLowerInvariant()
        Assert-True ($stagedHash -ceq $inventory[$name].sha256) 'A staged entry has the wrong hash.'
    }
    Assert-True (@(Get-ChildItem -LiteralPath $stagePath -File -Recurse).Count -eq ($expectedNames.Count + 1)) 'The stage contains an unexpected file count.'
    Assert-True (-not (Test-Path -LiteralPath $finalPath)) 'The output directory appeared during staging.'
    [System.IO.File]::Delete($marker)
    [System.IO.Directory]::Move($stagePath, $finalPath)
    $stageCreated = $false
    Write-Host "Decrypted and validated $($expectedNames.Count) restricted entries for run $ExpectedRunId. Review pixels and privacy before publication."
}
finally {
    if ($stageCreated -and $stagePath -and (Test-Path -LiteralPath $stagePath -PathType Container)) {
        $resolvedStage = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $stagePath).Path)
        $stageInfo = Get-Item -LiteralPath $resolvedStage
        $markerPath = Join-Path $resolvedStage '.bambu-stage-owner'
        $markerMatches = (Test-Path -LiteralPath $markerPath -PathType Leaf) -and
            ([System.IO.File]::ReadAllText($markerPath) -ceq $stageId)
        $unmarkedOwned = -not (Test-Path -LiteralPath $markerPath) -and
            @(Get-ChildItem -LiteralPath $resolvedStage -File -Recurse |
                Where-Object { $expectedNames -cnotcontains $_.FullName.Substring($resolvedStage.Length + 1).Replace('\', '/') }).Count -eq 0
        if ([System.IO.Path]::GetDirectoryName($resolvedStage) -ieq $parentPath -and
            [System.IO.Path]::GetFileName($resolvedStage) -ieq [System.IO.Path]::GetFileName($stagePath) -and
            -not ($stageInfo.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -and
            ($markerMatches -or $unmarkedOwned)) {
            Remove-Item -LiteralPath $resolvedStage -Recurse -Force
        }
        else { Write-Warning 'Generated stage ownership could not be confirmed; it was preserved.' }
    }
    $rsa.Dispose()
    foreach ($imageBytes in $validated.Values) {
        [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($imageBytes)
    }
    foreach ($bytes in @($privateBytes, $key, $plain, $cipher)) {
        if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
}
