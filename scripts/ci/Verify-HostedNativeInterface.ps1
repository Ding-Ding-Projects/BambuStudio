# One scope per disposable job. The envelope deliberately retains the existing
# bambu-automation-v2 protocol and dedicated recipient/reader.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string] $ExpectedSourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [Parameter(Mandatory)][string] $OutputDirectory,
    [Parameter(Mandatory)][ValidateSet('menus','vocabulary','slice-controls','combined-print','combined-send','cancellation')][string] $Scope,
    [ValidateSet('en','yue_HK','bilingual_en_yue_HK')][string] $Language = 'en',
    [ValidateSet('light','dark')][string] $Theme = 'light',
    [ValidateSet('1','1.25','1.5','2')][string] $Scale = '1',
    [ValidateSet('1200x800','1000x600')][string] $Viewport = '1200x800'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'Automation verification requires a disposable GitHub-hosted Windows runner.'
}
if ((& git rev-parse HEAD).Trim() -cne $ExpectedSourceCommit) { throw 'Verifier source SHA mismatch.' }
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
    & $python "$PSScriptRoot/../md3/drive-native-interface.py" --exe $exe --cli $cli --install-receipt $installReceipt --source-commit $ExpectedSourceCommit --release-tag $Tag --output $raw --scope $Scope --language $Language --theme $Theme --scale $Scale --viewport $Viewport
    $driverExit = $LASTEXITCODE
    $driver = Get-Content -LiteralPath (Join-Path $raw 'runtime.json') -Raw | ConvertFrom-Json
    $receipt.scope = $Scope
    $receipt.requested_tuple = @{language=$Language; theme=$Theme; scale=$Scale; viewport=$Viewport}
    $receipt.operation_count = @($driver.operations).Count
    $receipt.capture_count = @($driver.captures).Count
    # Native observations contain labels, paths and window identities. They stay
    # inside the encrypted runtime.json, never in this public-safe receipt.
    $receipt.runtime = $driver.status
    $receipt.capture = 'encrypted_pending_pixel_review'
    if ($driverExit -ne 0) { throw 'Native interface runtime checks failed; restricted diagnostics retained.' }
    $receipt.status = 'runtime_verified_capture_pending_review_hardware_unverified'
} catch {
    # Never publish subprocess output, host paths, profile contents or native error messages.
    $receipt.status = 'failed'
    $receipt.failure = 'Install, bootstrap, packaged runtime or evidence verification failed; inspect restricted evidence.'
    throw 'Hosted native interface verification failed.'
} finally {
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
