[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = Join-Path $env:TEMP ('BambuHostedCrypto-' + [guid]::NewGuid().ToString('N'))
$source = 'a' * 40
$exeHash = 'b' * 64
$runId = '987654321'
$tag = 'md3-v9999'
$names = @('home', 'prepare', 'preview', 'device', 'project', 'ink',
    'preferences-appearance', 'preferences-general', 'preferences-user',
    'preferences-3d', 'preferences-other') |
    ForEach-Object { "${_}--en-light-comfortable--hosted.png" }
$key = $null
$plain = $null
$cipher = $null
$rsa = [System.Security.Cryptography.RSA]::Create()
try {
    $images = Join-Path $root 'images'
    [void](New-Item -ItemType Directory -Path $images -Force)
    $captures = @()
    foreach ($name in $names) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes("synthetic bytes for $name")
        [System.IO.File]::WriteAllBytes((Join-Path $images $name), $bytes)
        $captures += [ordered]@{
            surface = [System.IO.Path]::GetFileNameWithoutExtension($name)
            bytes = $bytes.Length
            sha256 = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($bytes))).ToLowerInvariant()
        }
    }
    $captures = @($captures | Sort-Object { $_['surface'] })
    $zipPath = Join-Path $root 'images.zip'
    [System.IO.Compression.ZipFile]::CreateFromDirectory($images, $zipPath)
    $plain = [System.IO.File]::ReadAllBytes($zipPath)
    $cipher = [byte[]]::new($plain.Length)
    $key = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $nonce = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
    $tagBytes = [byte[]]::new(16)
    $bindingLines = @('bambu-hosted-gui-v1', $runId, $source, $tag, $exeHash)
    foreach ($capture in $captures) {
        $bindingLines += "$($capture.surface)|$($capture.bytes)|$($capture.sha256)"
    }
    $aad = [System.Text.Encoding]::UTF8.GetBytes(($bindingLines -join "`n") + "`n")
    $publicPath = Join-Path $PSScriptRoot 'hosted-gui-public.pem'
    $rsa.ImportFromPem([System.IO.File]::ReadAllText($publicPath))
    $publicKeyHash = ([Convert]::ToHexString(
        [System.Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo()))).ToLowerInvariant()
    $wrappedKey = $rsa.Encrypt($key, [System.Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    $aes = [System.Security.Cryptography.AesGcm]::new($key, 16)
    try { $aes.Encrypt($nonce, $plain, $cipher, $tagBytes, $aad) }
    finally { $aes.Dispose() }
    $bundlePath = Join-Path $root 'images.zip.aesgcm'
    [System.IO.File]::WriteAllBytes($bundlePath, $cipher)
    $bundleHash = (Get-FileHash -LiteralPath $bundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt = [ordered]@{
        schema = 1
        status = 'encrypted_capture_pending_restricted_review'
        image_availability = 'encrypted_bundle_only'
        run_id = $runId
        source_commit = $source
        release_tag = $tag
        installed_exe_sha256 = $exeHash
        encrypted_bundle_sha256 = $bundleHash
        captures = $captures
    }
    $envelope = [ordered]@{
        schema = 1
        run_id = $runId
        source_commit = $source
        release_tag = $tag
        installed_exe_sha256 = $exeHash
        public_key_sha256 = $publicKeyHash
        ciphertext_sha256 = $bundleHash
        aad_sha256 = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($aad))).ToLowerInvariant()
        wrapped_key = [Convert]::ToBase64String($wrappedKey)
        nonce = [Convert]::ToBase64String($nonce)
        tag = [Convert]::ToBase64String($tagBytes)
    }
    $receiptPath = Join-Path $root 'receipt.json'
    $envelopePath = Join-Path $root 'envelope.json'
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath -Encoding utf8
    $envelope | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $envelopePath -Encoding utf8
    $openArgs = @{
        ReceiptPath = $receiptPath
        EnvelopePath = $envelopePath
        ExpectedRunId = $runId
        ExpectedCommit = $source
        ExpectedTag = $tag
        ExpectedExeSha256 = $exeHash
    }

    $tampered = [byte[]]$cipher.Clone()
    $tampered[0] = $tampered[0] -bxor 1
    $tamperedPath = Join-Path $root 'tampered.aesgcm'
    [System.IO.File]::WriteAllBytes($tamperedPath, $tampered)
    $tamperedHash = (Get-FileHash -LiteralPath $tamperedPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.encrypted_bundle_sha256 = $tamperedHash
    $envelope.ciphertext_sha256 = $tamperedHash
    $tamperedReceiptPath = Join-Path $root 'tampered-receipt.json'
    $tamperedEnvelopePath = Join-Path $root 'tampered-envelope.json'
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $tamperedReceiptPath -Encoding utf8
    $envelope | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $tamperedEnvelopePath -Encoding utf8
    $tamperedArgs = $openArgs.Clone()
    $tamperedArgs.ReceiptPath = $tamperedReceiptPath
    $tamperedArgs.EnvelopePath = $tamperedEnvelopePath
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot 'Open-HostedReleaseGuiEvidence.ps1') `
            @tamperedArgs -BundlePath $tamperedPath -OutputDirectory (Join-Path $root 'rejected')
    }
    catch { $rejected = $true }
    if (-not $rejected -or (Test-Path -LiteralPath (Join-Path $root 'rejected'))) {
        throw 'Tampered ciphertext was not rejected before extraction.'
    }

    $output = Join-Path $root 'opened'
    & (Join-Path $PSScriptRoot 'Open-HostedReleaseGuiEvidence.ps1') `
        @openArgs -BundlePath $bundlePath -OutputDirectory $output
    $actualNames = @((Get-ChildItem -LiteralPath $output -File).Name | Sort-Object)
    if (@(Compare-Object -ReferenceObject @($names | Sort-Object) -DifferenceObject $actualNames).Count -ne 0 -or
        @(Get-ChildItem -LiteralPath $root -Directory -Filter '.opened.stage-*').Count -ne 0) {
        throw 'The final output is incomplete or a generated stage remains.'
    }
    foreach ($name in $names) {
        $actual = [System.IO.File]::ReadAllBytes((Join-Path $output $name))
        $expected = [System.IO.File]::ReadAllBytes((Join-Path $images $name))
        if (-not [System.Linq.Enumerable]::SequenceEqual([byte[]]$actual, [byte[]]$expected)) {
            throw "Roundtrip changed $name."
        }
    }
    Write-Host 'Synthetic encrypted evidence roundtrip passed; tampered ciphertext was rejected.'
}
finally {
    $rsa.Dispose()
    foreach ($bytes in @($key, $plain, $cipher)) {
        if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
    $tempRoot = [System.IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    $resolvedRoot = [System.IO.Path]::GetFullPath($root)
    if ($resolvedRoot.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedRoot)) {
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    }
}
