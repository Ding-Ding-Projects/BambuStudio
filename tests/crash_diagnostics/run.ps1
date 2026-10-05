param([string]$OutputDirectory = (Join-Path $env:TEMP 'BambuStudio-crash-diagnostics-test'))
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ tools were not found.' }
$environmentScript = Join-Path $installation 'VC/Auxiliary/Build/vcvars64.bat'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$executable = Join-Path $OutputDirectory 'stackwalker_regression.exe'
$commandFile = Join-Path $OutputDirectory 'build.cmd'
@"
@echo off
call "$environmentScript" >nul
cl /nologo /EHsc /std:c++17 /I "$root/src" "$PSScriptRoot/stackwalker_regression.cpp" "$root/src/StackWalker.cpp" /Fe:"$executable" /Fo:"$OutputDirectory\\" /link dbghelp.lib version.lib
if errorlevel 1 exit /b 1
"$executable"
"@ | Set-Content -LiteralPath $commandFile -Encoding ascii
& cmd /c $commandFile
if ($LASTEXITCODE -ne 0) { throw "Crash diagnostic regression failed with exit $LASTEXITCODE." }
