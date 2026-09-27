[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $ReceiptPath,
    [Parameter(Mandatory)][string] $EnvelopePath,
    [Parameter(Mandatory)][string] $BundlePath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^\d+$')][string] $ExpectedRunId,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
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
Assert-True (-not (Test-Path -LiteralPath $OutputDirectory)) 'The output directory already exists.'
Assert-True (Test-Path -LiteralPath $privatePath -PathType Leaf) 'The local DPAPI-protected key is unavailable.'
$receipt = Get-Content -LiteralPath $ReceiptPath -Raw | ConvertFrom-Json
$envelope = Get-Content -LiteralPath $EnvelopePath -Raw | ConvertFrom-Json
$expectedSource = $ExpectedCommit.ToLowerInvariant()
$expectedExe = $ExpectedExeSha256.ToLowerInvariant()
Assert-True ($receipt.schema -eq 1 -and $envelope.schema -eq 1) 'Unsupported evidence schema.'
Assert-True ($receipt.status -ceq 'encrypted_capture_pending_restricted_review') 'The capture receipt is not complete.'
Assert-True ($receipt.image_availability -ceq 'encrypted_bundle_only') 'The receipt does not describe an encrypted image bundle.'
foreach ($row in @($receipt, $envelope)) {
    Assert-True ($row.run_id -ceq $ExpectedRunId) 'The run ID does not match.'
    Assert-True ($row.source_commit -ceq $expectedSource) 'The source commit does not match.'
    Assert-True ($row.release_tag -ceq $ExpectedTag) 'The release tag does not match.'
    Assert-True ($row.installed_exe_sha256 -ceq $expectedExe) 'The installed executable hash does not match.'
}
Assert-True ($receipt.encrypted_bundle_sha256 -ceq $envelope.ciphertext_sha256) 'The receipt and envelope name different bundles.'
Assert-True ($envelope.public_key_sha256 -ceq (Get-FileHash -LiteralPath $publicPath -Algorithm SHA256).Hash.ToLowerInvariant()) 'The public key identity does not match.'
$bundle = Get-Item -LiteralPath $BundlePath
Assert-True ($bundle.Length -gt 0 -and $bundle.Length -le 268435456) 'The encrypted bundle exceeds the 256 MiB limit.'
Assert-True ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $envelope.ciphertext_sha256) 'The encrypted bundle hash does not match.'

$expectedNames = @('home', 'prepare', 'preview', 'device', 'project', 'ink',
    'preferences-appearance', 'preferences-general', 'preferences-user',
    'preferences-3d', 'preferences-other') |
    ForEach-Object { "${_}--en-light-comfortable--hosted.png" }
$expectedNames = @($expectedNames | Sort-Object)
$captures = @($receipt.captures | Sort-Object surface)
Assert-True ($captures.Count -eq 11) 'The receipt does not describe eleven captures.'
$captureByName = @{}
foreach ($capture in $captures) {
    $name = [string]$capture.surface + '.png'
    Assert-True ($expectedNames -ccontains $name) 'The receipt contains an unexpected image name.'
    Assert-True (-not $captureByName.ContainsKey($name)) 'The receipt contains a duplicate image name.'
    Assert-True ($capture.sha256 -cmatch '^[0-9a-f]{64}$') 'A capture hash is malformed.'
    Assert-True ($capture.bytes -gt 0 -and $capture.bytes -le 33554432) 'A capture exceeds the 32 MiB image limit.'
    $captureByName[$name] = $capture
}

$bindingLines = @('bambu-hosted-gui-v1', $ExpectedRunId, $expectedSource, $ExpectedTag, $expectedExe)
foreach ($capture in $captures) {
    $bindingLines += "$($capture.surface)|$($capture.bytes)|$($capture.sha256)"
}
$aad = [System.Text.Encoding]::UTF8.GetBytes(($bindingLines -join "`n") + "`n")
$aadHash = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($aad))).ToLowerInvariant()
Assert-True ($aadHash -ceq $envelope.aad_sha256) 'The envelope binding does not match the receipt.'

$privateBytes = $null
$key = $null
$plain = $null
$cipher = $null
$rsa = [System.Security.Cryptography.RSA]::Create()
try {
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
    $validated = @{}
    try {
        Assert-True ($archive.Entries.Count -eq 11) 'The archive does not contain exactly eleven images.'
        $totalBytes = [long]0
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName
            Assert-True ($name -ceq $entry.Name -and $expectedNames -ccontains $name) 'The archive contains an unexpected or traversing path.'
            Assert-True (-not $validated.ContainsKey($name)) 'The archive contains a duplicate path.'
            Assert-True ((($entry.ExternalAttributes -shr 16) -band 0xF000) -ne 0xA000) 'The archive contains a symbolic link.'
            Assert-True ($entry.Length -gt 0 -and $entry.Length -le 33554432) 'An image exceeds the 32 MiB extraction limit.'
            $totalBytes += $entry.Length
            Assert-True ($totalBytes -le 268435456) 'The archive exceeds the 256 MiB extraction limit.'
            Assert-True ($entry.Length -eq $captureByName[$name].bytes) 'An image length differs from the receipt.'
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
            Assert-True ($hash -ceq $captureByName[$name].sha256) 'An image hash differs from the receipt.'
            $validated[$name] = $imageBytes
        }
    }
    finally { $archive.Dispose(); $stream.Dispose() }

    [void](New-Item -ItemType Directory -Path $OutputDirectory)
    foreach ($name in $expectedNames) {
        [System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory $name), $validated[$name])
        [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($validated[$name])
    }
    Write-Host "Decrypted and validated 11 images for run $ExpectedRunId. Review pixels and privacy before publication."
}
finally {
    $rsa.Dispose()
    foreach ($bytes in @($privateBytes, $key, $plain, $cipher)) {
        if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
}
