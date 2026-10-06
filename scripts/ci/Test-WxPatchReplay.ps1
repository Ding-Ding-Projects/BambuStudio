[CmdletBinding()]
param([string] $RepositoryRoot = '', [Parameter(Mandatory)][string] $CMakePath, [Parameter(Mandatory)][string] $WxSourceDirectory)
if (-not $RepositoryRoot) { $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')) }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
$git = (Get-Command git.exe).Source
$fixture = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) ('BambuWxReplay-' + [guid]::NewGuid())))
try {
    New-Item $fixture -ItemType Directory | Out-Null
    & $git -c init.defaultBranch=main init --quiet $fixture
    [IO.File]::WriteAllText((Join-Path $fixture '.gitignore'), "/deps/build/`n")
    $source = Join-Path $fixture 'deps/build/nested source with spaces'
    New-Item $source -ItemType Directory -Force | Out-Null
    & $git -c init.defaultBranch=main init --quiet $source
    & $git -C $source config core.autocrlf true
    $attributes = (& $git -C $WxSourceDirectory show HEAD:.gitattributes) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the pinned wx line-ending contract.' }
    [IO.File]::WriteAllText((Join-Path $source '.gitattributes'), $attributes + "`n", [Text.UTF8Encoding]::new($false))
    $paths = @('build/cmake/setup.cmake','build/cmake/setup.h.in','src/common/utilscmn.cpp')
    $original = @{}
    foreach ($path in $paths) {
        $text = (& $git -C $WxSourceDirectory show ('HEAD:' + $path)) -join "`n"
        if ($LASTEXITCODE -ne 0) { throw 'Cannot read the pinned wx fixture input.' }
        $original[$path] = $text + "`n"
        $target = Join-Path $source $path
        New-Item (Split-Path $target) -ItemType Directory -Force | Out-Null
        [IO.File]::WriteAllText($target, $original[$path], [Text.UTF8Encoding]::new($false))
    }
    & $git -C $source add -- .gitattributes @paths
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record the nested fixture baseline in its index.' }
    $patch = Join-Path $fixture 'relocatable.patch'
    $patchText = [IO.File]::ReadAllText((Join-Path $RepositoryRoot 'deps/wxWidgets/0002-relocatable-windows-prefix.patch')).Replace("`r`n","`n")
    [IO.File]::WriteAllText($patch, $patchText.Replace("`n","`r`n"), [Text.UTF8Encoding]::new($false))
    $normalized = Join-Path $fixture 'normalized.patch'
    [IO.File]::WriteAllText($normalized, $patchText, [Text.UTF8Encoding]::new($false))
    & $git -C $source apply $normalized
    Assert-True ($LASTEXITCODE -eq 0) 'The fixture must apply the actual wx patch once.'
    $template = Join-Path $source 'build/cmake/setup.h.in'
    $templateText = [IO.File]::ReadAllText($template).Replace("`r`n","`n").Replace("#cmakedefine wxRELOCATABLE_INSTALL_PREFIX`n", "#cmakedefine wxRELOCATABLE_INSTALL_PREFIX`r`n")
    [IO.File]::WriteAllText($template, $templateText, [Text.UTF8Encoding]::new($false))
    foreach ($path in @('build/cmake/setup.cmake','src/common/utilscmn.cpp')) {
        $target = Join-Path $source $path
        $text = [IO.File]::ReadAllText($target).Replace("`r`n","`n").Replace("`n","`r`n")
        [IO.File]::WriteAllText($target, $text, [Text.UTF8Encoding]::new($false))
    }
    try { $ErrorActionPreference = 'Continue'; $rawResult = & $git -C $source apply --reverse --check $patch 2>&1 }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$rawResult".Contains('setup.h.in')) 'The exact mixed-template shape must reproduce the strict raw reverse-check failure.'
    try { $ErrorActionPreference = 'Continue'; $forwardResult = & $git -C $source apply --check --ignore-space-change --whitespace=fix $patch 2>&1 }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$forwardResult".Contains('patch does not apply')) 'The original forward-only recipe must reject already-applied content.'
    & $git -C $source apply --reverse --check --ignore-space-change --whitespace=fix $normalized
    Assert-True ($LASTEXITCODE -eq 0) 'Normalized transport must prove all actual patch hunks applied.'
    $helper = Join-Path $RepositoryRoot 'cmake/modules/ApplyPatchesIdempotently.cmake'
    $arguments = @("-DPATCH_ROOT=$fixture", "-DPATCH_SOURCE=$source", "-DGIT_EXECUTABLE=$git", '-DPATCH_COUNT=1', "-DPATCH_1=$patch", '-P', $helper)
    $before = ($paths | ForEach-Object { (Get-FileHash (Join-Path $source $_)).Hash }) -join ':'
    $output = & $CMakePath @arguments 2>&1
    $after = ($paths | ForEach-Object { (Get-FileHash (Join-Path $source $_)).Hash }) -join ':'
    Assert-True ($LASTEXITCODE -eq 0 -and "$output".Contains('already applied') -and $before -ceq $after) 'Nested-repository mixed-line-ending replay must be proven and preserve source bytes.'
    $configure = Join-Path $fixture 'configure-template.cmake'
    @('set(wxRELOCATABLE_INSTALL_PREFIX ON)', "configure_file(`"$($template.Replace('\','/'))`" `"$($fixture.Replace('\','/'))/configured-setup.h`")") | Set-Content $configure
    & $CMakePath -P $configure
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content (Join-Path $fixture 'configured-setup.h') -Raw).Contains('#define wxRELOCATABLE_INSTALL_PREFIX')) 'The mixed template must still generate the intended configured macro.'
    $output = & $CMakePath @arguments 2>&1
    Assert-True ($LASTEXITCODE -eq 0 -and (($paths | ForEach-Object { (Get-FileHash (Join-Path $source $_)).Hash }) -join ':') -ceq $before) 'Post-configure replay must retain actual source bytes.'
    [IO.File]::WriteAllText((Join-Path $source 'src/common/utilscmn.cpp'), $original['src/common/utilscmn.cpp'], [Text.UTF8Encoding]::new($false))
    $partialHash = (Get-FileHash (Join-Path $source 'src/common/utilscmn.cpp')).Hash
    try { $ErrorActionPreference = 'Continue'; $output = & $CMakePath @arguments 2>&1 }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$output".Contains('neither applicable nor already applied') -and (Get-FileHash (Join-Path $source 'src/common/utilscmn.cpp')).Hash -ceq $partialHash) 'A semantically partial patch must fail closed and preserve source.'
    [IO.File]::WriteAllText((Join-Path $source 'build/cmake/setup.cmake'), "conflicting source`n")
    $conflictHash = (Get-FileHash (Join-Path $source 'build/cmake/setup.cmake')).Hash
    try { $ErrorActionPreference = 'Continue'; $output = & $CMakePath @arguments 2>&1 }
    finally { $ErrorActionPreference = 'Stop' }
    Assert-True ($LASTEXITCODE -ne 0 -and "$output".Contains('Forward check') -and "$output".Contains('Reverse check') -and (Get-FileHash (Join-Path $source 'build/cmake/setup.cmake')).Hash -ceq $conflictHash) 'Conflicting content must fail with both diagnostics and remain untouched.'
    $recipe = Get-Content (Join-Path $RepositoryRoot 'deps/wxWidgets/wxWidgets.cmake') -Raw
    Assert-True ($recipe.Contains('ApplyPatchesIdempotently.cmake') -and $recipe.Contains('-DPATCH_COUNT=1')) 'wx must use the verified replay route.'
} finally {
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $fixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture escaped temporary storage.' }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
Write-Host 'wx patch replay checks passed (10 assertions; real nested Git repositories and CMake template configuration only).'
