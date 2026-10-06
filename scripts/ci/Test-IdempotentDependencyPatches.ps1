[CmdletBinding()]
param([string] $RepositoryRoot = '', [Parameter(Mandatory)][string] $CMakePath)
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$helper = Join-Path $RepositoryRoot 'cmake/modules/ApplyPatchesIdempotently.cmake'
$git = (Get-Command git.exe).Source
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuPatchSequence-' + [guid]::NewGuid())))
try {
    $source = Join-Path $fixture 'source with spaces'
    New-Item $source -ItemType Directory -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $source 'one.txt'), "before-one`n")
    [IO.File]::WriteAllText((Join-Path $source 'two.txt'), "before-two`n")
    $patches = @()
    foreach ($name in @('one','two')) {
        $patch = Join-Path $fixture ($name + '.patch')
        $text = "diff --git a/$name.txt b/$name.txt`n--- a/$name.txt`n+++ b/$name.txt`n@@ -1 +1 @@`n-before-$name`n+after-$name`n"
        [IO.File]::WriteAllText($patch, $text)
        $patches += $patch
    }
    & $git -C $fixture apply '--directory=source with spaces' $patches[0]
    Assert-True ($LASTEXITCODE -eq 0) 'The fixture must start with only the first patch applied.'
    try {
        $ErrorActionPreference = 'Continue'
        $batchOutput = & $git -C $fixture apply --check '--directory=source with spaces' @patches 2>&1
    } finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$batchOutput".Contains('patch does not apply')) 'The former batch route must demonstrate its partial-sequence rerun failure.'
    $arguments = @("-DPATCH_ROOT=$fixture", "-DPATCH_SOURCE=$source", "-DGIT_EXECUTABLE=$git", '-DPATCH_COUNT=2', "-DPATCH_1=$($patches[0])", "-DPATCH_2=$($patches[1])", '-P', $helper)
    $output = & $CMakePath @arguments 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('already applied') -and "$output".Contains('Applied patch')) 'Partial sequence must skip the proven applied patch and apply the missing patch.'
    Assert-True ((Get-Content (Join-Path $source 'one.txt') -Raw).Trim() -eq 'after-one' -and (Get-Content (Join-Path $source 'two.txt') -Raw).Trim() -eq 'after-two') 'Both patch results must exist.'
    $before = @((Get-FileHash (Join-Path $source 'one.txt')).Hash, (Get-FileHash (Join-Path $source 'two.txt')).Hash) -join ':'
    $output = & $CMakePath @arguments 2>&1
    $after = @((Get-FileHash (Join-Path $source 'one.txt')).Hash, (Get-FileHash (Join-Path $source 'two.txt')).Hash) -join ':'
    Assert-True ($LASTEXITCODE -eq 0 -and $before -ceq $after) 'A complete sequence rerun must preserve source bytes.'
    [IO.File]::WriteAllText((Join-Path $source 'two.txt'), "conflicting-source`n")
    $conflictHash = (Get-FileHash (Join-Path $source 'two.txt')).Hash
    try { $ErrorActionPreference = 'Continue'; $output = & $CMakePath @arguments 2>&1 }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$output".Contains('neither applicable nor already applied')) 'Conflicting source must stop the patch sequence.'
    Assert-True ("$output".Contains('Forward check') -and "$output".Contains('Reverse check') -and "$output".Contains('patch does not apply')) 'Conflict evidence must preserve both actual Git diagnostics.'
    Assert-True ((Get-FileHash (Join-Path $source 'two.txt')).Hash -ceq $conflictHash) 'A rejected conflicting patch must not alter the source.'
    $recipe = Get-Content (Join-Path $RepositoryRoot 'deps/OpenCV/OpenCV.cmake') -Raw
    Assert-True ($recipe.Contains('ApplyPatchesIdempotently.cmake') -and $recipe.Contains('-DPATCH_COUNT=4')) 'OpenCV must use the repeat-safe route for all four patch inputs.'
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'Repeat-safe dependency patch checks passed (9 assertions; fixture Git/CMake only).'
