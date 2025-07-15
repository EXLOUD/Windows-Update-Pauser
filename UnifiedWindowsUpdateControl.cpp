#include <windows.h>
#include <commctrl.h>
#include <string>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <versionhelpers.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "version.lib")

#define IDC_BTN_PAUSE   1001
#define IDC_LBL_STATUS  1003
#define IDC_LBL_TITLE   1004

const wchar_t* CLASS_NAME = L"WindowsUpdatePauser";

HWND hMainWnd, hBtnPause, hLblStatus, hLblTitle;
HFONT hFontTitle, hFontButton, hFontStatus;
HBRUSH hBrushBg, hBrushCard, hBrushAccent, hBrushHover, hBrushActive;
HPEN hPenBorder, hPenAccent;
HCURSOR hHandCursor = NULL;
bool isHoveredPause = false;
bool isPressedPause = false;
bool isDarkMode = true;
bool isPaused = false;
std::wstring lastOperationResult = L"Ready to manage Windows Update pause";

HDC hMemDC = NULL;
HBITMAP hMemBitmap = NULL;
RECT clientRect;

const COLORREF BG_COLOR = RGB(32, 32, 32);
const COLORREF CARD_COLOR = RGB(45, 45, 45);
const COLORREF ACCENT_COLOR = RGB(0, 120, 215);
const COLORREF HOVER_COLOR = RGB(16, 132, 208);
const COLORREF ACTIVE_COLOR = RGB(0, 102, 180);
const COLORREF TEXT_PRIMARY = RGB(255, 255, 255);
const COLORREF TEXT_SECONDARY = RGB(150, 150, 150);
const COLORREF BORDER_COLOR = RGB(70, 70, 70);
const COLORREF PAUSE_COLOR = RGB(255, 193, 7);
const COLORREF RESUME_COLOR = RGB(0, 120, 215);

// Windows version checking functions
bool IsWindows10OrLater() {
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };

    // Use RtlGetVersion for accurate version detection
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandle(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            NTSTATUS status = RtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
            if (status == 0) { // STATUS_SUCCESS
                // Windows 10 is version 10.0, Windows 11 is also 10.0 but with build >= 22000
                return (osvi.dwMajorVersion > 10) ||
                    (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion >= 0);
            }
        }
    }

    // Fallback to GetVersionEx (deprecated but still works)
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        return (osvi.dwMajorVersion > 10) ||
            (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion >= 0);
    }

    // If all else fails, assume it's supported (better than blocking)
    return true;
}

bool CheckWindowsVersion() {
    if (!IsWindows10OrLater()) {
        MessageBox(NULL,
            L"This application requires Windows 10 or later.\n\n"
            L"Your current Windows version is not supported.\n"
            L"Please upgrade to Windows 10 or Windows 11 to use this application.",
            L"Unsupported Windows Version",
            MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

std::wstring GetWindowsVersionString() {
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };

    // Use RtlGetVersion for more accurate version detection
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandle(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            RtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
        }
    }

    std::wstring versionStr = L"Windows ";

    if (osvi.dwMajorVersion == 10) {
        if (osvi.dwBuildNumber >= 22000) {
            versionStr += L"11";
        }
        else {
            versionStr += L"10";
        }
        versionStr += L" (Build " + std::to_wstring(osvi.dwBuildNumber) + L")";
    }
    else if (osvi.dwMajorVersion == 6) {
        if (osvi.dwMinorVersion == 3) {
            versionStr += L"8.1";
        }
        else if (osvi.dwMinorVersion == 2) {
            versionStr += L"8";
        }
        else if (osvi.dwMinorVersion == 1) {
            versionStr += L"7";
        }
    }
    else {
        versionStr += L"Unknown Version";
    }

    return versionStr;
}

void CenterWindowOnScreen(HWND hWnd) {
    SetProcessDPIAware();

    RECT windowRect;
    GetWindowRect(hWnd, &windowRect);
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int centerX = (screenWidth - windowWidth) / 2;
    int centerY = (screenHeight - windowHeight) / 2;

    SetWindowPos(hWnd, NULL, centerX, centerY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void PlaySystemSound(bool isSuccess) {
    if (isSuccess) {
        PlaySound(MAKEINTRESOURCE(SND_ALIAS_SYSTEMDEFAULT), NULL, SND_ALIAS_ID | SND_ASYNC);
    }
    else {
        PlaySound(MAKEINTRESOURCE(SND_ALIAS_SYSTEMHAND), NULL, SND_ALIAS_ID | SND_ASYNC);
    }
}

std::wstring ReadRegString(const std::wstring& name) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        0, KEY_READ, &hKey) != ERROR_SUCCESS) return L"";

    wchar_t buf[256]; DWORD size = sizeof(buf);
    if (RegQueryValueEx(hKey, name.c_str(), NULL, NULL, (LPBYTE)buf, &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return L"";
    }
    RegCloseKey(hKey);
    return std::wstring(buf);
}

bool IsPaused() {
    std::wstring paused = ReadRegString(L"PauseUpdatesExpiryTime");
    return !paused.empty();
}

void UpdateStatus() {
    isPaused = IsPaused();
    SetWindowText(hLblStatus, lastOperationResult.c_str());
}

void RemovePause() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, L"PauseUpdatesExpiryTime");
        RegDeleteValue(hKey, L"PauseFeatureUpdatesEndTime");
        RegDeleteValue(hKey, L"PauseQualityUpdatesEndTime");
        RegDeleteValue(hKey, L"PauseFeatureUpdatesStartTime");
        RegDeleteValue(hKey, L"PauseQualityUpdatesStartTime");
        RegDeleteValue(hKey, L"FlightSettingsMaxPauseDays");
        RegCloseKey(hKey);
    }
}

