<#
.SYNOPSIS
Saves this run's Ninja build tree to the Windows build cache release, in parts
of at most 1.5 GB, for Restore-BuildCache.ps1 in the next build.

.DESCRIPTION
The tree is archived with 7-Zip into volumes of at most -PartBytes bytes
(1,500,000,000 by default) and uploaded, with a manifest listing every part's
size and SHA-256, to a draft release named by -Tag. The release stays a draft
for good: a draft never fires release events, never becomes the latest
release, and keeps its assets replaceable while published releases here are
immutable. Every part is read back from GitHub and must have the size that was
written, or the set is not advertised.

windows-build-latest.json then points at this set, unless a later run already
points it at its own. Only the newest -KeepSets sets stay in the release.

Nothing here can fail the build: a cache that could not be saved only means
the next build compiles more.
#>
# Every value defaults to the GitHub Actions environment, so the workflow can
# start this in the background without quoting a key that holds spaces.
[CmdletBinding()]
param(
    [string] $Repository = $env:GITHUB_REPOSITORY,
    [string] $Workspace = $env:GITHUB_WORKSPACE,
    [string] $CacheKey = $env:BUILD_CACHE_KEY,
    [string] $Commit = $env:GITHUB_SHA,
    [int] $RunNumber = [int]$env:GITHUB_RUN_NUMBER,
    [string] $Tag = 'build-cache-windows',
    [string] $BuildDirectory = 'build',
    [long] $PartBytes = 1500000000,
    [int] $KeepSets = 3
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3

$temp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$work = Join-Path $temp 'build-cache-save'
try {
    if (-not $env:GH_TOKEN) { throw 'no token that can write the draft cache release is available to this run' }
    if (-not $Repository -or -not $Workspace -or -not $CacheKey -or $RunNumber -le 0) {
        throw 'the repository, workspace, cache key and run number are all needed'
    }
    if ($Commit -notmatch '^[0-9a-f]{40}$') { throw "'$Commit' is not a full commit id" }
    if ($PartBytes -le 0 -or $PartBytes -gt 1500000000) { throw 'parts must be at most 1,500,000,000 bytes' }
    $tree = Join-Path $Workspace $BuildDirectory
    if (-not (Test-Path -LiteralPath (Join-Path $tree 'build.ninja') -PathType Leaf)) { throw "no Ninja build tree at $tree" }
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    New-Item -ItemType Directory -Path $work | Out-Null

    $name = "windows-build-$Commit"
    $archive = Join-Path $work "$name.7z"
    $treeBytes = (Get-ChildItem -LiteralPath $tree -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    # The archive is written while the payload is packaged on the same runner;
    # it is skipped rather than allowed to fill the drive under that step.
    $root = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($work))
    $free = [long]([System.IO.DriveInfo]::new($root).AvailableFreeSpace)
    if ($free -lt [long]$treeBytes + 10GB) {
        throw ("{0:N0} bytes are free on {1}; archiving a {2:N0} byte tree needs {3:N0}" -f $free, $root, $treeBytes, ([long]$treeBytes + 10GB))
    }
    $started = Get-Date
    # Stored relative to the workspace, so it extracts back to .\build. The
    # resources link in the tree points into the checkout; it is left out and
    # recreated by the build. Debug databases are not produced (SLIC3R_MSVC_PDB
    # is off) and are left out if one ever is, and so is the device page's
    # package store, which a build from scratch fills again anyway.
    Push-Location -LiteralPath $Workspace
    try {
        & 7z a -t7z -mx=1 -mmt=on "-v$($PartBytes)b" $archive $BuildDirectory "-x!$BuildDirectory\src\resources" '-xr!*.pdb' '-xr!*.ilk' '-xr!.pnpm-store' | Out-Null
        $sevenZip = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($sevenZip -ne 0) { throw "7-Zip exited with code $sevenZip" }
    $parts = @(Get-ChildItem -LiteralPath $work -Filter "$name.7z.*" -File | Sort-Object Name)
    if ($parts.Count -eq 0) { throw '7-Zip wrote no parts' }
    foreach ($part in $parts) {
        if ($part.Length -gt $PartBytes) { throw "part $($part.Name) is $($part.Length) bytes, over the $PartBytes limit" }
    }
    $archiveBytes = ($parts | Measure-Object -Property Length -Sum).Sum
    Write-Host ("Archived {0:N0} bytes of build tree into {1} part(s), {2:N0} bytes, in {3:N0} s." -f `
        $treeBytes, $parts.Count, $archiveBytes, ((Get-Date) - $started).TotalSeconds)

    $manifest = [ordered]@{
        schema     = 1
        commit     = $Commit
        run_number = $RunNumber
        key        = $CacheKey
        created    = [DateTime]::UtcNow.ToString('o')
        tree_bytes = [long]$treeBytes
        parts      = @($parts | ForEach-Object {
            [ordered]@{
                name   = $_.Name
                size   = [long]$_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        })
    }
    $manifestPath = Join-Path $work "$name.json"
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8

    # The cache only ever lives in a draft.
    $existing = & gh release view $Tag --repo $Repository --json isDraft 2>$null
    if ($LASTEXITCODE -ne 0) {
        & gh release create $Tag --repo $Repository --draft `
            --title 'Windows build cache (internal, never published)' `
            --notes 'The Ninja build tree of the latest main build, in parts of at most 1.5 GB, so the next hosted build only compiles what changed. Written by scripts/ci/Save-BuildCache.ps1 and read by scripts/ci/Restore-BuildCache.ps1. Not an app release: it is a draft and stays one.'
        if ($LASTEXITCODE -ne 0) { throw "creating the draft '$Tag' release failed" }
    } elseif (-not ($existing | ConvertFrom-Json).isDraft) {
        throw "'$Tag' is a published release; the build cache only lives in a draft"
    }

    $started = Get-Date
    foreach ($part in $parts) {
        & gh release upload $Tag $part.FullName --repo $Repository --clobber
        if ($LASTEXITCODE -ne 0) { throw "uploading $($part.Name) failed" }
    }
    & gh release upload $Tag $manifestPath --repo $Repository --clobber
    if ($LASTEXITCODE -ne 0) { throw 'uploading the manifest failed' }

    # Read every part back: a part that did not land whole is not advertised.
    $assets = @((& gh release view $Tag --repo $Repository --json assets | ConvertFrom-Json).assets)
    foreach ($part in $manifest.parts) {
        $asset = $assets | Where-Object { $_.name -ceq $part.name } | Select-Object -First 1
        if ($null -eq $asset -or [long]$asset.size -ne [long]$part.size) {
            throw "part $($part.name) did not land at $($part.size) bytes"
        }
    }
    Write-Host ("Uploaded and read back {0} part(s) in {1:N0} s." -f $parts.Count, ((Get-Date) - $started).TotalSeconds)

    # Point at this set unless a later run already points at its own.
    $pointerDir = Join-Path $work 'pointer'
    New-Item -ItemType Directory -Path $pointerDir | Out-Null
    $current = $null
    & gh release download $Tag --repo $Repository --pattern 'windows-build-latest.json' --dir $pointerDir --clobber 2>$null
    if ($LASTEXITCODE -eq 0) {
        $current = Get-Content -LiteralPath (Join-Path $pointerDir 'windows-build-latest.json') -Raw | ConvertFrom-Json
    }
    if ($null -eq $current -or [int]$current.run_number -le $RunNumber) {
        $pointer = Join-Path $work 'windows-build-latest.json'
        Copy-Item -LiteralPath $manifestPath -Destination $pointer
        & gh release upload $Tag $pointer --repo $Repository --clobber
        if ($LASTEXITCODE -ne 0) { throw 'updating windows-build-latest.json failed' }
        Write-Host "::notice::Build cache saved: run $RunNumber, $Commit, $($parts.Count) part(s)."
    } else {
        Write-Host "Build cache saved; run $($current.run_number) is newer and stays the latest."
    }

    # Keep the newest sets; a set is its manifest plus its parts. Parts without
    # a manifest (an upload cut short) go too once they are two hours old, so
    # a run still uploading its own parts is never pulled out from under it.
    $assets = @((& gh release view $Tag --repo $Repository --json assets | ConvertFrom-Json).assets)
    $kept = @($assets | Where-Object { $_.name -match '^windows-build-[0-9a-f]{40}\.json$' } |
        Sort-Object -Property createdAt -Descending | Select-Object -First $KeepSets |
        ForEach-Object { $_.name.Substring(0, $_.name.Length - '.json'.Length) })
    $cutoff = [DateTime]::UtcNow.AddHours(-2)
    foreach ($asset in $assets) {
        if ($asset.name -notmatch '^(windows-build-[0-9a-f]{40})\.(json|7z\.\d{3})$') { continue }
        $prefix = $Matches[1]
        if ($kept -contains $prefix -or $prefix -eq $name) { continue }
        $manifestAsset = @($assets | Where-Object { $_.name -eq "$prefix.json" }).Count -gt 0
        if (-not $manifestAsset -and ([DateTime]$asset.createdAt).ToUniversalTime() -gt $cutoff) { continue }
        & gh release delete-asset $Tag $asset.name --repo $Repository --yes
        if ($LASTEXITCODE -ne 0) { Write-Host "::warning::Could not remove the old cache asset $($asset.name)." }
    }
} catch {
    Write-Host "::warning::Build cache not saved: $($_.Exception.Message)"
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}
exit 0
