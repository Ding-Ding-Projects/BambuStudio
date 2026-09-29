<#
.SYNOPSIS
Restores the Ninja build tree of the latest main build from the Windows build
cache release, so the compile only rebuilds what changed since.

.DESCRIPTION
sccache cannot cache a compile that uses the precompiled header (it reports
them as "Non-cacheable: /Fp"), which is nearly every source here, so a hosted
build compiled all of them every time. This restores the whole build tree a
main build left behind instead, precompiled header included, and lets Ninja
decide what is out of date:

  1. It reads windows-build-latest.json from the draft release named by -Tag.
     The tree is used only when that build's key (compiler, Windows SDK, CMake,
     Ninja, dependency cache, workspace path and this script's layout) equals
     this run's key.
  2. It downloads the tree's parts (each at most 1.5 GB), checks every size and
     SHA-256 against the manifest, and extracts them into the workspace.
  3. A checkout gives every file the current time, which would make Ninja
     rebuild everything. Every tracked file is set to 2020-01-01, then every
     file that differs between the cached commit and this checkout is set to
     now, so exactly the changed files (and what depends on them) are newer
     than their objects. (2020, not earlier: Ninja on Windows counts time from
     about 2001, and an earlier time would come out negative.)
  4. The device page bundle is built into the source tree, which a checkout
     does not have, so its stamp in the build tree is removed to rebuild it.

Any problem leaves no build tree behind and the build compiles from scratch,
as it did before; a cache can make a build faster, never make it fail. -Cold
skips the restore on purpose (a commit message with [cold build] asks for it).

Writes the cache key to $env:GITHUB_ENV as BUILD_CACHE_KEY for the save step,
and the outcome (warm or cold) to $env:GITHUB_OUTPUT as state.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Repository,
    [Parameter(Mandatory)] [string] $Workspace,
    [Parameter(Mandatory)] [string] $DependencyCacheKey,
    [string] $Tag = 'build-cache-windows',
    [string] $BuildDirectory = 'build',
    [switch] $Cold
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

# Bump when the archive layout or the way this script marks sources changes.
$layout = 'layout-1'

function Get-ToolVersion {
    param([Parameter(Mandatory)] [string] $Command)
    if (-not (Get-Command $Command -CommandType Application -ErrorAction SilentlyContinue)) { return 'missing' }
    # The whole output is read before the first line is taken: stopping the
    # pipeline early can leave an exit code that would make the key vary.
    $lines = @(& $Command --version 2>$null)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -eq 0) { return 'unknown' }
    return ([string]$lines[0]).Trim()
}

function Get-FreeBytes {
    param([Parameter(Mandatory)] [string] $Path)
    $root = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($Path))
    return [long]([System.IO.DriveInfo]::new($root).AvailableFreeSpace)
}

function Write-State {
    param([Parameter(Mandatory)] [string] $State)
    if ($env:GITHUB_OUTPUT) {
        "state=$State" | Out-File -Append -FilePath $env:GITHUB_OUTPUT -Encoding utf8
    }
}

$key = @(
    "vc=$env:VCToolsVersion",
    "sdk=$(([string]$env:WindowsSDKVersion).TrimEnd('\'))",
    "cmake=$(Get-ToolVersion cmake)",
    "ninja=$(Get-ToolVersion ninja)",
    "deps=$DependencyCacheKey",
    # CMake refuses a tree configured for another source directory.
    "ws=$([System.IO.Path]::GetFullPath($Workspace).TrimEnd('\', '/'))",
    $layout
) -join '|'
if ($env:GITHUB_ENV) {
    "BUILD_CACHE_KEY=$key" | Out-File -Append -FilePath $env:GITHUB_ENV -Encoding utf8
}
Write-Host "Build cache key: $key"

