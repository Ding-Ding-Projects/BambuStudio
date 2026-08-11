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
Import-Module Microsoft.PowerShell.Security -ErrorAction Stop

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

if (-not $CiExecutionApproved -or $env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This script validates unsigned Squirrel.Windows artifacts only on an explicitly approved disposable GitHub-hosted runner.'
}

$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$releasesPath = (Resolve-Path -LiteralPath $Releases).Path
$packagePath = (Resolve-Path -LiteralPath $FullPackage).Path
$checksumPath = (Resolve-Path -LiteralPath $Checksum).Path
$sbomPath = (Resolve-Path -LiteralPath $Sbom).Path

$signature = Get-AuthenticodeSignature -LiteralPath $installerPath
Assert-True ($signature.Status -eq 'NotSigned') "Squirrel Setup.exe is not unsigned; Authenticode status is '$($signature.Status)'."
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
