cmake_minimum_required(VERSION 3.26)

# ------------------------------------------------------------------------------
# Project
# ------------------------------------------------------------------------------
project(
    WindowsUpdatePauser
    VERSION 2.0.1
    LANGUAGES CXX RC
)

# ------------------------------------------------------------------------------
# Toolchain expectations
# ------------------------------------------------------------------------------
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project requires Clang (LLVM).")
endif()

# ------------------------------------------------------------------------------
# C++ standard
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Global compile definitions
# ------------------------------------------------------------------------------
add_compile_definitions(
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    UNICODE
    _UNICODE
    NDEBUG
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ------------------------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------------------------
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-missing-field-initializers

    -O3
    -ffunction-sections
    -fdata-sections

    -fvisibility=hidden
    -fvisibility-inlines-hidden
)

# ------------------------------------------------------------------------------
# Linker flags (LLD / Windows)
# ------------------------------------------------------------------------------
add_link_options(
    -fuse-ld=lld
    -Wl,/OPT:REF
    -Wl,/OPT:ICF
    -Wl,/subsystem:windows
)

# ------------------------------------------------------------------------------
# LTO
# ------------------------------------------------------------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)

if (IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# ------------------------------------------------------------------------------
# Security flags
# ------------------------------------------------------------------------------
# Detect target arch from the compiler target triple (works for cross-compilation)
# CMAKE_SYSTEM_PROCESSOR is unreliable when cross-compiling without a toolchain file
string(FIND "${CMAKE_CXX_COMPILER_TARGET}" "aarch64" _is_arm64)
string(FIND "${CMAKE_CXX_FLAGS}" "aarch64" _is_arm64_flags)

if (_is_arm64 GREATER_EQUAL 0 OR _is_arm64_flags GREATER_EQUAL 0)
    set(_TARGET_ARM64 TRUE)
else()
    set(_TARGET_ARM64 FALSE)
endif()

add_compile_options(-fstack-protector-strong)
add_compile_definitions(_FORTIFY_SOURCE=2)

if (NOT _TARGET_ARM64)
    # Intel CET — x86/x64 only
    add_compile_options(-fcf-protection=full)
else()
    # ARM64 equivalent — PAC + BTI
    add_compile_options(-mbranch-protection=standard)
endif()

# ------------------------------------------------------------------------------
# Static runtime / static linking
# ------------------------------------------------------------------------------
add_link_options(-static)

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------
set(SOURCES
    WindowsUpdatePauser.cpp
    Resource.rc
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# ------------------------------------------------------------------------------
# Include paths
# ------------------------------------------------------------------------------
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ------------------------------------------------------------------------------
# Windows libraries
# ------------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        advapi32
        comctl32
        dwmapi
        uxtheme
        winmm
        version
        shcore
        shell32
        user32
        kernel32
        ole32
        oleaut32
        uuid
        d2d1
        d3d11
        dxgi
        dwrite
        windowscodecs
)

# ------------------------------------------------------------------------------
# Output directories & filename
# ------------------------------------------------------------------------------
if (NOT DEFINED ARCH_SUFFIX)
    set(ARCH_SUFFIX "x64")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "WindowsUpdatePauser-${ARCH_SUFFIX}"
)
# ------------------------------------------------------------------------------
# Strip symbols in Release
# ------------------------------------------------------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND llvm-strip --strip-all $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping symbols"
    )
endif()
