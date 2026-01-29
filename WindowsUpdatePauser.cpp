// WindowsUpdatePauser.cpp
// C++23 Compatible with llvm-mingw (libc++)

#include <expected>
#include <format>
#include <string_view>
#include <utility>
#include <memory>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <source_location>

// Windows SDK
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <winternl.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <mmsystem.h>
#include <versionhelpers.h>
#include <shellscalingapi.h>
#include <tlhelp32.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

// #pragma comment працює тільки в MSVC
#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib")
#endif

#include "Resource.hpp"

namespace win {
    using Microsoft::WRL::ComPtr;
    
    struct HandleDeleter {
        void operator()(HANDLE h) const noexcept {
            if (h && h != INVALID_HANDLE_VALUE) {
                ::CloseHandle(h);
            }
        }
    };
    using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;
    
    struct RegistryKeyDeleter {
        void operator()(HKEY h) const noexcept {
            if (h) ::RegCloseKey(h);
        }
    };
    using UniqueRegistryKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegistryKeyDeleter>;
}

// Colors - використовуємо aggregate initialization для constexpr
namespace colors {
    [[nodiscard]] constexpr D2D1_COLOR_F make(float r, float g, float b, float a = 1.0f) noexcept {
        return D2D1_COLOR_F{r / 255.0f, g / 255.0f, b / 255.0f, a};
    }
    
    inline constexpr D2D1_COLOR_F Background = make(25, 25, 25);
    inline constexpr D2D1_COLOR_F Card = make(34, 34, 34);
    inline constexpr D2D1_COLOR_F CardHover = make(44, 44, 44);
    inline constexpr D2D1_COLOR_F Accent = make(0, 120, 215);
    inline constexpr D2D1_COLOR_F AccentHover = make(16, 132, 208);
    inline constexpr D2D1_COLOR_F Active = make(0, 102, 180);
    inline constexpr D2D1_COLOR_F Pause = make(255, 193, 7);
    inline constexpr D2D1_COLOR_F Resume = make(40, 167, 69);
    inline constexpr D2D1_COLOR_F TextPrimary = make(255, 255, 255);
    inline constexpr D2D1_COLOR_F TextSecondary = make(180, 180, 180);
    inline constexpr D2D1_COLOR_F TextSuccess = make(16, 185, 129);
    inline constexpr D2D1_COLOR_F TextError = make(239, 68, 68);
    inline constexpr D2D1_COLOR_F Border = make(64, 64, 64, 0.5f);
    inline constexpr D2D1_COLOR_F Shadow = make(8, 8, 8, 0.3f);
}

struct ColorAnimation {
    D2D1_COLOR_F current{};
    D2D1_COLOR_F target{};
    
    constexpr void set_target(const D2D1_COLOR_F& color) noexcept { target = color; }
    constexpr void set_current(const D2D1_COLOR_F& color) noexcept { 
        current = color; 
        target = color; 
    }
    
    [[nodiscard]] bool update(float speed) noexcept {
        bool changed = false;
        auto lerp = [&](float& curr, float targ) {
            if (std::abs(curr - targ) > 0.001f) {
                float diff = targ - curr;
                float t = diff * speed;
                t = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                curr += t;
                changed = true;
            } else {
                curr = targ;
            }
        };
        lerp(current.r, target.r);
        lerp(current.g, target.g);
        lerp(current.b, target.b);
        lerp(current.a, target.a);
        return changed;
    }
};

struct D2DResources {
    win::ComPtr<ID2D1Factory> factory;
    win::ComPtr<ID2D1HwndRenderTarget> renderTarget;
    win::ComPtr<IDWriteFactory> writeFactory;
    win::ComPtr<IDWriteTextFormat> titleFormat;
    win::ComPtr<IDWriteTextFormat> buttonFormat;
    win::ComPtr<IDWriteTextFormat> statusFormat;
    win::ComPtr<ID2D1SolidColorBrush> solidBrush;
    win::ComPtr<ID2D1LinearGradientBrush> gradientBrush;
    
    void release() noexcept {
        gradientBrush.Reset();
        solidBrush.Reset();
        statusFormat.Reset();
        buttonFormat.Reset();
        titleFormat.Reset();
        writeFactory.Reset();
        renderTarget.Reset();
        factory.Reset();
    }
    
    [[nodiscard]] bool valid() const noexcept {
        return factory && renderTarget && writeFactory;
    }
};

struct AppState {
    HWND hWnd = nullptr;
    win::UniqueHandle hMutex;
    UINT dpi = 96;
    float dpiScale = 1.0f;
    bool isPaused = false;
    bool btnHover = false;
    bool btnPressed = false;
    bool statusHover = false;
    bool statusPressed = false;
    ColorAnimation buttonColor;
    ColorAnimation statusColor;
    ColorAnimation cardColor;
    std::wstring statusMessage = L"Ready to manage Windows Update";
    std::wstring originalStatusMessage;
    bool isOperationInProgress = false;
    float animationProgress = 0.0f;
    
    AppState() = default;
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    AppState(AppState&&) = default;
    AppState& operator=(AppState&&) = default;
};

inline D2DResources g_d2d;
inline AppState g_app;

