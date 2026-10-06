param([string]$VcVars = '', [switch]$Negative)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $VcVars) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio locator is unavailable. Pass -VcVars explicitly.' }
    $vs = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { $vs = & $vswhere -latest -prerelease -products '*' -property installationPath }
    if (-not $vs) { throw 'A supported MSVC C++ toolchain is required.' }
    $VcVars = Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
    if (-not (Test-Path -LiteralPath $VcVars)) { throw 'The selected Visual Studio installation has no C++ activation script.' }
}
$run = Join-Path ([IO.Path]::GetTempPath()) ('scheduled-settings-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run | Out-Null
$core = Join-Path $repo 'src/libslic3r/ScheduledSettings/Schedule.cpp'
if ($Negative) {
    $mutated = [IO.File]::ReadAllText($core).Replace('tick>=x.expires_at', 'false /* mutation: stale data must be rejected */')
    if ($mutated -eq [IO.File]::ReadAllText($core)) { throw 'Negative mutation did not apply.' }
    $core = Join-Path $run 'Schedule.cpp'
    [IO.File]::WriteAllText($core, $mutated)
}
$command = @"
@echo off
call "$VcVars" >nul
if errorlevel 1 exit /b 2
cd /d "$run"
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /D_CRT_SECURE_NO_WARNINGS /I "$repo/src" /I "$repo/src/libslic3r/ScheduledSettings" "$repo/tests/scheduled_settings/core.cpp" "$core" "$repo/src/libslic3r/ScheduledSettings/Service.cpp" /Fe:scheduled-settings-tests.exe
if errorlevel 1 exit /b 2
scheduled-settings-tests.exe
"@
$runner = Join-Path $run 'run.cmd'
[IO.File]::WriteAllText($runner, $command, [Text.Encoding]::ASCII)
& cmd /c $runner
$result = $LASTEXITCODE
if ($Negative) {
    if ($result -ne 1) { throw "Negative regression must fail an assertion with exit 1, received $result." }
    Write-Output "PASS: stale-response mutation was detected (exit $result)."
} elseif ($result -ne 0) { throw "Scheduled settings tests failed with exit $result." }
Write-Output "Build evidence retained at $run"
