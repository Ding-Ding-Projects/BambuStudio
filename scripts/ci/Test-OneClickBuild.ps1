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

Assert-True (Test-Path -LiteralPath $launcher -PathType Leaf) 'The one-click CMD launcher is missing.'
Assert-True (Test-Path -LiteralPath $script -PathType Leaf) 'The one-click PowerShell orchestrator is missing.'

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
    'NSIS.NSIS',
    '7zip.7zip',
    'git lfs pull',
    'System.Threading.Mutex',
    'build_win.bat',
    'cmake.exe --install',
    'Add-MesaFallback',
    'New-WindowsCycloneDxSbom.ps1',
    'GenerateUninstallInclude.ps1',
    '/INPUTCHARSET UTF8',
    '$checksum = "$installer.sha256"'
)) {
    Assert-True $text.Contains($required) "The one-click workflow is missing required contract '$required'."
}

$launcherText = Get-Content -LiteralPath $launcher -Raw
Assert-True $launcherText.Contains('Invoke-OneClickBuild.ps1') 'The CMD launcher does not call the PowerShell workflow.'
Assert-True $launcherText.Contains('BAMBU_ONE_CLICK_NO_PAUSE') 'The CMD launcher lacks the automation-safe no-pause escape hatch.'

Write-Host 'One-click Windows build static checks passed.'
