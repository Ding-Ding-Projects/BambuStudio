[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-SquirrelPackage.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-SquirrelPackage.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
foreach ($name in @('Get-Sha256Lower', 'Test-SquirrelToolTree', 'Publish-OwnedSquirrelCache', 'Resolve-SquirrelTool')) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    if (-not $definition) { throw "Missing verified cache function $name" }
    Invoke-Expression $definition.Extent.Text
}
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuSquirrelCache-' + [guid]::NewGuid())))
try {
    $package = Join-Path $fixture 'source'
    New-Item (Join-Path $package 'tools') -ItemType Directory -Force | Out-Null
    Set-Content (Join-Path $package 'tools/Squirrel.exe') 'fixture executable bytes, never executed'
    Set-Content (Join-Path $package 'tools/fixture.dll') 'fixture supporting bytes'
    $script:fixtureArchive = Join-Path $fixture 'verified.nupkg'
    [IO.Compression.ZipFile]::CreateFromDirectory($package, $script:fixtureArchive)
    $script:SquirrelPackageSha256 = Get-Sha256Lower $script:fixtureArchive
    $script:SquirrelPackageUri = 'https://fixture.invalid/never-requested'
    $script:TempPrefix = 'BambuSquirrelCacheTest-'
    $script:cacheParent = Join-Path $fixture 'cache'
    $script:downloads = 0
    function Get-SquirrelCacheParent { return $script:cacheParent }
    function Invoke-DownloadWithRetry { param($Uri, $OutFile) $script:downloads++; Copy-Item $script:fixtureArchive $OutFile }
    function Write-SquirrelLog { param($Message) }
    $tool = Resolve-SquirrelTool -Version 'fixture' -TemporaryParent $fixture
    Assert-True ((Test-Path $tool) -and $script:downloads -eq 1) 'An empty cache must prepare verified tool bytes.'
    $cache = Split-Path -Parent (Split-Path -Parent $tool)
    Assert-True (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg')) 'The cache must match every archived tool byte.'
    $same = Resolve-SquirrelTool -Version 'fixture' -TemporaryParent $fixture
    Assert-True ($same -eq $tool -and $script:downloads -eq 1) 'Verified warm tools must be reused without downloading.'
    Set-Content $tool 'tampered executable'
    Assert-True (-not (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg'))) 'Changed executable bytes must invalidate a warm cache.'
    $null = Resolve-SquirrelTool -Version 'fixture' -TemporaryParent $fixture
    Assert-True ($script:downloads -eq 2 -and (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg'))) 'Owned invalid cache must be repaired from the verified archive.'
    Assert-True (@(Get-ChildItem $script:cacheParent -Directory -Filter '*.previous-*').Count -eq 1) 'The previous invalid cache must remain recoverable.'
    Remove-Item (Join-Path $cache 'tools/fixture.dll')
    Assert-True (-not (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg'))) 'A missing supporting file must invalidate a partial cache.'
    $null = Resolve-SquirrelTool -Version 'fixture' -TemporaryParent $fixture
    Assert-True ($script:downloads -eq 3 -and (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg'))) 'A partial owned cache must recover.'
    Set-Content (Join-Path $cache 'tools/extra.dll') 'unexpected file'
    Assert-True (-not (Test-SquirrelToolTree $cache (Join-Path $cache '.verified-package.nupkg'))) 'Unexpected tool files must invalidate the cache.'
    Remove-Item (Join-Path $cache '.bambu-squirrel-cache-owner')
    $rejected = $false
    try { $null = Resolve-SquirrelTool -Version 'fixture' -TemporaryParent $fixture } catch { $rejected = $true }
    Assert-True ($rejected -and (Test-Path (Join-Path $cache 'tools/extra.dll'))) 'An ownership-uncertain cache must be preserved.'
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Verified Squirrel tool-cache checks passed (10 assertions; fixture archives only, no executable run).'
