<#
.SYNOPSIS
    Build an unsigned Squirrel.Windows package from an installed payload.

.DESCRIPTION
    Creates a NuGet package with the application under lib\net45, then runs the
    hash-pinned Squirrel.Windows releasify command. The output contains the
    standard Setup.exe, RELEASES index, full package, and any delta packages
    Squirrel produces. No signing command or signing credential is accepted.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string] $PayloadDirectory,

    [Parameter(Mandatory)][string] $OutputDirectory,

    [Parameter(Mandatory)][string] $ProductVersion,

    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $SourceCommit,

    [Parameter(Mandatory)][ValidatePattern('^https://')]
    [string] $Repository,

    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $IconPath,

    [string] $PackageId = 'BambuStudioMD3',

    [string] $SquirrelVersion = '2.0.1'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression

$script:SquirrelPackageSha256 = '923e18abb4fd50b5a4878a39dbcd042ed3f7eb68fc0f82c0955cd5380c921ac7'
$script:SquirrelPackageUri = "https://api.nuget.org/v3-flatcontainer/squirrel.windows/$SquirrelVersion/squirrel.windows.$SquirrelVersion.nupkg"
$script:TempPrefix = 'BambuStudio-Squirrel-'

function Write-SquirrelLog {
    param([Parameter(Mandatory)][string] $Message)
    Write-Host ('[{0}] {1}' -f (Get-Date).ToString('u'), $Message)
}

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

function Get-PeCertificateTable {
    param([Parameter(Mandatory)][string] $Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "'$Path' is not a PE executable (missing MZ header)."
        }
        $stream.Seek(0x3c, [System.IO.SeekOrigin]::Begin) | Out-Null
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset -gt ($stream.Length - 256)) {
            throw "'$Path' has an invalid PE header offset."
        }
        $stream.Seek($peOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "'$Path' is not a PE executable (missing PE signature)."
        }
        $stream.Seek(16, [System.IO.SeekOrigin]::Current) | Out-Null
        $optionalHeaderSize = $reader.ReadUInt16()
        $reader.ReadUInt16() | Out-Null
        $optionalHeaderStart = $stream.Position
        $magic = $reader.ReadUInt16()
        $dataDirectoryOffset = if ($magic -eq 0x10b) { 96 } elseif ($magic -eq 0x20b) { 112 } else {
            throw "'$Path' has an unsupported PE optional-header format."
        }
        $certificateEntry = $optionalHeaderStart + $dataDirectoryOffset + (8 * 4)
        if ($certificateEntry + 8 -gt $optionalHeaderStart + $optionalHeaderSize) {
            throw "'$Path' has no complete PE security directory."
        }
        $stream.Seek($certificateEntry, [System.IO.SeekOrigin]::Begin) | Out-Null
        return [pscustomobject]@{
            Offset = $reader.ReadUInt32()
            Size = $reader.ReadUInt32()
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Invoke-DownloadWithRetry {
    param(
        [Parameter(Mandatory)][string] $Uri,
        [Parameter(Mandatory)][string] $OutFile,
        [int] $MaxAttempts = 3,
        [int] $RetryDelaySeconds = 15
    )

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing
            return
        }
        catch {
            if ($attempt -eq $MaxAttempts) {
                throw
            }
            Write-SquirrelLog "Download attempt $attempt failed; retrying in $RetryDelaySeconds seconds."
            Start-Sleep -Seconds $RetryDelaySeconds
        }
    }
}

function ConvertTo-SquirrelVersion {
    param([Parameter(Mandatory)][string] $Version)
    $parts = $Version.Trim().Split('.')
    if ($parts.Count -lt 2 -or $parts.Count -gt 4 -or ($parts | Where-Object { $_ -notmatch '^\d+$' })) {
        throw "Product version '$Version' is not a numeric Squirrel-compatible version."
    }
    $normalizedParts = @($parts | ForEach-Object { ([int] $_).ToString() })
    if ($normalizedParts.Count -eq 4) {
        return (($normalizedParts[0..2] -join '.') + '-build' + $normalizedParts[3])
    }
    return ($normalizedParts -join '.')
}

function Get-SafeXmlText {
    param([Parameter(Mandatory)][string] $Text)
    return [System.Security.SecurityElement]::Escape($Text)
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory)][string] $Path,
        [Parameter(Mandatory)][string] $Value
    )

    [System.IO.File]::WriteAllText(
        $Path,
        $Value,
        (New-Object System.Text.UTF8Encoding($false))
    )
}

