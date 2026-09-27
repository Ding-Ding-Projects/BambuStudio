<#
.SYNOPSIS
Compares the official published portable package on a hosted hidden desktop.
.DESCRIPTION
Downloads a fixed public release ZIP, verifies its published digest and tag
commit, then observes a file-open attempt. Only safe JSON metadata is uploaded.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'The official portable comparison requires a disposable GitHub-hosted runner.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root 'diagnostic\official-runtime'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$summaryPath = Join-Path $out 'vendor-release-summary.json'
$summary = [ordered]@{
    schema = 1
    status = 'started'
    runner = 'github-hosted-windows'
    run_id = $env:GITHUB_RUN_ID
    run_attempt = $env:GITHUB_RUN_ATTEMPT
    vendor_repo = 'bambulab/BambuStudio'
    vendor_tag = 'v02.08.04.57'
    expected_tag_commit = 'f977235e6d736c4c0b650520ac5a5b72cbfe9244'
    vendor_asset = 'Bambu_Studio_win_v02.08.04.57-20260922164607.zip'
    asset_sha256 = $null
    portable_exe_sha256 = $null
    fixture_sha256 = $null
    method = 'official vendor portable package, not a self-built or Squirrel package'
}

function Invoke-GhJson {
    param([string[]]$Arguments)
    $json = & gh @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'GitHub CLI lookup failed.' }
    return ($json | ConvertFrom-Json)
}

try {
    Set-Location $root
    foreach ($name in @('gh', 'git', 'py')) {
        if (-not (Get-Command $name -ErrorAction SilentlyContinue)) { throw "Required hosted tool is missing: $name" }
    }
    $release = Invoke-GhJson -Arguments @('release', 'view', 'v02.08.04.57', '--repo',
        'bambulab/BambuStudio', '--json', 'tagName,isDraft,isPrerelease,assets')
    if ($release.tagName -cne $summary.vendor_tag -or $release.isDraft -or -not $release.isPrerelease) {
        throw 'The vendor release state differs from the reviewed prerelease.'
    }
    $asset = @($release.assets | Where-Object name -CEQ $summary.vendor_asset)
    if ($asset.Count -ne 1 -or $asset[0].size -ne 472832292 -or
        $asset[0].digest -cne 'sha256:833e1da8d16a02d3309eb8713abf7ebfd5d23cea1c8d3d026b97359b7752ef75') {
        throw 'The vendor asset size or published digest differs from the reviewed release.'
    }
    $tagRef = Invoke-GhJson -Arguments @('api', 'repos/bambulab/BambuStudio/git/ref/tags/v02.08.04.57')
    $tagCommit = if ($tagRef.object.type -ceq 'tag') {
        (Invoke-GhJson -Arguments @('api', "repos/bambulab/BambuStudio/git/tags/$($tagRef.object.sha)")).object.sha
    } else { $tagRef.object.sha }
    if ($tagCommit -cne $summary.expected_tag_commit) { throw 'The vendor tag does not peel to the reviewed official commit.' }
    $summary.tag_commit = $tagCommit

    $tempRoot = Join-Path $env:RUNNER_TEMP ("bambu-vendor-$($env:GITHUB_RUN_ID)-$($env:GITHUB_RUN_ATTEMPT)")
    if (Test-Path -LiteralPath $tempRoot) { throw 'Disposable comparison root already exists.' }
    New-Item -ItemType Directory -Path $tempRoot | Out-Null
    & gh release download $summary.vendor_tag --repo $summary.vendor_repo `
        --pattern $summary.vendor_asset --dir $tempRoot
    if ($LASTEXITCODE -ne 0) { throw 'The official portable asset download failed.' }
    $zip = Join-Path $tempRoot $summary.vendor_asset
    if ((Get-Item -LiteralPath $zip).Length -ne 472832292) { throw 'Downloaded asset size differs from release metadata.' }
    $zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($zipHash -cne $asset[0].digest.Substring(7)) { throw 'Downloaded asset hash differs from the vendor release digest.' }
    $summary.asset_sha256 = $zipHash

    Add-Type -AssemblyName System.IO.Compression.ZipFile
    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $expandedBytes = [long]0
        foreach ($entry in $archive.Entries) {
            $expandedBytes += $entry.Length
            if ($expandedBytes -gt 4294967296 -or $entry.FullName -match '(^[\\/]|^[A-Za-z]:|(^|[\\/])\.\.([\\/]|$))') {
                throw 'The portable archive has an unsafe entry or exceeds the expansion limit.'
            }
        }
    }
    finally { $archive.Dispose() }
    $payload = Join-Path $tempRoot 'payload'
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $payload)
    $executables = @(Get-ChildItem -LiteralPath $payload -Filter 'bambu-studio.exe' -File -Recurse)
    if ($executables.Count -ne 1) { throw 'The portable archive does not contain exactly one native executable.' }
    $exe = $executables[0].FullName
    $summary.portable_exe_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    $fixture = Join-Path $root 'resources\calib\filament_flow\flowrate-test-pass1.3mf'
    if (-not (Test-Path -LiteralPath $fixture -PathType Leaf)) { throw 'The public repository fixture is missing.' }
    $summary.fixture_sha256 = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($summary.fixture_sha256 -cne 'f71b839130000caab52f99c258a575dabb0f46b1ddc74591c9e438720740e03c') {
        throw 'The public fixture differs from the reviewed official source.'
    }

    $toolRoot = Join-Path $tempRoot 'lowlevel-source'
    $toolCommit = 'e6e42f2066d539256d6480401d7cef867f2b8dfe'
    & git clone --quiet --no-checkout https://github.com/Ding-Ding-Projects/lowlevel-computer-use-mcp.git $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Pinned hidden-desktop tool checkout failed.' }
    & git -C $toolRoot checkout --quiet --detach $toolCommit
    if ($LASTEXITCODE -ne 0 -or (& git -C $toolRoot rev-parse HEAD).Trim() -cne $toolCommit) {
        throw 'Pinned hidden-desktop tool source mismatch.'
    }
    $venv = Join-Path $tempRoot 'lowlevel-venv'
    & py -3 -m venv $venv
    if ($LASTEXITCODE -ne 0) { throw 'Isolated Python environment creation failed.' }
    $python = Join-Path $venv 'Scripts\python.exe'
    & $python -m pip install --disable-pip-version-check --quiet $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Pinned hidden-desktop tool bootstrap failed.' }
    $env:LLCU_CHEAP = Join-Path $venv 'Scripts\lowlevel-computer-use-cheap.exe'
    if (-not (Test-Path -LiteralPath $env:LLCU_CHEAP -PathType Leaf)) { throw 'Hidden-desktop tool executable is missing.' }
    $summary.hidden_desktop_tool_commit = $toolCommit

    & $python (Join-Path $root 'scripts\md3\drive-official-portable.py') `
        --exe $exe --fixture $fixture --datadir (Join-Path $tempRoot 'profile') `
        --output (Join-Path $out 'runtime-observation.json')
    $driverExit = $LASTEXITCODE
    $summary.status = if ($driverExit -eq 0) { 'process_window_observed_model_load_unverified' } else { 'runtime_diagnostic_incomplete' }
    if ($driverExit -ne 0) { throw 'The hosted runtime observation did not reach its process/window criterion.' }
}
catch {
    $summary.status = 'failed'
    $summary.failure_type = $_.Exception.GetType().Name
    throw
}
finally {
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding utf8
}
