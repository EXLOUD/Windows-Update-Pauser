<div align="center">

### 👇

  <p>
    <a href="https://github.com/EXLOUD/Windows-Update-Pauser/releases/tag/v2.0.0">
      <img src="https://img.shields.io/badge/_  >_Download_This_Program_<_-darkgreen?style=for-the-badge">
    </a>
  </p>

---
 
### 👀 Repository Views

<img alt="count" src="https://count.getloli.com/get/@:EXLOUD-WUP?theme=rule34" />

**⭐ If this tool helped you, please consider giving it a star! ⭐**

</div>

---

<p align="center">
 <img width="580px" src="assets/preview.gif" alt="Windows Update Pauser" />
</p>

<h2 align="center">Windows Update Pauser</h2>
<p align="center">Modern Windows Update Control Tool — C++23, Fully Static, ARM64/x64/x86</p>

<p align="center">
  <img src="https://custom-icon-badges.demolab.com/badge/Version-v2.0.0-brightgreen?logo=tag&logoColor=white" />
  <a href="https://github.com/EXLOUD/Windows-Update-Pauser/issues"><img alt="Issues" src="https://img.shields.io/github/issues/EXLOUD/Windows-Update-Pauser?color=F48D73" /></a>
  <img src="https://custom-icon-badges.demolab.com/badge/Toolchain-llvm--mingw%2020260127%20%28LLVM%2022.1.0%20RC%202%29-5A5CDA?logo=llvm&logoColor=white" /></a>
  <img src="https://custom-icon-badges.demolab.com/badge/Windows-10%2F11-0078D4?logo=windows&logoColor=white" />
  <a href="https://github.com/EXLOUD/Windows-Update-Pauser/blob/main/LICENSE"><img alt="License" src="https://img.shields.io/github/license/EXLOUD/Windows-Update-Pauser.svg" /></a>
</p>

<p align="center">
  <a href="#-what-is-windows-update-pauser">What is it?</a> | <a href="#-features">Features</a> | <a href="#-how-to-use">How to use</a> | <a href="#-building">Building</a>
</p>

---

## 🎯 What is Windows Update Pauser?

Windows Update Pauser is a modern, user-friendly Windows application designed to provide **complete, irreversible control** over Windows Updates. With its intuitive interface, it allows you to pause Windows Updates for **100 years**, or resume them instantly with just one click.

This v2.0.0 release brings a **complete C++23 modernization**, **zero external dependencies** (fully static builds), and a **unified resource management system** that eliminates data duplication between code and Windows resources.

---

## ✨ Features

<details>
<summary>Click to expand features</summary>

### 🎨 User Experience
- **Modern Dark UI Interface** - Clean, card-based design with smooth animations
- **One-Click Toggle** - Simple pause/resume functionality with large buttons
- **Real-time Status Feedback** - Color-coded success/error messages with visual indicators
- **Sound Notifications** - Audio confirmation after successful operations

### 🔧 Technical Capabilities
- **Complete Update Coverage** - Handles both feature and quality updates
- **Deep Registry Lockdown** - Fully disables Windows Update, drivers, metadata, and services
- **Multi-Architecture Support** - Native x64, x86, and ARM64 builds (Surface Pro X, Snapdragon X Elite)
- **Zero Dependencies** - Single portable executable (620-800 KB), no DLLs required
- **System Integration** - Automatically opens Windows Update settings after actions

### 🚀 Modern C++23 Architecture
- **Type Safety** - `std::format`, `std::expected`, `constexpr` everywhere
- **Memory Safety** - Full RAII implementation with `ComPtr` and custom smart handles
- **Unified Resources** - Single `Resource.hpp` file for both C++ and Windows RC
- **Cross-Platform Build** - llvm-mingw with static libc++ linkage

### 🛡️ System Integration
- **Administrator Support** - Requires and handles elevated privileges properly
- **Windows Compatibility** - Full support for Windows 10 and 11 (x86, x64, ARM64)
- **Error Handling** - Graceful error management with user-friendly messages
- **Security Hardening** - LTO, Control Flow Guard, and stack protection enabled

</details>

---

## 🚀 How to use?

### 4.1 Download Release (Recommended)

