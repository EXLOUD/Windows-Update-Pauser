// WindowsUpdatePauser.cpp
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include "Resource.h"
#include <winternl.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <mmsystem.h>
#include <versionhelpers.h>
#include <shellscalingapi.h>
#include <tlhelp32.h>
#include <string>
#include <memory>
#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib")
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")
// ==================================================================
// CONSTANTS AND GLOBALS
// ==================================================================
constexpr wchar_t CLASS_NAME[] = L"WUPauser";
constexpr int WINDOW_WIDTH = 465;
constexpr int WINDOW_HEIGHT = 250;
constexpr int H_MARGIN = 20;
constexpr int TIMER_ID = 1;
constexpr int TIMER_INTERVAL = 50;
// Color scheme
constexpr COLORREF BG_COLOR = RGB(28, 28, 28);
constexpr COLORREF CARD_COLOR = RGB(42, 42, 42);
constexpr COLORREF ACCENT_COLOR = RGB(0, 120, 215);
constexpr COLORREF HOVER_COLOR = RGB(16, 132, 208);
constexpr COLORREF ACTIVE_COLOR = RGB(0, 102, 180);
constexpr COLORREF PAUSE_COLOR = RGB(255, 193, 7);
constexpr COLORREF RESUME_COLOR = RGB(40, 167, 69);
constexpr COLORREF TEXT_PRIMARY = RGB(255, 255, 255);
constexpr COLORREF TEXT_SECONDARY = RGB(180, 180, 180);
constexpr COLORREF TEXT_SUCCESS = RGB(16, 185, 129);
constexpr COLORREF TEXT_ERROR = RGB(239, 68, 68);
constexpr COLORREF BORDER_COLOR = RGB(64, 64, 64);
constexpr COLORREF SHADOW_COLOR = RGB(8, 8, 8);
// Global variables
struct AppState {
    HWND hWnd = nullptr;
    UINT dpi = 96;
    bool isPaused = false;
    bool btnHover = false;
    bool btnPressed = false;
    std::wstring statusMessage = L"Ready to manage Windows Update pause";
    bool isOperationInProgress = false;
} g_app;
struct GDIResources {
    HFONT hFontTitle = nullptr;
    HFONT hFontButton = nullptr;
    HFONT hFontStatus = nullptr;
    HBRUSH hBrushBg = nullptr;
    HBRUSH hBrushCard = nullptr;
    HDC hMemDC = nullptr;
    HBITMAP hMemBitmap = nullptr;
    RECT clientRect = {};
} g_gdi;
// ==================================================================
// UTILITY FUNCTIONS
// ==================================================================
inline int Scale(int value) {
    return MulDiv(value, g_app.dpi, 96);
}
inline RECT ScaleRect(int left, int top, int right, int bottom) {
    return { Scale(left), Scale(top), Scale(right), Scale(bottom) };
}
// ==================================================================
// WINDOWS VERSION CHECKING
// ==================================================================
bool IsWindows10OrLater() {
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };
    // Use RtlGetVersion for accurate version detection
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandle(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            NTSTATUS status = RtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
            if (status == 0) {
                return (osvi.dwMajorVersion > 10) ||
                    (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion >= 0);
            }
        }
    }
    // Fallback
    return IsWindows10OrGreater();
}
bool CheckWindowsVersion() {
    if (!IsWindows10OrLater()) {
        MessageBoxW(nullptr,
            L"This application requires Windows 10 or later.\n"
            L"Your current Windows version is not supported.\n"
            L"Please upgrade to Windows 10 or Windows 11 to use this application.",
            L"Unsupported Windows Version",
            MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}
// ==================================================================
// ADMIN PRIVILEGE CHECK
// ==================================================================
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isAdmin != FALSE;
}
// ==================================================================
// WINDOW POSITIONING AND DPI
// ==================================================================
void CenterWindowOnMonitor(HWND hWnd) {
    RECT wr;
    GetWindowRect(hWnd, &wr);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    int cx = GetSystemMetricsForDpi(SM_CXSCREEN, dpiX);
    int cy = GetSystemMetricsForDpi(SM_CYSCREEN, dpiY);
    SetWindowPos(hWnd, nullptr, (cx - w) / 2, (cy - h) / 2, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
// ==================================================================
// SYSTEM INTERACTION
// ==================================================================
void PlaySystemSound(bool success) {
    PlaySoundW(success ? L"SystemDefault" : L"SystemHand",
        nullptr, SND_ALIAS | SND_ASYNC);
}

// Допоміжна функція для перевірки чи працює процес
bool IsProcessRunning(const wchar_t* processName) {
    bool found = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                    found = true;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    
    return found;
}

bool TerminateProcessByName(const wchar_t* processName) {
    bool terminated = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                    if (hProcess != nullptr) {
                        if (TerminateProcess(hProcess, 0)) {
                            terminated = true;
                            wchar_t msg[256];
                            swprintf_s(msg, L"Terminated %s (PID: %lu)\n", processName, pe32.th32ProcessID);
                            OutputDebugStringW(msg);
                        }
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    
    return terminated;
}

void OpenWindowsUpdateSettings() {
    const wchar_t* targetProcess = L"SystemSettings.exe";
    
    // Закриваємо SystemSettings якщо він працює
    if (IsProcessRunning(targetProcess)) {
        OutputDebugStringW(L"SystemSettings.exe is running, terminating...\n");
        
        if (TerminateProcessByName(targetProcess)) {
            // Чекаємо поки процес закриється
            const int maxWaitTime = 2000;  // 2 секунди максимум
            const int checkInterval = 50;  // Перевіряємо кожні 50ms
            int waited = 0;
            
            while (IsProcessRunning(targetProcess) && waited < maxWaitTime) {
                Sleep(checkInterval);
                waited += checkInterval;
            }
            
            if (!IsProcessRunning(targetProcess)) {
                OutputDebugStringW(L"SystemSettings.exe successfully terminated\n");
            }
            else {
                OutputDebugStringW(L"Warning: SystemSettings.exe may still be running\n");
            }
        }
    }
    else {
        OutputDebugStringW(L"SystemSettings.exe is not running\n");
    }
    
    // Відкриваємо налаштування Windows Update
    OutputDebugStringW(L"Opening Windows Update settings...\n");
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"ms-settings:windowsupdate",
        nullptr, nullptr, SW_SHOWNORMAL);
    
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to open Windows Update settings (error code: %d)\n", 
                   reinterpret_cast<INT_PTR>(result));
        OutputDebugStringW(errorMsg);
    }
    else {
        OutputDebugStringW(L"Successfully opened Windows Update settings\n");
    }
}
// ==================================================================
// REGISTRY OPERATIONS
// ==================================================================
std::wstring ReadRegString(const wchar_t* keyPath, const wchar_t* valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return L"";
    }
    wchar_t buffer[512];
    DWORD bufferSize = sizeof(buffer);
    std::wstring result;
    if (RegQueryValueExW(hKey, valueName, nullptr, nullptr,
        reinterpret_cast<BYTE*>(buffer), &bufferSize) == ERROR_SUCCESS) {
        result = buffer;
    }
    RegCloseKey(hKey);
    return result;
}
std::wstring ReadRegString(const wchar_t* valueName) {
    return ReadRegString(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", valueName);
}
bool SetRegString(const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value) {
    HKEY hKey;
    LONG result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    result = RegSetValueExW(hKey, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}
bool SetRegString(const wchar_t* valueName, const std::wstring& value) {
    return SetRegString(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", valueName, value);
}
bool SetRegDWORD(const wchar_t* keyPath, const wchar_t* valueName, DWORD value) {
    HKEY hKey;
    LONG result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS) {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"SetRegDWORD: Failed to open/create key: %s\n", keyPath);
        OutputDebugStringW(debugMsg);
        return false;
    }
    result = RegSetValueExW(hKey, valueName, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    RegCloseKey(hKey);
    if (result == ERROR_SUCCESS) {
        return true;
    }
    else {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"SetRegDWORD: Failed to set %s in %s\n", valueName, keyPath);
        OutputDebugStringW(debugMsg);
        return false;
    }
}
bool DeleteRegValue(const wchar_t* keyPath, const wchar_t* valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    LONG result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"DeleteRegValue: Failed to delete %s from %s (error %ld)\n", valueName, keyPath, result);
        OutputDebugStringW(debugMsg);
        return false;
    }
    return true; // Return true even if value didn't exist (normal during restore)
}
bool DeleteRegValue(const wchar_t* valueName) {
    return DeleteRegValue(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", valueName);
}
// ==================================================================
// PAUSE/RESUME LOGIC
// ==================================================================
bool IsPaused() {
    return !ReadRegString(L"PauseUpdatesExpiryTime").empty();
}
std::wstring GetCurrentTimeString() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    wchar_t timeStr[64];
    swprintf_s(timeStr, L"%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return timeStr;
}
bool SetMaxPauseDays(DWORD maxDays) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        result = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
            0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
            nullptr, &hKey, nullptr);
        if (result != ERROR_SUCCESS) {
            return false;
        }
    }
    result = RegSetValueExW(hKey, L"FlightSettingsMaxPauseDays", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&maxDays), sizeof(DWORD));
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}
std::wstring CalculateFutureDate100Years() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    int newYear = st.wYear + 100;
    st.wYear = static_cast<WORD>(newYear);
    st.wMonth = 12;
    st.wDay = 31;
    st.wHour = 16;
    st.wMinute = 15;
    st.wSecond = 25;
    st.wMilliseconds = 0;
    wchar_t debugMessage[256];
    swprintf_s(debugMessage, L"CalculateFutureDate100Years: Set date to %04d-%02d-%02d %02d:%02d:%02d\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    OutputDebugStringW(debugMessage);
    wchar_t result[32];
    swprintf_s(result, L"%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return std::wstring(result);
}
bool ApplyPause() {
    // Set maximum pause days - 100 years
    const DWORD maxDays = 36525; // 100 years
    OutputDebugStringW(L"Starting Windows Update pause for 100 years...\n");

    // First, set maximum pause days
    if (!SetMaxPauseDays(maxDays)) {
        OutputDebugStringW(L"Warning: Failed to set FlightSettingsMaxPauseDays\n");
    }
    else {
        OutputDebugStringW(L"Successfully set FlightSettingsMaxPauseDays\n");
    }

    // Get current time and calculate end date (100 years ahead)
    const std::wstring startTime = GetCurrentTimeString();
    const std::wstring endTime = CalculateFutureDate100Years();

    if (startTime.empty()) {
        OutputDebugStringW(L"Error: Failed to get current time\n");
        return false;
    }

    // Log dates
    wchar_t logMessage[512];
    swprintf_s(logMessage, L"Pause start time: %s\n", startTime.c_str());
    OutputDebugStringW(logMessage);
    swprintf_s(logMessage, L"Pause end time: %s\n", endTime.c_str());
    OutputDebugStringW(logMessage);
    swprintf_s(logMessage, L"Total pause duration: %d days (100 years using 365.25 formula)\n", maxDays);
    OutputDebugStringW(logMessage);

    // Set registry values with detailed logging
    bool success = true;

    // ========== ОСНОВНІ ПАРАМЕТРИ ПАУЗИ ==========
    
    // PauseUpdatesStartTime
    if (!SetRegString(L"PauseUpdatesStartTime", startTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseUpdatesStartTime\n");
    }

    // PauseUpdatesExpiryTime
    if (!SetRegString(L"PauseUpdatesExpiryTime", endTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseUpdatesExpiryTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseUpdatesExpiryTime\n");
    }

    // Feature Updates
    if (!SetRegString(L"PauseFeatureUpdatesStartTime", startTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseFeatureUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseFeatureUpdatesStartTime\n");
    }

    if (!SetRegString(L"PauseFeatureUpdatesEndTime", endTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseFeatureUpdatesEndTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseFeatureUpdatesEndTime\n");
    }

    // Quality Updates
    if (!SetRegString(L"PauseQualityUpdatesStartTime", startTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseQualityUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseQualityUpdatesStartTime\n");
    }

    if (!SetRegString(L"PauseQualityUpdatesEndTime", endTime)) {
        OutputDebugStringW(L"Error: Failed to set PauseQualityUpdatesEndTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PauseQualityUpdatesEndTime\n");
    }
	
	    // ========== НОВИЙ КОД: UpdatePolicy Settings ==========
    const wchar_t* updatePolicyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    
    // Встановлюємо статуси паузи
    if (!SetRegDWORD(updatePolicyKey, L"PausedFeatureStatus", 1)) {
        OutputDebugStringW(L"Warning: Failed to set PausedFeatureStatus\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PausedFeatureStatus=1\n");
    }
    
    if (!SetRegDWORD(updatePolicyKey, L"PausedQualityStatus", 1)) {
        OutputDebugStringW(L"Warning: Failed to set PausedQualityStatus\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PausedQualityStatus=1\n");
    }
    
    // Встановлюємо дати паузи
    if (!SetRegString(updatePolicyKey, L"PausedQualityDate", endTime)) {
        OutputDebugStringW(L"Warning: Failed to set PausedQualityDate\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PausedQualityDate\n");
    }
    
    if (!SetRegString(updatePolicyKey, L"PausedFeatureDate", endTime)) {
        OutputDebugStringW(L"Warning: Failed to set PausedFeatureDate\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully set PausedFeatureDate\n");
    }

    // ============ ADDITIONAL SETTINGS TO FULLY DISABLE WINDOWS UPDATE ============
    OutputDebugStringW(L"Applying additional registry tweaks to fully disable Windows Update...\n");

    // 1. Disable "Update Health Service"
    if (SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 4)) {
        OutputDebugStringW(L"✅ uhssvc service disabled (Start=4)\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to disable uhssvc service\n");
        success = false;
    }
	
	// 1.5. Disable "Windows as a Service Medic Service"
    if (SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 4)) {
        OutputDebugStringW(L"✅ WaaSMedicSvc service disabled (Start=4)\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to disable WaaSMedicSvc service\n");
        success = false;
    }

    // 2. Exclude drivers from quality updates
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", L"ExcludeWUDriversInQualityUpdate", 1)) {
        OutputDebugStringW(L"✅ ExcludeWUDriversInQualityUpdate=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set ExcludeWUDriversInQualityUpdate\n");
        success = false;
    }

    // 3. Disable driver searching via Windows Update
    if (SetRegDWORD(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"SearchOrderConfig", 0)) {
        OutputDebugStringW(L"✅ DriverSearching\\SearchOrderConfig=0\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DriverSearching\\SearchOrderConfig\n");
        success = false;
    }

    if (SetRegDWORD(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"DontSearchWindowsUpdate", 1)) {
        OutputDebugStringW(L"✅ DriverSearching\\DontSearchWindowsUpdate=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DriverSearching\\DontSearchWindowsUpdate\n");
        success = false;
    }

    // 4. Prevent device metadata from network
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Device Metadata", L"PreventDeviceMetadataFromNetwork", 1)) {
        OutputDebugStringW(L"✅ PreventDeviceMetadataFromNetwork=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set PreventDeviceMetadataFromNetwork\n");
        success = false;
    }

    // 5. Disable driver searching in system
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DriverSearching", L"SearchOrderConfig", 0)) {
        OutputDebugStringW(L"✅ DriverSearching (CurrentVersion)\\SearchOrderConfig=0\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DriverSearching (CurrentVersion)\\SearchOrderConfig\n");
        success = false;
    }

    // 6. Windows Update Policies — disable access, servers, OS upgrades, etc.
    const wchar_t* wuPolicyKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    
    if (SetRegString(wuPolicyKey, L"WUServer", L" ")) {
        OutputDebugStringW(L"✅ WUServer=\" \"\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set WUServer\n");
        success = false;
    }

    if (SetRegString(wuPolicyKey, L"WUStatusServer", L" ")) {
        OutputDebugStringW(L"✅ WUStatusServer=\" \"\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set WUStatusServer\n");
        success = false;
    }

    if (SetRegString(wuPolicyKey, L"UpdateServiceUrlAlternate", L" ")) {
        OutputDebugStringW(L"✅ UpdateServiceUrlAlternate=\" \"\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set UpdateServiceUrlAlternate\n");
        success = false;
    }

    if (SetRegDWORD(wuPolicyKey, L"DisableWindowsUpdateAccess", 1)) {
        OutputDebugStringW(L"✅ DisableWindowsUpdateAccess=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DisableWindowsUpdateAccess\n");
        success = false;
    }

    if (SetRegDWORD(wuPolicyKey, L"DisableOSUpgrade", 1)) {
        OutputDebugStringW(L"✅ DisableOSUpgrade=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DisableOSUpgrade\n");
        success = false;
    }

    if (SetRegDWORD(wuPolicyKey, L"SetDisableUXWUAccess", 1)) {
        OutputDebugStringW(L"✅ SetDisableUXWUAccess=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set SetDisableUXWUAccess\n");
        success = false;
    }

    if (SetRegDWORD(wuPolicyKey, L"ExcludeWUDriversInQualityUpdate", 1)) {
        OutputDebugStringW(L"✅ WindowsUpdate\\ExcludeWUDriversInQualityUpdate=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set WindowsUpdate\\ExcludeWUDriversInQualityUpdate\n");
        success = false;
    }

    if (SetRegDWORD(wuPolicyKey, L"DoNotConnectToWindowsUpdateInternetLocations", 1)) {
        OutputDebugStringW(L"✅ DoNotConnectToWindowsUpdateInternetLocations=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set DoNotConnectToWindowsUpdateInternetLocations\n");
        success = false;
    }

    // 7. AutoUpdate policies
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    
    if (SetRegDWORD(auKey, L"NoAutoUpdate", 1)) {
        OutputDebugStringW(L"✅ AU\\NoAutoUpdate=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set AU\\NoAutoUpdate\n");
        success = false;
    }

    if (SetRegDWORD(auKey, L"UseWUServer", 1)) {
        OutputDebugStringW(L"✅ AU\\UseWUServer=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to set AU\\UseWUServer\n");
        success = false;
    }

    OutputDebugStringW(L"✅ Additional registry tweaks applied successfully.\n");

    // Final report
    if (success) {
        OutputDebugStringW(L"✅ Windows Updates successfully paused for 100 years with full lockdown!\n");
        OutputDebugStringW(L"✅ Active hours set from 13:00 to 07:00\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Windows Update pause completed with some warnings. Check administrator privileges.\n");
    }

    return success;
}
bool RemovePause() {
    OutputDebugStringW(L"Starting Windows Update resume...\n");
    bool success = true;
	// Delete core pause values
	if (!DeleteRegValue(L"PauseUpdatesStartTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseUpdatesStartTime\n");
    }
    if (!DeleteRegValue(L"PauseUpdatesExpiryTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseUpdatesExpiryTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseUpdatesExpiryTime\n");
    }
    if (!DeleteRegValue(L"PauseFeatureUpdatesEndTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseFeatureUpdatesEndTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseFeatureUpdatesEndTime\n");
    }
    if (!DeleteRegValue(L"PauseQualityUpdatesEndTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseQualityUpdatesEndTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseQualityUpdatesEndTime\n");
    }
    if (!DeleteRegValue(L"PauseFeatureUpdatesStartTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseFeatureUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseFeatureUpdatesStartTime\n");
    }
    if (!DeleteRegValue(L"PauseQualityUpdatesStartTime")) {
        OutputDebugStringW(L"Warning: Failed to delete PauseQualityUpdatesStartTime\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PauseQualityUpdatesStartTime\n");
    }
	
    // ========== НОВИЙ КОД: UpdatePolicy Settings ==========
    const wchar_t* updatePolicyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    
    // Скидаємо статуси паузи на 0
    if (!SetRegDWORD(updatePolicyKey, L"PausedFeatureStatus", 0)) {
        OutputDebugStringW(L"Warning: Failed to reset PausedFeatureStatus\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully reset PausedFeatureStatus=0\n");
    }
    
    if (!SetRegDWORD(updatePolicyKey, L"PausedQualityStatus", 0)) {
        OutputDebugStringW(L"Warning: Failed to reset PausedQualityStatus\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully reset PausedQualityStatus=0\n");
    }
    
    // Видаляємо дати паузи
    if (!DeleteRegValue(updatePolicyKey, L"PausedQualityDate")) {
        OutputDebugStringW(L"Warning: Failed to delete PausedQualityDate\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PausedQualityDate\n");
    }
    
    if (!DeleteRegValue(updatePolicyKey, L"PausedFeatureDate")) {
        OutputDebugStringW(L"Warning: Failed to delete PausedFeatureDate\n");
        success = false;
    }
    else {
        OutputDebugStringW(L"Successfully deleted PausedFeatureDate\n");
    }	
	
    HKEY hSettingsKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_SET_VALUE, &hSettingsKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hSettingsKey, L"FlightSettingsMaxPauseDays");
        RegCloseKey(hSettingsKey);
        OutputDebugStringW(L"Successfully cleared FlightSettingsMaxPauseDays\n");
    }
    // ============ RESTORE REGISTRY SETTINGS ============
    OutputDebugStringW(L"Restoring registry settings to re-enable Windows Update...\n");
    // 1. Enable "Update Health Service"
    if (SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 2)) {
        OutputDebugStringW(L"✅ uhssvc service enabled (Start=2)\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to enable uhssvc service\n");
        success = false;
    }
	// 1.5 Enable "Windows as a Service Medic Service"
    if (SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 3)) {
        OutputDebugStringW(L"✅ WaaSMedicSvc service enabled (Start=3)\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to enable WaaSMedicSvc service\n");
        success = false;
    }
    // 2. Re-include drivers in quality updates
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", L"ExcludeWUDriversInQualityUpdate", 0)) {
        OutputDebugStringW(L"✅ ExcludeWUDriversInQualityUpdate=0\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to reset ExcludeWUDriversInQualityUpdate\n");
        success = false;
    }
    // 3. Restore driver searching via Windows Update
    if (DeleteRegValue(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"SearchOrderConfig")) {
        OutputDebugStringW(L"✅ DriverSearching\\SearchOrderConfig deleted (restored to default)\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete DriverSearching\\SearchOrderConfig\n");
        success = false;
    }
    if (DeleteRegValue(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"DontSearchWindowsUpdate")) {
        OutputDebugStringW(L"✅ DriverSearching\\DontSearchWindowsUpdate deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete DriverSearching\\DontSearchWindowsUpdate\n");
        success = false;
    }
    // 4. Allow device metadata from network
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Device Metadata", L"PreventDeviceMetadataFromNetwork", 0)) {
        OutputDebugStringW(L"✅ PreventDeviceMetadataFromNetwork=0\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to reset PreventDeviceMetadataFromNetwork\n");
        success = false;
    }
    // 5. Restore driver searching in system
    if (SetRegDWORD(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DriverSearching", L"SearchOrderConfig", 1)) {
        OutputDebugStringW(L"✅ DriverSearching (CurrentVersion)\\SearchOrderConfig=1\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to reset DriverSearching (CurrentVersion)\\SearchOrderConfig\n");
        success = false;
    }
    // 6. Delete Windows Update Policies
    const wchar_t* wuPolicyKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    if (DeleteRegValue(wuPolicyKey, L"WUServer")) {
        OutputDebugStringW(L"✅ WUServer deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete WUServer\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"WUStatusServer")) {
        OutputDebugStringW(L"✅ WUStatusServer deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete WUStatusServer\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"UpdateServiceUrlAlternate")) {
        OutputDebugStringW(L"✅ UpdateServiceUrlAlternate deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete UpdateServiceUrlAlternate\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"DisableWindowsUpdateAccess")) {
        OutputDebugStringW(L"✅ DisableWindowsUpdateAccess deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete DisableWindowsUpdateAccess\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"DisableOSUpgrade")) {
        OutputDebugStringW(L"✅ DisableOSUpgrade deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete DisableOSUpgrade\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"SetDisableUXWUAccess")) {
        OutputDebugStringW(L"✅ SetDisableUXWUAccess deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete SetDisableUXWUAccess\n");
        success = false;
    }
    if (SetRegDWORD(wuPolicyKey, L"ExcludeWUDriversInQualityUpdate", 0)) {
        OutputDebugStringW(L"✅ WindowsUpdate\\ExcludeWUDriversInQualityUpdate=0\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to reset WindowsUpdate\\ExcludeWUDriversInQualityUpdate\n");
        success = false;
    }
    if (DeleteRegValue(wuPolicyKey, L"DoNotConnectToWindowsUpdateInternetLocations")) {
        OutputDebugStringW(L"✅ DoNotConnectToWindowsUpdateInternetLocations deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete DoNotConnectToWindowsUpdateInternetLocations\n");
        success = false;
    }
    // 7. Delete AutoUpdate policies
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    if (DeleteRegValue(auKey, L"NoAutoUpdate")) {
        OutputDebugStringW(L"✅ AU\\NoAutoUpdate deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete AU\\NoAutoUpdate\n");
        success = false;
    }
    if (DeleteRegValue(auKey, L"UseWUServer")) {
        OutputDebugStringW(L"✅ AU\\UseWUServer deleted\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Failed to delete AU\\UseWUServer\n");
        success = false;
    }
    OutputDebugStringW(L"✅ Registry settings restored successfully.\n");
    if (success) {
        OutputDebugStringW(L"✅ Windows Updates successfully resumed!\n");
    }
    else {
        OutputDebugStringW(L"⚠️ Windows Update resume completed with some warnings\n");
    }
    return success;
}
void TogglePause() {
    if (g_app.isOperationInProgress) return;
    g_app.isOperationInProgress = true;
    bool wasThePaused = g_app.isPaused;
    if (wasThePaused) {
        // Resume updates
        if (RemovePause() && !IsPaused()) {
            g_app.statusMessage = L"✅ Windows Update fully re-enabled";
            PlaySystemSound(true);
            OpenWindowsUpdateSettings();
        }
        else {
            g_app.statusMessage = L"❌ Failed to resume updates - Check administrator privileges";
            PlaySystemSound(false);
        }
    }
    else {
        // Pause updates
        if (ApplyPause() && IsPaused()) {
            g_app.statusMessage = L"✅ Windows Update fully disabled for 100 years";
            PlaySystemSound(true);
            OpenWindowsUpdateSettings();
        }
        else {
            g_app.statusMessage = L"❌ Failed to pause updates - Check administrator privileges";
            PlaySystemSound(false);
        }
    }
    g_app.isPaused = IsPaused();
    g_app.isOperationInProgress = false;
}
// ==================================================================
// GDI RESOURCE MANAGEMENT
// ==================================================================
void CreateGDIResources() {
    // Create fonts
    auto createFont = [](int size, int weight, const wchar_t* family = L"Segoe UI Variable") -> HFONT {
        return CreateFontW(-Scale(size), 0, 0, 0, weight, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, family);
        };
    g_gdi.hFontTitle = createFont(22, FW_SEMIBOLD, L"Segoe UI Variable Display");
    g_gdi.hFontButton = createFont(15, FW_MEDIUM);
    g_gdi.hFontStatus = createFont(13, FW_NORMAL);
    // Create brushes
    g_gdi.hBrushBg = CreateSolidBrush(BG_COLOR);
    g_gdi.hBrushCard = CreateSolidBrush(CARD_COLOR);
}
void DestroyGDIResources() {
    if (g_gdi.hFontTitle) { DeleteObject(g_gdi.hFontTitle); g_gdi.hFontTitle = nullptr; }
    if (g_gdi.hFontButton) { DeleteObject(g_gdi.hFontButton); g_gdi.hFontButton = nullptr; }
    if (g_gdi.hFontStatus) { DeleteObject(g_gdi.hFontStatus); g_gdi.hFontStatus = nullptr; }
    if (g_gdi.hBrushBg) { DeleteObject(g_gdi.hBrushBg); g_gdi.hBrushBg = nullptr; }
    if (g_gdi.hBrushCard) { DeleteObject(g_gdi.hBrushCard); g_gdi.hBrushCard = nullptr; }
    if (g_gdi.hMemDC) { DeleteDC(g_gdi.hMemDC); g_gdi.hMemDC = nullptr; }
    if (g_gdi.hMemBitmap) { DeleteObject(g_gdi.hMemBitmap); g_gdi.hMemBitmap = nullptr; }
}
void UpdateGDIResources() {
    DestroyGDIResources();
    CreateGDIResources();
}
void InitializeDoubleBuffering(HWND hWnd) {
    GetClientRect(hWnd, &g_gdi.clientRect);
    HDC hdc = GetDC(hWnd);
    if (g_gdi.hMemDC) DeleteDC(g_gdi.hMemDC);
    if (g_gdi.hMemBitmap) DeleteObject(g_gdi.hMemBitmap);
    g_gdi.hMemDC = CreateCompatibleDC(hdc);
    g_gdi.hMemBitmap = CreateCompatibleBitmap(hdc, g_gdi.clientRect.right, g_gdi.clientRect.bottom);
    SelectObject(g_gdi.hMemDC, g_gdi.hMemBitmap);
    ReleaseDC(hWnd, hdc);
}
// ==================================================================
// DRAWING FUNCTIONS
// ==================================================================
// round-corner helper
inline HRGN CreateRoundRectRgnForRect(const RECT& r, int radius)
{
    return CreateRoundRectRgn(
        r.left, r.top,
        r.right + 1, r.bottom + 1,   // +1 бо CreateRoundRectRgn працює «включно»
        Scale(radius), Scale(radius));
}
void DrawCard(HDC hdc,
    const RECT& outer,
    int inset,
    int height = -1,   // -1 → автоматична висота
    bool withShadow = true)
{
    // 1. Формуємо inner-rect
    RECT inner = outer;
    inner.left += Scale(inset);
    inner.right -= Scale(inset);
    if (height == -1)
    {
        // висота з outer
        inner.top += Scale(inset);
        inner.bottom -= Scale(inset);
    }
    else
    {
        // фіксована висота, центруємо
        int h = Scale(height);
        int cy = (outer.bottom - outer.top);
        inner.top = outer.top + (cy - h) / 2;
        inner.bottom = inner.top + h;
    }
    const int RADIUS = 12;
    // 2. Тінь
    if (withShadow)
    {
        constexpr int SHADOW_OFFSET = 3;
        RECT shadowRect = inner;
        shadowRect.left += Scale(SHADOW_OFFSET);
        shadowRect.top += Scale(SHADOW_OFFSET);
        shadowRect.right += Scale(SHADOW_OFFSET);
        shadowRect.bottom += Scale(SHADOW_OFFSET);
        HRGN shadowRgn = CreateRoundRectRgnForRect(shadowRect, RADIUS);
        HBRUSH shadowBr = CreateSolidBrush(SHADOW_COLOR);
        FillRgn(hdc, shadowRgn, shadowBr);
        DeleteObject(shadowBr);
        DeleteObject(shadowRgn);
    }
    // 3. Фон
    HRGN rgn = CreateRoundRectRgnForRect(inner, RADIUS);
    FillRgn(hdc, rgn, g_gdi.hBrushCard);
    // 4. Бордер
    HBRUSH borderBr = CreateSolidBrush(BORDER_COLOR);
    FrameRgn(hdc, rgn, borderBr, 1, 1);
    DeleteObject(borderBr);
    DeleteObject(rgn);
}
void DrawButton(HDC hdc, const RECT& rect, const wchar_t* text,
    bool isHovered, bool isPressed)
{
    const int RADIUS = 15;   // радіус заокруглення (в логічних одиницях)
    COLORREF clr = isPressed ? ACTIVE_COLOR :
        isHovered ? (g_app.isPaused ? PAUSE_COLOR : RESUME_COLOR)
        : ACCENT_COLOR;
    // фон
    HRGN rgn = CreateRoundRectRgnForRect(rect, RADIUS);
    HBRUSH br = CreateSolidBrush(clr);
    FillRgn(hdc, rgn, br);
    DeleteObject(br);
    // бордер
    HBRUSH brBorder = CreateSolidBrush(BORDER_COLOR);
    FrameRgn(hdc, rgn, brBorder, 1, 1);
    DeleteObject(brBorder);
    DeleteObject(rgn);
    // текст
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_PRIMARY);
    SelectObject(hdc, g_gdi.hFontButton);
    DrawTextW(hdc, text, -1, const_cast<RECT*>(&rect),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
void DrawStatusPanel(HDC hdc,
    const RECT& outer,
    int inset,
    int height = -1,
    bool withShadow = true)
{
    RECT inner = outer;
    inner.left += Scale(inset);
    inner.right -= Scale(inset);
    if (height == -1)
    {
        inner.top += Scale(inset);
        inner.bottom -= Scale(inset);
    }
    else
    {
        int h = Scale(height);
        int cy = (outer.bottom - outer.top);
        inner.top = outer.top + (cy - h) / 2;
        inner.bottom = inner.top + h;
    }
    const int RADIUS = 8;
    // тінь
    if (withShadow)
    {
        constexpr int SHADOW_OFFSET = 3;
        RECT shadowRect = inner;
        shadowRect.left += Scale(SHADOW_OFFSET);
        shadowRect.top += Scale(SHADOW_OFFSET);
        shadowRect.right += Scale(SHADOW_OFFSET);
        shadowRect.bottom += Scale(SHADOW_OFFSET);
        HRGN shadowRgn = CreateRoundRectRgnForRect(shadowRect, RADIUS);
        HBRUSH shadowBr = CreateSolidBrush(SHADOW_COLOR);
        FillRgn(hdc, shadowRgn, shadowBr);
        DeleteObject(shadowBr);
        DeleteObject(shadowRgn);
    }
    HRGN rgn = CreateRoundRectRgnForRect(inner, RADIUS);
    FillRgn(hdc, rgn, g_gdi.hBrushCard);
    HBRUSH borderBr = CreateSolidBrush(BORDER_COLOR);
    FrameRgn(hdc, rgn, borderBr, 1, 1);
    DeleteObject(borderBr);
    DeleteObject(rgn);
    // текст
    COLORREF txt = TEXT_SECONDARY;
    if (g_app.statusMessage.find(L"✅") != std::wstring::npos)
        txt = TEXT_SUCCESS;
    else if (g_app.statusMessage.find(L"❌") != std::wstring::npos)
        txt = TEXT_ERROR;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, txt);
    SelectObject(hdc, g_gdi.hFontStatus);
    DrawTextW(hdc, g_app.statusMessage.c_str(), -1,
        const_cast<RECT*>(&inner),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
void PaintWindow(HWND hWnd)
{
    if (!g_gdi.hMemDC) return;
    FillRect(g_gdi.hMemDC, &g_gdi.clientRect, g_gdi.hBrushBg);
    // Title
    RECT titleRect = ScaleRect(H_MARGIN, 15,
        WINDOW_WIDTH - H_MARGIN * 2, 50);
    SetBkMode(g_gdi.hMemDC, TRANSPARENT);
    SetTextColor(g_gdi.hMemDC, TEXT_PRIMARY);
    SelectObject(g_gdi.hMemDC, g_gdi.hFontTitle);
    DrawTextW(g_gdi.hMemDC, L"Windows Update Pauser", -1, &titleRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // Card
    RECT cardRect = ScaleRect(H_MARGIN, 60,
        WINDOW_WIDTH - H_MARGIN * 2, 130);
    DrawCard(g_gdi.hMemDC, cardRect, 10, 70);
    // Button
    RECT buttonRect = ScaleRect(H_MARGIN + 30, 80,
        WINDOW_WIDTH - H_MARGIN * 2 - 30, 115);
    const wchar_t* buttonText = g_app.isPaused ? L"▶ Resume Updates"
        : L"⏸ Pause for 100 years";
    DrawButton(g_gdi.hMemDC, buttonRect, buttonText,
        g_app.btnHover, g_app.btnPressed);
    // Status panel
    RECT statusRect = ScaleRect(H_MARGIN, 145,
        WINDOW_WIDTH - H_MARGIN * 2, 180);
    DrawStatusPanel(g_gdi.hMemDC, statusRect, 10, 35);
}
// ==================================================================
// WINDOW PROCEDURES
// ==================================================================
void EnableModernWindowStyle(HWND hWnd) {
    // Enable dark mode
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    // Enable rounded corners (Windows 11)
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
    enum DWM_WINDOW_CORNER_PREFERENCE {
        DWMWCP_DEFAULT = 0,
        DWMWCP_DONOTROUND = 1,
        DWMWCP_ROUND = 2,
        DWMWCP_ROUNDSMALL = 3
    };
#endif
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}
RECT GetButtonRect() {
    return ScaleRect(40, 80, WINDOW_WIDTH - 40, 115);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_app.hWnd = hWnd;
        g_app.dpi = GetDpiForWindow(hWnd);
        g_app.isPaused = IsPaused();
        CreateGDIResources();
        InitializeDoubleBuffering(hWnd);
        EnableModernWindowStyle(hWnd);
        SetTimer(hWnd, TIMER_ID, TIMER_INTERVAL, nullptr);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        PaintWindow(hWnd);
        BitBlt(hdc, 0, 0, g_gdi.clientRect.right, g_gdi.clientRect.bottom,
            g_gdi.hMemDC, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // Prevent flicker
    case WM_SIZE:
        InitializeDoubleBuffering(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (wParam == TIMER_ID) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            RECT buttonRect = GetButtonRect(); // Зберігаємо в локальну змінну
            bool newHover = PtInRect(&buttonRect, pt); // Використовуємо адресу локальної змінної
            if (newHover != g_app.btnHover) {
                g_app.btnHover = newHover;
                InvalidateRect(hWnd, &buttonRect, FALSE); // Використовуємо адресу локальної змінної
            }
        }
        return 0;
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);
        RECT buttonRect = GetButtonRect(); // Зберігаємо в локальну змінну
        if (PtInRect(&buttonRect, pt)) { // Використовуємо адресу локальної змінної
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT buttonRect = GetButtonRect(); // Зберігаємо в локальну змінну
        if (PtInRect(&buttonRect, pt)) { // Використовуємо адресу локальної змінної
            g_app.btnPressed = true;
            SetCapture(hWnd);
            InvalidateRect(hWnd, &buttonRect, FALSE); // Використовуємо адресу локальної змінної
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_app.btnPressed) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ReleaseCapture();
            g_app.btnPressed = false;
            RECT buttonRect = GetButtonRect(); // Зберігаємо в локальну змінну
            if (PtInRect(&buttonRect, pt)) { // Використовуємо адресу локальної змінної
                TogglePause();
            }
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        g_app.dpi = LOWORD(wParam);
        UpdateGDIResources();
        RECT* prcNewWindow = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd, nullptr, prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InitializeDoubleBuffering(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        DestroyGDIResources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
// ==================================================================
// ENTRY POINT
// ==================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Check Windows version compatibility
    if (!CheckWindowsVersion()) {
        return 1;
    }

    // Check admin privileges and relaunch if necessary
    if (!IsRunningAsAdmin()) {
        // Підготовка до перезапуску з правами адміністратора
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, szPath, MAX_PATH) == 0) {
            MessageBoxW(nullptr,
                L"Failed to get application path.",
                L"Error",
                MB_OK | MB_ICONERROR);
            return 1;
        }

        // Виклик ShellExecute з 'runas' для запиту підвищення прав
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei)) {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED) {
                MessageBoxW(nullptr,
                    L"Administrator privileges are required to run this application.\n"
                    L"The application will now exit.",
                    L"Administrator Privileges Required",
                    MB_OK | MB_ICONWARNING);
            }
            else {
                // Інша помилка
                MessageBoxW(nullptr,
                    L"Failed to restart the application with administrator privileges.",
                    L"Error",
                    MB_OK | MB_ICONERROR);
            }
            return 1;
        }

        return 0;
    }

    // Enable DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    // Register window class
    HICON hIconLarge = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON));
    HICON hIconSmall = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON_SMALL));
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = hIconLarge;
    wc.hIconSm = hIconSmall;

    if (!RegisterClassEx(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    UINT dpi = 96;
    HMONITOR hMon = MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY);
    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpi, &dpi);
    int scaledWidth = MulDiv(WINDOW_WIDTH, dpi, 96);
    int scaledHeight = MulDiv(WINDOW_HEIGHT, dpi, 96);

    // Create main window
    HWND hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Windows Update Pauser",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        scaledWidth, scaledHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Center and show window
    CenterWindowOnMonitor(hWnd);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}
