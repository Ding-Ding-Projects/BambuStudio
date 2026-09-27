[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $InstallReceipt,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'Hosted GUI capture requires a disposable GitHub-hosted Windows runner.'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Capture output directory already exists.' }
[void](New-Item -ItemType Directory -Path $OutputDirectory)
$evidence = [ordered]@{
    schema = 1
    status = 'failed'
    run_id = $env:GITHUB_RUN_ID
    source_commit = $ExpectedCommit.ToLowerInvariant()
    release_tag = $Tag
    package_version = $null
    installed_exe_sha256 = $null
    installer_sha256 = $null
    runner = 'github-hosted-windows'
    capture_method = 'lowlevel-computer-use-cheap hidden desktop PrintWindow'
    capture_tool_commit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    privacy = 'fresh disposable runner profile; raw images withheld from public workflow artifacts; no pixel privacy review'
    image_availability = 'not_uploaded'
    captures = @()
}
$zipPath = $null

try {
    $receipt = Get-Content -LiteralPath $InstallReceipt -Raw | ConvertFrom-Json
    if ($receipt.status -cne 'verified' -or $receipt.source_commit -cne $ExpectedCommit.ToLowerInvariant() -or
        $receipt.release_tag -cne $Tag) {
        throw 'The isolated installation receipt does not match this published source and release.'
    }
    $installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
    $exe = Join-Path (Join-Path $installRoot "app-$($receipt.package_version)") 'bambu-studio.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'The verified installed executable is missing.' }
    $exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($exeHash -cne $receipt.installed_exe_sha256 -or $exeHash -cne $receipt.package_exe_sha256) {
        throw 'The installed executable changed after Squirrel verification.'
    }
    $evidence.package_version = $receipt.package_version
    $evidence.installed_exe_sha256 = $exeHash
    $evidence.installer_sha256 = $receipt.asset_sha256.'Setup.exe'

    $toolRoot = Join-Path $env:RUNNER_TEMP ('lowlevel-capture-' + $env:GITHUB_RUN_ID)
    $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not fetch the pinned headless capture tool.' }
    & git -C $toolRoot checkout --quiet --detach $toolCommit
    if ($LASTEXITCODE -ne 0) { throw 'Could not check out the pinned headless capture tool.' }
    if ((& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) { throw 'Headless capture tool source mismatch.' }

    $venv = Join-Path $env:RUNNER_TEMP ('lowlevel-capture-venv-' + $env:GITHUB_RUN_ID)
    & py -3 -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Python could not create an isolated capture environment.' }
    $python = Join-Path $venv 'Scripts\python.exe'
    & $python -m pip install --disable-pip-version-check --quiet $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Could not bootstrap the pinned headless capture tool.' }
    $env:LLCU_CHEAP = Join-Path $venv 'Scripts\lowlevel-computer-use-cheap.exe'
    if (-not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) { throw 'The headless capture executable is missing.' }

    $dataRoot = Join-Path $env:RUNNER_TEMP ('bambu-capture-profile-' + $env:GITHUB_RUN_ID)
    & $python (Join-Path $PSScriptRoot 'prepare-capture-datadirs.py') $dataRoot `
        --languages en --themes light --densities comfortable
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare a fresh capture profile.' }
    $tuple = 'en-light-comfortable'
    $dataDir = Join-Path $dataRoot $tuple
    $images = Join-Path $env:RUNNER_TEMP ('bambu-capture-images-' + $env:GITHUB_RUN_ID)
    [void](New-Item -ItemType Directory -Path $images)
    & $python (Join-Path $PSScriptRoot 'capture-tuple.py') `
        --exe $exe --datadir $dataDir --tuple $tuple --out $images `
        --suffix hosted --desktop ('bambu-' + $env:GITHUB_RUN_ID)
    if ($LASTEXITCODE -ne 0) { throw 'The hidden-desktop capture did not complete.' }

    $files = @(Get-ChildItem -LiteralPath $images -File -Filter '*.png' | Sort-Object Name)
    if ($files.Count -ne 11) { throw "Expected 11 captured surfaces, found $($files.Count)." }
    $expectedNames = @('home', 'prepare', 'preview', 'device', 'project', 'ink',
        'preferences-appearance', 'preferences-general', 'preferences-user',
        'preferences-3d', 'preferences-other') |
        ForEach-Object { "${_}--en-light-comfortable--hosted.png" }
    if (@(Compare-Object -ReferenceObject ($expectedNames | Sort-Object) -DifferenceObject @($files.Name | Sort-Object)).Count -ne 0) {
        throw 'The capture set contains an unexpected or missing surface.'
    }
    foreach ($file in $files) {
        if ($file.Length -lt 10000 -or $file.Length -gt 33554432) {
            throw "Capture '$($file.Name)' is outside the 10 KiB to 32 MiB image limit."
        }
        $imageJson = & $python -c 'import json,sys; from PIL import Image,ImageStat; im=Image.open(sys.argv[1]).convert("RGB"); w,h=im.size; im.thumbnail((128,128)); colors=im.getcolors(maxcolors=65536); print(json.dumps({"width":w,"height":h,"distinct_colors":len(colors) if colors is not None else 65536,"max_channel_stddev":max(ImageStat.Stat(im).stddev)}))' $file.FullName
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect captured pixels in '$($file.Name)'." }
        $image = $imageJson | ConvertFrom-Json
        if ($image.width -lt 700 -or $image.height -lt 500 -or
            $image.distinct_colors -lt 12 -or $image.max_channel_stddev -lt 10) {
            throw "Capture '$($file.Name)' is blank, uniform, or below the expected viewport."
        }
        $evidence.captures += [ordered]@{
            surface = $file.BaseName
            bytes = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            width = $image.width
            height = $image.height
            distinct_colors = $image.distinct_colors
        }
    }
    if ([string]::IsNullOrWhiteSpace($env:GITHUB_RUN_ID) -or $env:GITHUB_RUN_ID -notmatch '^\d+$') {
        throw 'The hosted run ID is missing or invalid.'
    }
    $zipPath = Join-Path $env:RUNNER_TEMP ('bambu-capture-' + $env:GITHUB_RUN_ID + '.zip')
    Add-Type -AssemblyName System.IO.Compression.ZipFile
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $images, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    $zipFile = Get-Item -LiteralPath $zipPath
    if ($zipFile.Length -le 0 -or $zipFile.Length -gt 268435456) {
        throw 'The capture ZIP exceeds the 256 MiB encryption limit.'
    }

    $bindingLines = @('bambu-hosted-gui-v1', $env:GITHUB_RUN_ID,
        $evidence.source_commit, $Tag, $exeHash)
    foreach ($capture in @($evidence.captures | Sort-Object { $_['surface'] })) {
        $bindingLines += "$($capture.surface)|$($capture.bytes)|$($capture.sha256)"
    }
    $aad = [System.Text.Encoding]::UTF8.GetBytes(($bindingLines -join "`n") + "`n")
    $publicPath = Join-Path $PSScriptRoot 'hosted-gui-public.pem'
    $publicPem = [System.IO.File]::ReadAllText($publicPath)
    $rsa = [System.Security.Cryptography.RSA]::Create()
    $key = $null
    $plaintext = $null
    $ciphertext = $null
    try {
        $key = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
        $nonce = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $tagBytes = [byte[]]::new(16)
        $plaintext = [System.IO.File]::ReadAllBytes($zipPath)
        $ciphertext = [byte[]]::new($plaintext.Length)
        $rsa.ImportFromPem($publicPem)
        $wrappedKey = $rsa.Encrypt($key, [System.Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $aes = [System.Security.Cryptography.AesGcm]::new($key, 16)
        try { $aes.Encrypt($nonce, $plaintext, $ciphertext, $tagBytes, $aad) }
        finally { $aes.Dispose() }
        $cipherPath = Join-Path $OutputDirectory 'images.zip.aesgcm'
        [System.IO.File]::WriteAllBytes($cipherPath, $ciphertext)
        $cipherHash = (Get-FileHash -LiteralPath $cipherPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $envelope = [ordered]@{
            schema = 1
            run_id = $env:GITHUB_RUN_ID
            source_commit = $evidence.source_commit
            release_tag = $Tag
            installed_exe_sha256 = $exeHash
            public_key_sha256 = (Get-FileHash -LiteralPath $publicPath -Algorithm SHA256).Hash.ToLowerInvariant()
            ciphertext_sha256 = $cipherHash
            aad_sha256 = ([Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($aad))).ToLowerInvariant()
            wrapped_key = [Convert]::ToBase64String($wrappedKey)
            nonce = [Convert]::ToBase64String($nonce)
            tag = [Convert]::ToBase64String($tagBytes)
        }
        $envelope | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'envelope.json') -Encoding utf8
        $evidence.encrypted_bundle_sha256 = $cipherHash
        $evidence.image_availability = 'encrypted_bundle_only'
        $evidence.status = 'encrypted_capture_pending_restricted_review'
    }
    finally {
        $rsa.Dispose()
        foreach ($bytes in @($key, $plaintext, $ciphertext)) {
            if ($null -ne $bytes) { [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($bytes) }
        }
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    }
}
catch {
    $evidence.failure = $_.Exception.Message
    throw
}
finally {
    if ($zipPath -and (Test-Path -LiteralPath $zipPath)) { Remove-Item -LiteralPath $zipPath -Force }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
}