[[nodiscard]] inline float get_dpi_scale() noexcept { return g_app.dpiScale; }
[[nodiscard]] inline int scale_int(int value) noexcept { 
    return static_cast<int>(value * g_app.dpiScale); 
}
[[nodiscard]] inline float scale_float(float value) noexcept { 
    return value * g_app.dpiScale; 
}

[[nodiscard]] inline D2D1_RECT_F get_centered_rect(float cx, float cy, float w, float h) noexcept {
    return D2D1::RectF(cx - w/2, cy - h/2, cx + w/2, cy + h/2);
}

[[nodiscard]] inline D2D1_RECT_F get_scaled_rect(float cx, float cy, float w, float h) noexcept {
    return get_centered_rect(cx * g_app.dpiScale, cy * g_app.dpiScale, 
                            w * g_app.dpiScale, h * g_app.dpiScale);
}

// EnumWindows callback - має бути статичною функцією, не lambda!
struct EnumData { 
    HWND found = nullptr; 
    std::wstring_view name; 
};

static BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam) noexcept {
    auto* d = reinterpret_cast<EnumData*>(lParam);
    std::array<wchar_t, 256> cls{};
    if (::GetClassNameW(hWnd, cls.data(), static_cast<int>(cls.size())) > 0) {
        if (std::wstring_view(cls.data()) == d->name) { 
            d->found = hWnd; 
            return FALSE; 
        }
    }
    return TRUE;
}

[[nodiscard]] HWND find_existing_instance() noexcept {
    EnumData data;
    data.name = resources::strings::CLASS_NAME;
    ::EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data.found;
}

[[nodiscard]] bool check_single_instance() {
    g_app.hMutex.reset(::CreateMutexW(nullptr, TRUE, resources::strings::MUTEX_NAME.data()));
    if (!g_app.hMutex) return false;
    
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = find_existing_instance()) {
            ::SetForegroundWindow(existing);
            ::ShowWindow(existing, SW_RESTORE);
        }
        g_app.hMutex.reset();
        return false;
    }
    return true;
}

enum class WinVersion : int { Unsupported = 0, Win10 = 10, Win11 = 11 };

[[nodiscard]] WinVersion get_windows_version() noexcept {
    OSVERSIONINFOEXW osvi{ .dwOSVersionInfoSize = sizeof(osvi) };
    using RtlGetVersionFunc = NTSTATUS(NTAPI*)(PRTL_OSVERSIONINFOW);
    
    if (HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll")) {
        if (auto RtlGetVersion = reinterpret_cast<RtlGetVersionFunc>(::GetProcAddress(hMod, "RtlGetVersion"))) {
            if (RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&osvi)) == 0) {
                if (osvi.dwMajorVersion >= 10) {
                    return (osvi.dwBuildNumber >= 22000) ? WinVersion::Win11 : WinVersion::Win10;
                }
            }
        }
    }
    return ::IsWindows10OrGreater() ? WinVersion::Win10 : WinVersion::Unsupported;
}

[[nodiscard]] inline bool is_win10_or_later() noexcept {
    auto v = get_windows_version();
    return v == WinVersion::Win10 || v == WinVersion::Win11;
}

[[nodiscard]] inline bool is_win11() noexcept {
    return get_windows_version() == WinVersion::Win11;
}

// Registry functions
[[nodiscard]] std::expected<std::wstring, LONG> reg_read_string(
    std::wstring_view keyPath, std::wstring_view valueName) 
{
    HKEY raw = nullptr;
    if (LONG res = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, KEY_READ, &raw); res != ERROR_SUCCESS) {
        return std::unexpected(res);
    }
    win::UniqueRegistryKey key(raw);
    
    std::array<wchar_t, 512> buf{};
    DWORD size = static_cast<DWORD>(buf.size() * sizeof(wchar_t));
    DWORD type = 0;
    
    LONG res = ::RegQueryValueExW(key.get(), valueName.data(), nullptr, &type,
                                  reinterpret_cast<BYTE*>(buf.data()), &size);
    if (res != ERROR_SUCCESS || type != REG_SZ) return std::unexpected(res);
    return std::wstring(buf.data());
}

[[nodiscard]] inline std::expected<std::wstring, LONG> reg_read_string(std::wstring_view valueName) {
    return reg_read_string(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", valueName);
}

bool reg_write_string(std::wstring_view keyPath, std::wstring_view name, std::wstring_view value) {
    HKEY raw = nullptr;
    if (::RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS) return false;
    win::UniqueRegistryKey key(raw);
    
    return ::RegSetValueExW(key.get(), name.data(), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.data()),
        static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool reg_write_dword(std::wstring_view keyPath, std::wstring_view name, DWORD value) {
    HKEY raw = nullptr;
    if (::RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &raw, nullptr) != ERROR_SUCCESS) return false;
    win::UniqueRegistryKey key(raw);
    
    return ::RegSetValueExW(key.get(), name.data(), 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(DWORD)) == ERROR_SUCCESS;
}

bool reg_delete_value(std::wstring_view keyPath, std::wstring_view name) {
    HKEY raw = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, KEY_SET_VALUE, &raw) != ERROR_SUCCESS) return false;
    win::UniqueRegistryKey key(raw);
    LONG res = ::RegDeleteValueW(key.get(), name.data());
    return res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND;
}

