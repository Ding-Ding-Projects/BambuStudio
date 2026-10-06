[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) { throw 'Symbol collection requires a hosted workflow.' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root 'diagnostic/native-symbols'
[void][IO.Directory]::CreateDirectory($out)
foreach ($name in @('symbols.zip.aesgcm', 'envelope.json')) {
    $previous = Join-Path $out $name
    if (Test-Path -LiteralPath $previous) { Remove-Item -LiteralPath $previous -Force }
}
$receipt = [ordered]@{ schema = 1; status = 'started'; source_sha = $env:GITHUB_SHA; run_id = $env:GITHUB_RUN_ID; run_attempt = $env:GITHUB_RUN_ATTEMPT; binaries = @(); failure_type = $null }
$stage = Join-Path $env:RUNNER_TEMP ('native-symbols-' + [guid]::NewGuid().ToString('N'))
$rsa = [Security.Cryptography.RSA]::Create()
$key = $null; $plain = $null; $cipher = $null
function Read-PdbIdentity([string]$Path) {
    # MSF 7 directory describes stream 1, whose PDB header carries age and GUID.
    if ((Get-Item -LiteralPath $Path).Length -gt 1073741824) { throw 'PDB exceeds the transport size limit.' }
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 56 -or [Text.Encoding]::ASCII.GetString($bytes, 0, 24) -ne 'Microsoft C/C++ MSF 7.00') { throw 'Unsupported PDB container.' }
    $page = [BitConverter]::ToUInt32($bytes, 32)
    $directoryBytes = [BitConverter]::ToUInt32($bytes, 44)
    $map = [BitConverter]::ToUInt32($bytes, 52)
    if ($page -lt 512 -or $page -gt 65536 -or $directoryBytes -gt 16777216) { throw 'Invalid PDB directory bounds.' }
    $directory = [byte[]]::new($directoryBytes)
    for ($i = 0; $i -lt [Math]::Ceiling($directoryBytes / [double]$page); $i++) {
        $block = [BitConverter]::ToUInt32($bytes, [int]($map * $page + $i * 4))
        $count = [Math]::Min($page, $directoryBytes - $i * $page)
        [Array]::Copy($bytes, [long]$block * $page, $directory, [long]$i * $page, $count)
    }
    $streams = [BitConverter]::ToUInt32($directory, 0)
    if ($streams -lt 2 -or $streams -gt 100000) { throw 'Invalid PDB stream count.' }
    $stream0 = [BitConverter]::ToUInt32($directory, 4)
    $stream1 = [BitConverter]::ToUInt32($directory, 8)
    if ($stream1 -lt 28 -or $stream1 -eq [uint32]::MaxValue) { throw 'Missing PDB identity stream.' }
    $skip = if ($stream0 -eq [uint32]::MaxValue) { 0 } else { [Math]::Ceiling($stream0 / [double]$page) }
    $block1 = [BitConverter]::ToUInt32($directory, [int](4 + $streams * 4 + $skip * 4))
    $offset = [int]($block1 * $page)
    $guidBytes = [byte[]]::new(16)
    [Array]::Copy($bytes, $offset + 12, $guidBytes, 0, 16)
    return @{ guid = ([guid]::new($guidBytes)).ToString(); age = [BitConverter]::ToUInt32($bytes, $offset + 8) }
}
try {
    if ([string]::IsNullOrWhiteSpace($env:SYMBOL_PUBLIC_KEY)) { throw 'Symbol encryption public key is missing.' }
    if ($env:SYMBOL_PUBLIC_KEY.Trim() -notmatch '\A-----BEGIN PUBLIC KEY-----\s+[A-Za-z0-9+/=\s]+-----END PUBLIC KEY-----\z') { throw 'Only a public SPKI PEM is accepted.' }
    $rsa.ImportFromPem($env:SYMBOL_PUBLIC_KEY)
    if ($rsa.KeySize -lt 3072) { throw 'Symbol encryption requires RSA 3072 or stronger.' }
    [void][IO.Directory]::CreateDirectory($stage)
    $pdbs = @(Get-ChildItem (Join-Path $root 'build/src') -Recurse -File -Filter '*.pdb')
    foreach ($binaryName in @('BambuStudio.dll', 'bambu-studio.exe')) {
        $binary = @(Get-ChildItem (Join-Path $root 'install-dir') -Recurse -File -Filter $binaryName)
        if ($binary.Count -ne 1) { throw 'Required native binary is unavailable.' }
        $headers = (& dumpbin.exe /headers $binary[0].FullName 2>$null) -join "`n"
        if ($LASTEXITCODE -ne 0) { throw 'Native CodeView inspection failed.' }
        $identity = [regex]::Match($headers, 'Format:\s*RSDS,\s*\{([0-9A-Fa-f-]+)\},\s*(\d+),\s*([^\r\n]+)')
        if (-not $identity.Success) { throw 'Native binary lacks an RSDS identity.' }
        $guid = $identity.Groups[1].Value.ToLowerInvariant()
        $age = [uint32]$identity.Groups[2].Value
        $pdbName = [IO.Path]::GetFileName($identity.Groups[3].Value.Trim())
        $matches = @($pdbs | Where-Object Name -eq $pdbName | Where-Object {
            $id = Read-PdbIdentity $_.FullName
            $id.guid -eq $guid -and $id.age -eq $age
        })
        if ($matches.Count -ne 1) { throw 'No unique matching native PDB was found.' }
        $pdb = $matches[0]
        Copy-Item -LiteralPath $pdb.FullName -Destination (Join-Path $stage $pdbName)
        Copy-Item -LiteralPath $binary[0].FullName -Destination (Join-Path $stage $binaryName)
        $receipt.binaries += [ordered]@{ binary = $binaryName; binary_sha256 = (Get-FileHash $binary[0].FullName).Hash.ToLowerInvariant(); pdb = $pdbName; pdb_sha256 = (Get-FileHash $pdb.FullName).Hash.ToLowerInvariant(); guid = $guid; age = $age }
    }
    $receipt.status = 'encrypted'
    $aadText = $receipt | ConvertTo-Json -Depth 6 -Compress
    $aad = [Text.Encoding]::UTF8.GetBytes($aadText)
    $receipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $stage 'symbols.json') -Encoding utf8
    $zip = Join-Path $env:RUNNER_TEMP ('native-symbols-' + [guid]::NewGuid().ToString('N') + '.zip')
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    if ((Get-Item $zip).Length -gt 1073741824) { throw 'Native symbol ZIP exceeds the 1 GiB transport limit.' }
    $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
    $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
    $tag = [byte[]]::new(16)
    $plain = [IO.File]::ReadAllBytes($zip)
    $cipher = [byte[]]::new($plain.Length)
    $aes = [Security.Cryptography.AesGcm]::new($key, 16)
    try { $aes.Encrypt($nonce, $plain, $cipher, $tag, $aad) } finally { $aes.Dispose() }
    $cipherPath = Join-Path $out 'symbols.zip.aesgcm'
    [IO.File]::WriteAllBytes($cipherPath, $cipher)
    [ordered]@{ schema = 1; kind = 'bambu-native-symbols'; receipt = $receipt; aad = [Convert]::ToBase64String($aad); public_key_sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo())).ToLowerInvariant(); ciphertext_sha256 = (Get-FileHash $cipherPath).Hash.ToLowerInvariant(); wrapped_key = [Convert]::ToBase64String($rsa.Encrypt($key, [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)); nonce = [Convert]::ToBase64String($nonce); tag = [Convert]::ToBase64String($tag) } | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $out 'envelope.json') -Encoding utf8
}
catch {
    $receipt.status = 'failed'; $receipt.failure_type = $_.Exception.GetType().FullName
    throw 'Native symbol collection or encryption failed; see the safe receipt.'
}
finally {
    $receipt | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $out 'receipt.json') -Encoding utf8
    $rsa.Dispose()
    foreach ($bytes in @($key, $plain, $cipher)) { if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) } }
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    if (Get-Variable zip -ErrorAction SilentlyContinue) { if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force } }
}
