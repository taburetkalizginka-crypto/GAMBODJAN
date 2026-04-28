@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>nul
title GAMBODJAN Build

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
::  Setup VS environment
:: ============================
if exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>nul
    echo [+] VS environment configured (x64)
) else (
    echo [!] vcvarsall.bat not found
    pause
    exit /b 1
)

:: ============================
::  Check CMake
:: ============================
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    if exist "%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    ) else (
        echo [!] CMake not found. Download: https://cmake.org/download/
        pause
        exit /b 1
    )
)
echo [+] CMake OK

:: ============================
::  Configure
:: ============================
echo.
echo [1/2] CMake configure...
if not exist "build" mkdir build

cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release >nul 2>nul

if %ERRORLEVEL% NEQ 0 (
    echo [*] Ninja not found, trying Visual Studio generator...

    set "GEN="
    for %%G in ("Visual Studio 17 2022" "Visual Studio 16 2019") do (
        if not defined GEN (
            cmake -B build -G "%%~G" -A x64 >nul 2>nul
            if !ERRORLEVEL! EQU 0 set "GEN=%%~G"
        )
    )

    if not defined GEN (
        echo [!] CMake configuration failed
        cmake -B build -G "Visual Studio 17 2022" -A x64
        pause
        exit /b 1
    )
    echo [+] Generator: !GEN!
)
echo [+] Configure OK

:: ============================
::  Build
:: ============================
echo.
echo [2/2] Building...
cmake --build build --config Release -- /m /v:m
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
