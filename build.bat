@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>nul
title GAMBODJAN Build

echo.
echo ==========================================
echo   GAMBODJAN Build Script
echo ==========================================
echo.

:: ============================
::  Find Visual Studio
:: ============================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [!] Visual Studio not found
    echo     Download: https://visualstudio.microsoft.com/downloads/
    pause
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo [!] Visual Studio not found
    pause
    exit /b 1
)
echo [+] Visual Studio: %VS_PATH%

:: ============================
::  Find vcvarsall.bat
:: ============================
set "VCVARS="

:: Try standard path
if exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
)

:: Try alternative paths
if not defined VCVARS (
    for /f "usebackq delims=" %%f in (`dir /s /b "%VS_PATH%\vcvarsall.bat" 2^>nul`) do (
        if not defined VCVARS set "VCVARS=%%f"
    )
)

if not defined VCVARS (
    echo [!] vcvarsall.bat not found!
    echo     You need to install "Desktop development with C++" workload.
    echo     Open Visual Studio Installer and add this workload.
    echo.
    echo     Path checked: %VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat
    pause
    exit /b 1
)

echo [+] vcvarsall: %VCVARS%
call "%VCVARS%" x64 >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [!] Failed to configure VS environment
    call "%VCVARS%" x64
    pause
    exit /b 1
)
echo [+] VS environment configured (x64)

:: ============================
::  Find CMake
:: ============================
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    :: Try VS bundled CMake
    set "VS_CMAKE=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    if exist "!VS_CMAKE!\cmake.exe" (
        set "PATH=!VS_CMAKE!;%PATH%"
    ) else (
        echo [!] CMake not found
        echo     CMake should come with Visual Studio C++ workload.
        echo     Or download: https://cmake.org/download/
        pause
        exit /b 1
    )
)
echo [+] CMake OK

:: ============================
::  Find Ninja (optional)
:: ============================
set "USE_NINJA=0"
where ninja >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set "USE_NINJA=1"
) else (
    set "VS_NINJA=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    if exist "!VS_NINJA!\ninja.exe" (
        set "PATH=!VS_NINJA!;%PATH%"
        set "USE_NINJA=1"
    )
)

:: ============================
::  Clean old build if needed
:: ============================
if exist "build\CMakeCache.txt" (
    echo [*] Cleaning old build...
    rmdir /s /q build >nul 2>nul
)

:: ============================
::  Configure
:: ============================
echo.
echo [1/2] CMake configure...
if not exist "build" mkdir build

if "%USE_NINJA%"=="1" (
    echo [+] Using Ninja generator
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release 2>&1
) else (
    echo [+] Using Visual Studio generator
    cmake -B build -G "Visual Studio 17 2022" -A x64 2>&1
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] CMake configuration failed!
    pause
    exit /b 1
)
echo [+] Configure OK

:: ============================
::  Build
:: ============================
echo.
echo [2/2] Building...
if "%USE_NINJA%"=="1" (
    cmake --build build --config Release
) else (
    cmake --build build --config Release -- /m /v:m
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] Build failed! Check errors above.
    pause
    exit /b 1
)

:: ============================
::  Done
:: ============================
echo.
echo ==========================================
echo   BUILD COMPLETE
echo ==========================================
echo.

if exist "release\GAMBODJAN_dll.dll"    echo   GAMBODJAN_dll.dll     - Cheat DLL
if exist "release\GAMBODJAN.exe"        echo   GAMBODJAN.exe         - Injector
if exist "release\loader.exe"           echo   loader.exe            - Loader

echo.
echo   Output: release\
echo.
pause
