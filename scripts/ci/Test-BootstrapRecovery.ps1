[CmdletBinding()]
param([string] $RepositoryRoot = '', [ValidateSet('All', 'Recovery', 'Strawberry')][string] $Case = 'All', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) {
    $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the requested baseline source.' }
}
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
function Import-TestFunction([string] $Name) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name }, $true)
    if ($null -eq $definition) { throw "Missing bootstrap function $Name" }
    . ([scriptblock]::Create($definition.Extent.Text))
    Set-Item "function:script:$Name" (Get-Item "function:$Name").ScriptBlock
}
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
if ($Case -in @('All', 'Recovery')) {
Import-TestFunction 'Install-WingetPackageIfMissing'
function Write-BuildLog { param($Message) }
function Update-SessionPath { }
$Plan = $false
$script:operations = @()
function winget.exe { $script:operations += ,@($args); $global:LASTEXITCODE = 42 }
$thrown = $false
try { Install-WingetPackageIfMissing -DisplayName 'fixture tool' -PackageId 'Fixture.Tool' -Probe { $false } } catch { $thrown = $true }
Assert-True $thrown 'Unavailable tools must stop the bootstrap.'
Assert-True (@($script:operations | Where-Object { $_[0] -eq 'uninstall' }).Count -eq 0) 'A failed install must never uninstall an existing package.'
Assert-True ($script:operations.Count -eq 2) 'Recovery must make exactly one bounded non-destructive retry.'
$script:operations = @()
Install-WingetPackageIfMissing -DisplayName 'fixture tool' -PackageId 'Fixture.Tool' -Probe { $true }
Assert-True ($script:operations.Count -eq 0) 'Available tools must not invoke winget.'
$Plan = $true
Install-WingetPackageIfMissing -DisplayName 'fixture tool' -PackageId 'Fixture.Tool' -Probe { $false }
Assert-True ($script:operations.Count -eq 0) 'Planning must not invoke winget.'
$Plan = $false
$script:probeCount = 0
Install-WingetPackageIfMissing -DisplayName 'fixture tool' -PackageId 'Fixture.Tool' -Probe { $script:probeCount++; $script:probeCount -ge 3 }
Assert-True ($script:operations.Count -eq 2) 'A working tool after retry must be accepted even when winget returns nonzero.'
}
if ($Case -in @('All', 'Strawberry')) {
$install = $ast.Find({ param($node) $node -is [Management.Automation.Language.CommandAst] -and
    $node.GetCommandName() -eq 'Install-WingetPackageIfMissing' -and $node.Extent.Text.Contains('StrawberryPerl.StrawberryPerl') }, $true)
$probeElement = $install.CommandElements | Where-Object { $_ -is [Management.Automation.Language.ScriptBlockExpressionAst] } | Select-Object -First 1
function Get-Command { [pscustomobject]@{ Source = 'C:\unrelated\pkg-config.exe' } }
function Get-StrawberryPkgConfigPath { return $null }
try {
    Assert-True (-not (& ([scriptblock]::Create($probeElement.ScriptBlock.Extent.Text.Trim('{}'))))) 'An unrelated pkg-config must not satisfy the Strawberry package probe.'
} finally { Remove-Item function:Get-Command; Remove-Item function:Get-StrawberryPkgConfigPath }
Import-TestFunction 'Get-StrawberryPkgConfigPath'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('BambuBootstrap-' + [guid]::NewGuid())
$originalPath = $env:Path
try {
    $bin = Join-Path $fixture 'perl/bin'
    New-Item $bin -ItemType Directory -Force | Out-Null
    New-Item (Join-Path $bin 'perl.exe') -ItemType File | Out-Null
    New-Item (Join-Path $bin 'pkg-config.bat') -ItemType File | Out-Null
    $env:Path = $bin
    Assert-True ((Get-StrawberryPkgConfigPath) -eq (Join-Path $bin 'pkg-config.bat')) 'The paired Perl pkg-config wrapper must be discovered.'
    Remove-Item (Join-Path $bin 'perl.exe')
    Assert-True ((Get-StrawberryPkgConfigPath) -ne (Join-Path $bin 'pkg-config.bat')) 'An unrelated pkg-config without paired Perl must not satisfy the probe.'
} finally {
    $env:Path = $originalPath
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $fixture = [IO.Path]::GetFullPath($fixture)
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary directory.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
}
Write-Host "Bootstrap recovery behavioral checks passed (case=$Case; All contains 9 assertions)."

