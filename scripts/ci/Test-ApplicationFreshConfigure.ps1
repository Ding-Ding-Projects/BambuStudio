[CmdletBinding()]
param([string] $RepositoryRoot = '', [Parameter(Mandatory)][string] $CMakePath, [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
$definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Preserve-ApplicationConfiguration' }, $true)
if ($definition) { Invoke-Expression $definition.Extent.Text }
else { function Preserve-ApplicationConfiguration { param($BuildDirectory) } }
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Write-BuildLog { param($Message) }
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuFreshConfigure-' + [guid]::NewGuid())))
try {
    $project = Join-Path $fixture 'project'
    $build = Join-Path $project 'build'
    $prefixA = Join-Path $fixture 'prefixA'
    $prefixB = Join-Path $fixture 'prefixB'
    New-Item $project -ItemType Directory -Force | Out-Null
    foreach ($prefix in @($prefixA, $prefixB)) {
        New-Item (Join-Path $prefix 'include/libnoise'), (Join-Path $prefix 'lib') -ItemType Directory -Force | Out-Null
        Set-Content (Join-Path $prefix 'include/libnoise/noise.h') 'configure-only fixture header'
        Set-Content (Join-Path $prefix 'lib/libnoise_static.lib') 'configure-only fixture library, never linked'
    }
    Copy-Item (Join-Path $RepositoryRoot 'cmake/modules/Findlibnoise.cmake') (Join-Path $project 'Findlibnoise.cmake')
    @'
cmake_minimum_required(VERSION 3.21)
project(DependencyDiscoveryFixture NONE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(libnoise REQUIRED)
get_target_property(location noise::noise IMPORTED_LOCATION)
file(WRITE "${CMAKE_BINARY_DIR}/selected-location.txt" "${location}")
'@ | Set-Content (Join-Path $project 'CMakeLists.txt')
    & $CMakePath -S $project -B $build -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_PREFIX_PATH=$prefixA" | Out-Null
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content (Join-Path $build 'selected-location.txt') -Raw).Contains('prefixA')) 'Initial configure must discover prefix A.'
    & $CMakePath -S $project -B $build "-DCMAKE_PREFIX_PATH=$prefixB" | Out-Null
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content (Join-Path $build 'selected-location.txt') -Raw).Contains('prefixA')) 'The unchanged discovery cache must demonstrate stale prefix A.'
    $objectMarker = Join-Path $build 'retained-object.obj'
    Set-Content $objectMarker 'object-tree retention fixture'
    $script:RepositoryRoot = $project
    Preserve-ApplicationConfiguration -BuildDirectory $build
    & $CMakePath -S $project -B $build -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_PREFIX_PATH=$prefixB" | Out-Null
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content (Join-Path $build 'selected-location.txt') -Raw).Contains('prefixB')) 'Preserving/resetting the configuration cache must discover prefix B.'
    Assert-True ((Get-Content $objectMarker -Raw).Contains('object-tree retention')) 'Configuration reset must retain object-tree contents.'
    $preserved = @(Get-ChildItem (Join-Path $project 'artifacts/windows/application-configurations') -Filter CMakeCache.txt -Recurse -File)
    Assert-True ($preserved.Count -eq 1 -and (Get-Content $preserved[0].FullName -Raw).Contains('LIBNOISE_LIBRARY:FILEPATH=')) 'Prior discovery cache must remain recoverable.'
    $rejected = $false
    try { Preserve-ApplicationConfiguration -BuildDirectory (Join-Path $fixture 'other-build') } catch { $rejected = $true }
    Assert-True $rejected 'A nonowned build directory must be rejected.'
    $cachePath = Join-Path $build 'CMakeCache.txt'
    $cacheText = (Get-Content $cachePath -Raw) -replace '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=.*$', 'CMAKE_HOME_DIRECTORY:INTERNAL=C:/unrelated-source'
    Set-Content $cachePath $cacheText
    $cacheHash = (Get-FileHash $cachePath).Hash
    $rejected = $false
    try { Preserve-ApplicationConfiguration -BuildDirectory $build } catch { $rejected = $true }
    Assert-True ($rejected -and (Get-FileHash $cachePath).Hash -ceq $cacheHash) 'A source-ownership mismatch must preserve the configuration in place.'
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Application fresh-configure checks passed (7 assertions; real CMake configure only, no compiler/build).'
