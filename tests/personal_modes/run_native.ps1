param([string]$Compiler = 'g++', [switch]$Negative, [switch]$Msvc)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('personal-modes-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
try {
    $headers = Join-Path $scratch 'slic3r/GUI/PersonalModes'
    New-Item -ItemType Directory -Path $headers -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'src/slic3r/GUI/PersonalModes') -Destination (Join-Path $scratch 'slic3r/GUI') -Recurse -Force
    $exe = Join-Path $scratch 'personal-modes-tests.exe'
    function Invoke-Check([bool]$ExpectedSuccess) {
        if ($Msvc) {
            & cl '/nologo' '/std:c++17' '/EHsc' '/utf-8' '/DNOMINMAX' (Join-Path $PSScriptRoot 'personal_modes_tests.cpp') "/I$scratch" "/I$(Join-Path $root 'src')" "/Fe$exe" "/Fo$(Join-Path $scratch 'tests.obj')" '/link' 'ole32.lib' 'oleaut32.lib' 'uuid.lib'
        } else {
            & $Compiler '-std=c++17' '-DNOMINMAX' (Join-Path $PSScriptRoot 'personal_modes_tests.cpp') '-I' $scratch '-I' (Join-Path $root 'src') '-lole32' '-loleaut32' '-luuid' '-o' $exe
        }
        if ($LASTEXITCODE -ne 0) { throw 'Compilation failed; no test verdict exists.' }
        & $exe
        $result = $LASTEXITCODE
        if ($ExpectedSuccess -ne ($result -eq 0)) { throw "Unexpected behavioral result: $result" }
    }
    Invoke-Check $true
    if ($Negative) {
        $queue = Join-Path $headers 'SpeechQueue.hpp'
        $original = [IO.File]::ReadAllText($queue)
        $needle = 'if (m_in_flight && !backend_complete) return {};'
        if (!$original.Contains($needle)) { throw 'Completion mutation anchor absent.' }
        [IO.File]::WriteAllText($queue, $original.Replace($needle, 'if (false) return {};'))
        Invoke-Check $false
        [IO.File]::WriteAllText($queue, $original)
        $school = Join-Path $headers 'SchoolMode.hpp'
        $original = [IO.File]::ReadAllText($school)
        $needle = 'base.personal_vocabulary = base.dim_sum = false;'
        if (!$original.Contains($needle)) { throw 'Suppression mutation anchor absent.' }
        [IO.File]::WriteAllText($school, $original.Replace($needle, 'base.personal_vocabulary = base.dim_sum = true;'))
        Invoke-Check $false
        [IO.File]::WriteAllText($school, $original)
        Invoke-Check $true
        Write-Output 'PASS 2 negative regressions, restored baseline green'
    }
} finally {
    $resolved = [IO.Path]::GetFullPath($scratch)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if (!$resolved.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('personal-modes-check-')) {
        throw 'Scratch cleanup boundary mismatch.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