bool reg_write_binary(std::wstring_view keyPath, std::wstring_view name, 
                                    const std::vector<BYTE>& data) {
    HKEY raw = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, KEY_SET_VALUE, &raw) != ERROR_SUCCESS) return false;
    win::UniqueRegistryKey key(raw);
    return ::RegSetValueExW(key.get(), name.data(), 0, REG_BINARY, 
                           data.data(), static_cast<DWORD>(data.size())) == ERROR_SUCCESS;
}

// Time utilities
[[nodiscard]] std::wstring get_current_time_string() {
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    return std::format(L"{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

[[nodiscard]] std::wstring calculate_future_date_100_years() {
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    st.wYear += 100;
    st.wMonth = 12; st.wDay = 31; st.wHour = 16; st.wMinute = 15; st.wSecond = 25;
    return std::format(L"{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

[[nodiscard]] bool is_running_as_admin() noexcept {
    BOOL admin = FALSE;
    HANDLE token = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev{};
        DWORD size = sizeof(elev);
        if (::GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size)) {
            admin = elev.TokenIsElevated;
        }
        ::CloseHandle(token);
    }
    return admin != FALSE;
}

void show_error_and_exit(std::wstring_view msg) {
    ::PlaySoundW(L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC);
    ::MessageBoxW(g_app.hWnd, msg.data(), L"Error", MB_OK | MB_ICONERROR);
    ::PostQuitMessage(0);
}

void perform_system_restart() {
    ::PlaySoundW(L"SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
    
    HANDLE hToken = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        show_error_and_exit(L"Failed to get process token for restart.");
        return;
    }
    win::UniqueHandle tokenGuard(hToken);
    
    TOKEN_PRIVILEGES tkp{};
    if (!::LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        show_error_and_exit(L"Failed to lookup shutdown privilege.");
        return;
    }
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    if (!::AdjustTokenPrivileges(tokenGuard.get(), FALSE, &tkp, 0, nullptr, 0) ||
        ::GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        show_error_and_exit(L"Failed to enable shutdown privilege.");
        return;
    }
    
    if (!::ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
        SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_INSTALLATION | SHTDN_REASON_FLAG_PLANNED)) {
        show_error_and_exit(std::format(L"Failed to restart (Error: {})", ::GetLastError()));
    }
}

[[nodiscard]] bool is_process_running(std::wstring_view name) {
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    win::UniqueHandle guard(snap);
    
    PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
    if (!::Process32FirstW(snap, &pe)) return false;
    
    do {
        if (_wcsicmp(pe.szExeFile, name.data()) == 0) return true;
    } while (::Process32NextW(snap, &pe));
    return false;
}

bool terminate_process_by_name(std::wstring_view name) {
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    win::UniqueHandle guard(snap);
    
    PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
    bool terminated = false;
    
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name.data()) == 0) {
                if (HANDLE proc = ::OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID)) {
                    if (::TerminateProcess(proc, 0)) terminated = true;
                    ::CloseHandle(proc);
                }
            }
        } while (::Process32NextW(snap, &pe));
    }
    return terminated;
}

void open_windows_update_settings() {
    const wchar_t* target = L"SystemSettings.exe";
    if (is_process_running(target)) {
        terminate_process_by_name(target);
        ::Sleep(100);
    }
    ::ShellExecuteW(nullptr, L"open", L"ms-settings:windowsupdate", nullptr, nullptr, SW_SHOWNORMAL);
}

void show_about_dialog() {
    ::ShellExecuteW(nullptr, L"open", 
        L"https://github.com/EXLOUD?tab=repositories", nullptr, nullptr, SW_SHOWNORMAL);
}

[[nodiscard]] bool is_updates_paused() {
    auto result = reg_read_string(L"PauseUpdatesExpiryTime");
    return result.has_value() && !result->empty();
}

bool set_disabled_failure_actions(std::wstring_view servicePath) {
    std::vector<BYTE> actions(44, 0);
    return reg_write_binary(servicePath, L"FailureActions", actions);
}

