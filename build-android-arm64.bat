@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build-android-arm64"
set "APK_PATH=%BUILD_DIR%\android-build\OpenShell.apk"

if exist "D:\Program Files\Java\jdk-21.0.1\bin\java.exe" set "JAVA_HOME=D:\Program Files\Java\jdk-21.0.1"
if not defined JAVA_HOME if exist "D:\Program Files\Java\jdk-17.0.11\bin\java.exe" set "JAVA_HOME=D:\Program Files\Java\jdk-17.0.11"
if not defined ANDROID_SDK_ROOT if exist "E:\android\sdk\platform-tools\adb.exe" set "ANDROID_SDK_ROOT=E:\android\sdk"
if not defined ANDROID_NDK_ROOT if exist "%ANDROID_SDK_ROOT%\ndk\27.0.12077973\build\cmake\android.toolchain.cmake" set "ANDROID_NDK_ROOT=%ANDROID_SDK_ROOT%\ndk\27.0.12077973"
if not defined CMAKE_EXE if exist "E:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined NINJA_DIR if exist "E:\Qt\Tools\Ninja\ninja.exe" set "NINJA_DIR=E:\Qt\Tools\Ninja"

if not exist "%BUILD_DIR%\build.ninja" (
    call "%ROOT_DIR%\configure-android-arm64.bat"
    if errorlevel 1 exit /b %errorlevel%
)

if not defined JAVA_HOME (
    echo JDK 17 or 21 was not found. Set JAVA_HOME before running this script.
    exit /b 1
)
if not defined CMAKE_EXE (
    echo CMake was not found. Set CMAKE_EXE before running this script.
    exit /b 1
)
if not defined NINJA_DIR (
    echo Ninja was not found. Set NINJA_DIR before running this script.
    exit /b 1
)

set "PATH=%JAVA_HOME%\bin;%NINJA_DIR%;%PATH%"

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Debug
if errorlevel 1 exit /b %errorlevel%

if exist "%APK_PATH%" (
    echo Android APK: %APK_PATH%
) else (
    echo Android APK was not found after build.
    exit /b 1
)
exit /b 0
