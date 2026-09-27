param(
    [Parameter(Mandatory = $true)]
    [string] $PayloadDirectory,

    [string] $CacheDirectory = (Join-Path $env:LOCALAPPDATA 'BambuStudioRendererCache')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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
        $downloadHash = (Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($downloadHash -cne $renderer.Sha256) {
            throw "$($renderer.Name) $($renderer.Version) archive SHA-256 mismatch: $downloadHash"
        }
        Move-Item -LiteralPath $download -Destination $archive
    }

    $actualHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -cne $renderer.Sha256) {
        throw "$($renderer.Name) $($renderer.Version) cached archive SHA-256 mismatch: $actualHash"
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
    $executable = Join-Path $destination $renderer.Executable
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "$($renderer.Name) $($renderer.Version) archive did not stage $($renderer.Executable)."
    }
    Write-Host "Staged $($renderer.Name) $($renderer.Version) from verified archive $actualHash at $executable."
}