bool set_waasmedic_failure_actions() {
    std::vector<BYTE> actions = {
        0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xc0, 0xd4, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xe0, 0x93, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    return reg_write_binary(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"FailureActions", actions);
}

bool apply_pause() {
    constexpr DWORD maxDays = 36500;
    reg_write_dword(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 
                   L"FlightSettingsMaxPauseDays", maxDays);
    
    auto startTime = get_current_time_string();
    auto endTime = calculate_future_date_100_years();
    
    bool success = true;
    const wchar_t* settingsKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings";
    const wchar_t* policyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    
    success &= reg_write_string(settingsKey, L"PauseUpdatesStartTime", startTime);
    success &= reg_write_string(settingsKey, L"PauseUpdatesExpiryTime", endTime);
    success &= reg_write_string(settingsKey, L"PauseFeatureUpdatesStartTime", startTime);
    success &= reg_write_string(settingsKey, L"PauseFeatureUpdatesEndTime", endTime);
    success &= reg_write_string(settingsKey, L"PauseQualityUpdatesStartTime", startTime);
    success &= reg_write_string(settingsKey, L"PauseQualityUpdatesEndTime", endTime);
    
    success &= reg_write_dword(policyKey, L"PausedFeatureStatus", 1);
    success &= reg_write_dword(policyKey, L"PausedQualityStatus", 1);
    success &= reg_write_string(policyKey, L"PausedQualityDate", endTime);
    success &= reg_write_string(policyKey, L"PausedFeatureDate", endTime);
    
    if (get_windows_version() == WinVersion::Win10 && !is_win11()) {
        reg_write_dword(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 4);
        set_disabled_failure_actions(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc");
    }
    
    reg_write_dword(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 4);
    set_disabled_failure_actions(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc");
    
    reg_write_dword(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 
                   L"ExcludeWUDriversInQualityUpdate", 1);
    reg_write_dword(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", 
                   L"SearchOrderConfig", 0);
    reg_write_dword(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", 
                   L"DontSearchWindowsUpdate", 1);
    reg_write_dword(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Device Metadata", 
                   L"PreventDeviceMetadataFromNetwork", 1);
    
    const wchar_t* wuPolicy = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    reg_write_string(wuPolicy, L"WUServer", L" ");
    reg_write_string(wuPolicy, L"WUStatusServer", L" ");
    reg_write_string(wuPolicy, L"UpdateServiceUrlAlternate", L" ");
    reg_write_dword(wuPolicy, L"DisableWindowsUpdateAccess", 1);
    reg_write_dword(wuPolicy, L"DisableOSUpgrade", 1);
    reg_write_dword(wuPolicy, L"SetDisableUXWUAccess", 1);
    reg_write_dword(wuPolicy, L"DoNotConnectToWindowsUpdateInternetLocations", 1);
    reg_write_dword(wuPolicy, L"AllowAutoWindowsUpdateDownloadOverMeteredNetwork", 0);
    
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    reg_write_dword(auKey, L"NoAutoUpdate", 1);
    reg_write_dword(auKey, L"UseWUServer", 1);
    reg_write_dword(auKey, L"AUOptions", 2);
    reg_write_dword(auKey, L"NoAutoRebootWithLoggedOnUsers", 1);
    reg_write_dword(auKey, L"AllowMUUpdateService", 0);
    
    return success;
}

bool remove_pause() {
    bool success = true;
    const wchar_t* settingsKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings";
    const wchar_t* policyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    
    success &= reg_delete_value(settingsKey, L"PauseUpdatesStartTime");
    success &= reg_delete_value(settingsKey, L"PauseUpdatesExpiryTime");
    success &= reg_delete_value(settingsKey, L"PauseFeatureUpdatesEndTime");
    success &= reg_delete_value(settingsKey, L"PauseQualityUpdatesEndTime");
    success &= reg_delete_value(settingsKey, L"PauseFeatureUpdatesStartTime");
    success &= reg_delete_value(settingsKey, L"PauseQualityUpdatesStartTime");
    
    success &= reg_write_dword(policyKey, L"PausedFeatureStatus", 0);
    success &= reg_write_dword(policyKey, L"PausedQualityStatus", 0);
    reg_delete_value(policyKey, L"PausedQualityDate");
    reg_delete_value(policyKey, L"PausedFeatureDate");
    
    if (get_windows_version() == WinVersion::Win10 && !is_win11()) {
        reg_write_dword(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 2);
        reg_delete_value(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"FailureActions");
    }
    
    reg_write_dword(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 3);
    set_waasmedic_failure_actions();
    
    reg_write_dword(settingsKey, L"ExcludeWUDriversInQualityUpdate", 0);
    reg_delete_value(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"SearchOrderConfig");
    reg_delete_value(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"DontSearchWindowsUpdate");
    reg_delete_value(L"SOFTWARE\\Policies\\Microsoft\\Windows\\Device Metadata", 
                    L"PreventDeviceMetadataFromNetwork");
    
    const wchar_t* wuPolicy = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    reg_delete_value(wuPolicy, L"WUServer");
    reg_delete_value(wuPolicy, L"WUStatusServer");
    reg_delete_value(wuPolicy, L"UpdateServiceUrlAlternate");
    reg_delete_value(wuPolicy, L"DisableWindowsUpdateAccess");
    reg_delete_value(wuPolicy, L"DisableOSUpgrade");
    reg_delete_value(wuPolicy, L"SetDisableUXWUAccess");
    reg_delete_value(wuPolicy, L"DoNotConnectToWindowsUpdateInternetLocations");
    reg_delete_value(wuPolicy, L"AllowAutoWindowsUpdateDownloadOverMeteredNetwork");
    
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    reg_delete_value(auKey, L"NoAutoUpdate");
    reg_delete_value(auKey, L"UseWUServer");
    reg_delete_value(auKey, L"AUOptions");
    reg_delete_value(auKey, L"NoAutoRebootWithLoggedOnUsers");
    reg_delete_value(auKey, L"AllowMUUpdateService");
    reg_delete_value(settingsKey, L"FlightSettingsMaxPauseDays");
    
    return success;
}

void toggle_pause() {
    if (g_app.isOperationInProgress) return;
    g_app.isOperationInProgress = true;
    g_app.originalStatusMessage.clear();
    
    bool wasPaused = g_app.isPaused;
    bool success = false;
    
    if (wasPaused) {
        success = remove_pause() && !is_updates_paused();
        g_app.statusMessage = success ? L"⚠️ Restart required to resume updates" : L"❌ Failed to resume updates";
        g_app.isPaused = !success;
        if (success) open_windows_update_settings();
    } else {
        success = apply_pause() && is_updates_paused();
        g_app.statusMessage = success ? L"⚠️ Restart required to apply pause" : L"❌ Failed to pause updates";
        g_app.isPaused = success;
        if (success) open_windows_update_settings();
    }
    
    ::PlaySoundW(success ? L"SystemDefault" : L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC);
    g_app.isOperationInProgress = false;
    ::InvalidateRect(g_app.hWnd, nullptr, FALSE);
}

HRESULT initialize_direct2d(HWND hWnd) {
    HRESULT hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, 
                                     g_d2d.factory.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    
    RECT rc;
    ::GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.dpiX = 96.0f; props.dpiY = 96.0f;
    
    hr = g_d2d.factory->CreateHwndRenderTarget(props,
        D2D1::HwndRenderTargetProperties(hWnd, size),
        g_d2d.renderTarget.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    
    g_d2d.renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    
    hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_d2d.writeFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) return hr;
    
    auto create_format = [&](float size, DWRITE_FONT_WEIGHT weight, win::ComPtr<IDWriteTextFormat>& fmt) {
        return g_d2d.writeFactory->CreateTextFormat(L"Segoe UI Variable", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-US", fmt.ReleaseAndGetAddressOf());
    };
    
    if (SUCCEEDED(hr = create_format(22.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, g_d2d.titleFormat))) {
        g_d2d.titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (SUCCEEDED(hr = create_format(15.0f, DWRITE_FONT_WEIGHT_MEDIUM, g_d2d.buttonFormat))) {
        g_d2d.buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.buttonFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (SUCCEEDED(hr = create_format(13.0f, DWRITE_FONT_WEIGHT_NORMAL, g_d2d.statusFormat))) {
        g_d2d.statusFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.statusFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    
    hr = g_d2d.renderTarget->CreateSolidColorBrush(colors::TextPrimary, 
                                                   g_d2d.solidBrush.ReleaseAndGetAddressOf());
    
    if (SUCCEEDED(hr)) {
        win::ComPtr<ID2D1GradientStopCollection> stops;
        std::array<D2D1_GRADIENT_STOP, 2> gs{};
        gs[0] = {0.0f, D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.8f)};
        gs[1] = {1.0f, D2D1::ColorF(0.06f, 0.52f, 0.89f, 0.3f)};
        
        g_d2d.renderTarget->CreateGradientStopCollection(gs.data(), static_cast<UINT32>(gs.size()),
            D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, stops.ReleaseAndGetAddressOf());
        
        if (stops) {
            g_d2d.renderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0), D2D1::Point2F(100,100)),
                stops.Get(), g_d2d.gradientBrush.ReleaseAndGetAddressOf());
        }
    }
    return hr;
}

void resize_direct2d(HWND hWnd) {
    if (!g_d2d.renderTarget) return;
    RECT rc;
    ::GetClientRect(hWnd, &rc);
    g_d2d.renderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
}

void draw_rounded_card(const D2D1_RECT_F& rect, float radius, 
                      const D2D1_COLOR_F& color, bool withShadow = true) {
    if (!g_d2d.renderTarget || !g_d2d.solidBrush) return;

    float r = radius * g_app.dpiScale;
    if (withShadow) {
        auto shadow = rect;
        float off = 2.0f * g_app.dpiScale;
        shadow.left += off; shadow.top += off; 
        shadow.right += off; shadow.bottom += off;
        g_d2d.solidBrush->SetColor(D2D1::ColorF(0,0,0,0.2f));
        g_d2d.renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(shadow, r + g_app.dpiScale, r + g_app.dpiScale), 
            g_d2d.solidBrush.Get());
    }

    g_d2d.solidBrush->SetColor(color);
    g_d2d.renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), g_d2d.solidBrush.Get());
    g_d2d.solidBrush->SetColor(colors::Border);
    g_d2d.renderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), 
        g_d2d.solidBrush.Get(), 0.5f * g_app.dpiScale);
}

