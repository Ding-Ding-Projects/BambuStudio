[CmdletBinding()]
param([string] $RepositoryRoot = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$producer = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
$attributes = Get-Content (Join-Path $RepositoryRoot '.gitattributes') -Raw
$ast = [Management.Automation.Language.Parser]::ParseInput($producer, [ref]$null, [ref]$null)
foreach ($name in @('Get-PinnedSourceCommit', 'Assert-LastExitCode')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    Invoke-Expression $definition.Extent.Text
}
$route = 'src/slic3r/GUI/DeviceWeb/device_page/src/routeTree.gen.ts'
$content = "export const routeTree = {};`n"
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('BambuRouteLineEndings-' + [guid]::NewGuid())
try {
    foreach ($kind in @('old', 'lf')) {
        $fixture = Join-Path $fixtureRoot $kind
        $file = Join-Path $fixture $route
        New-Item -ItemType Directory -Path (Split-Path $file) -Force | Out-Null
        & git -c init.defaultBranch=main init --quiet $fixture
        & git -C $fixture config core.autocrlf true
        & git -C $fixture config user.name 'Claude Fable 5.1'
        & git -C $fixture config user.email 'noreply@anthropic.com'
        if ($kind -eq 'lf') { [IO.File]::WriteAllText((Join-Path $fixture '.gitattributes'), $attributes) }
        [IO.File]::WriteAllText($file, $content)
        & git -C $fixture add -- .
        & git -C $fixture commit --quiet -m "Record generated-route fixture`n`nKeep the fixture still so line endings do the talking.`n`nCo-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
        if ($LASTEXITCODE -ne 0) { throw 'Fixture commit failed.' }
        Remove-Item -LiteralPath $file
        & git -C $fixture checkout -- $route
        [IO.File]::WriteAllText($file, $content)
        $status = @(& git -C $fixture status --porcelain)
        $workingHash = & git -C $fixture hash-object -- $route
        $headHash = & git -C $fixture rev-parse ('HEAD:' + $route)
        Assert-True ($workingHash -eq $headHash) 'The generator rewrite must retain the tracked content hash.'
        $script:RepositoryRoot = $fixture
        if ($kind -eq 'old') {
            Assert-True ($status.Count -eq 1 -and $status[0].StartsWith(' M ')) 'The old autocrlf route must reproduce persistent porcelain modification.'
            $diff = @(& git -C $fixture diff --numstat)
            Assert-True ($diff.Count -eq 0) 'The reproduced modification must have no content diff.'
            $rejected = $false
            try { $null = Get-PinnedSourceCommit } catch { $rejected = $true }
            Assert-True $rejected 'The source identity check must expose the old generator-only state.'
        } else {
            Assert-True ($status.Count -eq 0) 'The exact LF attribute must keep generator-only rewrites clean.'
            $head = & git -C $fixture rev-parse HEAD
            Assert-True ((Get-PinnedSourceCommit) -eq $head) 'The unchanged generated route must retain pinned source identity.'
            [IO.File]::WriteAllText($file, $content + "export const changed = true;`n")
            $rejected = $false
            try { $null = Get-PinnedSourceCommit } catch { $rejected = $true }
            Assert-True $rejected 'Actual generated-route content changes must still fail source identity.'
        }
    }
} finally {
    $resolved = [IO.Path]::GetFullPath($fixtureRoot)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary root.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'Generated route line-ending checks passed (8 assertions; disposable Git fixtures only).'
