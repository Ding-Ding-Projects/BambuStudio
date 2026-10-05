[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $ReceiptPath,
    [Parameter(Mandatory)][string] $EnvelopePath,
    [Parameter(Mandatory)][string] $BundlePath,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^\d+$')][string] $ExpectedRunId,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedCommit,
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $ExpectedTag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string] $ExpectedExeSha256,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string] $ExpectedCliSha256,
    [ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedVerifierCommit,
    [string] $ExpectedVerifierManifestPath
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Hash-Bytes([byte[]] $Bytes) {
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
function Read-BoundedJson([string] $Path) {
    $file = Get-Item -LiteralPath $Path
    Assert-True ($file.Length -gt 0 -and $file.Length -le 1048576) 'Evidence JSON exceeds its bound.'
    return [IO.File]::ReadAllText($file.FullName) | ConvertFrom-Json -Depth 32
}
function Assert-UniqueJson($Element) {
    if ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object) {
        $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($property in $Element.EnumerateObject()) {
            Assert-True ($names.Add($property.Name)) 'Diagnostic JSON contains duplicate fields.'
            Assert-UniqueJson $property.Value
        }
    } elseif ($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array) {
        foreach ($item in $Element.EnumerateArray()) { Assert-UniqueJson $item }
    }
}
function Assert-DiagnosticJson([string] $Text) {
    $document = [Text.Json.JsonDocument]::Parse($Text)
    try {
        Assert-True ($document.RootElement.ValueKind -eq [Text.Json.JsonValueKind]::Object) 'Diagnostic JSON root is not an object.'
        Assert-UniqueJson $document.RootElement
    } finally { $document.Dispose() }
}
$final = [IO.Path]::GetFullPath($OutputDirectory)
$parent = [IO.Path]::GetDirectoryName($final)
Assert-True ((Test-Path -LiteralPath $parent -PathType Container) -and -not (Test-Path -LiteralPath $final)) 'Output requires an existing parent and a new directory.'
Assert-True (-not ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) 'Output parent may not be a reparse point.'
$receipt = Read-BoundedJson $ReceiptPath
$envelope = Read-BoundedJson $EnvelopePath
foreach ($row in @($receipt, $envelope)) {
    Assert-True ($row.schema -eq 2 -and $row.protocol -ceq 'bambu-automation-v2') 'Unsupported automation evidence schema.'
    Assert-True ($row.run_id -ceq $ExpectedRunId -and $row.source_commit -ceq $ExpectedCommit -and $row.release_tag -ceq $ExpectedTag) 'Run, source or release identity mismatch.'
    Assert-True ($row.exe_sha256 -ceq $ExpectedExeSha256 -and $row.cli_sha256 -ceq $ExpectedCliSha256) 'Native or companion executable identity mismatch.'
}
Assert-True ($receipt.capture -ceq 'encrypted_pending_pixel_review' -and
    $receipt.status -in @('failed','runtime_verified_capture_pending_review_hardware_unverified')) 'Receipt has no reviewable encrypted runtime evidence.'
Assert-True ($receipt.hardware -ceq 'unverified_no_printer_commands') 'Hardware verification state is inconsistent.'
Assert-True ($receipt.encrypted_bundle_sha256 -ceq $envelope.ciphertext_sha256) 'Receipt and envelope bundle identity mismatch.'
$bundle = Get-Item -LiteralPath $BundlePath
Assert-True ($bundle.Length -gt 0 -and $bundle.Length -le 67108864) 'Ciphertext exceeds 64 MiB.'
Assert-True ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -ceq $envelope.ciphertext_sha256) 'Ciphertext hash mismatch.'
$inventory = @{}
$binding = @('bambu-automation-v2', $ExpectedRunId, $ExpectedCommit, $ExpectedTag, $ExpectedExeSha256,
    $ExpectedCliSha256, [string]$receipt.status, [string]$receipt.hardware, [string]$receipt.capture)
