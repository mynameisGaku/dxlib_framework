@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildWindows.ps1" -AllConfigurations -RunDeviceSmoke -Audio %*
exit /b %errorlevel%