void draw_status_panel(const D2D1_RECT_F& rect, std::wstring_view text, const D2D1_COLOR_F& bg) {
    if (!g_d2d.renderTarget || !g_d2d.solidBrush) return;
    draw_rounded_card(rect, 8.0f, bg, true);
    
    D2D1_COLOR_F tc = colors::TextSecondary;
    if (text.find(L"✅") != std::wstring_view::npos) tc = colors::TextSuccess;
    else if (text.find(L"❌") != std::wstring_view::npos) tc = colors::TextError;
    else if (text.find(L"🔗") != std::wstring_view::npos) tc = colors::Accent;
    
    g_d2d.solidBrush->SetColor(tc);
    g_d2d.renderTarget->DrawText(text.data(), static_cast<UINT32>(text.length()),
        g_d2d.statusFormat.Get(), rect, g_d2d.solidBrush.Get());
}

void update_button_target_color() {
    D2D1_COLOR_F target;
    bool restartMode = g_app.statusMessage.find(L"Restart required") != std::wstring::npos;
    
    if (restartMode) {
        target = g_app.btnPressed ? 
            D2D1::ColorF(colors::Resume.r * 0.8f, colors::Resume.g * 0.8f, colors::Resume.b * 0.8f) :
            colors::Resume;
    } else {
        if (g_app.btnPressed) {
            target = g_app.isPaused ? 
                D2D1::ColorF(colors::Resume.r * 0.8f, colors::Resume.g * 0.8f, colors::Resume.b * 0.8f) :
                D2D1::ColorF(colors::Pause.r * 0.8f, colors::Pause.g * 0.8f, colors::Pause.b * 0.8f);
        } else if (g_app.btnHover) {
            target = g_app.isPaused ? colors::Resume : colors::Pause;
        } else {
            target = colors::Accent;
        }
    }
    g_app.buttonColor.set_target(target);
}

