param([string]$OutputDirectory = (Join-Path $env:TEMP 'bambu-notification-history-tests'))
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installations = if (Test-Path -LiteralPath $vswhere) { & $vswhere -all -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
$vcvars = $installations | ForEach-Object { Join-Path $_ 'VC/Auxiliary/Build/vcvars64.bat' } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (!$vcvars) { throw 'A Visual Studio C++ toolset is required.' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$script = @"
@call "$vcvars"
@if errorlevel 1 exit /b %errorlevel%
cl /nologo /EHsc /std:c++17 /utf-8 /I"$repo/src" "$PSScriptRoot/reviewed_selection_test.cpp" "$repo/src/slic3r/GUI/NotificationHistory.cpp" /Fe:reviewed_selection_test.exe
@if errorlevel 1 exit /b %errorlevel%
reviewed_selection_test.exe
"@
$command = Join-Path $OutputDirectory 'compile.cmd'
Set-Content -LiteralPath $command -Value $script
Push-Location -LiteralPath $OutputDirectory
try { & cmd /c $command; if ($LASTEXITCODE -ne 0) { throw "Native notification test failed: $LASTEXITCODE" } }
finally { Pop-Location }
