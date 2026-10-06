[CmdletBinding()]
param([string] $RepositoryRoot = '', [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Get-Content (Join-Path $RepositoryRoot 'scripts/windows/Invoke-OneClickBuild.ps1') -Raw
if ($SourceRevision) { $source = (& git -C $RepositoryRoot show ($SourceRevision + ':scripts/windows/Invoke-OneClickBuild.ps1')) -join "`n" }
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
function Import-TestFunction([string] $Name, [switch] $Optional) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name }, $true)
    if (-not $definition) { if ($Optional) { return }; throw "Missing identity function $Name" }
    Set-Item "function:script:$Name" ([scriptblock]::Create($definition.Body.Extent.Text.Trim('{}')))
}
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuAppCache-' + [guid]::NewGuid())))
$originalCompilerOptions = [Environment]::GetEnvironmentVariable('_CL_', 'Process')
try {
    $script:RepositoryRoot = $fixture
    $deps = Join-Path $fixture 'dependencies'
    $prefix = Join-Path $deps 'usr/local'
    $install = Join-Path $fixture 'install'
    New-Item $prefix, (Join-Path $fixture 'build') -ItemType Directory -Force | Out-Null
    $cache = Join-Path $fixture 'build/CMakeCache.txt'
    @('CMAKE_INSTALL_PREFIX:PATH=C:/wrong-prefix', 'BAMBU_RELEASE_SOURCE_PATH_POLICY:INTERNAL=msvc-pathmap-v1', "CMAKE_HOME_DIRECTORY:INTERNAL=$fixture") | Set-Content $cache
    Import-TestFunction 'Invoke-ApplicationBuild'
    Import-TestFunction 'Test-ApplicationCacheIdentity' -Optional
    Import-TestFunction 'Preserve-ApplicationConfiguration' -Optional
    Import-TestFunction 'Get-BoundedCompilerOptions' -Optional
    function Get-ApplicationCacheIdentity { return 'fixture-id' }
    function Get-BuildParallelism { return 2 }
    function Get-PythonInterpreterPath { return 'fixture-python' }
    function Write-BuildLog { param($Message) }
    function Invoke-RepositoryCommand { param($Label, $Command) & $Command }
    $script:calls = @()
    $script:suffixes = @()
    function fake-cmake { $script:calls += ,@($args); $script:suffixes += $env:_CL_; $global:LASTEXITCODE = 0 }
    [Environment]::SetEnvironmentVariable('_CL_', '/DAPPKEEP', 'Process')
    $toolchain = @{ CMake = 'fake-cmake'; Generator = 'fixture'; GeneratorInstance = 'fixture'; SdkIncludePath = 'fixture-sdk' }
    Invoke-ApplicationBuild -Toolchain $toolchain -DependencyDestination $deps -InstallPrefix $install
    Assert-True ($script:calls[0] -contains '-S') 'An existing cache with a mismatched install prefix must reconfigure.'
    Assert-True ($script:calls[0] -contains '-DBAMBU_APPLICATION_CACHE_ID:STRING=fixture-id') 'Configure must record the exact cache identity.'
    Assert-True ($script:suffixes[1] -eq '/DAPPKEEP /MP1' -and $script:suffixes[2] -eq '/DAPPKEEP /MP1') 'Both application production commands must receive the compiler cap.'
    Assert-True (($script:calls[2] -join ' ').EndsWith('--parallel 2')) 'Application MSBuild must retain the selected worker budget.'
    Assert-True ($env:_CL_ -eq '/DAPPKEEP') 'Application success must restore the compiler environment.'
    @("CMAKE_INSTALL_PREFIX:PATH=$($install.Replace('\','/'))", "CMAKE_PREFIX_PATH:STRING=$($prefix.Replace('\','/'))", 'BAMBU_APPLICATION_CACHE_ID:STRING=fixture-id') | Set-Content $cache
    Assert-True (Test-ApplicationCacheIdentity $cache 'fixture-id' $install $prefix) 'An exact cache identity must match.'
    Assert-True (-not (Test-ApplicationCacheIdentity $cache 'other-id' $install $prefix)) 'A changed source/toolchain identity must invalidate reuse.'
    Assert-True (-not (Test-ApplicationCacheIdentity $cache 'fixture-id' $install ($prefix + '-other'))) 'A changed dependency destination must invalidate reuse.'
    Import-TestFunction 'Get-FileSha256Lower'
    Import-TestFunction 'Assert-LastExitCode'
    Import-TestFunction 'Get-ApplicationCacheIdentity'
    $script:tree = '100644 blob fixture CMakeLists.txt'
    function git { $global:LASTEXITCODE = 0; return $script:tree }
    $cmake = Join-Path $fixture 'cmake.exe'
    $compiler = Join-Path $fixture 'cl.exe'
    Set-Content $cmake 'fixture CMake bytes'
    Set-Content $compiler 'fixture compiler bytes'
    $library = Join-Path $prefix 'fixture.lib'
    Set-Content $library 'fixture dependency bytes'
    $toolchain = @{ CMake = $cmake; CompilerPath = $compiler; CompilerVersion = '1'; Generator = 'fixture'; GeneratorInstance = 'fixture'; VisualStudioVersion = '1'; SdkIncludePath = 'fixture-sdk' }
    $identity = Get-ApplicationCacheIdentity $toolchain $deps $install
    Assert-True ($identity -cmatch '^[a-f0-9]{64}$' -and (Get-ApplicationCacheIdentity $toolchain $deps $install) -ceq $identity) 'Unchanged inputs must produce a stable identity.'
    Set-Content $library 'changed dependency bytes'
    Assert-True ((Get-ApplicationCacheIdentity $toolchain $deps $install) -cne $identity) 'Changed dependency bytes must invalidate identity even at the same path.'
    Set-Content $library 'fixture dependency bytes'
    Set-Content $compiler 'changed compiler bytes'
    Assert-True ((Get-ApplicationCacheIdentity $toolchain $deps $install) -cne $identity) 'Changed compiler bytes must invalidate identity.'
    Set-Content $compiler 'fixture compiler bytes'
    $script:tree = 'changed source tree'
    Assert-True ((Get-ApplicationCacheIdentity $toolchain $deps $install) -cne $identity) 'Changed relevant source tree must invalidate identity.'
    $script:tree = '100644 blob fixture CMakeLists.txt'
    $toolchain.SdkIncludePath = 'other-sdk'
    Assert-True ((Get-ApplicationCacheIdentity $toolchain $deps $install) -cne $identity) 'Changed SDK selection must invalidate identity.'
    function Get-ApplicationCacheIdentity { return 'fixture-id' }
    @("CMAKE_INSTALL_PREFIX:PATH=$($install.Replace('\','/'))", "CMAKE_PREFIX_PATH:STRING=$($prefix.Replace('\','/'))", 'BAMBU_APPLICATION_CACHE_ID:STRING=fixture-id', 'BAMBU_RELEASE_SOURCE_PATH_POLICY:INTERNAL=msvc-pathmap-v1', "CMAKE_HOME_DIRECTORY:INTERNAL=$fixture") | Set-Content $cache
    function fake-cmake { throw 'Expected fixture production failure.' }
    $rejected = $false
    try { Invoke-ApplicationBuild -Toolchain @{ CMake = 'fake-cmake'; Generator = 'fixture'; GeneratorInstance = 'fixture'; SdkIncludePath = 'fixture-sdk' } -DependencyDestination $deps -InstallPrefix $install } catch { $rejected = $true }
    Assert-True ($rejected -and $env:_CL_ -eq '/DAPPKEEP') 'Application failure must propagate and restore the compiler environment.'
} finally {
    [Environment]::SetEnvironmentVariable('_CL_', $originalCompilerOptions, 'Process')
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Application cache identity checks passed (14 assertions; fixture files and stub CMake only).'
