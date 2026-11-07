@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-RpgFramework.ps1" %*
set "BUILD_RESULT=%ERRORLEVEL%"
if not "%BUILD_RESULT%"=="0" echo BUILD FAILED. Keep the first error above and the build transcript.
pause
exit /b %BUILD_RESULT%
