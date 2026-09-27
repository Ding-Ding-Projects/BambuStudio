@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem Standard repository entry point: bootstrap prerequisites and build a runnable
rem Release payload without packaging or launching the application.
set "BAMBU_ONE_CLICK_NO_PAUSE=1"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\windows\Invoke-OneClickBuild.ps1" -BuildMode Incremental -BuildOnly
set "EXIT_CODE=%ERRORLEVEL%"
if not "%SILENT%"=="1" if /I not "%~1"=="/s" if /I not "%~1"=="--silent" (
    echo.
    if "%EXIT_CODE%"=="0" (echo Bambu Studio build completed.) else (echo Bambu Studio build failed with exit code %EXIT_CODE%.)
)
exit /b %EXIT_CODE%