$total = [long]0
$manifest = @($receipt.manifest)
Assert-True ($manifest.Count -ge 2 -and $manifest.Count -le 32) 'Manifest entry count is invalid.'
foreach ($row in $manifest) {
    $name = [string]$row.path
    Assert-True ($name -cmatch '^(install\.json|runtime\.json|\d{3}-[a-z0-9-]+\.png)$' -and -not $inventory.ContainsKey($name)) 'Manifest name is unsafe or duplicate.'
    Assert-True ($row.sha256 -cmatch '^[0-9a-f]{64}$' -and $row.bytes -gt 0 -and $row.bytes -le 33554432) 'Manifest hash or length is invalid.'
    $total += [long]$row.bytes
    Assert-True ($total -le 67108864) 'Expanded inventory exceeds 64 MiB.'
    $inventory[$name] = $row
    $binding += "$name|$($row.bytes)|$($row.sha256)"
}
Assert-True ($inventory.ContainsKey('install.json') -and $inventory.ContainsKey('runtime.json')) 'Manifest lacks install or runtime identity.'
$aad = [Text.Encoding]::UTF8.GetBytes(($binding -join "`n") + "`n")
Assert-True ((Hash-Bytes $aad) -ceq $envelope.aad_sha256) 'Authenticated binding mismatch.'
$rsa = [Security.Cryptography.RSA]::Create()
$privateBytes = $null
$key = $null
$plain = $null
$validated = @{}
$stage = $null
try {
    $rsa.ImportFromPem([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'hosted-automation-public-v1.pem')))
    $keyId = Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())
    Assert-True ($keyId -ceq $envelope.public_key_sha256) 'Unknown automation evidence recipient.'
    $privatePath = Join-Path $env:LOCALAPPDATA ("BambuStudio/HostedAutomationEvidence/keys/$keyId.dpapi")
    Assert-True (Test-Path -LiteralPath $privatePath -PathType Leaf) 'Protected local review key is unavailable.'
    $privateBytes = [Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes($privatePath), $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    $read = 0
    $rsa.ImportPkcs8PrivateKey($privateBytes, [ref]$read)
    Assert-True ($read -eq $privateBytes.Length -and (Hash-Bytes ($rsa.ExportSubjectPublicKeyInfo())) -ceq $keyId) 'Protected review key identity mismatch.'
    $key = $rsa.Decrypt([Convert]::FromBase64String($envelope.wrapped_key), [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
    $nonce = [Convert]::FromBase64String($envelope.nonce)
    $tag = [Convert]::FromBase64String($envelope.tag)
    Assert-True ($key.Length -eq 32 -and $nonce.Length -eq 12 -and $tag.Length -eq 16) 'Malformed encryption envelope.'
    $cipher = [IO.File]::ReadAllBytes($BundlePath)
    $plain = [byte[]]::new($cipher.Length)
    $aes = [Security.Cryptography.AesGcm]::new($key, 16)
    try { $aes.Decrypt($nonce, $cipher, $tag, $plain, $aad) } finally { $aes.Dispose() }
    $stream = [IO.MemoryStream]::new($plain, $false)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Read, $false)
    try {
        Assert-True ($archive.Entries.Count -eq $inventory.Count) 'ZIP inventory count mismatch.'
        foreach ($entry in $archive.Entries) {
            $name = $entry.FullName
            Assert-True ($inventory.ContainsKey($name) -and -not $validated.ContainsKey($name)) 'ZIP contains an undeclared or duplicate entry.'
            Assert-True ((($entry.ExternalAttributes -shr 16) -band 0xF000) -ne 0xA000) 'ZIP symlink is prohibited.'
            Assert-True ($entry.Length -eq $inventory[$name].bytes) 'ZIP entry length mismatch.'
            $bytes = [byte[]]::new([int]$entry.Length)
            $input = $entry.Open()
            try {
                $offset = 0
                while ($offset -lt $bytes.Length) {
                    $count = $input.Read($bytes, $offset, $bytes.Length - $offset)
                    Assert-True ($count -gt 0) 'ZIP entry ended early.'
                    $offset += $count
                }
                Assert-True ($input.ReadByte() -eq -1) 'ZIP entry exceeds its declaration.'
            } finally { $input.Dispose() }
            Assert-True ((Hash-Bytes $bytes) -ceq $inventory[$name].sha256) 'ZIP entry hash mismatch.'
            $validated[$name] = $bytes
        }
    } finally { $archive.Dispose(); $stream.Dispose() }
    $install = [Text.Encoding]::UTF8.GetString($validated['install.json']).TrimStart([char]0xFEFF) | ConvertFrom-Json
    $runtime = [Text.Encoding]::UTF8.GetString($validated['runtime.json']).TrimStart([char]0xFEFF) | ConvertFrom-Json
    Assert-True ($install.status -ceq 'verified' -and $install.source_commit -ceq $ExpectedCommit -and $install.release_tag -ceq $ExpectedTag -and $install.installed_exe_sha256 -ceq $ExpectedExeSha256) 'Decrypted installation identity mismatch.'
    Assert-True ($runtime.run_id -ceq $ExpectedRunId -and $runtime.source_commit -ceq $ExpectedCommit -and $runtime.release_tag -ceq $ExpectedTag -and $runtime.exe_sha256 -ceq $ExpectedExeSha256 -and $runtime.cli_sha256 -ceq $ExpectedCliSha256) 'Decrypted runtime identity mismatch.'
    Assert-True ($runtime.status -ceq $receipt.runtime) 'Decrypted runtime verdict differs from the receipt.'
    $diagnosticProperty = $runtime.PSObject.Properties['diagnostic_only']
    $diagnosticScope = $runtime.PSObject.Properties['scope']
    $receiptDiagnostic = $receipt.PSObject.Properties['diagnostic_only']
    if (($null -ne $receiptDiagnostic -and $receiptDiagnostic.Value -eq $true) -or
        ($null -ne $diagnosticProperty -and $diagnosticProperty.Value -eq $true) -or
        ($null -ne $diagnosticScope -and $diagnosticScope.Value -ceq 'startup-diagnostic')) {
        Assert-True ($ExpectedVerifierCommit -and $ExpectedVerifierManifestPath) 'Diagnostic evidence requires an independent verifier identity and manifest.'
        Assert-DiagnosticJson ([Text.Encoding]::UTF8.GetString($validated['runtime.json']).TrimStart([char]0xFEFF))
        Assert-True ($runtime.diagnostic_only -is [bool] -and $runtime.diagnostic_only -eq $true -and
            $runtime.scope -is [string] -and $runtime.scope -ceq 'startup-diagnostic' -and
            $runtime.status -is [string] -and $runtime.status -cin @('failed','diagnostic_completed') -and
            $receipt.status -ceq 'failed' -and $receipt.diagnostic_only -is [bool] -and $receipt.diagnostic_only -eq $true -and
            $runtime.operations -is [array] -and $runtime.operations.Count -eq 0 -and
            $runtime.captures -is [array] -and $runtime.captures.Count -eq 0) 'Diagnostic evidence claims product interaction or success.'
        # This independently prepared manifest must come from the exact expected
        # verifier Git revision, never from the downloaded evidence itself.
        $expectedFile = Get-Item -LiteralPath $ExpectedVerifierManifestPath
        Assert-True (-not $expectedFile.PSIsContainer -and $expectedFile.Length -gt 0 -and
            $expectedFile.Length -le 65536 -and -not ($expectedFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) 'Expected verifier manifest is unavailable.'
        $expectedText = [IO.File]::ReadAllText($expectedFile.FullName)
        Assert-DiagnosticJson $expectedText
        $expected = $expectedText | ConvertFrom-Json
        $actual = $runtime.verifier_binding
        $paths = @('.github/workflows/hosted-startup-diagnostic.yml', 'scripts/ci/Verify-HostedNativeInterface.ps1',
            'scripts/ci/Verify-HostedSquirrelInstall.ps1', 'scripts/md3/drive-native-interface.py',
            'scripts/md3/startup_diagnostics.py', 'scripts/md3/drive-packaged-behavior.py',
            'scripts/md3/hosted_launch_holder.py', 'scripts/md3/hosted_process.py',
            'scripts/md3/behavior_contract.py', 'scripts/md3/recapture.py', 'scripts/md3/hosted-automation-public-v1.pem')
        foreach ($record in @($expected,$actual)) {
            Assert-True ($record -is [pscustomobject] -and @($record.PSObject.Properties).Count -eq 3 -and
                $record.source_commit -is [string] -and $record.source_commit -ceq $ExpectedVerifierCommit -and
                $record.hash_format -is [string] -and $record.hash_format -ceq 'sha256-lf-v1' -and $record.files -is [pscustomobject] -and
                @($record.files.PSObject.Properties).Count -eq $paths.Count) 'Verifier manifest schema or source mismatch.'
            foreach ($path in $paths) {
                $value = $record.files.PSObject.Properties[$path]
                Assert-True ($null -ne $value -and $value.Value -is [string] -and
                    $value.Value -cmatch '^[0-9a-f]{64}$') 'Verifier manifest hash is missing or invalid.'
            }
        }
        foreach ($path in $paths) {
            Assert-True ($actual.files.PSObject.Properties[$path].Value -ceq $expected.files.PSObject.Properties[$path].Value) 'Authenticated verifier file differs from expected source.'
        }
    } elseif ($ExpectedVerifierCommit -or $ExpectedVerifierManifestPath) {
        throw 'Separate verifier expectations cannot be applied to ordinary runtime evidence.'
    }
    $captureNames = @()
    foreach ($image in @($runtime.captures)) {
        Assert-True ($image.file -cmatch '^\d{3}-[a-z0-9-]+\.png$' -and $inventory.ContainsKey($image.file) -and $captureNames -cnotcontains $image.file) 'Runtime capture inventory is inconsistent.'
        Assert-True ($image.sha256 -ceq $inventory[$image.file].sha256) 'Runtime capture hash mismatch.'
        $captureNames += $image.file
    }
    Assert-True ($captureNames.Count -eq @($inventory.Keys | Where-Object { $_.EndsWith('.png') }).Count) 'ZIP contains an unreported capture.'
    $stage = Join-Path $parent ('.automation-stage-' + [guid]::NewGuid().ToString('N'))
    Assert-True (-not (Test-Path -LiteralPath $stage)) 'Generated staging directory exists.'
    [void][IO.Directory]::CreateDirectory($stage)
    foreach ($name in $inventory.Keys) { [IO.File]::WriteAllBytes((Join-Path $stage $name), $validated[$name]) }
    @{schema=1; run_id=$ExpectedRunId; source_commit=$ExpectedCommit; release_tag=$ExpectedTag; integrity='verified'; pixel_review='unverified'; privacy_review='unverified'; publication='not_authorized'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'review-state.json') -Encoding utf8
    Assert-True (-not (Test-Path -LiteralPath $final)) 'Output appeared during validation.'
    [IO.Directory]::Move($stage, $final)
    $stage = $null
    Write-Host 'Automation evidence integrity verified. Pixel and privacy review remain unverified; publication is not authorized.'
} finally {
    $rsa.Dispose()
    foreach ($bytes in @($privateBytes, $key, $plain)) {
        if ($null -ne $bytes) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
    }
    foreach ($name in $validated.Keys) { [Security.Cryptography.CryptographicOperations]::ZeroMemory($validated[$name]) }
    # Preserve incomplete owned staging for investigation rather than recursively deleting an uncertain path.
    if ($stage) { Write-Warning 'Incomplete restricted staging was preserved; no publication is authorized.' }
}