<details>
<summary>Click to expand</summary>

#### 4.1.1 Download

Download the latest **v2.0.0** release from [GitHub Releases](https://github.com/EXLOUD/Windows-Update-Pauser/releases).

#### 4.1.2 Installation

1. Extract the downloaded archive.
2. Choose the folder matching your system:
   - `-x86` → For 32-bit Intel/AMD
   - `-x64` → For 64-bit Intel/AMD (Recommended)
   - `-arm64` → For ARM64 devices (Surface Pro X, Snapdragon X Elite)
3. Run `WindowsUpdatePauser-x64.exe` (or `-x86.exe` / `-arm64.exe`) **as Administrator**.

#### 4.1.3 Usage

1. Launch the application as Administrator.
2. Click the **⏸️ Pause for 100 years** button to fully disable updates.
3. Click the **▶️ Resume Updates** button to restore default behavior.
4. Receive visual and audio feedback when operation completes.
5. Windows Update settings will open automatically after each action.

> **Note:** All builds are fully static portable executables. No installation or external DLLs required.

</details>

---

These settings block: driver updates, metadata fetching, update servers, auto-updates, OS upgrades, and the Update Health Service. 

---

## 💻 System Requirements

- **Operating System**: Windows 10/11 (64-bit or 32-bit, including ARM64)  
- **Architecture**: x86, x64, ARM64  
- **Privileges**: Administrator rights (required)  
- **Dependencies**: None (fully static executable)  

---

## 🔨 Building

<details>
<summary>Click to expand building instructions</summary>

### Prerequisites
- [llvm-mingw 20260127 (LLVM 22.1.0 RC 2)](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260127) — Download here  
  - `llvm-mingw-20260127-msvcrt-x86_64.zip`  
  - `llvm-mingw-20260127-ucrt-x86_64.zip`  
  - `llvm-mingw-20260127-ucrt-aarch64.zip` (for ARM64)  

### Required files in project root:
- `WindowsUpdatePauser.cpp`  
- `Resource.rc`  
- `Resource.hpp`  
- `icon.ico` (32x32)  
- `icon_small.ico` (16x16)  

### One-Click Build
Run `build-release.bat` to automatically generate 6 fully static binaries:
├── msvcrt
│ ├── x64 → WindowsUpdatePauser-x64.exe (694 KB)
│ ├── x86 → WindowsUpdatePauser-x86.exe (801 KB)
│ └── arm64 → WindowsUpdatePauser-arm64.exe (665 KB)
└── ucrt
├── x64 → WindowsUpdatePauser-x64.exe (645 KB)
├── x86 → WindowsUpdatePauser-x86.exe (751 KB)
└── arm64 → WindowsUpdatePauser-arm64.exe (623 KB)

text


Script validates all source files and compiler paths before starting. Uses C++23 with `-std=c++23 -stdlib=libc++`.

### Manual Build (Single Target)

# For x64 UCRT build:
C:\llvm-mingw-ucrt\bin\x86_64-w64-mingw32-clang++.exe ^
  -target x86_64-w64-windows-gnu ^
  -std=c++23 -stdlib=libc++ -fexperimental-library ^
  -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE -O3 -flto=thin ^
  -static-libgcc -static-libstdc++ ^
  -Wl,-Bstatic -lwinpthread -lc++ -lc++abi -lunwind -Wl,-Bdynamic ^
  -Wl,-subsystem,windows:6.0 -Wl,--gc-sections ^
  -o WindowsUpdatePauser.exe ^
  WindowsUpdatePauser.cpp Resource.rc.o ^
  -lcomctl32 -ldwmapi -luxtheme -lwinmm -lversion -lshcore ^
  -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid ^
  -ld2d1 -ldwrite -lwindowscodecs
  
</details>

⚠️ Important Disclaimer
This software modifies the Windows registry to control update behavior. While designed to be safe and reversible, use it at your own risk. The authors are not responsible for any system damage, loss of updates, or security issues.

📄 License
The code is available under the MIT license.

👨‍💻 Author
<div align="center">
  
EXLOUD

Made with ❤️ for the Windows development community

</div><div align="center">
  
⭐ If you find this project helpful, please consider giving it a star! ⭐

</div>
