@echo off
setlocal

REM === Configuration ===
set "PROJECT_DIR=%~dp0"

REM Paths to LLVM-MinGW installations
set "MINGW_MSVCRT_ROOT=C:\llvm-mingw-msvcrt"
set "MINGW_UCRT_ROOT=C:\llvm-mingw-ucrt"
set "BIN_MSVCRT=%MINGW_MSVCRT_ROOT%\bin"
set "BIN_UCRT=%MINGW_UCRT_ROOT%\bin"

REM === Validate required source files ===
if not exist "WindowsUpdatePauser.cpp" (
    echo ERROR: WindowsUpdatePauser.cpp not found in current directory.
    pause
    exit /b 1
)
if not exist "Resource.rc" (
    echo ERROR: Resource.rc not found in current directory.
    pause
    exit /b 1
)
if not exist "Resource.h" (
    echo ERROR: Resource.h not found in current directory.
    pause
    exit /b 1
)

REM === Validate required icon files ===
if not exist "icon.ico" (
    echo ERROR: icon.ico not found! This file is required by Resource.rc.
    pause
    exit /b 1
)
if not exist "icon_small.ico" (
    echo ERROR: icon_small.ico not found! This file is required by Resource.rc.
    pause
    exit /b 1
)

REM === Validate compiler paths ===
if not exist "%BIN_MSVCRT%\x86_64-w64-mingw32-clang++.exe" (
    echo ERROR: MSVCRT compiler not found at %BIN_MSVCRT%
    pause
    exit /b 1
)
if not exist "%BIN_UCRT%\x86_64-w64-mingw32-clang++.exe" (
    echo ERROR: UCRT compiler not found at %BIN_UCRT%
    pause
    exit /b 1
)

REM === Create output folder structure ===
for %%R in (msvcrt ucrt) do (
    for %%A in (x86 x64 arm64) do (
        if not exist "%%R\%%A" mkdir "%%R\%%A"
    )
)

set ERROR_COUNT=0

REM === Common libraries for Direct2D build ===
set "COMMON_LIBS=-lcomctl32 -ldwmapi -luxtheme -lwinmm -lversion -lshcore -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid"
set "D2D_LIBS=-ld2d1 -ldwrite -lwindowscodecs"
set "ALL_LIBS=%COMMON_LIBS% %D2D_LIBS%"

REM ========================================================
REM === Build with MSVCRT runtime ===
REM ========================================================
echo.
echo ========================================================
echo Building with MSVCRT runtime (Direct2D version)...
echo ========================================================

REM --- x64 ---
echo.
echo --- Compiling for x64 (msvcrt) ---
echo Command: "%BIN_MSVCRT%\llvm-windres" --target=x86_64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_MSVCRT%\llvm-windres" --target=x86_64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for x64 (msvcrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_MSVCRT%\x86_64-w64-mingw32-clang++.exe" -target x86_64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m64 -o msvcrt\x64\WindowsUpdatePauser-x64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_MSVCRT%\x86_64-w64-mingw32-clang++.exe" -target x86_64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m64 -o msvcrt\x64\WindowsUpdatePauser-x64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for x64 (msvcrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_MSVCRT_ROOT%\bin\libwinpthread-1.dll" "msvcrt\x64\" >nul 2>&1
        echo SUCCESS: msvcrt\x64\WindowsUpdatePauser-x64.exe
    )
)

REM --- x86 ---
echo.
echo --- Compiling for x86 (msvcrt) ---
echo Command: "%BIN_MSVCRT%\llvm-windres" --target=i686-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_MSVCRT%\llvm-windres" --target=i686-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for x86 (msvcrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_MSVCRT%\i686-w64-mingw32-clang++.exe" -target i686-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m32 -o msvcrt\x86\WindowsUpdatePauser-x86.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_MSVCRT%\i686-w64-mingw32-clang++.exe" -target i686-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m32 -o msvcrt\x86\WindowsUpdatePauser-x86.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for x86 (msvcrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_MSVCRT_ROOT%\bin\libwinpthread-1.dll" "msvcrt\x86\" >nul 2>&1
        echo SUCCESS: msvcrt\x86\WindowsUpdatePauser-x86.exe
    )
)

REM --- ARM64 ---
echo.
echo --- Compiling for ARM64 (msvcrt) ---
echo Command: "%BIN_MSVCRT%\llvm-windres" --target=aarch64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_MSVCRT%\llvm-windres" --target=aarch64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for ARM64 (msvcrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_MSVCRT%\aarch64-w64-mingw32-clang++.exe" -target aarch64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -o msvcrt\arm64\WindowsUpdatePauser-arm64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_MSVCRT%\aarch64-w64-mingw32-clang++.exe" -target aarch64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -o msvcrt\arm64\WindowsUpdatePauser-arm64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for ARM64 (msvcrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_MSVCRT_ROOT%\bin\libwinpthread-1.dll" "msvcrt\arm64\" >nul 2>&1
        echo SUCCESS: msvcrt\arm64\WindowsUpdatePauser-arm64.exe
    )
)

