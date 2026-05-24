@echo off
setlocal

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build-vs"
set "APP_EXE=%BUILD_DIR%\bin\Debug\OpenShell.exe"

call :stop_running_app "%APP_EXE%"

if not defined CMAKE_EXE (
    for /f "delims=" %%C in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%C"
)

if not defined CMAKE_EXE if exist "E:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=E:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined CMAKE_EXE if exist "C:\Qt\Tools\CMake_64\bin\cmake.exe" set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
if not defined CMAKE_EXE if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"

if not defined CMAKE_EXE (
    echo CMake was not found.
    echo Install CMake or set CMAKE_EXE before re-running, for example:
    echo   set "CMAKE_EXE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
    exit /b 1
)

if not defined QT_PREFIX if exist "E:\Qt\6.11.1\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake" set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"

if not defined QT_PREFIX (
    for /d %%Q in ("E:\Qt\6.*\msvc*_64" "C:\Qt\6.*\msvc*_64") do (
        if exist "%%~Q\lib\cmake\Qt6\Qt6Config.cmake" if not defined QT_PREFIX set "QT_PREFIX=%%~Q"
    )
)

if not defined QT_PREFIX (
    echo Qt for MSVC was not found.
    echo Set QT_PREFIX manually, for example:
    echo   set "QT_PREFIX=E:\Qt\6.11.1\msvc2022_64"
    exit /b 1
)

set "PATH=%QT_PREFIX%\bin;%PATH%"

"%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PREFIX%"
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Debug
if errorlevel 1 exit /b %errorlevel%

start "" "%APP_EXE%"
exit /b 0

:stop_running_app
set "TARGET_EXE=%~f1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$target = [System.IO.Path]::GetFullPath('%TARGET_EXE%'); Get-Process OpenShell -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ([System.IO.Path]::GetFullPath($_.Path) -ieq $target) } | ForEach-Object { Write-Host ('Stopping running OpenShell.exe: ' + $_.Path); Stop-Process -Id $_.Id -Force }"
exit /b 0
