[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
foreach ($name in @('Get-PinnedSourceCommit', 'Assert-PinnedBuildSource', 'Assert-LastExitCode')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    if (-not $definition) { throw "Missing source identity function $name" }
    Invoke-Expression $definition.Extent.Text
}
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$script:RepositoryRoot = 'fixture'
$script:head = 'a' * 40
$script:changes = @()
$script:gitExit = 0
function git { $global:LASTEXITCODE = $script:gitExit; if ($args -contains 'rev-parse') { return $script:head }; return $script:changes }
Assert-True ((Get-PinnedSourceCommit) -eq ('a' * 40)) 'Clean source must produce its exact commit.'
Assert-PinnedBuildSource -SourceCommit ('a' * 40)
foreach ($case in @('changed-head', 'dirty', 'untracked', 'invalid-head', 'git-failed')) {
    $script:head = 'a' * 40; $script:changes = @(); $script:gitExit = 0
    switch ($case) {
        'changed-head' { $script:head = 'b' * 40 }
        'dirty' { $script:changes = @(' M src/changed.cpp') }
        'untracked' { $script:changes = @('?? resources/new-file.dat') }
        'invalid-head' { $script:head = 'invalid' }
        'git-failed' { $script:gitExit = 7 }
    }
    $rejected = $false
    try { Assert-PinnedBuildSource -SourceCommit ('a' * 40) } catch { $rejected = $true }
    Assert-True $rejected "Source identity accepted '$case'."
}
$producer = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-OneClickBuild' }, $true).Extent.Text
Assert-True ($producer.IndexOf('$sourceCommit = Get-PinnedSourceCommit') -lt $producer.IndexOf('Invoke-DependencyBuild -Toolchain')) 'Source identity must be pinned before compilation.'
Assert-True ($producer.Contains('Assert-PinnedBuildSource -SourceCommit $sourceCommit')) 'Packaging must assert the pinned source.'
Assert-True (-not $producer.Contains('Tracked working-tree changes are included')) 'Dirty source must fail rather than merely warn.'
Assert-True ($source.Contains('--untracked-files=normal')) 'Nonignored untracked source must participate in the identity check.'
Assert-True ($producer -match 'if \(\$BuildOnly\)\s*\{\s*Assert-PinnedBuildSource -SourceCommit \$sourceCommit') 'Build-only success must recheck source after payload staging.'
Write-Host 'Pinned build-source checks passed (11 assertions; mocked Git only).'
