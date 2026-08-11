[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Installer,

    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Releases,

    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $FullPackage,

    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Checksum,

    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $Sbom,

    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $SourceCommit,

    [Parameter(Mandatory)][switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-True {
    param([Parameter(Mandatory)][bool] $Condition, [Parameter(Mandatory)][string] $Message)
    if (-not $Condition) { throw $Message }
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

if (-not $CiExecutionApproved -or $env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This script validates unsigned Squirrel.Windows artifacts only on an explicitly approved disposable GitHub-hosted runner.'
}

$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$releasesPath = (Resolve-Path -LiteralPath $Releases).Path
$packagePath = (Resolve-Path -LiteralPath $FullPackage).Path
$checksumPath = (Resolve-Path -LiteralPath $Checksum).Path
$sbomPath = (Resolve-Path -LiteralPath $Sbom).Path

$certificateTable = Get-PeCertificateTable -Path $installerPath
Assert-True ($certificateTable.Size -eq 0) "Squirrel Setup.exe is not unsigned; its PE security directory contains $($certificateTable.Size) certificate bytes."
$actualHash = Get-Sha256Lower -Path $installerPath
$expectedChecksum = "$actualHash *Setup.exe"
Assert-True ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -ceq $expectedChecksum) 'Setup.exe SHA-256 sidecar does not match the installer.'

$releaseText = Get-Content -LiteralPath $releasesPath -Raw
Assert-True ($releaseText -match [regex]::Escape((Split-Path -Leaf $packagePath))) 'RELEASES does not reference the full package.'

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $appEntry = @($archive.Entries | Where-Object {
        $_.FullName.Replace('\', '/').TrimStart('/') -ieq 'lib/net45/bambu-studio.exe'
    })
    $nuspecEntry = @($archive.Entries | Where-Object { $_.FullName -match '\.nuspec$' })
    Assert-True ($appEntry.Count -eq 1 -and $appEntry[0].Length -gt 0) 'The Squirrel full package is missing lib/net45/bambu-studio.exe.'
    Assert-True ($nuspecEntry.Count -eq 1) 'The Squirrel full package must contain exactly one nuspec.'
    $reader = New-Object System.IO.StreamReader($nuspecEntry[0].Open())
    try {
        $nuspecText = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }
    Assert-True ($nuspecText.Contains($SourceCommit)) 'The Squirrel nuspec does not carry the exact source commit.'
}
finally {
    $archive.Dispose()
}

$sbomDocument = Get-Content -LiteralPath $sbomPath -Raw | ConvertFrom-Json
Assert-True ([string]$sbomDocument.metadata.component.version -ne '') 'The CycloneDX SBOM has no product version.'
Assert-True ($sbomDocument.components.Count -gt 0) 'The CycloneDX SBOM has no payload components.'

Write-Host "Squirrel.Windows package validation passed for source commit $SourceCommit."
