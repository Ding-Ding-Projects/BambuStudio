@echo off
setlocal EnableExtensions
title Bambu Studio - One-click Squirrel.Windows installer build
cd /d "%~dp0"

set "BAMBU_ONE_CLICK_SCRIPT=%~dp0scripts\windows\Invoke-OneClickBuild.ps1"
if not exist "%BAMBU_ONE_CLICK_SCRIPT%" (
    echo ERROR: Missing "%BAMBU_ONE_CLICK_SCRIPT%".
    set "BAMBU_ONE_CLICK_EXIT=2"
    goto :finish
)

if "%SILENT%"=="1" set "BAMBU_ONE_CLICK_NO_PAUSE=1"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\Invoke-BuildEntryPoint.ps1" -EntryPoint "%~f0" %*
set "BAMBU_ONE_CLICK_EXIT=%ERRORLEVEL%"

:finish
echo.
if "%BAMBU_ONE_CLICK_EXIT%"=="0" (
    echo Bambu Studio installer build completed successfully.
) else (
    echo Bambu Studio installer build failed with exit code %BAMBU_ONE_CLICK_EXIT%.
)
echo.
if /I not "%BAMBU_ONE_CLICK_NO_PAUSE%"=="1" pause
exit /b %BAMBU_ONE_CLICK_EXIT%
