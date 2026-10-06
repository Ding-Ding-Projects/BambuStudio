[CmdletBinding()]
param([string] $RepositoryRoot = '', [Parameter(Mandatory)][string] $CMakePath, [string] $SourceRevision = '')
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
function Read-Source([string] $Path) {
    if ($SourceRevision) { return ((& git -C $RepositoryRoot show ($SourceRevision + ':' + $Path)) -join "`n") }
    return Get-Content (Join-Path $RepositoryRoot $Path) -Raw
}
$source = Read-Source 'scripts/windows/Invoke-OneClickBuild.ps1'
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$null, [ref]$null)
function Import-TestFunction([string] $Name) {
    $definition = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name }, $true)
    if ($null -eq $definition) { throw "Missing function $Name" }
    Set-Item "function:script:$Name" ([scriptblock]::Create($definition.Body.Extent.Text.Trim('{}')))
}
Import-TestFunction 'Get-BuildParallelism'
$originalJobs = $env:BAMBU_BUILD_JOBS
try {
    $env:BAMBU_BUILD_JOBS = '3'
    Assert-True ((Get-BuildParallelism) -eq 3) 'An explicit positive worker count must be preserved.'
    foreach ($invalid in @('0', '-1', 'three', '2147483648')) {
        $env:BAMBU_BUILD_JOBS = $invalid
        $rejected = $false
        try { $null = Get-BuildParallelism } catch { $rejected = $true }
        Assert-True $rejected "Invalid worker count '$invalid' was accepted."
    }
} finally { $env:BAMBU_BUILD_JOBS = $originalJobs }
Import-TestFunction 'Get-BoundedCompilerOptions'
Assert-True ((Get-BoundedCompilerOptions '') -eq '/MP1') 'An empty compiler suffix must receive /MP1.'
Assert-True ((Get-BoundedCompilerOptions '/DKEEP /MP8') -eq '/DKEEP /MP8 /MP1') 'Caller options must be preserved with the cap last.'
Assert-True ((Get-BoundedCompilerOptions '/DKEEP /link /DEBUG') -eq '/DKEEP /MP1 /link /DEBUG') 'The compiler cap must precede linker-only arguments.'
$rejected = $false
try { $null = Get-BoundedCompilerOptions ('x' * 1024) } catch { $rejected = $true }
Assert-True $rejected 'Compiler environment overflow must fail explicitly.'
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuParallelism-' + [guid]::NewGuid())))
$originalCompilerOptions = [Environment]::GetEnvironmentVariable('_CL_', 'Process')
$originalLevel = $env:CMAKE_BUILD_PARALLEL_LEVEL
try {
    New-Item $fixture -ItemType Directory | Out-Null
    $cmake = Read-Source 'deps/CMakeLists.txt'
    $start = $cmake.IndexOf('include(ProcessorCount)')
    $end = $cmake.IndexOf('option(DEP_BUILD_PNG')
    $countCode = $cmake.Substring($start, $end - $start)
    $start = $cmake.IndexOf('    set(_gen "")')
    $end = $cmake.IndexOf('    message(STATUS "bambustudio_add_cmake_project', $start)
    $argumentsCode = $cmake.Substring($start, $end - $start)
    $probePath = Join-Path $fixture 'workers.cmake'
    ($countCode + "`nset(MSVC TRUE)`n" + $argumentsCode + '`nmessage(STATUS "WORKERS=${NPROC}; ARG=${_build_j}")'.Replace('`n', "`n")) | Set-Content $probePath
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '7'
    $output = & $CMakePath -DNPROC=2 -P $probePath 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('WORKERS=2; ARG=/m:2')) 'Explicit NPROC must override environment and generate numbered MSBuild arguments.'
    $openssl = Read-Source 'deps/OpenSSL/OpenSSL.cmake'
    $opensslCount = $openssl.Substring(0, $openssl.IndexOf('if(DEFINED OPENSSL_ARCH)'))
    ($countCode + "`n" + $opensslCount + "`nset(MSVC TRUE)`n" + $argumentsCode + '`nmessage(STATUS "WORKERS=${NPROC}; ARG=${_build_j}")'.Replace('`n', "`n")) | Set-Content $probePath
    $output = & $CMakePath -DNPROC=2 -P $probePath 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('WORKERS=2; ARG=/m:2')) 'OpenSSL include order must not overwrite the worker budget for later dependencies.'
    $output = & $CMakePath -P $probePath 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('WORKERS=7; ARG=/m:7')) 'Environment worker selection must remain supported for direct dependency configuration.'
    (Get-Content $probePath -Raw).Replace('set(MSVC TRUE)', 'set(MSVC FALSE)') | Set-Content $probePath
    $output = & $CMakePath -DNPROC=2 -P $probePath 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('WORKERS=2; ARG=-j2')) 'Non-MSVC dependency arguments must retain their existing form.'
    foreach ($invalid in @('0', '-1', 'three')) {
        try {
            $ErrorActionPreference = 'Continue'
            $output = & $CMakePath "-DNPROC=$invalid" -P $probePath 2>&1
        } finally { $ErrorActionPreference = 'Stop' }
        Assert-True ($LASTEXITCODE -ne 0 -and "$output".Contains('NPROC must be a positive integer')) "CMake accepted invalid NPROC '$invalid'."
    }
    Import-TestFunction 'Invoke-DependencyBuild'
    function Get-BuildParallelism { return 2 }
    function Invoke-RepositoryCommand { param($Label, $Command) & $Command }
    function Invoke-LoggedNativeCommand { param($FilePath, $Arguments) & $FilePath @Arguments }
    function Write-BuildLog { param($Message) }
    $script:calls = @()
    $script:suffixes = @()
    function fake-cmake { $script:calls += ,@($args); $script:suffixes += $env:_CL_; $global:LASTEXITCODE = 0 }
    $script:RepositoryRoot = $fixture
    $destination = Join-Path $fixture 'dest'
    New-Item (Join-Path $destination 'usr/local') -ItemType Directory -Force | Out-Null
    [Environment]::SetEnvironmentVariable('_CL_', '/DKEEP', 'Process')
    Invoke-DependencyBuild -Toolchain @{ CMake = 'fake-cmake'; Generator = 'fixture'; GeneratorInstance = 'fixture' } -Destination $destination
    Assert-True ($script:calls[0] -contains '-DNPROC=2') 'The root configure must forward the selected worker budget.'
    Assert-True (($script:calls[1] -join ' ') -eq "--build $(Join-Path $fixture 'deps\build') --config Release --parallel 1") 'The outer dependency graph must be serialized.'
    Assert-True ($script:suffixes[1] -eq '/DKEEP /MP1') 'Nested compilers must receive the bounded suffix.'
    Assert-True ($env:_CL_ -eq '/DKEEP') 'The compiler suffix must be restored after build completion.'
    function fake-cmake { if ($args -contains '--build') { throw 'Expected fixture build failure.' }; $global:LASTEXITCODE = 0 }
    $rejected = $false
    try { Invoke-DependencyBuild -Toolchain @{ CMake = 'fake-cmake'; Generator = 'fixture'; GeneratorInstance = 'fixture' } -Destination $destination } catch { $rejected = $true }
    Assert-True ($rejected -and $env:_CL_ -eq '/DKEEP') 'Build failure must propagate and restore the compiler suffix.'
} finally {
    $env:CMAKE_BUILD_PARALLEL_LEVEL = $originalLevel
    [Environment]::SetEnvironmentVariable('_CL_', $originalCompilerOptions, 'Process')
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Dependency worker-budget checks passed (21 assertions; script-mode CMake and stub build only).'