function Write-SquirrelZipArchive {
    param(
        [Parameter(Mandatory)][string] $SourceDirectory,
        [Parameter(Mandatory)][string] $ArchivePath
    )

    $archive = [System.IO.Compression.ZipFile]::Open(
        $ArchivePath,
        [System.IO.Compression.ZipArchiveMode]::Create
    )
    try {
        $sourceRoot = [System.IO.Path]::GetFullPath($SourceDirectory).TrimEnd('\') + '\'
        foreach ($item in @(Get-ChildItem -LiteralPath $SourceDirectory -Recurse -File | Sort-Object FullName)) {
            $entryName = $item.FullName.Substring($sourceRoot.Length).Replace('\', '/')
            $entry = $archive.CreateEntry(
                $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal
            )
            $input = [System.IO.File]::OpenRead($item.FullName)
            $output = $entry.Open()
            try {
                $input.CopyTo($output)
            }
            finally {
                $output.Dispose()
                $input.Dispose()
            }
        }
    }
    finally {
        $archive.Dispose()
    }
}

function Resolve-SquirrelTool {
    param([Parameter(Mandatory)][string] $Version)

    $localPackageRoot = Join-Path ([Environment]::GetFolderPath('UserProfile')) ".nuget\packages\squirrel.windows\$Version\tools"
    $cachePackageRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) "BambuStudio\toolcache\squirrel.windows\$Version"
    foreach ($candidate in @(
        (Join-Path $localPackageRoot 'Squirrel.exe'),
        (Join-Path $cachePackageRoot 'tools\Squirrel.exe')
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    $temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ($script:TempPrefix + [guid]::NewGuid().ToString('N'))
    $archive = Join-Path $temporaryRoot "squirrel.windows.$Version.nupkg"
    try {
        New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
        Write-SquirrelLog "Downloading Squirrel.Windows $Version from NuGet..."
        Invoke-DownloadWithRetry -Uri $script:SquirrelPackageUri -OutFile $archive
        $actual = Get-Sha256Lower -Path $archive
        if ($actual -cne $script:SquirrelPackageSha256) {
            throw "Squirrel.Windows package SHA-256 mismatch; expected $($script:SquirrelPackageSha256), got $actual."
        }

        $extractRoot = Join-Path $temporaryRoot 'package'
        [System.IO.Compression.ZipFile]::ExtractToDirectory($archive, $extractRoot)
        $squirrel = Join-Path $extractRoot 'tools\Squirrel.exe'
        if (-not (Test-Path -LiteralPath $squirrel -PathType Leaf)) {
            throw 'The Squirrel.Windows package did not contain tools\Squirrel.exe.'
        }
        $cacheParent = Split-Path -Parent $cachePackageRoot
        New-Item -ItemType Directory -Path $cacheParent -Force | Out-Null
        if (-not (Test-Path -LiteralPath $cachePackageRoot)) {
            Move-Item -LiteralPath $extractRoot -Destination $cachePackageRoot
        }
        $cached = Join-Path $cachePackageRoot 'tools\Squirrel.exe'
        if (-not (Test-Path -LiteralPath $cached -PathType Leaf)) {
            throw "Squirrel.Windows extraction completed but '$cached' is missing."
        }
        return $cached
    }
    finally {
        if (Test-Path -LiteralPath $temporaryRoot) {
            $resolved = [IO.Path]::GetFullPath($temporaryRoot)
            $temporaryRootPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
            $temporaryName = [IO.Path]::GetFileName($resolved)
            if (-not $resolved.StartsWith($temporaryRootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
                -not $temporaryName.StartsWith($script:TempPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove unexpected temporary directory '$resolved'."
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}

function New-SquirrelNuGetPackage {
    param(
        [Parameter(Mandatory)][string] $Payload,
        [Parameter(Mandatory)][string] $PackageRoot,
        [Parameter(Mandatory)][string] $Id,
        [Parameter(Mandatory)][string] $Version,
        [Parameter(Mandatory)][string] $Commit,
        [Parameter(Mandatory)][string] $RepositoryUrl,
        [Parameter(Mandatory)][string] $NupkgPath
    )

    $appDirectory = Join-Path $PackageRoot 'lib\net45'
    New-Item -ItemType Directory -Path $appDirectory -Force | Out-Null
    foreach ($item in @(Get-ChildItem -LiteralPath $Payload -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $appDirectory -Recurse -Force
    }

    $escapedId = Get-SafeXmlText -Text $Id
    $escapedVersion = Get-SafeXmlText -Text $Version
    $escapedRepository = Get-SafeXmlText -Text $RepositoryUrl
    $description = Get-SafeXmlText -Text "Bambu Studio MD3 Windows application built from source commit $Commit."
    # Squirrel.Windows reads release notes through its NuGet manifest parser.  It
    # expects the notes payload to be CDATA-wrapped; plain text here makes
    # NuGet.ZipPackage report that the package has no manifest even though the
    # .nuspec file is present at the archive root.
    $releaseNotes = Get-SafeXmlText -Text "<![CDATA[
<p>Source commit: $Commit; repository: $RepositoryUrl</p>
]]>"
    $nuspec = @"
<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://schemas.microsoft.com/packaging/2010/07/nuspec.xsd">
  <metadata>
    <id>$escapedId</id>
    <version>$escapedVersion</version>
    <title>Bambu Studio MD3</title>
    <authors>codingmachineedge</authors>
    <owners>codingmachineedge</owners>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>$description</description>
    <releaseNotes>$releaseNotes</releaseNotes>
    <projectUrl>$escapedRepository</projectUrl>
  </metadata>
</package>
"@
    Write-Utf8NoBom -Path (Join-Path $PackageRoot "$Id.nuspec") -Value $nuspec
    $corePropertiesDirectory = Join-Path $PackageRoot 'package\services\metadata\core-properties'
    New-Item -ItemType Directory -Path (Join-Path $PackageRoot '_rels'),$corePropertiesDirectory -Force | Out-Null
    $corePropertiesName = "$Id.psmdcp"
    $coreProperties = @"
<?xml version="1.0" encoding="utf-8"?>
<coreProperties xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns="http://schemas.openxmlformats.org/package/2006/metadata/core-properties">
  <dc:creator>codingmachineedge</dc:creator>
  <dc:description>Bambu Studio MD3 Windows application</dc:description>
  <dc:identifier>$escapedId</dc:identifier>
  <version>$escapedVersion</version>
  <lastModifiedBy>BambuStudio Squirrel package builder</lastModifiedBy>
</coreProperties>
"@
    Write-Utf8NoBom -Path (Join-Path $corePropertiesDirectory $corePropertiesName) -Value $coreProperties
    $relationships = @"
<?xml version="1.0" encoding="utf-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Type="http://schemas.microsoft.com/packaging/2010/07/manifest" Target="/$Id.nuspec" Id="Rmanifest" />
  <Relationship Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="/package/services/metadata/core-properties/$corePropertiesName" Id="Rcore" />
</Relationships>
"@
    Write-Utf8NoBom -Path (Join-Path $PackageRoot '_rels\.rels') -Value $relationships
    $extensions = @('rels', 'psmdcp', 'nuspec') + @(
        Get-ChildItem -LiteralPath $PackageRoot -Recurse -File |
            ForEach-Object { $_.Extension.TrimStart('.').ToLowerInvariant() } |
            Where-Object { $_ } |
            Sort-Object -Unique
    ) | Sort-Object -Unique
    $contentTypeLines = foreach ($extension in $extensions) {
        if ($extension -eq 'rels') {
            '  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />'
        } elseif ($extension -eq 'psmdcp') {
            '  <Default Extension="psmdcp" ContentType="application/vnd.openxmlformats-package.core-properties+xml" />'
        } else {
            '  <Default Extension="{0}" ContentType="application/octet" />' -f $extension
        }
    }
    $contentTypes = @(
        '<?xml version="1.0" encoding="utf-8"?>'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        $contentTypeLines
        '</Types>'
    ) -join [Environment]::NewLine
    Write-Utf8NoBom -Path (Join-Path $PackageRoot '[Content_Types].xml') -Value $contentTypes
    $nupkg = [IO.Path]::GetFullPath($NupkgPath)
    Write-SquirrelZipArchive -SourceDirectory $PackageRoot -ArchivePath $nupkg
    return $nupkg
}

function Assert-SquirrelOutputs {
    param(
        [Parameter(Mandatory)][string] $ReleaseDirectory,
        [Parameter(Mandatory)][string] $PackageId,
        [Parameter(Mandatory)][string] $PayloadExecutable
    )

    $setup = Join-Path $ReleaseDirectory 'Setup.exe'
    $releases = Join-Path $ReleaseDirectory 'RELEASES'
    $fullPackages = @(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*-full.nupkg' -File)
    if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) { throw 'Squirrel did not produce Setup.exe.' }
    if (-not (Test-Path -LiteralPath $releases -PathType Leaf)) { throw 'Squirrel did not produce RELEASES.' }
    if ($fullPackages.Count -ne 1) { throw "Expected one Squirrel full package, found $($fullPackages.Count)." }

    $releaseText = Get-Content -LiteralPath $releases -Raw
    if ($releaseText -notmatch [regex]::Escape($fullPackages[0].Name)) {
        throw "RELEASES does not reference '$($fullPackages[0].Name)'."
    }
    $archive = [System.IO.Compression.ZipFile]::OpenRead($fullPackages[0].FullName)
    try {
        $entry = @($archive.Entries | Where-Object {
                $_.FullName.Replace('\', '/').TrimStart('/') -ieq "lib/net45/$PayloadExecutable"
            })
        if ($entry.Count -ne 1 -or $entry[0].Length -le 0) {
            throw "Squirrel full package is missing lib/net45/$PayloadExecutable."
        }
    }
    finally {
        $archive.Dispose()
    }

    $certificateTable = Get-PeCertificateTable -Path $setup
    if ($certificateTable.Size -ne 0) {
        throw "Squirrel Setup.exe is not unsigned; its PE security directory contains $($certificateTable.Size) certificate bytes."
    }
    return @{
        Setup = $setup
        Releases = $releases
        FullPackage = $fullPackages[0].FullName
        DeltaPackages = @(Get-ChildItem -LiteralPath $ReleaseDirectory -Filter '*-delta.nupkg' -File)
    }
}

$resolvedPayload = (Resolve-Path -LiteralPath $PayloadDirectory).Path
$resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
$squirrelVersion = $SquirrelVersion.Trim()
$normalizedVersion = ConvertTo-SquirrelVersion -Version $ProductVersion
$squirrelTool = Resolve-SquirrelTool -Version $squirrelVersion
$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ($script:TempPrefix + [guid]::NewGuid().ToString('N'))
$packageRoot = Join-Path $temporaryRoot 'package'
$nupkgOutput = Join-Path $temporaryRoot 'nupkg'
$releaseDirectory = Join-Path $temporaryRoot 'release'
$finalDirectory = Join-Path $resolvedOutput 'squirrel'

try {
    New-Item -ItemType Directory -Path $temporaryRoot,$nupkgOutput,$releaseDirectory,$finalDirectory -Force | Out-Null
    $nupkg = New-SquirrelNuGetPackage -Payload $resolvedPayload -PackageRoot $packageRoot -Id $PackageId -Version $normalizedVersion -Commit $SourceCommit -RepositoryUrl $Repository -NupkgPath (Join-Path $nupkgOutput "$PackageId.$normalizedVersion.nupkg")
    if (-not (Test-Path -LiteralPath $nupkg -PathType Leaf)) {
        throw "Squirrel input package '$nupkg' was not created."
    }
    Write-SquirrelLog "Verified Squirrel input package: $nupkg"
    Write-SquirrelLog "Releasifying $nupkg with Squirrel.Windows $squirrelVersion..."
    $squirrelProcess = Start-Process -FilePath $squirrelTool -ArgumentList @('--releasify', $nupkg, '--releaseDir', $releaseDirectory, '--no-msi', '--setupIcon', $IconPath) -WindowStyle Hidden -Wait -PassThru
    $squirrelExitCode = $squirrelProcess.ExitCode
    if ($squirrelExitCode -ne 0) {
        throw "Squirrel.Windows releasify failed with exit code $squirrelExitCode."
    }

    $outputs = Assert-SquirrelOutputs -ReleaseDirectory $releaseDirectory -PackageId $PackageId `
        -PayloadExecutable 'bambu-studio.exe'
    foreach ($file in @(Get-ChildItem -LiteralPath $releaseDirectory -File)) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $finalDirectory $file.Name) -Force
    }
    $setupFinal = Join-Path $finalDirectory 'Setup.exe'
    $setupHash = Get-Sha256Lower -Path $setupFinal
    "$setupHash *Setup.exe" | Set-Content -LiteralPath (Join-Path $finalDirectory 'Setup.exe.sha256') -Encoding Ascii
    Write-SquirrelLog "Squirrel Setup.exe: $setupFinal"
    Write-SquirrelLog "Squirrel RELEASES: $(Join-Path $finalDirectory 'RELEASES')"
    Write-SquirrelLog "Squirrel full package: $(Join-Path $finalDirectory (Split-Path -Leaf $outputs.FullPackage))"
    Write-SquirrelLog "Squirrel Setup.exe SHA-256: $setupHash"
    Write-SquirrelLog "Squirrel delta packages: $($outputs.DeltaPackages.Count)"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolved = [IO.Path]::GetFullPath($temporaryRoot)
        $temporaryRootPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        $temporaryName = [IO.Path]::GetFileName($resolved)
        $safeTemporaryPath = $resolved.StartsWith($temporaryRootPrefix, [StringComparison]::OrdinalIgnoreCase) -and
            $temporaryName.StartsWith($script:TempPrefix, [StringComparison]::OrdinalIgnoreCase)
        if (-not $safeTemporaryPath) {
            throw "Refusing to remove unexpected temporary directory '$resolved' (temp prefix '$temporaryRootPrefix', name '$temporaryName', expected prefix '$($script:TempPrefix)')."
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
