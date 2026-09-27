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
    source_commit = $ExpectedCommit.ToLowerInvariant()
    release_tag = $Tag
    package_version = $null
    installed_exe_sha256 = $null
    installer_sha256 = $null
    runner = 'github-hosted-windows'
    capture_method = 'lowlevel-computer-use-cheap hidden desktop PrintWindow'
    capture_tool_commit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    privacy = 'fresh disposable runner profile; raw images withheld from public workflow artifacts; no pixel privacy review'
    image_availability = 'ephemeral_runner_only_not_uploaded'
    captures = @()
}

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
    foreach ($file in $files) {
        if ($file.Length -lt 10000) { throw "Capture '$($file.Name)' is too small for rendered evidence." }
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
    $evidence.status = 'capture_metrics_recorded_images_ephemeral'
}
catch {
    $evidence.failure = $_.Exception.Message
    throw
}
finally {
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'receipt.json') -Encoding utf8
}