void ApplyPause() {
    const std::wstring end = L"4750-12-12T00:00:00Z";
    SYSTEMTIME st; GetSystemTime(&st);
    wchar_t start[64];
    wsprintf(start, L"%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, L"PauseUpdatesExpiryTime", 0, REG_SZ, (const BYTE*)end.c_str(), (DWORD)((end.size() + 1) * sizeof(wchar_t)));
        RegSetValueEx(hKey, L"PauseFeatureUpdatesEndTime", 0, REG_SZ, (const BYTE*)end.c_str(), (DWORD)((end.size() + 1) * sizeof(wchar_t)));
        RegSetValueEx(hKey, L"PauseQualityUpdatesEndTime", 0, REG_SZ, (const BYTE*)end.c_str(), (DWORD)((end.size() + 1) * sizeof(wchar_t)));
        RegSetValueEx(hKey, L"PauseFeatureUpdatesStartTime", 0, REG_SZ, (const BYTE*)start, (DWORD)((wcslen(start) + 1) * sizeof(wchar_t)));
        RegSetValueEx(hKey, L"PauseQualityUpdatesStartTime", 0, REG_SZ, (const BYTE*)start, (DWORD)((wcslen(start) + 1) * sizeof(wchar_t)));
        DWORD days = 2000 * 365;
        RegSetValueEx(hKey, L"FlightSettingsMaxPauseDays", 0, REG_SZ, (const BYTE*)std::to_wstring(days).c_str(), (DWORD)((std::to_wstring(days).size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void TogglePause() {
    if (isPaused) {
        RemovePause();

        if (!IsPaused()) {
            lastOperationResult = L"✅ Pause successfully removed - Updates can now install";
            PlaySystemSound(true);

            ShellExecute(NULL, L"open", L"ms-settings:windowsupdate", NULL, NULL, SW_SHOWNORMAL);
        }
        else {
            lastOperationResult = L"❌ Failed to remove pause - Check administrator privileges";
            PlaySystemSound(false);
        }
    }
    else {
        ApplyPause();

        if (IsPaused()) {
            lastOperationResult = L"✅ Pause successfully applied - Updates blocked until 4750";
            PlaySystemSound(true);

            ShellExecute(NULL, L"open", L"ms-settings:windowsupdate", NULL, NULL, SW_SHOWNORMAL);
        }
        else {
            lastOperationResult = L"❌ Failed to apply pause - Check administrator privileges";
            PlaySystemSound(false);
        }
    }

    UpdateStatus();
}

void CreateModernGDI() {
    hFontTitle = CreateFont(24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI Variable Display");

    hFontButton = CreateFont(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI Variable Text");

    hFontStatus = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, L"Segoe UI Variable Text");

    hBrushBg = CreateSolidBrush(BG_COLOR);
    hBrushCard = CreateSolidBrush(CARD_COLOR);
    hBrushAccent = CreateSolidBrush(ACCENT_COLOR);
    hBrushHover = CreateSolidBrush(HOVER_COLOR);
    hBrushActive = CreateSolidBrush(ACTIVE_COLOR);

    hPenBorder = CreatePen(PS_SOLID, 1, BORDER_COLOR);
    hPenAccent = CreatePen(PS_SOLID, 2, ACCENT_COLOR);
}

void DestroyModernGDI() {
    DeleteObject(hFontTitle);
    DeleteObject(hFontButton);
    DeleteObject(hFontStatus);
    DeleteObject(hBrushBg);
    DeleteObject(hBrushCard);
    DeleteObject(hBrushAccent);
    DeleteObject(hBrushHover);
    DeleteObject(hBrushActive);
    DeleteObject(hPenBorder);
    DeleteObject(hPenAccent);

    if (hMemDC) {
        DeleteDC(hMemDC);
        hMemDC = NULL;
    }
    if (hMemBitmap) {
        DeleteObject(hMemBitmap);
        hMemBitmap = NULL;
    }
}

void InitializeDoubleBuffering(HWND hWnd) {
    GetClientRect(hWnd, &clientRect);
    HDC hdc = GetDC(hWnd);

    if (hMemDC) DeleteDC(hMemDC);
    if (hMemBitmap) DeleteObject(hMemBitmap);

    hMemDC = CreateCompatibleDC(hdc);
    hMemBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    SelectObject(hMemDC, hMemBitmap);

    ReleaseDC(hWnd, hdc);
}

void DrawModernButton(HDC hdc, RECT rect, const wchar_t* text, bool isHovered, bool isPressed, bool isPauseButton = false) {
    HBRUSH brush;

    if (isPressed) {
        brush = hBrushActive;
    }
    else if (isHovered) {
        if (isPauseButton && isPaused) {
            brush = CreateSolidBrush(PAUSE_COLOR);
        }
        else if (isPauseButton && !isPaused) {
            brush = CreateSolidBrush(RGB(40, 167, 69));
        }
        else {
            brush = hBrushHover;
        }
    }
    else {
        if (isPauseButton && isPaused) {
            brush = CreateSolidBrush(RESUME_COLOR);
        }
        else if (isPauseButton && !isPaused) {
            brush = CreateSolidBrush(ACCENT_COLOR);
        }
        else {
            brush = hBrushAccent;
        }
    }

    FillRect(hdc, &rect, brush);

    if (isPauseButton && (isHovered || (!isPaused && !isHovered) || (isPaused && !isHovered))) {
        if (brush != hBrushAccent && brush != hBrushHover && brush != hBrushActive) {
            DeleteObject(brush);
        }
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_PRIMARY);
    SelectObject(hdc, hFontButton);
    DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawModernCard(HDC hdc, RECT rect) {
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 2, 2);

    HBRUSH shadowBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &shadowRect, shadowBrush);
    DeleteObject(shadowBrush);

    FillRect(hdc, &rect, hBrushCard);
    HBRUSH borderBrush = CreateSolidBrush(BORDER_COLOR);
    FrameRect(hdc, &rect, borderBrush);
    DeleteObject(borderBrush);
}

void DrawStatusPanel(HDC hdc, RECT rect) {
    FillRect(hdc, &rect, hBrushCard);

    HBRUSH borderBrush = CreateSolidBrush(BORDER_COLOR);
    FrameRect(hdc, &rect, borderBrush);
    DeleteObject(borderBrush);

    SetBkMode(hdc, TRANSPARENT);

    if (lastOperationResult.find(L"✅") != std::wstring::npos) {
        SetTextColor(hdc, RGB(16, 124, 16));
    }
    else if (lastOperationResult.find(L"❌") != std::wstring::npos) {
        SetTextColor(hdc, RGB(220, 53, 69));
    }
    else {
        SetTextColor(hdc, TEXT_SECONDARY);
    }

    SelectObject(hdc, hFontStatus);
    DrawText(hdc, lastOperationResult.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void PaintWindow(HWND hWnd) {
    if (!hMemDC) return;

    FillRect(hMemDC, &clientRect, hBrushBg);

    RECT cardRect = { 20, 60, 415, 125 };
    DrawModernCard(hMemDC, cardRect);

    RECT titleRect = { 10, 15, 425, 50 };
    SetBkMode(hMemDC, TRANSPARENT);
    SetTextColor(hMemDC, TEXT_PRIMARY);
    SelectObject(hMemDC, hFontTitle);
    DrawText(hMemDC, L"Windows Update Pauser", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT pauseRect = { 40, 75, 395, 110 };

    const wchar_t* pauseText = isPaused ? L"▶ Resume Updates" : L"▌▌ Pause Until 4750";

    DrawModernButton(hMemDC, pauseRect, pauseText, isHoveredPause, isPressedPause, true);

    HPEN hPenHorizontalSeparator = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN hOldPen = (HPEN)SelectObject(hMemDC, hPenHorizontalSeparator);
    SelectObject(hMemDC, hOldPen);
    DeleteObject(hPenHorizontalSeparator);

    RECT statusRect = { 20, 143, 415, 173 };

    DrawStatusPanel(hMemDC, statusRect);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        hMainWnd = hWnd;
        isPaused = IsPaused();

        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hWnd, 20, &darkMode, sizeof(darkMode));

        // Load custom icons from resources
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
        HICON hIconSmall = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON_SMALL));

        if (!hIcon) {
            hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }
        if (!hIconSmall) {
            hIconSmall = LoadIcon(NULL, IDI_APPLICATION);
        }

        SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

        InitializeDoubleBuffering(hWnd);

        hLblTitle = CreateWindow(L"STATIC", L"Windows Update Pause Control",
            WS_CHILD | SS_CENTER,
            10, 15, 415, 35, hWnd, (HMENU)IDC_LBL_TITLE, NULL, NULL);

        hBtnPause = CreateWindow(L"BUTTON",
            isPaused ? L"▶ Resume Updates" : L"▌▌ Pause Until 4750",
            WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
            40, 75, 355, 35, hWnd, (HMENU)IDC_BTN_PAUSE, NULL, NULL);

        hLblStatus = CreateWindow(L"STATIC", L"Ready to manage Windows Update pause",
            WS_CHILD | SS_CENTER,
            20, 140, 395, 30, hWnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);

        UpdateStatus();

        SetTimer(hWnd, 1, 50, NULL);

        CenterWindowOnScreen(hWnd);
    } break;

    case WM_SIZE: {
        InitializeDoubleBuffering(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
    } break;

    case WM_TIMER: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        static bool prevHoverPause = false;

        RECT pauseRect = { 40, 75, 395, 110 };

        bool newHoverPause = PtInRect(&pauseRect, pt);

        if (newHoverPause != prevHoverPause) {
            isHoveredPause = newHoverPause;
            prevHoverPause = newHoverPause;

            InvalidateRect(hWnd, NULL, FALSE);
        }
    } break;

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BTN_PAUSE:
            TogglePause();
            SetWindowText(hBtnPause, isPaused ? L"▶ Resume Updates" : L"▌▌ Pause Until 4750");
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (hMemDC) {
            PaintWindow(hWnd);

            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hMemDC, 0, 0, SRCCOPY);
        }

        EndPaint(hWnd, &ps);
        return 0;
    } break;

    case WM_ERASEBKGND: {
        return 1;
    } break;

    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

        RECT pauseRect = { 40, 75, 395, 110 };

        if (PtInRect(&pauseRect, pt)) {
            isPressedPause = true;
            SetCapture(hWnd);
            InvalidateRect(hWnd, &pauseRect, FALSE);
            return 0;
        }
    } break;

    case WM_LBUTTONUP: {
        if (GetCapture() == hWnd) {
            ReleaseCapture();

            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT pauseRect = { 40, 75, 395, 110 };

            if (isPressedPause) {
                isPressedPause = false;
                InvalidateRect(hWnd, &pauseRect, FALSE);

                if (PtInRect(&pauseRect, pt)) {
                    SendMessage(hWnd, WM_COMMAND, IDC_BTN_PAUSE, 0);
                }
            }
        }
    } break;

    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hWnd, &pt);

        RECT pauseRect = { 40, 75, 395, 110 };

        if (PtInRect(&pauseRect, pt)) {
            SetCursor(hHandCursor);
            return TRUE;
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    } break;

    case WM_DESTROY: {
        KillTimer(hWnd, 1);
        DestroyModernGDI();

        // Get and destroy icons if they exist
        HICON hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_BIG, 0);
        HICON hIconSmall = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0);
        if (hIcon) DestroyIcon(hIcon);
        if (hIconSmall) DestroyIcon(hIconSmall);

        PostQuitMessage(0);
    } break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // Check Windows version before doing anything else
    if (!CheckWindowsVersion()) {
        return 1; // Exit if not Windows 10+
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    CreateModernGDI();

    hHandCursor = LoadCursor(NULL, IDC_HAND);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(CLASS_NAME, L"Windows Update Pauser",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 240,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}