void update_status_target_color() {
    D2D1_COLOR_F target = colors::Card;
    if (g_app.statusPressed) target = D2D1::ColorF(colors::Card.r * 0.85f, colors::Card.g * 0.85f, colors::Card.b * 0.85f);
    else if (g_app.statusHover) target = colors::CardHover;
    g_app.statusColor.set_target(target);
}

void update_card_target_color() {
    D2D1_COLOR_F target = colors::Card;
    if (g_app.btnHover || g_app.btnPressed) target = colors::CardHover;
    g_app.cardColor.set_target(target);
}

void paint_window(HWND hWnd) {
    if (!g_d2d.renderTarget) return;
    
    g_d2d.renderTarget->BeginDraw();
    g_d2d.renderTarget->Clear(D2D1::ColorF(20.0f/255.0f, 20.0f/255.0f, 20.0f/255.0f));
    
    static win::ComPtr<ID2D1LinearGradientBrush> bgBrush;
    if (!bgBrush && g_d2d.renderTarget) {
        win::ComPtr<ID2D1GradientStopCollection> coll;
        std::array<D2D1_GRADIENT_STOP, 2> stops{};
        stops[0] = {0.0f, D2D1::ColorF(20.0f/255.0f, 20.0f/255.0f, 20.0f/255.0f, 1.0f)};
        stops[1] = {1.0f, D2D1::ColorF(30.0f/255.0f, 30.0f/255.0f, 30.0f/255.0f, 1.0f)};
        g_d2d.renderTarget->CreateGradientStopCollection(stops.data(), 2, coll.ReleaseAndGetAddressOf());
        if (coll) {
            g_d2d.renderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0,0), 
                D2D1::Point2F(0, static_cast<float>(resources::ui::WINDOW_HEIGHT) * g_app.dpiScale)),
                coll.Get(), bgBrush.ReleaseAndGetAddressOf());
        }
    }
    if (bgBrush) {
        g_d2d.renderTarget->FillRectangle(
            D2D1::RectF(0, 0, resources::ui::WINDOW_WIDTH * g_app.dpiScale, 
                       resources::ui::WINDOW_HEIGHT * g_app.dpiScale), bgBrush.Get());
    }
    
    auto titleRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 35.0f, 
                                    static_cast<float>(resources::ui::WINDOW_WIDTH), 30.0f);
    g_d2d.solidBrush->SetColor(colors::TextPrimary);
    constexpr std::wstring_view title = L"Windows Update Pauser";
    g_d2d.renderTarget->DrawText(title.data(), static_cast<UINT32>(title.length()),
        g_d2d.titleFormat.Get(), titleRect, g_d2d.solidBrush.Get());
    
    auto cardRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                   resources::ui::CARD_WIDTH, resources::ui::CARD_HEIGHT);
    draw_rounded_card(cardRect, 12.0f, g_app.cardColor.current, true);
    
    auto btnRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                  resources::ui::BUTTON_WIDTH, resources::ui::BUTTON_HEIGHT);
    bool restartMode = g_app.statusMessage.find(L"Restart required") != std::wstring::npos;
    std::wstring_view btnText = restartMode ? L"🔄 Restart Now" : 
        (g_app.isPaused ? L"▶  Resume Updates" : L"⏸  Pause for 100 years");
    
    auto finalBtnRect = btnRect;
    if (g_app.btnPressed) {
        float nw = (btnRect.right - btnRect.left) * 0.98f;
        float nh = (btnRect.bottom - btnRect.top) * 0.98f;
        float cx = (btnRect.left + btnRect.right) / 2.0f;
        float cy = (btnRect.top + btnRect.bottom) / 2.0f;
        finalBtnRect = D2D1::RectF(cx - nw/2, cy - nh/2, cx + nw/2, cy + nh/2);
    }
    
    draw_rounded_card(finalBtnRect, 12.0f, g_app.buttonColor.current, !g_app.btnPressed);
    
    if (g_d2d.gradientBrush) {
        g_d2d.gradientBrush->SetStartPoint(D2D1::Point2F(finalBtnRect.left, finalBtnRect.top));
        g_d2d.gradientBrush->SetEndPoint(D2D1::Point2F(finalBtnRect.right, finalBtnRect.bottom));
        g_d2d.gradientBrush->SetOpacity(0.3f);
        g_d2d.renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(finalBtnRect, 12.0f * g_app.dpiScale, 12.0f * g_app.dpiScale),
            g_d2d.gradientBrush.Get());
        g_d2d.gradientBrush->SetOpacity(1.0f);
    }
    
    g_d2d.solidBrush->SetColor(colors::TextPrimary);
    g_d2d.renderTarget->DrawText(btnText.data(), static_cast<UINT32>(btnText.length()),
        g_d2d.buttonFormat.Get(), finalBtnRect, g_d2d.solidBrush.Get());
    
    auto statusRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 160.0f, 
                                     resources::ui::STATUS_WIDTH, resources::ui::STATUS_HEIGHT);
    draw_status_panel(statusRect, g_app.statusMessage, g_app.statusColor.current);
    
    HRESULT hr = g_d2d.renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_d2d.release();
        (void)initialize_direct2d(hWnd);
    }
}

