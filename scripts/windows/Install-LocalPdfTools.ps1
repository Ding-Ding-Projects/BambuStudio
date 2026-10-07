#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Destination,
    [string]$SdkDestination,
    [string]$CacheDirectory = (Join-Path $PSScriptRoot '../../artifacts/local-pdf-cache'),
    [switch]$VerifyOnly,
    [string]$TrustedManifestPath,
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$manifestPath = Join-Path $PSScriptRoot 'local-pdf-tools.json'
if ($TrustedManifestPath) {
    if (!$VerifyOnly) { throw 'A completed native build manifest may only be used for package verification.' }
    $manifestPath = [IO.Path]::GetFullPath($TrustedManifestPath)
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
function Get-SafeChild([string]$Root, [string]$Relative) {
    if ($Relative -notmatch '^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$' -or $Relative.Split('/') -contains '..') { throw 'Invalid package path.' }
    $result = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    $current = $result
    while ($current) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse points are not allowed in package paths.' }
        $current = [IO.Path]::GetDirectoryName($current)
    }
    return $result
}
function Assert-File([string]$Path, [string]$Hash, [long]$Bytes = -1) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required package file is missing: $([IO.Path]::GetFileName($Path))" }
    if ($Bytes -ge 0 -and (Get-Item -LiteralPath $Path).Length -ne $Bytes) { throw 'Package file size mismatch.' }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) { throw "Package hash mismatch: $([IO.Path]::GetFileName($Path))" }
}
function Assert-Package([string]$Root, $Entries, [bool]$Runtime) {
    $expected = @($Entries | ForEach-Object { $_.path })
    if ($Runtime) { $expected += @((@($manifest.licenses) + @($manifest.metadataFiles)) | ForEach-Object { $_.path }); $expected += 'manifest.json' }
    foreach ($item in Get-ChildItem -LiteralPath $Root -Recurse -Force -File) {
        $relative = [IO.Path]::GetRelativePath($Root, $item.FullName).Replace('\', '/')
        if ($relative -cnotin $expected) { throw 'Unexpected file in package tree.' }
    }
    foreach ($entry in $Entries) { Assert-File (Get-SafeChild $Root $entry.path) $entry.sha256 $entry.bytes }
    if ($Runtime) {
        foreach ($entry in (@($manifest.licenses) + @($manifest.metadataFiles))) { Assert-File (Get-SafeChild $Root $entry.path) $entry.sha256 }
        Assert-File (Get-SafeChild $Root 'manifest.json') (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    }
}
$Destination = [IO.Path]::GetFullPath($Destination)
# One absolute cache root keeps gh, Invoke-WebRequest and the .NET file APIs on the same files.
$CacheDirectory = [IO.Path]::GetFullPath($CacheDirectory)
if ($SdkDestination) { $SdkDestination = [IO.Path]::GetFullPath($SdkDestination) }
if ($VerifyOnly) {
    Assert-Package $Destination $manifest.files $true
    if ($SdkDestination) { Assert-Package $SdkDestination $manifest.sdkFiles $false }
    Write-Output 'Verified qpdf 12.4.2 package hashes and license notices.'
    return
}
$targets = @(@{root=$Destination; entries=$manifest.files; runtime=$true})
if ($SdkDestination) { $targets += @{root=$SdkDestination; entries=$manifest.sdkFiles; runtime=$false} }
$pending = @()
foreach ($target in $targets) {
    if (Test-Path -LiteralPath $target.root) {
        # Existing trees are verified, never repaired in place or silently replaced.
        Assert-Package $target.root $target.entries $target.runtime
    } else { $pending += $target }
}
if (!$pending.Count) { Write-Output 'Reused verified qpdf package.'; return }
$archive = Get-SafeChild ([IO.Path]::GetFullPath($CacheDirectory)) $manifest.archive
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($archive)) | Out-Null
if (!(Test-Path -LiteralPath $archive)) {
    if ($Offline) { throw 'Pinned qpdf archive is unavailable in the offline cache.' }
    $download = Join-Path $CacheDirectory ('download-' + [Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($download) | Out-Null
    $downloaded = Join-Path $download $manifest.archive
    # A token or signed-in GitHub CLI uses the release API first. Without one,
    # or when that download fails, the same official release asset is fetched
    # over HTTPS, so a local build needs no GitHub sign-in. Whichever copy
    # arrives must still match the pinned size and SHA-256 below.
    $gh = Get-Command gh -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $fetched = $false
    if ($gh) {
        $signedIn = [bool]($env:GH_TOKEN -or $env:GITHUB_TOKEN)
        if (!$signedIn) { & $gh.Source auth status *> $null; $signedIn = $LASTEXITCODE -eq 0 }
        if ($signedIn) {
            & $gh.Source release download $manifest.tag --repo $manifest.repository --pattern $manifest.archive --dir $download
            $fetched = $LASTEXITCODE -eq 0
            if (!$fetched) { Write-Warning "gh release download exited with $LASTEXITCODE; fetching the same release asset over HTTPS." }
        }
    }
    if (!$fetched) {
        $uri = "https://github.com/$($manifest.repository)/releases/download/$($manifest.tag)/$($manifest.archive)"
        try { Invoke-WebRequest -Uri $uri -OutFile $downloaded -MaximumRetryCount 3 -RetryIntervalSec 15 }
        catch { throw "Official qpdf release download failed: $($_.Exception.Message)" }
    }
    Assert-File $downloaded $manifest.archive_sha256 $manifest.archive_bytes
    [IO.File]::Move($downloaded, $archive, $false)
    # Only the verified archive is kept; the per-download directory is now empty.
    Remove-Item -LiteralPath $download -Recurse -Force -ErrorAction SilentlyContinue
}
Assert-File $archive $manifest.archive_sha256 $manifest.archive_bytes
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    foreach ($target in $pending) {
        # A private sibling stage becomes visible by one rename only after complete verification.
        $stage = $target.root + '.stage-' + [Guid]::NewGuid().ToString('N')
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName((Get-SafeChild $stage 'manifest.json'))) | Out-Null
        foreach ($entry in $target.entries) {
            $matches = @($zip.Entries | Where-Object FullName -CEQ $entry.archivePath)
            if ($matches.Count -ne 1 -or $matches[0].Length -ne $entry.bytes) { throw 'Pinned archive entry missing, duplicated, or wrong size.' }
            $output = Get-SafeChild $stage $entry.path
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($output)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($matches[0], $output, $false)
            Assert-File $output $entry.sha256 $entry.bytes
        }
        if ($target.runtime) {
            foreach ($license in (@($manifest.licenses) + @($manifest.metadataFiles))) {
                $output = Get-SafeChild $stage $license.path
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($output)) | Out-Null
                [IO.File]::WriteAllText($output, $license.text, [Text.UTF8Encoding]::new($false))
            }
            Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $stage 'manifest.json')
        }
        Assert-Package $stage $target.entries $target.runtime
        [IO.Directory]::Move($stage, $target.root)
        Write-Output "Staged verified qpdf 12.4.2 package: $($target.root)"
    }
} finally { $zip.Dispose() }
