@echo off
setlocal

set "ROOT_DIR=%~dp0"
set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build-vs"
set "DEPLOY_DIR=%BUILD_DIR%\bin\Debug"
set "APP_EXE=%DEPLOY_DIR%\OpenShell.exe"

if not defined QT_PREFIX if exist "E:\Qt\6.11.1\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"

if not defined QT_PREFIX (
    for /d %%Q in ("E:\Qt\6.*\msvc*_64" "C:\Qt\6.*\msvc*_64") do (
        if exist "%%~Q\bin\windeployqt.exe" if not defined QT_PREFIX set "QT_PREFIX=%%~Q"
    )
)

if not defined QT_PREFIX (
    echo Qt for MSVC was not found.
    exit /b 1
)

if not exist "%APP_EXE%" (
    echo Debug executable was not found. Build it first:
    echo run-vs-debug.bat
    exit /b 1
)

if exist "%DEPLOY_DIR%\qml" rmdir /s /q "%DEPLOY_DIR%\qml"

"%QT_PREFIX%\bin\windeployqt.exe" --debug --qmldir "%ROOT_DIR%\qml" "%APP_EXE%"
if errorlevel 1 exit /b %errorlevel%

if exist "%ROOT_DIR%\assets" xcopy /e /i /y "%ROOT_DIR%\assets" "%DEPLOY_DIR%\assets" >nul

echo Debug deployment finished: %DEPLOY_DIR%
