@echo off
setlocal EnableExtensions
rem The service keeps its existing named data volume across application updates.
powershell.exe -NoLogo -NoProfile -File "%~dp0lan-model-drop\host.ps1" %*
exit /b %ERRORLEVEL%
