@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"

call "%ROOT_DIR%\scripts\build-android-arm64.bat"
if errorlevel 1 exit /b %errorlevel%

call "%ROOT_DIR%\scripts\install-android-arm64.bat"
if errorlevel 1 exit /b %errorlevel%

exit /b 0
