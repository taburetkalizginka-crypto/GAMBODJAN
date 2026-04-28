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
    echo     Скачай: https://visualstudio.microsoft.com/downloads/
    echo     При установке выбери "Разработка классических приложений на C++"
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
        echo [!] CMake not found. Скачай: https://cmake.org/download/
        pause
        exit /b 1
    )
)
echo [+] CMake OK

:: ============================
::  Check/Install vcpkg
:: ============================
set "VCPKG_DIR=%~dp0vcpkg"
set "VCPKG_EXE=%VCPKG_DIR%\vcpkg.exe"

if not exist "%VCPKG_EXE%" (
    echo.
    echo [*] Устанавливаю vcpkg (нужен для protobuf и curl)...
    where git >nul 2>nul
    if !ERRORLEVEL! NEQ 0 (
        echo [!] Git not found. Скачай: https://git-scm.com/download/win
        pause
        exit /b 1
    )
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%" >nul 2>nul
    if !ERRORLEVEL! NEQ 0 (
        echo [!] Не удалось скачать vcpkg
        pause
        exit /b 1
    )
    call "%VCPKG_DIR%\bootstrap-vcpkg.bat" -disableMetrics >nul 2>nul
)
echo [+] vcpkg OK

:: ============================
::  Install dependencies
:: ============================
echo [*] Проверяю зависимости...
"%VCPKG_EXE%" install protobuf:x64-windows curl:x64-windows >nul 2>nul
echo [+] Зависимости OK

:: ============================
::  Configure
:: ============================
echo.
echo [1/2] Конфигурация CMake...
if not exist "build" mkdir build

cmake -B build -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" >nul 2>nul

if %ERRORLEVEL% NEQ 0 (
    echo [*] Ninja не найден, пробую Visual Studio...
    
    set "GEN="
    for %%G in ("Visual Studio 17 2022" "Visual Studio 16 2019") do (
        if not defined GEN (
            cmake -B build -G "%%~G" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake" >nul 2>nul
            if !ERRORLEVEL! EQU 0 set "GEN=%%~G"
        )
    )
    
    if not defined GEN (
        echo [!] CMake configuration failed
        cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"
        pause
        exit /b 1
    )
    echo [+] Generator: !GEN!
)
echo [+] Конфигурация OK

:: ============================
::  Build
:: ============================
echo.
echo [2/2] Компиляция...
cmake --build build --config Release -- /m /v:m
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] Ошибка компиляции! Смотри ошибки выше.
    pause
    exit /b 1
)

:: ============================
::  Done
:: ============================
echo.
echo ==========================================
echo   ГОТОВО!
echo ==========================================
echo.

if exist "release\GAMBODJAN_dll.dll"    echo   GAMBODJAN_dll.dll     - Чит DLL (инжектить в dota2.exe)
if exist "release\GAMBODJAN.exe"        echo   GAMBODJAN.exe         - Инжектор
if exist "release\GAMBODJAN_overlay.dll" echo   GAMBODJAN_overlay.dll - Оверлей (без хуков)
if exist "release\loader.exe"           echo   loader.exe            - Лоадер

echo.
echo   Файлы в папке: release\
echo.
pause
