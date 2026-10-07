[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/ci/Test-InkTerminology.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/ci/Test-InkTerminology.ps1')) -join "`n" }
$lines = $source -split '\r?\n'
$selection = @($lines | Where-Object { $_ -match '^\s*(?:\[string\[\]\]\s*)?\$python = if ' })
$invocation = @($lines | Where-Object { $_ -match '^\$overrideOutput = & \$python\[0\]' })
if ($selection.Count -ne 1 -or $invocation.Count -ne 1) { throw 'Expected one interpreter selection and one invocation.' }
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Get-Command {
    [CmdletBinding()]
    param([string] $Name)
    if ($Name -ne 'py') { throw 'Unexpected interpreter discovery query.' }
    if ($script:hasLauncher) { return [pscustomobject]@{ Name = 'py' } }
}
function py { $script:invoked = 'py'; $script:received = @($args | ForEach-Object { foreach ($value in $_) { $value } }); return 'FIXTURE_OK' }
function python { $script:invoked = 'python'; $script:received = @($args | ForEach-Object { foreach ($value in $_) { $value } }); return 'FIXTURE_OK' }
foreach ($route in @('py', 'python')) {
    $script:hasLauncher = $route -eq 'py'
    $script:invoked = ''
    $script:received = @()
    $overrideCheck = 'fixture with spaces.py'
    Invoke-Expression $selection[0]
    Invoke-Expression $invocation[0]
    Assert-True ($script:invoked -eq $route -and $overrideOutput -eq 'FIXTURE_OK') 'The complete interpreter command must be invoked in both routes.'
    Assert-True ($python -is [array]) 'The selected interpreter arguments must remain an array, including the singleton route.'
    $expected = if ($route -eq 'py') { '-3|fixture with spaces.py' } else { 'fixture with spaces.py' }
    Assert-True (($script:received -join '|') -eq $expected) 'Only the launcher route must receive -3, followed by the intact script path.'
}
Write-Host 'Ink Python invocation checks passed (6 assertions; real PowerShell selection/invocation with fixture commands only).'
