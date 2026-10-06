[CmdletBinding()]
param([string] $RepositoryRoot = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuEntryPoint-' + [guid]::NewGuid())))
try {
    $scripts = Join-Path $fixture 'scripts/windows'
    New-Item $scripts -ItemType Directory -Force | Out-Null
    Copy-Item (Join-Path $RepositoryRoot 'scripts/windows/Invoke-BuildEntryPoint.ps1') $scripts
    foreach ($entry in @('build.bat', 'build-installer.bat')) { Copy-Item (Join-Path $RepositoryRoot $entry) $fixture }
    @'
param([switch] $Plan, [switch] $BuildOnly, [string] $OutputDirectory, [int] $ReleaseNumber, [string] $PreviousPackageVersion)
@{ Plan = [bool]$Plan; BuildOnly = [bool]$BuildOnly; ReleaseNumber = $ReleaseNumber; PreviousPackageVersion = $PreviousPackageVersion } | ConvertTo-Json | Set-Content $OutputDirectory
exit 37
'@ | Set-Content (Join-Path $scripts 'Invoke-OneClickBuild.ps1')
    foreach ($entry in @('build.bat', 'build-installer.bat')) {
        $receipt = Join-Path $fixture ($entry + ' receipt.json')
        & (Join-Path $fixture $entry) /s -Plan -OutputDirectory $receipt -ReleaseNumber 238 -PreviousPackageVersion '2.8.4814'
        Assert-True ($LASTEXITCODE -eq 37) "$entry lost the child exit code."
        $data = Get-Content $receipt -Raw | ConvertFrom-Json
        Assert-True ($data.Plan -and $data.ReleaseNumber -eq 238 -and $data.PreviousPackageVersion -eq '2.8.4814') "$entry lost named arguments."
        Assert-True ($data.BuildOnly -eq ($entry -eq 'build.bat')) "$entry selected the wrong production route."
    }
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary directory.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Build entrypoint forwarding checks passed (6 assertions; stub producer only).'
