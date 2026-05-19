@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build-android-arm64"

if exist "D:\Program Files\Java\jdk-21.0.1\bin\java.exe" set "JAVA_HOME=D:\Program Files\Java\jdk-21.0.1"
if not defined JAVA_HOME if exist "D:\Program Files\Java\jdk-17.0.11\bin\java.exe" set "JAVA_HOME=D:\Program Files\Java\jdk-17.0.11"

if not defined ANDROID_SDK_ROOT if exist "E:\android\sdk\platform-tools\adb.exe" set "ANDROID_SDK_ROOT=E:\android\sdk"
if not defined ANDROID_NDK_ROOT if exist "%ANDROID_SDK_ROOT%\ndk\27.0.12077973\build\cmake\android.toolchain.cmake" set "ANDROID_NDK_ROOT=%ANDROID_SDK_ROOT%\ndk\27.0.12077973"

if not defined QT_ANDROID_PREFIX if exist "E:\Qt\6.11.1\android_arm64_v8a\bin\qt-cmake.bat" set "QT_ANDROID_PREFIX=E:\Qt\6.11.1\android_arm64_v8a"
if not defined CMAKE_EXE if exist "E:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined NINJA_DIR if exist "E:\Qt\Tools\Ninja\ninja.exe" set "NINJA_DIR=E:\Qt\Tools\Ninja"

if not defined JAVA_HOME (
    echo JDK 17 or 21 was not found. Set JAVA_HOME before running this script.
    exit /b 1
)
if not defined ANDROID_SDK_ROOT (
    echo Android SDK was not found. Set ANDROID_SDK_ROOT before running this script.
    exit /b 1
)
if not defined ANDROID_NDK_ROOT (
    echo Android NDK 27.0.12077973 was not found. Set ANDROID_NDK_ROOT before running this script.
    exit /b 1
)
if not defined QT_ANDROID_PREFIX (
    echo Qt Android arm64 kit was not found. Set QT_ANDROID_PREFIX before running this script.
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

echo Configuring Android arm64 build...
echo   JAVA_HOME=%JAVA_HOME%
echo   ANDROID_SDK_ROOT=%ANDROID_SDK_ROOT%
echo   ANDROID_NDK_ROOT=%ANDROID_NDK_ROOT%
echo   QT_ANDROID_PREFIX=%QT_ANDROID_PREFIX%

"%QT_ANDROID_PREFIX%\bin\qt-cmake.bat" ^
    -S "%ROOT_DIR%" ^
    -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DANDROID_SDK_ROOT="%ANDROID_SDK_ROOT%" ^
    -DANDROID_NDK_ROOT="%ANDROID_NDK_ROOT%" ^
    -DANDROID_ABI=arm64-v8a ^
    -DANDROID_PLATFORM=android-35
if errorlevel 1 exit /b %errorlevel%

echo Android arm64 configure finished: %BUILD_DIR%
exit /b 0