[[nodiscard]] bool point_in_rect(const D2D1_RECT_F& rect, POINT pt) noexcept {
    return pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_app.hWnd = hWnd;
        g_app.dpi = ::GetDpiForWindow(hWnd);
        g_app.dpiScale = static_cast<float>(g_app.dpi) / 96.0f;
        g_app.isPaused = is_updates_paused();
        
        g_app.buttonColor.set_current(colors::Accent);
        g_app.statusColor.set_current(colors::Card);
        g_app.cardColor.set_current(colors::Card);
        update_button_target_color();
        update_status_target_color();
        update_card_target_color();
        
        (void)initialize_direct2d(hWnd);
        
        BOOL dark = TRUE;
        ::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        enum { DWMWCP_ROUND = 2 };
        int corner = DWMWCP_ROUND;
        ::DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));
        
        ::SetTimer(hWnd, resources::ui::TIMER_ID, resources::ui::TIMER_INTERVAL, nullptr);
        ::SetTimer(hWnd, resources::ui::TIMER_ANIMATION_ID, resources::ui::TIMER_ANIMATION_INTERVAL, nullptr);
        return 0;
    }
    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        ::BeginPaint(hWnd, &ps);
        paint_window(hWnd);
        ::EndPaint(hWnd, &ps);
        return 0;
    }
    
    case WM_ERASEBKGND: return 1;
    
    case WM_SIZE:
        resize_direct2d(hWnd);
        ::InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    
    case WM_TIMER:
        if (wParam == resources::ui::TIMER_ID) {
            POINT pt;
            ::GetCursorPos(&pt);
            ::ScreenToClient(hWnd, &pt);
            
            auto btnRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                          resources::ui::BUTTON_WIDTH, resources::ui::BUTTON_HEIGHT);
            bool newHover = point_in_rect(btnRect, pt);
            if (newHover != g_app.btnHover) {
                g_app.btnHover = newHover;
                update_button_target_color();
                update_card_target_color();
                ::InvalidateRect(hWnd, nullptr, FALSE);
            }
            
            auto statusRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 160.0f, 
                                             resources::ui::STATUS_WIDTH, resources::ui::STATUS_HEIGHT);
            bool newStatusHover = point_in_rect(statusRect, pt);
            if (newStatusHover != g_app.statusHover) {
                g_app.statusHover = newStatusHover;
                update_status_target_color();
                
                bool restartMode = g_app.statusMessage.find(L"Restart required") != std::wstring::npos;
                if (g_app.statusHover && !restartMode) {
                    if (g_app.originalStatusMessage.empty()) g_app.originalStatusMessage = g_app.statusMessage;
                    g_app.statusMessage = L"🔗 Open author's GitHub repositories";
                } else if (!g_app.statusHover && !restartMode && !g_app.originalStatusMessage.empty()) {
                    g_app.statusMessage = g_app.originalStatusMessage;
                    g_app.originalStatusMessage.clear();
                }
                ::InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (wParam == resources::ui::TIMER_ANIMATION_ID) {
            bool redraw = g_app.buttonColor.update(resources::ui::COLOR_TRANSITION_SPEED);
            redraw |= g_app.statusColor.update(resources::ui::COLOR_TRANSITION_SPEED);
            redraw |= g_app.cardColor.update(resources::ui::COLOR_TRANSITION_SPEED);
            if (redraw) ::InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    
    case WM_SETCURSOR: {
        POINT pt;
        ::GetCursorPos(&pt);
        ::ScreenToClient(hWnd, &pt);
        auto btnRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                      resources::ui::BUTTON_WIDTH, resources::ui::BUTTON_HEIGHT);
        auto statusRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 160.0f, 
                                         resources::ui::STATUS_WIDTH, resources::ui::STATUS_HEIGHT);
        if (point_in_rect(btnRect, pt) || point_in_rect(statusRect, pt)) {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }
    
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        auto btnRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                      resources::ui::BUTTON_WIDTH, resources::ui::BUTTON_HEIGHT);
        auto statusRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 160.0f, 
                                         resources::ui::STATUS_WIDTH, resources::ui::STATUS_HEIGHT);
        
        if (point_in_rect(btnRect, pt)) {
            g_app.btnPressed = true;
            update_button_target_color();
            update_card_target_color();
            ::SetCapture(hWnd);
            ::InvalidateRect(hWnd, nullptr, FALSE);
        } else if (point_in_rect(statusRect, pt)) {
            g_app.statusPressed = true;
            update_status_target_color();
            ::SetCapture(hWnd);
            ::InvalidateRect(hWnd, nullptr, FALSE);
            ::PlaySoundW(L"SystemDefault", nullptr, SND_ALIAS | SND_ASYNC);
        }
        return 0;
    }
    
    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        auto btnRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 95.0f, 
                                      resources::ui::BUTTON_WIDTH, resources::ui::BUTTON_HEIGHT);
        auto statusRect = get_scaled_rect(resources::ui::WINDOW_WIDTH / 2.0f, 160.0f, 
                                         resources::ui::STATUS_WIDTH, resources::ui::STATUS_HEIGHT);
        
        if (g_app.btnPressed) {
            ::ReleaseCapture();
            g_app.btnPressed = false;
            update_button_target_color();
            update_card_target_color();
            if (point_in_rect(btnRect, pt)) {
                if (g_app.statusMessage.find(L"Restart required") != std::wstring::npos)
                    perform_system_restart();
                else
                    toggle_pause();
            }
            ::InvalidateRect(hWnd, nullptr, TRUE);
        } else if (g_app.statusPressed) {
            ::ReleaseCapture();
            g_app.statusPressed = false;
            update_status_target_color();
            if (point_in_rect(statusRect, pt)) show_about_dialog();
            ::InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    
    case WM_DPICHANGED: {
        g_app.dpi = LOWORD(wParam);
        g_app.dpiScale = static_cast<float>(g_app.dpi) / 96.0f;
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        
        int cw = ::MulDiv(resources::ui::WINDOW_WIDTH, g_app.dpi, 96);
        int ch = ::MulDiv(resources::ui::WINDOW_HEIGHT, g_app.dpi, 96);
        
        RECT wr = {0, 0, cw, ch};
        DWORD style = ::GetWindowLong(hWnd, GWL_STYLE);
        DWORD exStyle = ::GetWindowLong(hWnd, GWL_EXSTYLE);
        ::AdjustWindowRectExForDpi(&wr, style, FALSE, exStyle, g_app.dpi);
        
        ::SetWindowPos(hWnd, nullptr, prc->left, prc->top, 
                      wr.right - wr.left, wr.bottom - wr.top,
                      SWP_NOZORDER | SWP_NOACTIVATE);
        resize_direct2d(hWnd);
        ::InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }
    
    case WM_DESTROY:
        ::KillTimer(hWnd, resources::ui::TIMER_ID);
        ::KillTimer(hWnd, resources::ui::TIMER_ANIMATION_ID);
        g_d2d.release();
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    if (!check_single_instance()) return 0;
    
    if (!is_win10_or_later()) {
        ::MessageBoxW(nullptr, L"Windows 10 or later required.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    if (!is_running_as_admin()) {
        wchar_t path[MAX_PATH];
        if (!::GetModuleFileNameW(nullptr, path, MAX_PATH)) return 1;
        
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = path;
        sei.nShow = SW_NORMAL;
        
        if (!::ShellExecuteExW(&sei)) {
            if (::GetLastError() == ERROR_CANCELLED) {
                ::MessageBoxW(nullptr, L"Administrator privileges required.", L"Warning", MB_OK | MB_ICONWARNING);
            }
            return 1;
        }
        return 0;
    }
    
    HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return 1;
    auto comGuard = std::unique_ptr<void, decltype([](void*){ ::CoUninitialize(); })>(reinterpret_cast<void*>(1));
    
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES };
    ::InitCommonControlsEx(&icex);
    
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = resources::strings::CLASS_NAME.data();
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = ::LoadIconW(hInst, MAKEINTRESOURCEW(resources::icons::MAIN));
    wc.hIconSm = ::LoadIconW(hInst, MAKEINTRESOURCEW(resources::icons::SMALL));
    
    if (!::RegisterClassExW(&wc)) return 1;
    
    UINT dpi = 96;
    HMONITOR mon = ::MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY);
    ::GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpi, &dpi);
    g_app.dpi = dpi;
    g_app.dpiScale = static_cast<float>(dpi) / 96.0f;
    
    int cw = ::MulDiv(resources::ui::WINDOW_WIDTH, dpi, 96);
    int ch = ::MulDiv(resources::ui::WINDOW_HEIGHT, dpi, 96);
    
    RECT wr = {0, 0, cw, ch};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOPMOST;
    ::AdjustWindowRectExForDpi(&wr, style, FALSE, exStyle, dpi);
    
    MONITORINFO mi = { sizeof(mi) };
    ::GetMonitorInfoW(mon, &mi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - (wr.right - wr.left)) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - (wr.bottom - wr.top)) / 2;
    
    HWND hWnd = ::CreateWindowExW(exStyle, resources::strings::CLASS_NAME.data(),
        resources::strings::WINDOW_TITLE.data(), style, x, y,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr);
    
    if (!hWnd) return 1;
    
    ::ShowWindow(hWnd, nCmdShow);
    ::UpdateWindow(hWnd);
    
    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    
    return static_cast<int>(msg.wParam);
}