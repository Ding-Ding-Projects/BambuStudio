[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^md3-v\d+$')][string] $Tag,
    [Parameter(Mandatory)][ValidatePattern('^[^/]+/[^/]+$')][string] $Repository,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{40}$')][string] $ExpectedCommit,
    [Parameter(Mandatory)][string] $OutputPath,
    [Parameter(Mandatory)][switch] $CiExecutionApproved
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not $CiExecutionApproved -or $env:GITHUB_ACTIONS -ne 'true' -or
    $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or -not $env:RUNNER_TEMP) {
    throw 'Isolated Squirrel installation requires an explicitly approved disposable GitHub-hosted Windows runner.'
}

function Assert-True {
    param([bool] $Condition, [string] $Message)
    if (-not $Condition) { throw $Message }
}

function Get-EntrySha256 {
    param([Parameter(Mandatory)][System.IO.Compression.ZipArchiveEntry] $Entry)
    $stream = $Entry.Open()
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
        $stream.Dispose()
    }
}

$receipt = [ordered]@{
    schema = 1
    source_commit = $ExpectedCommit.ToLowerInvariant()
    release_tag = $Tag
    runner = 'github-hosted-windows'
    status = 'failed'
}

try {
    $release = & gh release view $Tag --repo $Repository --json tagName,targetCommitish,isDraft,assets | ConvertFrom-Json
    Assert-True ($LASTEXITCODE -eq 0) 'Could not read the published release.'
    Assert-True (-not $release.isDraft) 'The release is still a draft.'
    Assert-True ($release.targetCommitish -ceq $ExpectedCommit) 'The release target does not match the expected source commit.'

    $downloadRoot = Join-Path $env:RUNNER_TEMP ('bambu-release-install-' + $env:GITHUB_RUN_ID)
    Assert-True (-not (Test-Path -LiteralPath $downloadRoot)) 'The isolated download directory already exists.'
    New-Item -ItemType Directory -Path $downloadRoot | Out-Null
    & gh release download $Tag --repo $Repository --dir $downloadRoot
    Assert-True ($LASTEXITCODE -eq 0) 'Downloading the published release assets failed.'

    $required = @('Setup.exe', 'RELEASES', 'Setup.exe.sha256', 'BambuStudioMD3.cdx.json')
    foreach ($name in $required) {
        Assert-True (Test-Path -LiteralPath (Join-Path $downloadRoot $name) -PathType Leaf) "Missing published asset '$name'."
    }
    $packages = @(Get-ChildItem -LiteralPath $downloadRoot -File | Where-Object { $_.Name -match '^BambuStudioMD3-\d+\.\d+\.\d+-full\.nupkg$' })
    Assert-True ($packages.Count -eq 1) 'Expected exactly one published full Squirrel package.'
    $package = $packages[0]
    $version = [regex]::Match($package.Name, '^BambuStudioMD3-(\d+\.\d+\.\d+)-full\.nupkg$').Groups[1].Value
    $expectedNames = @($required + $package.Name | Sort-Object)
    $actualNames = @(Get-ChildItem -LiteralPath $downloadRoot -File | ForEach-Object Name | Sort-Object)
    Assert-True ([string]::Join('|', $actualNames) -ceq [string]::Join('|', $expectedNames)) 'Published release asset names do not match the Squirrel contract.'

    $assetHashes = [ordered]@{}
    foreach ($name in $actualNames) {
        $asset = @($release.assets | Where-Object name -CEQ $name)
        Assert-True ($asset.Count -eq 1) "Release metadata is missing '$name'."
        $file = Get-Item -LiteralPath (Join-Path $downloadRoot $name)
        Assert-True ($file.Length -eq $asset[0].size) "Published asset '$name' has the wrong byte length."
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        Assert-True ($asset[0].digest -ceq "sha256:$hash") "Published asset '$name' failed its SHA-256 digest."
        $assetHashes[$name] = $hash
    }

    & (Join-Path $PSScriptRoot 'Test-SquirrelWindowsPackage.ps1') `
        -Installer (Join-Path $downloadRoot 'Setup.exe') `
        -Releases (Join-Path $downloadRoot 'RELEASES') `
        -FullPackage $package.FullName `
        -Checksum (Join-Path $downloadRoot 'Setup.exe.sha256') `
        -Sbom (Join-Path $downloadRoot 'BambuStudioMD3.cdx.json') `
        -SourceCommit $ExpectedCommit -CiExecutionApproved

    $indexRows = @(Get-Content -LiteralPath (Join-Path $downloadRoot 'RELEASES') | Where-Object { $_.Trim() })
    Assert-True ($indexRows.Count -eq 1) 'Expected exactly one Squirrel RELEASES row.'
    $indexRow = [regex]::Match($indexRows[0], '^(?<sha1>[0-9a-fA-F]{40}) (?<name>[^\\/\s]+) (?<size>\d+)$')
    Assert-True ($indexRow.Success) 'The Squirrel RELEASES row is malformed or contains a path.'
    Assert-True ($indexRow.Groups['name'].Value -ceq $package.Name) 'The Squirrel RELEASES row names the wrong package.'
    Assert-True ([long]$indexRow.Groups['size'].Value -eq $package.Length) 'The Squirrel RELEASES row has the wrong byte length.'
    $sha1 = (Get-FileHash -LiteralPath $package.FullName -Algorithm SHA1).Hash
    Assert-True ($indexRow.Groups['sha1'].Value -ieq $sha1) 'The Squirrel RELEASES row has the wrong SHA-1.'

    $installRoot = Join-Path $env:LOCALAPPDATA 'BambuStudioMD3'
    Assert-True (-not (Test-Path -LiteralPath $installRoot)) 'A prior Squirrel installation exists on this runner.'
    $setup = Start-Process -FilePath (Join-Path $downloadRoot 'Setup.exe') -ArgumentList '--silent' -PassThru -WindowStyle Hidden
    Wait-Process -Id $setup.Id -Timeout 600
    $setup.Refresh()
    Assert-True ($setup.ExitCode -eq 0) "Squirrel Setup.exe exited with code $($setup.ExitCode)."
    $installedExe = Join-Path (Join-Path $installRoot "app-$version") 'bambu-studio.exe'
    for ($attempt = 0; $attempt -lt 60 -and -not (Test-Path -LiteralPath $installedExe -PathType Leaf); ++$attempt) {
        Start-Sleep -Seconds 5
    }
    Assert-True (Test-Path -LiteralPath $installedExe -PathType Leaf) 'The expected installed executable did not appear.'
    Assert-True (Test-Path -LiteralPath (Join-Path $installRoot 'Update.exe') -PathType Leaf) 'The installed Squirrel updater is missing.'

    $archive = [System.IO.Compression.ZipFile]::OpenRead($package.FullName)
    try {
        $entry = @($archive.Entries | Where-Object { $_.FullName.Replace('\', '/') -ieq 'lib/net45/bambu-studio.exe' })
        Assert-True ($entry.Count -eq 1) 'The full package has no unique application executable.'
        $packageExeHash = Get-EntrySha256 -Entry $entry[0]
    }
    finally {
        $archive.Dispose()
    }
    $installedExeHash = (Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-True ($installedExeHash -ceq $packageExeHash) 'The installed executable differs from the downloaded full package.'

    $receipt.package_version = $version
    $receipt.product_version = (Get-Item -LiteralPath $installedExe).VersionInfo.ProductVersion
    $receipt.installed_exe_sha256 = $installedExeHash
    $receipt.package_exe_sha256 = $packageExeHash
    $receipt.asset_sha256 = $assetHashes
    $receipt.status = 'verified'
    Write-Host "Verified isolated Squirrel installation for $ExpectedCommit at package version $version."
}
catch {
    $receipt.failure = $_.Exception.Message
    throw
}
finally {
    $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputPath -Encoding utf8
}