REM ========================================================
REM === Build with UCRT runtime ===
REM ========================================================
echo.
echo ========================================================
echo Building with UCRT runtime (Direct2D version)...
echo ========================================================

REM --- x64 ---
echo.
echo --- Compiling for x64 (ucrt) ---
echo Command: "%BIN_UCRT%\llvm-windres" --target=x86_64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_UCRT%\llvm-windres" --target=x86_64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for x64 (ucrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_UCRT%\x86_64-w64-mingw32-clang++.exe" -target x86_64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m64 -o ucrt\x64\WindowsUpdatePauser-x64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_UCRT%\x86_64-w64-mingw32-clang++.exe" -target x86_64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m64 -o ucrt\x64\WindowsUpdatePauser-x64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for x64 (ucrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_UCRT_ROOT%\bin\libwinpthread-1.dll" "ucrt\x64\" >nul 2>&1
        echo SUCCESS: ucrt\x64\WindowsUpdatePauser-x64.exe
    )
)

REM --- x86 ---
echo.
echo --- Compiling for x86 (ucrt) ---
echo Command: "%BIN_UCRT%\llvm-windres" --target=i686-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_UCRT%\llvm-windres" --target=i686-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for x86 (ucrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_UCRT%\i686-w64-mingw32-clang++.exe" -target i686-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m32 -o ucrt\x86\WindowsUpdatePauser-x86.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_UCRT%\i686-w64-mingw32-clang++.exe" -target i686-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -m32 -o ucrt\x86\WindowsUpdatePauser-x86.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for x86 (ucrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_UCRT_ROOT%\bin\libwinpthread-1.dll" "ucrt\x86\" >nul 2>&1
        echo SUCCESS: ucrt\x86\WindowsUpdatePauser-x86.exe
    )
)

REM --- ARM64 ---
echo.
echo --- Compiling for ARM64 (ucrt) ---
echo Command: "%BIN_UCRT%\llvm-windres" --target=aarch64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
"%BIN_UCRT%\llvm-windres" --target=aarch64-w64-windows-gnu -i Resource.rc -o Resource.rc.o
if errorlevel 1 (
    echo ERROR: Failed to compile resources for ARM64 (ucrt)
    set /a ERROR_COUNT+=1
) else (
    echo Command: "%BIN_UCRT%\aarch64-w64-mingw32-clang++.exe" -target aarch64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -o ucrt\arm64\WindowsUpdatePauser-arm64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    "%BIN_UCRT%\aarch64-w64-mingw32-clang++.exe" -target aarch64-w64-windows-gnu -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O2 -o ucrt\arm64\WindowsUpdatePauser-arm64.exe WindowsUpdatePauser.cpp Resource.rc.o %ALL_LIBS% -Wl,-subsystem,windows -static-libgcc -static-libstdc++
    if errorlevel 1 (
        echo ERROR: Failed to compile executable for ARM64 (ucrt)
        set /a ERROR_COUNT+=1
    ) else (
        copy "%MINGW_UCRT_ROOT%\bin\libwinpthread-1.dll" "ucrt\arm64\" >nul 2>&1
        echo SUCCESS: ucrt\arm64\WindowsUpdatePauser-arm64.exe
    )
)

REM === Cleanup: Delete temporary object file ===
if exist "Resource.rc.o" (
    del "Resource.rc.o"
    echo.
    echo Temporary file Resource.rc.o deleted.
)

REM === Completion ===
echo.
echo ========================================================
if %ERROR_COUNT% equ 0 (
    echo ALL BUILDS COMPLETED SUCCESSFULLY (Direct2D version).
) else (
    echo BUILD COMPLETED WITH %ERROR_COUNT% ERROR(S).
)
echo ========================================================
echo.
echo Final structure (Direct2D build):
echo.
echo msvcrt\
echo    x64\WindowsUpdatePauser-x64.exe + libwinpthread-1.dll
echo    x86\WindowsUpdatePauser-x86.exe + libwinpthread-1.dll
echo    arm64\WindowsUpdatePauser-arm64.exe + libwinpthread-1.dll
echo.
echo ucrt\
echo    x64\WindowsUpdatePauser-x64.exe + libwinpthread-1.dll
echo    x86\WindowsUpdatePauser-x86.exe + libwinpthread-1.dll
echo    arm64\WindowsUpdatePauser-arm64.exe + libwinpthread-1.dll
echo.
echo Note: This build uses Direct2D for modern graphics rendering
echo       instead of the legacy GDI technology.
echo.

pause




