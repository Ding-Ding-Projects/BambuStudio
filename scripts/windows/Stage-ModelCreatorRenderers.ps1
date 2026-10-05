param(
    [Parameter(Mandatory = $true)]
    [string] $PayloadDirectory,

    [string] $CacheDirectory = (Join-Path $env:LOCALAPPDATA 'BambuStudioRendererCache')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-Sha256Lower {
    param([Parameter(Mandatory)][string] $Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

# Use framework ZIP support without depending on optional PowerShell modules.
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
function Expand-RendererArchive {
    param([string] $ArchivePath, [string] $DestinationPath)
    $rootPath = [IO.Path]::GetFullPath($DestinationPath).TrimEnd('\', '/')
    $prefix = $rootPath + [IO.Path]::DirectorySeparatorChar
    $zip = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        # Resolve every entry before writing any archive content.
        $entries = foreach ($entry in $zip.Entries) {
            if ($entry.FullName.Contains(':') -or [IO.Path]::IsPathRooted($entry.FullName) -or
                (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000) {
                throw "Unsafe renderer archive entry '$($entry.FullName)'."
            }
            $target = [IO.Path]::GetFullPath([IO.Path]::Combine($rootPath, $entry.FullName))
            if (-not $target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Renderer archive entry escapes its destination: '$($entry.FullName)'."
            }
            $ancestor = $target
            while ($ancestor -and $ancestor.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
                if ([IO.File]::Exists($ancestor) -or [IO.Directory]::Exists($ancestor)) {
                    if (([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                        throw "Renderer archive destination contains a reparse point: '$ancestor'."
                    }
                }
                $ancestor = [IO.Path]::GetDirectoryName($ancestor)
            }
            [pscustomobject]@{ Entry = $entry; Target = $target }
        }
        foreach ($item in $entries) {
            if ($item.Entry.FullName.EndsWith('/') -or $item.Entry.FullName.EndsWith('\')) {
                [IO.Directory]::CreateDirectory($item.Target) | Out-Null
                continue
            }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Target)) | Out-Null
            $inputStream = $item.Entry.Open()
            try {
                $outputStream = [IO.File]::Open($item.Target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
                try { $inputStream.CopyTo($outputStream) }
                finally { $outputStream.Dispose() }
            }
            finally { $inputStream.Dispose() }
        }
    }
    finally { $zip.Dispose() }
}

# Portable upstream distributions are staged without an installer or machine-wide changes.
# The archive digests are part of the source review boundary, not learned at build time.
$renderers = @(
    @{
        Name = 'OpenSCAD'
        Version = '2021.01'
        Archive = 'OpenSCAD-2021.01-x86-64.zip'
        Url = 'https://files.openscad.org/OpenSCAD-2021.01-x86-64.zip'
        Sha256 = 'fb0caabf5bbc89f8f2f80c10b79ae64d697aaff6efd58b2756f5d6270edb7ba7'
        Executable = 'openscad-2021.01\openscad.com'
    },
    @{
        Name = 'Blender'
        Version = '5.2.2'
        Archive = 'blender-5.2.2-windows-x64.zip'
        Url = 'https://download.blender.org/release/Blender5.2/blender-5.2.2-windows-x64.zip'
        Sha256 = '3849d17a682cba006075aaa3f3597ecb5c9c30ec31035b2e092c53e40679b535'
        Executable = 'blender-5.2.2-windows-x64\blender.exe'
    }
)

if (-not [IO.Path]::IsPathRooted($PayloadDirectory) -or
    -not [IO.Path]::IsPathRooted($CacheDirectory)) {
    throw 'PayloadDirectory and CacheDirectory must be absolute paths.'
}

$payload = [IO.Path]::GetFullPath($PayloadDirectory)
$cache = [IO.Path]::GetFullPath($CacheDirectory)
$destination = Join-Path $payload 'renderers'
New-Item -ItemType Directory -Path $cache,$destination -Force | Out-Null

foreach ($renderer in $renderers) {
    $archive = Join-Path $cache $renderer.Archive
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
        $download = Join-Path $cache ('.' + $renderer.Archive + '.' + [guid]::NewGuid().ToString('N') + '.download')
        $lastError = $null
        for ($attempt = 1; $attempt -le 3; ++$attempt) {
            try {
                Invoke-WebRequest -Uri $renderer.Url -OutFile $download -UseBasicParsing
                $lastError = $null
                break
            } catch {
                $lastError = $_
                if ($attempt -lt 3) { Start-Sleep -Seconds (3 * $attempt) }
            }
        }
        if ($null -ne $lastError) {
            throw "$($renderer.Name) $($renderer.Version) download failed from $($renderer.Url): $lastError"
        }
        $downloadHash = (Get-Sha256Lower -Path $download)
        if ($downloadHash -cne $renderer.Sha256) {
            throw "$($renderer.Name) $($renderer.Version) archive SHA-256 mismatch: $downloadHash"
        }
        Move-Item -LiteralPath $download -Destination $archive
    }

    $actualHash = (Get-Sha256Lower -Path $archive)
    if ($actualHash -cne $renderer.Sha256) {
        throw "$($renderer.Name) $($renderer.Version) cached archive SHA-256 mismatch: $actualHash"
    }
    Expand-RendererArchive -ArchivePath $archive -DestinationPath $destination
    $executable = Join-Path $destination $renderer.Executable
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "$($renderer.Name) $($renderer.Version) archive did not stage $($renderer.Executable)."
    }
    Write-Host "Staged $($renderer.Name) $($renderer.Version) from verified archive $actualHash at $executable."
}
