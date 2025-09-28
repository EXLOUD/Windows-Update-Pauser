@echo off
setlocal enabledelayedexpansion

REM === Configuration ===
set "PROJECT_DIR=%~dp0"
set "MINGW_MSVCRT_ROOT=C:\llvm-mingw-msvcrt"
set "MINGW_UCRT_ROOT=C:\llvm-mingw-ucrt"

REM === Validate required files ===
set "REQUIRED_FILES=WindowsUpdatePauser.cpp Resource.rc Resource.h icon.ico icon_small.ico"
for %%F in (%REQUIRED_FILES%) do (
    if not exist "%%F" (
        echo ERROR: %%F not found in current directory.
        pause
        exit /b 1
    )
)

REM === Validate compiler paths ===
for %%R in (MSVCRT UCRT) do (
    if not exist "!MINGW_%%R_ROOT!\bin\x86_64-w64-mingw32-clang++.exe" (
        echo ERROR: %%R compiler not found at !MINGW_%%R_ROOT!\bin
        pause
        exit /b 1
    )
)

REM === Create output folder structure ===
for %%R in (msvcrt ucrt) do (
    for %%A in (x86 x64 arm64) do (
        if not exist "%%R\%%A" mkdir "%%R\%%A"
    )
)

REM === Common settings ===
set "COMMON_FLAGS=-D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -Wl,-subsystem,windows -static-libgcc -static-libstdc++"
set "COMMON_LIBS=-lcomctl32 -ldwmapi -luxtheme -lwinmm -lversion -lshcore -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid"
set "D2D_LIBS=-ld2d1 -ldwrite -lwindowscodecs"
set "ALL_LIBS=%COMMON_LIBS% %D2D_LIBS%"
set "ERROR_COUNT=0"

REM === Build function ===
echo.
echo ========================================================
echo Building Direct2D version for all architectures...
echo ========================================================

for %%R in (msvcrt ucrt) do (
    echo.
    echo Building with %%R runtime...
    echo ----------------------------------------
    
    set "RUNTIME_ROOT=!MINGW_%%R_ROOT!"
    set "BIN_DIR=!RUNTIME_ROOT!\bin"
    
    for %%A in (x64:x86_64:64 x86:i686:32 arm64:aarch64:) do (
        for /f "tokens=1-3 delims=:" %%a in ("%%A") do (
            set "ARCH=%%a"
            set "TARGET=%%b"
            set "BIT_FLAG=%%c"
            
            echo Compiling for !ARCH! ^(%%R^)...
            
            REM Compile resources
            "!BIN_DIR!\llvm-windres" --target=!TARGET!-w64-windows-gnu -i Resource.rc -o Resource_!ARCH!.rc.o
            
            if errorlevel 1 (
                echo ERROR: Failed to compile resources for !ARCH! ^(%%R^)
                set /a ERROR_COUNT+=1
            ) else (
                REM Set architecture-specific flags
                if "!BIT_FLAG!"=="" (
                    set "ARCH_FLAGS="
                ) else (
                    set "ARCH_FLAGS=-m!BIT_FLAG!"
                )
                
                REM Compile executable
                "!BIN_DIR!\!TARGET!-w64-mingw32-clang++.exe" ^
                    -target !TARGET!-w64-windows-gnu ^
                    %COMMON_FLAGS% !ARCH_FLAGS! ^
                    -o "%%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe" ^
                    WindowsUpdatePauser.cpp Resource_!ARCH!.rc.o %ALL_LIBS%
                
                if errorlevel 1 (
                    echo ERROR: Failed to compile executable for !ARCH! ^(%%R^)
                    set /a ERROR_COUNT+=1
                ) else (
                    copy "!BIN_DIR!\libwinpthread-1.dll" "%%R\!ARCH!\" >nul 2>&1
                    echo SUCCESS: %%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe
                )
                
                REM Cleanup temporary resource file
                if exist "Resource_!ARCH!.rc.o" del "Resource_!ARCH!.rc.o"
            )
        )
    )
)

REM === Completion ===
echo.
echo ========================================================
if %ERROR_COUNT% equ 0 (
    echo ALL BUILDS COMPLETED SUCCESSFULLY ^(Direct2D version^).
) else (
    echo BUILD COMPLETED WITH %ERROR_COUNT% ERROR^(S^).
)
echo ========================================================
echo.
echo Output structure:
for %%R in (msvcrt ucrt) do (
    echo %%R\
    for %%A in (x64 x86 arm64) do (
        echo    %%A\WindowsUpdatePauser-%%A.exe + libwinpthread-1.dll
    )
    echo.
)
echo Note: This build uses Direct2D for modern graphics rendering.
echo.

pause