$temp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$work = Join-Path $temp 'build-cache-restore'
$tree = Join-Path $Workspace $BuildDirectory
$extracting = $false
try {
    if ($Cold) { throw 'a cold build was asked for' }
    if (-not $env:GH_TOKEN) {
        throw 'no token that can read the draft cache release is available to this run'
    }
    if (Test-Path -LiteralPath $tree) {
        throw "a build tree already exists at $tree"
    }
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
    New-Item -ItemType Directory -Path $work | Out-Null

    & gh release download $Tag --repo $Repository --pattern 'windows-build-latest.json' --dir $work --clobber
    if ($LASTEXITCODE -ne 0) { throw "the '$Tag' release has no cached build yet" }
    $manifest = Get-Content -LiteralPath (Join-Path $work 'windows-build-latest.json') -Raw | ConvertFrom-Json
    if ([string]$manifest.key -cne $key) {
        throw "the cached tree was built with '$($manifest.key)'"
    }
    $commit = [string]$manifest.commit
    if ($commit -notmatch '^[0-9a-f]{40}$') { throw 'the cache manifest names no commit' }

    # The parts and the tree may share a drive; the build that follows needs
    # room of its own, as it would after a build from scratch.
    $partBytes = [long](@($manifest.parts) | Measure-Object -Property size -Sum).Sum
    $needed = $partBytes + [long]$manifest.tree_bytes + 4GB
    foreach ($path in @($work, $Workspace)) {
        $free = Get-FreeBytes $path
        if ($free -lt $needed) {
            throw ("{0:N0} bytes are free on the drive of {1}; the cached tree needs {2:N0}" -f $free, $path, $needed)
        }
    }

    $name = "windows-build-$commit"
    & gh release download $Tag --repo $Repository --pattern "$name.7z.*" --dir $work --clobber
    if ($LASTEXITCODE -ne 0) { throw "downloading the parts of $name failed" }
    foreach ($part in @($manifest.parts)) {
        $file = Join-Path $work ([string]$part.name)
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "part $($part.name) is missing" }
        $size = (Get-Item -LiteralPath $file).Length
        if ($size -ne [long]$part.size) { throw "part $($part.name) is $size bytes; the manifest says $($part.size)" }
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -cne [string]$part.sha256) { throw "part $($part.name) does not match its SHA-256" }
    }

    $extracting = $true
    & 7z x (Join-Path $work "$name.7z.001") "-o$Workspace" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "extracting the cached tree failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath (Join-Path $tree 'build.ninja') -PathType Leaf)) {
        throw 'the extracted tree has no build.ninja'
    }
    Remove-Item -LiteralPath $work -Recurse -Force

    # The files that changed since the cached build are the ones Ninja must see
    # as newer than their objects; every other tracked file is made older. The
    # diff is against the working tree, so a file an earlier step edited counts.
    & git -C $Workspace fetch --no-tags --depth=1 origin $commit 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "fetching the cached commit $commit failed" }
    $changed = @(((& git -C $Workspace diff --name-only --no-renames -z $commit) -join '').Split([char]0) |
        Where-Object { $_ })
    if ($LASTEXITCODE -ne 0) { throw 'listing the files changed since the cached build failed' }
    $tracked = @(((& git -C $Workspace ls-files -z) -join '').Split([char]0) | Where-Object { $_ })
    if ($LASTEXITCODE -ne 0 -or $tracked.Count -eq 0) { throw 'listing the tracked files failed' }

    $old = [datetime]::new(2020, 1, 1, 0, 0, 0, [DateTimeKind]::Utc)
    foreach ($relative in $tracked) {
        $path = Join-Path $Workspace $relative
        if ([System.IO.File]::Exists($path)) { [System.IO.File]::SetLastWriteTimeUtc($path, $old) }
    }
    $now = [datetime]::UtcNow
    foreach ($relative in $changed) {
        $path = Join-Path $Workspace $relative
        if ([System.IO.File]::Exists($path)) { [System.IO.File]::SetLastWriteTimeUtc($path, $now) }
    }

    # The device page bundle is written into the source tree, which a fresh
    # checkout does not have; its stamp must not claim it is built.
    Get-ChildItem -LiteralPath $tree -Recurse -Filter 'device_page.stamp' -File -ErrorAction SilentlyContinue |
        Remove-Item -Force

    Write-State 'warm'
    Write-Host "::notice::Build cache: restored the tree built from $commit (run $($manifest.run_number)); $($changed.Count) files changed since."
} catch {
    Write-Host "::warning::Build cache not used, building from scratch: $($_.Exception.Message)"
    # Only a tree this script put there is removed; half a tree would be worse than none.
    if ($extracting -and (Test-Path -LiteralPath $tree)) { Remove-Item -LiteralPath $tree -Recurse -Force -ErrorAction SilentlyContinue }
    Write-State 'cold'
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}
exit 0
