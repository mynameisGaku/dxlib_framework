@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\SetupDxLib.ps1" %*
exit /b %errorlevel%
