@echo off
setlocal enabledelayedexpansion

REM === Configuration ===
set "PROJECT_DIR=%~dp0"
set "MINGW_MSVCRT_ROOT=C:\llvm-mingw-msvcrt"
set "MINGW_UCRT_ROOT=C:\llvm-mingw-ucrt"

REM === Validate required files ===
set "REQUIRED_FILES=WindowsUpdatePauser.cpp Resource.rc Resource.hpp icon.ico icon_small.ico"
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

REM === Common settings (C++23 optimized) ===
set "COMMON_FLAGS=-std=c++23 -stdlib=libc++ -fexperimental-library"
set "COMMON_FLAGS=%COMMON_FLAGS% -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DUNICODE -D_UNICODE -DNDEBUG"
set "COMMON_FLAGS=%COMMON_FLAGS% -O3 -flto=thin -Wl,-subsystem,windows:6.0"

REM Static linking flags (ВКЛЮЧАЮЧИ winpthread)
set "COMMON_FLAGS=%COMMON_FLAGS% -static-libgcc -static-libstdc++"
set "COMMON_FLAGS=%COMMON_FLAGS% -Wl,-Bstatic -lwinpthread -lc++ -lc++abi -lunwind -Wl,-Bdynamic"

set "COMMON_FLAGS=%COMMON_FLAGS% -ffunction-sections -fdata-sections -Wl,--gc-sections"
set "COMMON_FLAGS=%COMMON_FLAGS% -fvisibility=hidden -fvisibility-inlines-hidden"
set "COMMON_FLAGS=%COMMON_FLAGS% -Wall -Wextra -Wpedantic -Wno-missing-field-initializers"

REM Security flags для x86/x64 (arm64 не підтримує -fcf-protection)
set "SEC_FLAGS_X86=-fstack-protector-strong -D_FORTIFY_SOURCE=2 -fcf-protection=full"
set "SEC_FLAGS_ARM64=-fstack-protector-strong -D_FORTIFY_SOURCE=2 -mbranch-protection=standard"

set "COMMON_LIBS=-lcomctl32 -ldwmapi -luxtheme -lwinmm -lversion -lshcore -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid"
set "D2D_LIBS=-ld2d1 -ldwrite -lwindowscodecs"
set "ALL_LIBS=%COMMON_LIBS% %D2D_LIBS%"
set "ERROR_COUNT=0"

REM === Build function ===
echo.
echo ========================================================
echo Building C++23 Direct2D version for all architectures...
echo ========================================================
echo Features: Static linking, LTO, ARM64/x86/x64 support
echo.

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
            
            REM Architecture specific security flags
            if "!ARCH!"=="arm64" (
                set "SEC_FLAGS=!SEC_FLAGS_ARM64!"
            ) else (
                set "SEC_FLAGS=!SEC_FLAGS_X86!"
            )
            
            set "ALL_FLAGS=%COMMON_FLAGS% !SEC_FLAGS!"
            
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
                    !ALL_FLAGS! !ARCH_FLAGS! ^
                    -o "%%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe" ^
                    WindowsUpdatePauser.cpp Resource_!ARCH!.rc.o %ALL_LIBS%
                
                if errorlevel 1 (
                    echo ERROR: Failed to compile executable for !ARCH! ^(%%R^)
                    set /a ERROR_COUNT+=1
                ) else (
                    REM Strip symbols для зменшення розміру
                    "!BIN_DIR!\llvm-strip.exe" --strip-all "%%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe" >nul 2>&1
                    
                    REM Перевіряємо чи потрібна DLL (якщо статичне лінкування не спрацювало)
                    "!BIN_DIR!\llvm-objdump.exe" -p "%%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe" | findstr "libwinpthread" >nul
                    if !errorlevel! equ 0 (
                        echo WARNING: libwinpthread-1.dll required, copying...
                        copy "!BIN_DIR!\libwinpthread-1.dll" "%%R\!ARCH!\" >nul 2>&1
                    ) else (
                        echo INFO: Fully static build ^(no DLL dependencies^)
                    )
                    
                    for %%F in ("%%R\!ARCH!\WindowsUpdatePauser-!ARCH!.exe") do (
                        echo SUCCESS: %%~nxF ^(%%~zF bytes^)
                    )
                )
                
                REM Cleanup
                if exist "Resource_!ARCH!.rc.o" del "Resource_!ARCH!.rc.o"
            )
        )
    )
)

REM === Completion ===
echo.
echo ========================================================
if %ERROR_COUNT% equ 0 (
    echo ALL BUILDS COMPLETED SUCCESSFULLY ^(C++23 Direct2D version^).
) else (
    echo BUILD COMPLETED WITH %ERROR_COUNT% ERROR^(S^).
)
echo ========================================================
echo.
echo Output structure:
for %%R in (msvcrt ucrt) do (
    echo %%R\
    for %%A in (x64 x86 arm64) do (
        if exist "%%R\%%A\WindowsUpdatePauser-%%A.exe" (
            for %%F in ("%%R\%%A\WindowsUpdatePauser-%%A.exe") do (
                set "size=%%~zF"
                if exist "%%R\%%A\libwinpthread-1.dll" (
                    echo    [%%A] %%~zF bytes ^+ libwinpthread-1.dll
                ) else (
                    echo    [%%A] %%~zF bytes ^(static^)
                )
            )
        ) else (
            echo    [%%A] --- FAILED ---
        )
    )
    echo.
)
echo Note: C++23 std::expected/format with Direct2D graphics.
echo       Static linking enabled for portable executables.
echo.

pause