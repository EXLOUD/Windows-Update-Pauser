// WindowsUpdatePauser.cpp

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#include <winternl.h>
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <mmsystem.h>
#include <versionhelpers.h>
#include <shellscalingapi.h>
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

constexpr wchar_t CLASS_NAME[] = L"WUPauser_Improved";
constexpr int WINDOW_WIDTH = 465;
constexpr int WINDOW_HEIGHT = 240;
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
            L"This application requires Windows 10 or later.\n\n"
            L"Your current Windows version is not supported.\n"
            L"Please upgrade to Windows 10 or Windows 11 to use this application.",
            L"Unsupported Windows Version",
            MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
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
    PlaySoundW(success ? MAKEINTRESOURCEW(SND_ALIAS_SYSTEMDEFAULT)
        : MAKEINTRESOURCEW(SND_ALIAS_SYSTEMHAND),
        nullptr, SND_ALIAS_ID | SND_ASYNC);
}

void OpenWindowsUpdateSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:windowsupdate",
        nullptr, nullptr, SW_SHOWNORMAL);
}

// ==================================================================
// REGISTRY OPERATIONS
// ==================================================================

std::wstring ReadRegString(const wchar_t* valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_READ, &hKey) != ERROR_SUCCESS) {
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

bool SetRegString(const wchar_t* valueName, const std::wstring& value) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegSetValueExW(hKey, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool DeleteRegValue(const wchar_t* valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
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

bool ApplyPause() {
    const std::wstring endTime = L"4750-12-12T00:00:00Z";
    const std::wstring startTime = GetCurrentTimeString();

    bool success = true;
    success &= SetRegString(L"PauseUpdatesExpiryTime", endTime);
    success &= SetRegString(L"PauseFeatureUpdatesEndTime", endTime);
    success &= SetRegString(L"PauseQualityUpdatesEndTime", endTime);
    success &= SetRegString(L"PauseFeatureUpdatesStartTime", startTime);
    success &= SetRegString(L"PauseQualityUpdatesStartTime", startTime);

    return success;
}

bool RemovePause() {
    bool success = true;
    success &= DeleteRegValue(L"PauseUpdatesExpiryTime");
    success &= DeleteRegValue(L"PauseFeatureUpdatesEndTime");
    success &= DeleteRegValue(L"PauseQualityUpdatesEndTime");
    success &= DeleteRegValue(L"PauseFeatureUpdatesStartTime");
    success &= DeleteRegValue(L"PauseQualityUpdatesStartTime");

    return success;
}

void TogglePause() {
    if (g_app.isOperationInProgress) return;

    g_app.isOperationInProgress = true;
    bool wasThePaused = g_app.isPaused;

    if (wasThePaused) {
        // Resume updates
        if (RemovePause() && !IsPaused()) {
            g_app.statusMessage = L"✅ Updates resumed successfully";
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
            g_app.statusMessage = L"✅ Updates paused until year 4750";
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

void DrawCard(HDC hdc, const RECT& rect, bool withShadow = true)
{
    if (withShadow)
    {
        constexpr int SHADOW_OFFSET = 3;
        constexpr int SHADOW_BLUR = 0;
        RECT shadowRect = rect;
        shadowRect.left += Scale(SHADOW_OFFSET);
        shadowRect.top += Scale(SHADOW_OFFSET);
        shadowRect.right += Scale(SHADOW_OFFSET);
        shadowRect.bottom += Scale(SHADOW_OFFSET);

        HBRUSH shadowBrush = CreateSolidBrush(SHADOW_COLOR);
        FillRect(hdc, &shadowRect, shadowBrush);
        DeleteObject(shadowBrush);
    }

    FillRect(hdc, &rect, g_gdi.hBrushCard);

    HBRUSH borderBrush = CreateSolidBrush(BORDER_COLOR);
    FrameRect(hdc, &rect, borderBrush);
    DeleteObject(borderBrush);
}

void DrawButton(HDC hdc, const RECT& rect, const wchar_t* text, bool isHovered, bool isPressed) {
    COLORREF buttonColor;

    if (isPressed) {
        buttonColor = ACTIVE_COLOR;
    }
    else if (isHovered) {
        buttonColor = g_app.isPaused ? PAUSE_COLOR : RESUME_COLOR;
    }
    else {
        buttonColor = g_app.isPaused ? ACCENT_COLOR : ACCENT_COLOR;
    }

    HBRUSH buttonBrush = CreateSolidBrush(buttonColor);
    FillRect(hdc, &rect, buttonBrush);
    DeleteObject(buttonBrush);

    // Draw button text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_PRIMARY);
    SelectObject(hdc, g_gdi.hFontButton);
    DrawTextW(hdc, text, -1, const_cast<RECT*>(&rect), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawStatusPanel(HDC hdc, const RECT& rect, bool withShadow = true)
{
    DrawCard(hdc, rect, withShadow);

    SetBkMode(hdc, TRANSPARENT);

    COLORREF textColor = TEXT_SECONDARY;
    if (g_app.statusMessage.find(L"✅") != std::wstring::npos)
        textColor = TEXT_SUCCESS;
    else if (g_app.statusMessage.find(L"❌") != std::wstring::npos)
        textColor = TEXT_ERROR;

    SetTextColor(hdc, textColor);
    SelectObject(hdc, g_gdi.hFontStatus);
    DrawTextW(hdc, g_app.statusMessage.c_str(), -1,
        const_cast<RECT*>(&rect),
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
    DrawCard(g_gdi.hMemDC, cardRect);

    // Button
    RECT buttonRect = ScaleRect(H_MARGIN + 20, 80,
        WINDOW_WIDTH - H_MARGIN * 2 - 20, 115);
    const wchar_t* buttonText = g_app.isPaused ? L"▶ Resume Updates"
        : L"⏸ Pause Until 4750";
    DrawButton(g_gdi.hMemDC, buttonRect, buttonText,
        g_app.btnHover, g_app.btnPressed);

    // Status panel
    RECT statusRect = ScaleRect(H_MARGIN, 145,
        WINDOW_WIDTH - H_MARGIN * 2, 180);
    DrawStatusPanel(g_gdi.hMemDC, statusRect, true);
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

            RECT buttonRect = GetButtonRect();
            bool newHover = PtInRect(&buttonRect, pt);

            if (newHover != g_app.btnHover) {
                g_app.btnHover = newHover;
                InvalidateRect(hWnd, &buttonRect, FALSE);
            }
        }
        return 0;

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        if (PtInRect(&GetButtonRect(), pt)) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&GetButtonRect(), pt)) {
            g_app.btnPressed = true;
            SetCapture(hWnd);
            InvalidateRect(hWnd, &GetButtonRect(), FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_app.btnPressed) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ReleaseCapture();
            g_app.btnPressed = false;

            if (PtInRect(&GetButtonRect(), pt)) {
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Check Windows version compatibility
    if (!CheckWindowsVersion()) {
        return 1;
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

    // Create main window
    HWND hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Windows Update Pauser",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        Scale(WINDOW_WIDTH), Scale(WINDOW_HEIGHT),
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
