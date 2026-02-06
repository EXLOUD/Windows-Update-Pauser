@echo off
setlocal enabledelayedexpansion

REM === Configuration ===
set "PROJECT_DIR=%~dp0"
set "LLVM_UCRT_ROOT=C:\llvm-ucrt-rus"

REM === Validate required files ===
set "REQUIRED_FILES=WindowsUpdatePauser.cpp Resource.rc Resource.hpp icon.ico icon_small.ico"
for %%F in (%REQUIRED_FILES%) do (
    if not exist "%%F" (
        echo ERROR: %%F not found in current directory.
        pause
        exit /b 1
    )
)

REM === Validate compiler path ===
if not exist "%LLVM_UCRT_ROOT%\bin\clang++.exe" (
    echo ERROR: LLVM compiler not found at %LLVM_UCRT_ROOT%\bin
    pause
    exit /b 1
)

REM === Create output folder structure ===
if not exist "ucrt" mkdir "ucrt"
if not exist "ucrt\x64" mkdir "ucrt\x64"
if not exist "ucrt\arm64" mkdir "ucrt\arm64"

REM === Common settings ===
set "COMMON_FLAGS=-std=c++23"
set "COMMON_FLAGS=%COMMON_FLAGS% -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DUNICODE -D_UNICODE -DNDEBUG"
set "COMMON_FLAGS=%COMMON_FLAGS% -O3 -flto=thin -fuse-ld=lld"

REM Static linking
set "COMMON_FLAGS=%COMMON_FLAGS% -static"

REM Linker optimizations
set "COMMON_FLAGS=%COMMON_FLAGS% -Wl,/subsystem:windows"
set "COMMON_FLAGS=%COMMON_FLAGS% -Wl,/OPT:REF -Wl,/OPT:ICF"

set "COMMON_FLAGS=%COMMON_FLAGS% -ffunction-sections -fdata-sections"
set "COMMON_FLAGS=%COMMON_FLAGS% -fvisibility=hidden -fvisibility-inlines-hidden"
set "COMMON_FLAGS=%COMMON_FLAGS% -Wall -Wextra -Wpedantic -Wno-missing-field-initializers"

REM Security flags
set "SEC_FLAGS_X64=-fstack-protector-strong -D_FORTIFY_SOURCE=2 -fcf-protection=full"
set "SEC_FLAGS_ARM64=-fstack-protector-strong -D_FORTIFY_SOURCE=2"

REM Libraries
set "COMMON_LIBS=-ladvapi32 -lcomctl32 -ldwmapi -luxtheme -lwinmm -lversion -lshcore -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid"
set "D2D_LIBS=-ld2d1 -ld3d11 -ldxgi -ldwrite -lwindowscodecs"
set "ALL_LIBS=%COMMON_LIBS% %D2D_LIBS%"
set "ERROR_COUNT=0"

set "BIN_DIR=%LLVM_UCRT_ROOT%\bin"

echo.
echo ========================================================
echo Building C++23 Direct2D/D3D11 version (LLVM UCRT)
echo ========================================================
echo Features: Static linking, LTO, DXGI Flip Model
echo Render:   ID2D1DeviceContext6 + IDXGISwapChain1
echo DWrite:   IDWriteFactory7 + IDWriteTextFormat3
echo.

REM === Build x64 ===
echo.
echo Compiling for x64 (UCRT)...
echo ----------------------------------------

set "ARCH=x64"
set "TARGET=x86_64"
set "SEC_FLAGS=%SEC_FLAGS_X64%"
set "ALL_FLAGS=%COMMON_FLAGS% %SEC_FLAGS%"

"%BIN_DIR%\llvm-rc.exe" /nologo /fo "Resource_x64.res" Resource.rc

if errorlevel 1 (
    echo ERROR: Failed to compile resources for x64
    set /a ERROR_COUNT+=1
) else (
    "%BIN_DIR%\clang++.exe" --target=x86_64-pc-windows-msvc %ALL_FLAGS% -o "ucrt\x64\WindowsUpdatePauser-x64.exe" WindowsUpdatePauser.cpp Resource_x64.res %ALL_LIBS%
    
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for x64
        set /a ERROR_COUNT+=1
    ) else (
        "%BIN_DIR%\llvm-strip.exe" --strip-all "ucrt\x64\WindowsUpdatePauser-x64.exe" >nul 2>&1
        for %%F in ("ucrt\x64\WindowsUpdatePauser-x64.exe") do echo SUCCESS: %%~nxF (%%~zF bytes)
    )
    
    if exist "Resource_x64.res" del "Resource_x64.res"
)

REM === Build ARM64 ===
echo.
echo Compiling for ARM64 (UCRT)...
echo ----------------------------------------

set "ARCH=arm64"
set "TARGET=aarch64"
set "SEC_FLAGS=%SEC_FLAGS_ARM64%"
set "ALL_FLAGS=%COMMON_FLAGS% %SEC_FLAGS%"

"%BIN_DIR%\llvm-rc.exe" /nologo /fo "Resource_arm64.res" Resource.rc

if errorlevel 1 (
    echo ERROR: Failed to compile resources for ARM64
    set /a ERROR_COUNT+=1
) else (
    "%BIN_DIR%\clang++.exe" --target=aarch64-pc-windows-msvc %ALL_FLAGS% -o "ucrt\arm64\WindowsUpdatePauser-arm64.exe" WindowsUpdatePauser.cpp Resource_arm64.res %ALL_LIBS%
    
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for ARM64
        set /a ERROR_COUNT+=1
    ) else (
        "%BIN_DIR%\llvm-strip.exe" --strip-all "ucrt\arm64\WindowsUpdatePauser-arm64.exe" >nul 2>&1
        for %%F in ("ucrt\arm64\WindowsUpdatePauser-arm64.exe") do echo SUCCESS: %%~nxF (%%~zF bytes)
    )
    
    if exist "Resource_arm64.res" del "Resource_arm64.res"
)

REM === Completion ===
echo.
echo ========================================================
if %ERROR_COUNT% equ 0 (
    echo ALL BUILDS COMPLETED SUCCESSFULLY
) else (
    echo BUILD COMPLETED WITH %ERROR_COUNT% ERROR(S)
)
echo ========================================================
echo.
echo Output structure:
echo ucrt\
if exist "ucrt\x64\WindowsUpdatePauser-x64.exe" (
    for %%F in ("ucrt\x64\WindowsUpdatePauser-x64.exe") do echo    [x64] %%~zF bytes
) else (
    echo    [x64] --- FAILED ---
)
if exist "ucrt\arm64\WindowsUpdatePauser-arm64.exe" (
    for %%F in ("ucrt\arm64\WindowsUpdatePauser-arm64.exe") do echo    [arm64] %%~zF bytes
) else (
    echo    [arm64] --- FAILED ---
)
echo.

pause