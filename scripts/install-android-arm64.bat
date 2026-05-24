@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "APK_PATH=%ROOT_DIR%\build-android-arm64\android-build\OpenShell.apk"

if not defined ANDROID_SDK_ROOT if exist "E:\android\sdk\platform-tools\adb.exe" set "ANDROID_SDK_ROOT=E:\android\sdk"
if not defined ADB_EXE if exist "%ANDROID_SDK_ROOT%\platform-tools\adb.exe" set "ADB_EXE=%ANDROID_SDK_ROOT%\platform-tools\adb.exe"

if not exist "%APK_PATH%" (
    echo APK was not found. Build it first:
    echo   build-android-arm64.bat
    exit /b 1
)
if not defined ADB_EXE (
    echo adb.exe was not found. Set ANDROID_SDK_ROOT or ADB_EXE before running this script.
    exit /b 1
)

"%ADB_EXE%" devices
if errorlevel 1 exit /b %errorlevel%

"%ADB_EXE%" install -r "%APK_PATH%"
if errorlevel 1 exit /b %errorlevel%

echo Installed: %APK_PATH%
exit /b 0
