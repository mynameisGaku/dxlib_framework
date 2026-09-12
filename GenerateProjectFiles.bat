@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\GenerateProjectFiles.ps1" %*
set "Result=%errorlevel%"
if "%~1"=="" pause
exit /b %Result%
