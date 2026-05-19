@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"

call "%ROOT_DIR%\build-android-arm64.bat"
if errorlevel 1 exit /b %errorlevel%

call "%ROOT_DIR%\install-android-arm64.bat"
if errorlevel 1 exit /b %errorlevel%

exit /b 0
