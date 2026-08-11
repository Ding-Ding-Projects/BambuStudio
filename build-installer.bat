@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem Standard repository entry point: bootstrap prerequisites and create the
rem unsigned Squirrel.Windows Setup.exe, release packages, checksum, and SBOM.
set "BAMBU_ONE_CLICK_NO_PAUSE=1"
set "FORWARD_ARGS=%*"
if /I "%~1"=="/s" set "FORWARD_ARGS="
if /I "%~1"=="--silent" set "FORWARD_ARGS="
call "%~dp0OneClickBuildInstaller.cmd" %FORWARD_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
if not "%SILENT%"=="1" if /I not "%~1"=="/s" if /I not "%~1"=="--silent" (
    echo.
    if "%EXIT_CODE%"=="0" (echo Bambu Studio installer build completed.) else (echo Bambu Studio installer build failed with exit code %EXIT_CODE%.)
)
exit /b %EXIT_CODE%
