[CmdletBinding()]
param([string] $RepositoryRoot = '', [Parameter(Mandatory)][string] $CMakePath, [Parameter(Mandatory)][string] $OcctSourceDirectory)
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('Bambu OCCT config ' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path (Join-Path $fixture 'adm/templates') -Force | Out-Null
    foreach ($path in @('CMakeLists.txt', 'adm/templates/OpenCASCADEConfig.cmake.in')) {
        Copy-Item -LiteralPath (Join-Path $OcctSourceDirectory $path) -Destination (Join-Path $fixture $path)
    }
    & git -c init.defaultBranch=main init --quiet $fixture
    $probe = Join-Path $fixture 'probe.cmake'
    $code = @'
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_FLAGS [========[ /pathmap:"C:\UnquotedFixture\path with spaces]=]and]==]=source/OCCT" /pathmap:"C:/UnquotedFixture/path with spaces=source/OCCT" /DKEEP="${literal}" ]========])
set(CMAKE_CXX_FLAGS [========[ /W4 /pathmap:"C:\UnquotedFixture\path with spaces]=]and]==]=source/OCCT" /MP ]========])
set(expected_c "${CMAKE_C_FLAGS}")
set(expected_cxx "${CMAKE_CXX_FLAGS}")
@DELIMITER_SETUP@
configure_file("${CMAKE_CURRENT_LIST_DIR}/adm/templates/OpenCASCADEConfig.cmake.in" "${CMAKE_CURRENT_LIST_DIR}/OpenCASCADEConfig.cmake" @ONLY)
include("${CMAKE_CURRENT_LIST_DIR}/OpenCASCADEConfig.cmake")
if(NOT OpenCASCADE_C_FLAGS STREQUAL expected_c OR NOT OpenCASCADE_CXX_FLAGS STREQUAL expected_cxx)
  message(FATAL_ERROR "Exported flags changed")
endif()
message(STATUS "OCCT_FLAG_BYTES_PRESERVED")
'@
    [IO.File]::WriteAllText($probe, $code.Replace('@DELIMITER_SETUP@', ''))
    try { $ErrorActionPreference = 'Continue'; $old = & $CMakePath -P $probe 2>&1; $oldExit = $LASTEXITCODE }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($oldExit -ne 0 -and "$old".Contains('Invalid character escape')) 'The original generated config must reject embedded quoted backslash paths.'
    $helper = Join-Path $RepositoryRoot 'cmake/modules/ApplyPatchesIdempotently.cmake'
    $patch = Join-Path $RepositoryRoot 'deps/OCCT/0002-OCCT-config-flag-quoting.patch'
    $git = (Get-Command git.exe).Source
    $applyArguments = @("-DPATCH_ROOT=$fixture", "-DPATCH_SOURCE=$fixture", "-DGIT_EXECUTABLE=$git", '-DPATCH_COUNT=1', "-DPATCH_1=$patch", '-P', $helper)
    & $CMakePath @applyArguments
    Assert-True ($LASTEXITCODE -eq 0) 'The template patch must apply to the real OCCT source shape.'
    $recipe = Get-Content (Join-Path $fixture 'CMakeLists.txt') -Raw
    $setup = [regex]::Match($recipe, '(?s)set \(OCCT_FLAGS_BRACKET.*?endwhile\(\)').Value
    [IO.File]::WriteAllText($probe, $code.Replace('@DELIMITER_SETUP@', $setup))
    $output = & $CMakePath -P $probe 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('OCCT_FLAG_BYTES_PRESERVED')) 'The generated full config must parse and preserve both flags byte-for-byte.'
    Assert-True ((Get-Content (Join-Path $fixture 'OpenCASCADEConfig.cmake') -Raw).Contains('[===[')) 'The delimiter must grow past closing sequences present in supported path strings.'
    $before = (Get-FileHash (Join-Path $fixture 'adm/templates/OpenCASCADEConfig.cmake.in')).Hash
    & $CMakePath @applyArguments
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-FileHash (Join-Path $fixture 'adm/templates/OpenCASCADEConfig.cmake.in')).Hash -eq $before) 'Repeating the template patch must preserve source bytes.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture cleanup escaped the temporary root.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host 'OCCT generated-config flags checks passed (5 assertions; real template configure/parse and patch replay only).'
