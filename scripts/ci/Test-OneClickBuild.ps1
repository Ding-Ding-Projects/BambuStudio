[CmdletBinding()]
param(
    [string] $RepositoryRoot = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-True {
    param(
        [Parameter(Mandatory)][bool] $Condition,
        [Parameter(Mandatory)][string] $Message
    )
    if (-not $Condition) { throw $Message }
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}
$RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$launcher = Join-Path $RepositoryRoot 'OneClickBuildInstaller.cmd'
$script = Join-Path $RepositoryRoot 'scripts\windows\Invoke-OneClickBuild.ps1'
$packager = Join-Path $RepositoryRoot 'scripts\windows\Invoke-SquirrelPackage.ps1'

Assert-True (Test-Path -LiteralPath $launcher -PathType Leaf) 'The one-click CMD launcher is missing.'
Assert-True (Test-Path -LiteralPath $script -PathType Leaf) 'The one-click PowerShell orchestrator is missing.'
Assert-True (Test-Path -LiteralPath $packager -PathType Leaf) 'The Squirrel package PowerShell script is missing.'

$tokens = $null
$errors = $null
$text = Get-Content -LiteralPath $script -Raw
$null = [System.Management.Automation.Language.Parser]::ParseInput(
    $text,
    [ref]$tokens,
    [ref]$errors
)
Assert-True ($errors.Count -eq 0) "The one-click PowerShell script has parse errors: $errors"

foreach ($required in @(
    'Install-VisualCppBuildTools',
    'Install-CMake',
    '7zip.7zip',
    'git lfs pull',
    'System.Threading.Mutex',
    'build_win.bat',
    'cmake.exe --install',
    'Add-MesaFallback',
    'New-WindowsCycloneDxSbom.ps1',
    'Invoke-SquirrelPackage.ps1',
    'Squirrel.Windows',
    'RELEASES',
    'Setup.exe',
    '$checksum = "$installer.sha256"'
)) {
    Assert-True $text.Contains($required) "The one-click workflow is missing required contract '$required'."
}

Assert-True (-not $text.Contains('NSIS.NSIS')) 'The one-click workflow must not bootstrap NSIS after the Squirrel.Windows migration.'
Assert-True (-not $text.Contains('Get-MakeNsisPath')) 'The one-click workflow must not invoke the retired NSIS compiler path.'

$packagerText = Get-Content -LiteralPath $packager -Raw
foreach ($required in @(
    'Write-Utf8NoBom',
    'Write-SquirrelZipArchive',
    "Replace('\', '/')"
)) {
    Assert-True $packagerText.Contains($required) "The Squirrel package workflow is missing required contract '$required'."
}

$launcherText = Get-Content -LiteralPath $launcher -Raw
Assert-True $launcherText.Contains('Invoke-OneClickBuild.ps1') 'The CMD launcher does not call the PowerShell workflow.'
Assert-True $launcherText.Contains('BAMBU_ONE_CLICK_NO_PAUSE') 'The CMD launcher lacks the automation-safe no-pause escape hatch.'

Write-Host 'One-click Windows build static checks passed.'
