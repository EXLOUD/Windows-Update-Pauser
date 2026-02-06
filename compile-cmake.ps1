#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Windows Update Pauser - CMake + Ninja " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Pre-flight checks
foreach ($tool in @("cmake", "ninja", "clang++")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Host "ERROR: '$tool' not found on PATH!" -ForegroundColor Red
        exit 1
    }
}

$archs = @(
    @{
        Name   = "x86"
        Dir    = "build-x86"
        Target = "i686-pc-windows-msvc"
        Proc   = "X86"
    },
    @{
        Name   = "x64"
        Dir    = "build-x64"
        Target = "x86_64-pc-windows-msvc"
        Proc   = "AMD64"
    },
    @{
        Name   = "arm64"
        Dir    = "build-arm64"
        Target = "aarch64-pc-windows-msvc"
        Proc   = "ARM64"
    }
)

foreach ($arch in $archs) {
    # Clean
    if (Test-Path $arch.Dir) {
        Write-Host "Cleaning $($arch.Name) build..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $arch.Dir
    }

    Write-Host ""
    Write-Host "Building $($arch.Name)..." -ForegroundColor Green
    Write-Host "----------------------------------------"

    # Configure
    cmake -G Ninja `
          -DCMAKE_BUILD_TYPE=Release `
          -DCMAKE_CXX_COMPILER=clang++ `
          -DCMAKE_CXX_COMPILER_TARGET="$($arch.Target)" `
          -DCMAKE_SYSTEM_NAME=Windows `
          -DCMAKE_SYSTEM_PROCESSOR="$($arch.Proc)" `
          -DARCH_SUFFIX="$($arch.Name)" `
          -B $arch.Dir `
          -S .

    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed for $($arch.Name)!" -ForegroundColor Red
        exit 1
    }

    # Build
    ninja -C $arch.Dir

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed for $($arch.Name)!" -ForegroundColor Red
        exit 1
    }

    $bin = "$($arch.Dir)/bin/WindowsUpdatePauser-$($arch.Name).exe"
    $size = (Get-Item $bin).Length
    Write-Host "SUCCESS: WindowsUpdatePauser-$($arch.Name).exe ($size bytes)" -ForegroundColor Green
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " ALL BUILDS COMPLETED SUCCESSFULLY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output binaries:" -ForegroundColor White
foreach ($arch in $archs) {
    $bin = "$($arch.Dir)/bin/WindowsUpdatePauser-$($arch.Name).exe"
    if (Test-Path $bin) {
        $size = (Get-Item $bin).Length
        Write-Host "  [$($arch.Name.PadRight(5))] $bin ($size bytes)" -ForegroundColor Gray
    }
}
Write-Host ""