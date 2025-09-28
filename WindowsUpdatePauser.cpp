// WindowsUpdatePauser.cpp
// Target Windows 10 (0x0A00) or later
#ifndef WINVER
  #define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0A00
#endif

// Exclude rarely-used stuff from Windows headers
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

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
#include <algorithm>
#include <cmath>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
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
// SINGLE INSTANCE CONSTANTS
// ==================================================================
constexpr wchar_t MUTEX_NAME[] = L"Global\\EXLOUD_WUP_140_SCHAVEL_NO_2025_SingleInstance";

// ==================================================================
// CONSTANTS AND GLOBALS
// ==================================================================
constexpr wchar_t CLASS_NAME[] = L"WUP";
constexpr int WINDOW_WIDTH = 445;
constexpr int WINDOW_HEIGHT = 210;

// Layout constants - centered elements
constexpr float BUTTON_WIDTH = 340.0f;
constexpr float BUTTON_HEIGHT = 35.0f;
constexpr float STATUS_WIDTH = 390.0f;
constexpr float STATUS_HEIGHT = 40.0f;
constexpr float CARD_WIDTH = 390.0f;
constexpr float CARD_HEIGHT = 70.0f;

constexpr int TIMER_ID = 1;
constexpr int TIMER_INTERVAL = 50;
constexpr int TIMER_ANIMATION_ID = 2;           
constexpr int TIMER_ANIMATION_INTERVAL = 16;
constexpr float COLOR_TRANSITION_SPEED = 0.08f;

// Modern color scheme
namespace Colors {
    // Базові кольори
    constexpr D2D1_COLOR_F Background = { 25.0f/255.0f, 25.0f/255.0f, 25.0f/255.0f, 1.0f };      // RGB(25, 25, 25)
    constexpr D2D1_COLOR_F Card = { 34.0f/255.0f, 34.0f/255.0f, 34.0f/255.0f, 1.0f };           // RGB(34, 34, 34)
    constexpr D2D1_COLOR_F CardHover = { 44.0f/255.0f, 44.0f/255.0f, 44.0f/255.0f, 1.0f };      // Трохи світліше для ховеру
    
    // Акцентні кольори
    constexpr D2D1_COLOR_F Accent = { 0.0f/255.0f, 120.0f/255.0f, 215.0f/255.0f, 1.0f };        // RGB(0, 120, 215)
    constexpr D2D1_COLOR_F AccentHover = { 16.0f/255.0f, 132.0f/255.0f, 208.0f/255.0f, 1.0f };  // RGB(16, 132, 208)
    constexpr D2D1_COLOR_F Active = { 0.0f/255.0f, 102.0f/255.0f, 180.0f/255.0f, 1.0f };        // RGB(0, 102, 180)
    
    // Кольори для кнопок
    constexpr D2D1_COLOR_F Pause = { 255.0f/255.0f, 193.0f/255.0f, 7.0f/255.0f, 1.0f };         // RGB(255, 193, 7)
    constexpr D2D1_COLOR_F Resume = { 40.0f/255.0f, 167.0f/255.0f, 69.0f/255.0f, 1.0f };        // RGB(40, 167, 69)
    
    // Кольори тексту
    constexpr D2D1_COLOR_F TextPrimary = { 255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f, 1.0f }; // RGB(255, 255, 255)
    constexpr D2D1_COLOR_F TextSecondary = { 180.0f/255.0f, 180.0f/255.0f, 180.0f/255.0f, 1.0f }; // RGB(180, 180, 180)
    constexpr D2D1_COLOR_F TextSuccess = { 16.0f/255.0f, 185.0f/255.0f, 129.0f/255.0f, 1.0f };  // RGB(16, 185, 129)
    constexpr D2D1_COLOR_F TextError = { 239.0f/255.0f, 68.0f/255.0f, 68.0f/255.0f, 1.0f };      // RGB(239, 68, 68)
    
    // Бордери та тіні
    constexpr D2D1_COLOR_F Border = { 64.0f/255.0f, 64.0f/255.0f, 64.0f/255.0f, 0.5f };         // RGB(64, 64, 64) з прозорістю
    constexpr D2D1_COLOR_F Shadow = { 8.0f/255.0f, 8.0f/255.0f, 8.0f/255.0f, 0.3f };            // RGB(8, 8, 8) з прозорістю
}

// Animation structure
struct ColorAnimation {
    D2D1_COLOR_F current;
    D2D1_COLOR_F target;
    
    void SetTarget(const D2D1_COLOR_F& color) {
        target = color;
    }
    
    void SetCurrent(const D2D1_COLOR_F& color) {
        current = color;
        target = color;
    }
    
	bool Update(float speed) {
		bool changed = false;
		auto lerp = [&](float& curr, float targ) {
			if (std::abs(curr - targ) > 0.001f) {
				float diff = targ - curr;
				float t = diff * speed;
				// Ease-out cubic: f(t) = 1 - (1 - t)^3
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

// Direct2D Resources
struct D2DResources {
    ID2D1Factory* factory = nullptr;
    ID2D1HwndRenderTarget* renderTarget = nullptr;
    IDWriteFactory* writeFactory = nullptr;
    
    // Text formats
    IDWriteTextFormat* titleFormat = nullptr;
    IDWriteTextFormat* buttonFormat = nullptr;
    IDWriteTextFormat* statusFormat = nullptr;
    
    // Brushes
    ID2D1SolidColorBrush* solidBrush = nullptr;
    ID2D1LinearGradientBrush* gradientBrush = nullptr;
    
    ~D2DResources() { Release(); }
    
    void Release() {
        if (gradientBrush) { gradientBrush->Release(); gradientBrush = nullptr; }
        if (solidBrush) { solidBrush->Release(); solidBrush = nullptr; }
        if (titleFormat) { titleFormat->Release(); titleFormat = nullptr; }
        if (buttonFormat) { buttonFormat->Release(); buttonFormat = nullptr; }
        if (statusFormat) { statusFormat->Release(); statusFormat = nullptr; }
        if (writeFactory) { writeFactory->Release(); writeFactory = nullptr; }
        if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
        if (factory) { factory->Release(); factory = nullptr; }
    }
} g_d2d;

// Application state
struct AppState {
    HWND hWnd = nullptr;
    HANDLE hMutex = nullptr;  // Single instance mutex
    UINT dpi = 96;
    float dpiScale = 1.0f;  // DPI scale factor
    bool isPaused = false;
    bool btnHover = false;
    bool btnPressed = false;
    bool statusHover = false;      
    bool statusPressed = false;    
    ColorAnimation buttonColor;
    ColorAnimation statusColor;
    ColorAnimation cardColor;
    std::wstring statusMessage = L"Ready to manage Windows Update";
    std::wstring originalStatusMessage = L"";
    bool isOperationInProgress = false;
    float animationProgress = 0.0f;
} g_app;

// ==================================================================
// DPI-AWARE HELPER FUNCTIONS
// ==================================================================
inline float GetDpiScale() {
    return g_app.dpiScale;
}

inline int ScaleInt(int value) {
    return static_cast<int>(value * g_app.dpiScale);
}

inline float ScaleFloat(float value) {
    return value * g_app.dpiScale;
}

// Get centered rect for element (DPI-aware)
D2D1_RECT_F GetCenteredRect(float centerX, float centerY, float width, float height) {
    float halfWidth = width / 2.0f;
    float halfHeight = height / 2.0f;
    return D2D1::RectF(
        centerX - halfWidth,
        centerY - halfHeight,
        centerX + halfWidth,
        centerY + halfHeight
    );
}

// ==================================================================
// SINGLE INSTANCE IMPLEMENTATION
// ==================================================================
BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam) {
    wchar_t className[256];
    if (GetClassNameW(hWnd, className, 256) > 0) {
        if (wcscmp(className, CLASS_NAME) == 0) {
            HWND* pFoundWnd = reinterpret_cast<HWND*>(lParam);
            *pFoundWnd = hWnd;
            return FALSE; // Зупиняємо пошук
        }
    }
    return TRUE; // Продовжуємо пошук
}

HWND FindExistingInstance() {
    HWND existingWnd = nullptr;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&existingWnd));
    return existingWnd;
}

bool CheckSingleInstance() {
    // Створюємо глобальний mutex
    g_app.hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    
    if (g_app.hMutex == nullptr) {
        return false;
    }
    
    // Перевіряємо чи mutex вже існує
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Програма вже запущена, шукаємо її вікно
        HWND existingWnd = FindExistingInstance();
        
        // Закриваємо mutex
        CloseHandle(g_app.hMutex);
        g_app.hMutex = nullptr;
        
        return false; // Не дозволяємо запуск другого екземпляру
    }
    
    return true; // Це перший екземпляр
}

void ReleaseSingleInstance() {
    if (g_app.hMutex) {
        CloseHandle(g_app.hMutex);
        g_app.hMutex = nullptr;
    }
}

// ==================================================================
// DIRECT2D INITIALIZATION (DPI-AWARE)
// ==================================================================
HRESULT InitializeDirect2D(HWND hWnd) {
    HRESULT hr = S_OK;
    
    // Create factory
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d.factory);
    if (FAILED(hr)) return hr;
    
    // Create render target
    RECT rc;
    GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    
    // Create render target with default DPI (96)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
    props.dpiX = 96.0f;
    props.dpiY = 96.0f;
    
    hr = g_d2d.factory->CreateHwndRenderTarget(
        props,
        D2D1::HwndRenderTargetProperties(hWnd, size),
        &g_d2d.renderTarget
    );
    if (FAILED(hr)) return hr;
    
    g_d2d.renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    
    // Create DWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_d2d.writeFactory)
    );
    if (FAILED(hr)) return hr;
    
    // Create text formats with base sizes (will be scaled during rendering)
    hr = g_d2d.writeFactory->CreateTextFormat(
        L"Segoe UI Variable Display",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        22.0f,  // Base size
        L"en-US",
        &g_d2d.titleFormat
    );
    if (SUCCEEDED(hr)) {
        g_d2d.titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    
    hr = g_d2d.writeFactory->CreateTextFormat(
        L"Segoe UI Variable",
        nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.0f,  // Base size
        L"en-US",
        &g_d2d.buttonFormat
    );
    if (SUCCEEDED(hr)) {
        g_d2d.buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.buttonFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    
    hr = g_d2d.writeFactory->CreateTextFormat(
        L"Segoe UI Variable",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.0f,  // Base size
        L"en-US",
        &g_d2d.statusFormat
    );
    if (SUCCEEDED(hr)) {
        g_d2d.statusFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_d2d.statusFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    
    // Create brushes
    hr = g_d2d.renderTarget->CreateSolidColorBrush(
        Colors::TextPrimary,
        &g_d2d.solidBrush
    );
    
    // Create gradient brush
    ID2D1GradientStopCollection* gradientStops = nullptr;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].color = D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.8f);
    stops[0].position = 0.0f;
    stops[1].color = D2D1::ColorF(0.06f, 0.52f, 0.89f, 0.3f);
    stops[1].position = 1.0f;
    
    hr = g_d2d.renderTarget->CreateGradientStopCollection(
        stops,
        2,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &gradientStops
    );
    
    if (SUCCEEDED(hr)) {
        hr = g_d2d.renderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0, 0),
                D2D1::Point2F(100, 100)
            ),
            gradientStops,
            &g_d2d.gradientBrush
        );
        gradientStops->Release();
    }
    
    return hr;
}

void ResizeDirect2D(HWND hWnd) {
    if (!g_d2d.renderTarget) return;
    
    RECT rc;
    GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    g_d2d.renderTarget->Resize(size);
}

// ==================================================================
// DRAWING FUNCTIONS (DPI-AWARE)
// ==================================================================
void DrawRoundedCard(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color, bool withShadow = true) {
    if (!g_d2d.renderTarget || !g_d2d.solidBrush) return;

    float scaledRadius = radius * g_app.dpiScale;

    if (withShadow) {
        D2D1_RECT_F shadowRect = rect;
        float shadowOffset = 2.0f * g_app.dpiScale;
        shadowRect.left += shadowOffset;
        shadowRect.top += shadowOffset;
        shadowRect.right += shadowOffset;
        shadowRect.bottom += shadowOffset;
        
        g_d2d.solidBrush->SetColor(D2D1::ColorF(0, 0, 0, 0.2f));
        g_d2d.renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(shadowRect, scaledRadius + g_app.dpiScale, scaledRadius + g_app.dpiScale),
            g_d2d.solidBrush
        );
    }

    g_d2d.solidBrush->SetColor(color);
    g_d2d.renderTarget->FillRoundedRectangle(
        D2D1::RoundedRect(rect, scaledRadius, scaledRadius),
        g_d2d.solidBrush
    );

    g_d2d.solidBrush->SetColor(Colors::Border);
    g_d2d.renderTarget->DrawRoundedRectangle(
        D2D1::RoundedRect(rect, scaledRadius, scaledRadius),
        g_d2d.solidBrush,
        0.5f * g_app.dpiScale
    );
}

void DrawModernButton(const D2D1_RECT_F& rect, const wchar_t* text, const D2D1_COLOR_F& color, bool isPressed) {
    if (!g_d2d.renderTarget || !g_d2d.solidBrush) return;

    const float radius = 12.0f;
    D2D1_RECT_F buttonRect = rect;

    // Ефект натискання: зменшення на 2%
    if (isPressed) {
        float centerX = (rect.left + rect.right) / 2.0f;
        float centerY = (rect.top + rect.bottom) / 2.0f;
        float width  = (rect.right - rect.left) * 0.98f;
        float height = (rect.bottom - rect.top) * 0.98f;
        buttonRect = D2D1::RectF(
            centerX - width / 2.0f,
            centerY - height / 2.0f,
            centerX + width / 2.0f,
            centerY + height / 2.0f
        );
    }

    // Малюємо фон кнопки
    DrawRoundedCard(buttonRect, radius, color, !isPressed);

    // Градієнтний оверлей
    if (g_d2d.gradientBrush) {
        g_d2d.gradientBrush->SetStartPoint(D2D1::Point2F(buttonRect.left, buttonRect.top));
        g_d2d.gradientBrush->SetEndPoint(D2D1::Point2F(buttonRect.right, buttonRect.bottom));
        g_d2d.gradientBrush->SetOpacity(0.3f);
        g_d2d.renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(buttonRect, radius * g_app.dpiScale, radius * g_app.dpiScale),
            g_d2d.gradientBrush
        );
        g_d2d.gradientBrush->SetOpacity(1.0f);
    }

    // Текст кнопки
    g_d2d.solidBrush->SetColor(Colors::TextPrimary);
    g_d2d.renderTarget->DrawText(
        text,
        static_cast<UINT32>(wcslen(text)),
        g_d2d.buttonFormat,
        buttonRect,
        g_d2d.solidBrush
    );
}

void DrawStatusPanel(const D2D1_RECT_F& rect, const wchar_t* text, const D2D1_COLOR_F& bgColor) {
    if (!g_d2d.renderTarget || !g_d2d.solidBrush) return;

    const float radius = 8.0f;

    // Фон панелі
    DrawRoundedCard(rect, radius, bgColor, true);

    // Визначення кольору тексту за емодзі
    D2D1_COLOR_F textColor = Colors::TextSecondary;
    if (wcsstr(text, L"✅")) {
        textColor = Colors::TextSuccess;
    } else if (wcsstr(text, L"❌")) {
        textColor = Colors::TextError;
    } else if (wcsstr(text, L"🔗")) {
        textColor = Colors::Accent;
    }

    // Текст статусу
    g_d2d.solidBrush->SetColor(textColor);
    g_d2d.renderTarget->DrawText(
        text,
        static_cast<UINT32>(wcslen(text)),
        g_d2d.statusFormat,
        rect,
        g_d2d.solidBrush
    );
}

D2D1_RECT_F GetScaledCenteredRect(float centerX, float centerY, float width, float height) {
    return GetCenteredRect(
        centerX * g_app.dpiScale,
        centerY * g_app.dpiScale,
        width * g_app.dpiScale,
        height * g_app.dpiScale
    );
}

void PaintWindow(HWND hWnd) {
    if (!g_d2d.renderTarget) return;

    g_d2d.renderTarget->BeginDraw();
    
    // Apply DPI scaling transform
    // g_d2d.renderTarget->SetTransform(D2D1::Matrix3x2F::Scale(g_app.dpiScale, g_app.dpiScale));

    // Clear background
    g_d2d.renderTarget->Clear(D2D1::ColorF(20.0f/255.0f, 20.0f/255.0f, 20.0f/255.0f));

    // Create subtle background gradient
    static ID2D1LinearGradientBrush* subtleBgGradient = nullptr;
    if (!subtleBgGradient) {
        D2D1_GRADIENT_STOP bgStops[2];
        bgStops[0].color = D2D1::ColorF(20.0f/255.0f, 20.0f/255.0f, 20.0f/255.0f, 1.0f);
        bgStops[0].position = 0.0f;
        bgStops[1].color = D2D1::ColorF(30.0f/255.0f, 30.0f/255.0f, 30.0f/255.0f, 1.0f);
        bgStops[1].position = 1.0f;

        ID2D1GradientStopCollection* bgCollection = nullptr;
        g_d2d.renderTarget->CreateGradientStopCollection(bgStops, 2, &bgCollection);
		float scaledHeight = WINDOW_HEIGHT * g_app.dpiScale;
		g_d2d.renderTarget->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(
				D2D1::Point2F(0, 0),
				D2D1::Point2F(0, scaledHeight)
			),
			bgCollection,
			&subtleBgGradient
		);
        if (bgCollection) bgCollection->Release();
    }

    if (subtleBgGradient) {
        float scaledWidth = WINDOW_WIDTH * g_app.dpiScale;
		float scaledHeight = WINDOW_HEIGHT * g_app.dpiScale;
		D2D1_RECT_F bgRect = D2D1::RectF(0, 0, scaledWidth, scaledHeight);
        g_d2d.renderTarget->FillRectangle(bgRect, subtleBgGradient);
    }

    // Layout calculations (logical coordinates)
    float centerX = WINDOW_WIDTH / 2.0f;

    // Title
    D2D1_RECT_F titleRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 35.0f, WINDOW_WIDTH, 30.0f);
    g_d2d.solidBrush->SetColor(Colors::TextPrimary);
    const wchar_t* titleText = L"Windows Update Pauser";
    g_d2d.renderTarget->DrawText(
        titleText,
        wcslen(titleText),
        g_d2d.titleFormat,
        titleRect,
        g_d2d.solidBrush
    );

    // Main card
    D2D1_RECT_F cardRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, CARD_WIDTH, CARD_HEIGHT);
    DrawRoundedCard(cardRect, 12.0f, g_app.cardColor.current, true);

    // Button
    D2D1_RECT_F buttonRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
    const wchar_t* buttonText;
	bool isRestartMode = false;

	if (g_app.statusMessage.find(L"Restart required") != std::wstring::npos) {
		buttonText = L"🔄 Restart Now";
		isRestartMode = true;
	} else {
		buttonText = g_app.isPaused ? L"▶  Resume Updates" : L"⏸  Pause for 100 years";
	}

    D2D1_RECT_F finalButtonRect = buttonRect;
	if (g_app.btnPressed) {
		float newWidth  = (buttonRect.right - buttonRect.left) * 0.98f;
		float newHeight = (buttonRect.bottom - buttonRect.top) * 0.98f;
		float centerX = (buttonRect.left + buttonRect.right) / 2.0f;
		float centerY = (buttonRect.top + buttonRect.bottom) / 2.0f;
		finalButtonRect = D2D1::RectF(
			centerX - newWidth / 2.0f,
			centerY - newHeight / 2.0f,
			centerX + newWidth / 2.0f,
			centerY + newHeight / 2.0f
		);
	}

    DrawRoundedCard(finalButtonRect, 12.0f, g_app.buttonColor.current, !g_app.btnPressed);

    // Gradient overlay
    if (g_d2d.gradientBrush) {
        g_d2d.gradientBrush->SetStartPoint(D2D1::Point2F(finalButtonRect.left, finalButtonRect.top));
        g_d2d.gradientBrush->SetEndPoint(D2D1::Point2F(finalButtonRect.right, finalButtonRect.bottom));
        g_d2d.gradientBrush->SetOpacity(0.3f);
        g_d2d.renderTarget->FillRoundedRectangle(
            D2D1::RoundedRect(finalButtonRect, 12.0f * g_app.dpiScale, 12.0f * g_app.dpiScale),
            g_d2d.gradientBrush
        );
        g_d2d.gradientBrush->SetOpacity(1.0f);
    }

    // Button text
    g_d2d.solidBrush->SetColor(Colors::TextPrimary);
    g_d2d.renderTarget->DrawText(
        buttonText,
        wcslen(buttonText),
        g_d2d.buttonFormat,
        finalButtonRect,
        g_d2d.solidBrush
    );

    // Status panel
    D2D1_RECT_F statusRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 160.0f, STATUS_WIDTH, STATUS_HEIGHT);
    DrawStatusPanel(statusRect, g_app.statusMessage.c_str(), g_app.statusColor.current);

    // End drawing
    HRESULT hr = g_d2d.renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_d2d.Release();
        InitializeDirect2D(hWnd);
    }
}

// ==================================================================
// HIT TESTING (DPI-AWARE)
// ==================================================================
// D2D1_RECT_F GetButtonRect() {
    // float centerX = WINDOW_WIDTH / 2.0f;
    // return GetCenteredRect(centerX, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
// }

// D2D1_RECT_F GetStatusRect() {
    // float centerX = WINDOW_WIDTH / 2.0f;
    // return GetCenteredRect(centerX, 160.0f, STATUS_WIDTH, STATUS_HEIGHT);
// }

bool PointInRect(const D2D1_RECT_F& rect, POINT pt) {
    return pt.x >= rect.left && pt.x <= rect.right &&
           pt.y >= rect.top && pt.y <= rect.bottom;
}

// ==================================================================
// COLOR UPDATES
// ==================================================================
void UpdateButtonTargetColor() {
    D2D1_COLOR_F targetColor;

    // Режим перезавантаження?
    if (g_app.statusMessage.find(L"Restart required") != std::wstring::npos) {
        targetColor = g_app.btnPressed ? 
            D2D1::ColorF(Colors::Resume.r * 0.8f, Colors::Resume.g * 0.8f, Colors::Resume.b * 0.8f) :
            Colors::Resume; // зелений — позитивна дія
    } else {
        // Стара логіка для Pause/Resume
        if (g_app.btnPressed) {
            if (g_app.isPaused) {
                targetColor = D2D1::ColorF(Colors::Resume.r * 0.8f, Colors::Resume.g * 0.8f, Colors::Resume.b * 0.8f);
            } else {
                targetColor = D2D1::ColorF(Colors::Pause.r * 0.8f, Colors::Pause.g * 0.8f, Colors::Pause.b * 0.8f);
            }
        } else if (g_app.btnHover) {
            targetColor = g_app.isPaused ? Colors::Resume : Colors::Pause;
        } else {
            targetColor = Colors::Accent;
        }
    }

    g_app.buttonColor.SetTarget(targetColor);
}

void UpdateStatusTargetColor() {
    D2D1_COLOR_F targetColor = Colors::Card;
    
    if (g_app.statusPressed) {
        targetColor = D2D1::ColorF(
            Colors::Card.r * 0.85f,
            Colors::Card.g * 0.85f,
            Colors::Card.b * 0.85f
        );
    } else if (g_app.statusHover) {
        targetColor = Colors::CardHover;
    }
    
    g_app.statusColor.SetTarget(targetColor);
}

void UpdateCardTargetColor() {
    D2D1_COLOR_F targetColor = Colors::Card;
    
    if (g_app.btnHover || g_app.btnPressed) {
        targetColor = Colors::CardHover;
    }
    
    g_app.cardColor.SetTarget(targetColor);
}

// ==================================================================
// WINDOWS VERSION DETECTION
// ==================================================================
enum class WindowsVersion {
    Unsupported,
    Windows10,
    Windows11
};

WindowsVersion GetWindowsVersion() {
    OSVERSIONINFOEX osvi = { sizeof(OSVERSIONINFOEX) };
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandle(L"ntdll.dll");
    
    if (hMod) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            NTSTATUS status = RtlGetVersion((PRTL_OSVERSIONINFOW)&osvi);
            if (status == 0) {
                if (osvi.dwMajorVersion >= 10) {
                    if (osvi.dwBuildNumber >= 22000) {
                        return WindowsVersion::Windows11;
                    }
                    return WindowsVersion::Windows10;
                }
            }
        }
    }
    
    if (IsWindows10OrGreater()) {
        return WindowsVersion::Windows10;
    }
    
    return WindowsVersion::Unsupported;
}

inline bool IsWindows10OrLater() {
    WindowsVersion ver = GetWindowsVersion();
    return ver == WindowsVersion::Windows10 || ver == WindowsVersion::Windows11;
}

inline bool IsWindows10Only() {
    return GetWindowsVersion() == WindowsVersion::Windows10;
}

inline bool IsWindows11() {
    return GetWindowsVersion() == WindowsVersion::Windows11;
}

bool CheckWindowsVersion() {
    if (!IsWindows10OrLater()) {
        MessageBoxW(nullptr,
            L"This application requires Windows 10 or later.\n"
            L"Your current Windows version is not supported.",
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
// WINDOW POSITIONING
// ==================================================================
void CenterWindowOnMonitor(HWND hWnd) {
    RECT wr;
    GetWindowRect(hWnd, &wr);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;
    HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hMon, &mi);
    int cx = mi.rcWork.right - mi.rcWork.left;
    int cy = mi.rcWork.bottom - mi.rcWork.top;
    SetWindowPos(hWnd, nullptr, 
        mi.rcWork.left + (cx - w) / 2, 
        mi.rcWork.top + (cy - h) / 2, 
        0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ==================================================================
// SYSTEM INTERACTION
// ==================================================================
void ShowErrorAndExit(const wchar_t* errorMsg);

void PlaySystemSound(bool success) {
    PlaySoundW(success ? L"SystemDefault" : L"SystemHand",
        nullptr, SND_ALIAS | SND_ASYNC);
}

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
    
    if (IsProcessRunning(targetProcess)) {
        TerminateProcessByName(targetProcess);
        Sleep(100);
    }
    
    ShellExecuteW(nullptr, L"open", L"ms-settings:windowsupdate",
        nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowAboutDialog(HWND hWnd) {
    ShellExecuteW(nullptr, L"open", 
        L"https://github.com/EXLOUD?tab=repositories", 
        nullptr, nullptr, SW_SHOWNORMAL);
}

// ==================================================================
// RequestSystemRestart
// ==================================================================

void PerformSystemRestart() {
    PlaySoundW(L"SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);

    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        ShowErrorAndExit(L"Failed to get process token for restart.");
        return;
    }

    TOKEN_PRIVILEGES tkp;
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0)) {
        CloseHandle(hToken);
        ShowErrorAndExit(L"Failed to enable shutdown privilege.");
        return;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        CloseHandle(hToken);
        ShowErrorAndExit(L"Required privilege not available.");
        return;
    }

    // 🔥 НЕГАЙНЕ ПЕРЕЗАВАНТАЖЕННЯ
    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
        SHTDN_REASON_MAJOR_APPLICATION |
        SHTDN_REASON_MINOR_INSTALLATION |
        SHTDN_REASON_FLAG_PLANNED)) {
        DWORD err = GetLastError();
        wchar_t msg[256];
        swprintf_s(msg, L"Failed to restart (Error: %lu).", err);
        ShowErrorAndExit(msg);
    }

    CloseHandle(hToken);
}

void ShowErrorAndExit(const wchar_t* errorMsg) {
    PlaySoundW(L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC);
    MessageBoxW(g_app.hWnd, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    PostQuitMessage(0);
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
        return false;
    }
    result = RegSetValueExW(hKey, valueName, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool DeleteRegValue(const wchar_t* keyPath, const wchar_t* valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    LONG result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}

bool DeleteRegValue(const wchar_t* valueName) {
    return DeleteRegValue(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", valueName);
}

bool SetRegBinary(const wchar_t* keyPath, const wchar_t* valueName, const BYTE* data, DWORD dataSize) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    result = RegSetValueExW(hKey, valueName, 0, REG_BINARY, data, dataSize);
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

// FailureActions для вимкненого стану (всі нулі)
bool SetDisabledFailureActions(const wchar_t* servicePath) {
    BYTE disabledFailureActions[44] = {0}; // 44 байти нулів
    return SetRegBinary(servicePath, L"FailureActions", disabledFailureActions, sizeof(disabledFailureActions));
}

// FailureActions для DoSvc (увімкнений стан)
// bool SetDoSvcFailureActions() {
    // BYTE failureActions[] = {
        // 0x80, 0x51, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        // 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0x60, 0xea, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0x60, 0xea, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00
    // };
    // return SetRegBinary(L"SYSTEM\\CurrentControlSet\\Services\\DoSvc", 
                       // L"FailureActions", failureActions, sizeof(failureActions));
// }

// FailureActions для UsoSvc (увімкнений стан)
// bool SetUsoSvcFailureActions() {
    // BYTE failureActions[] = {
        // 0x80, 0x51, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        // 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0xe0, 0x93, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0xe0, 0x93, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00
    // };
    // return SetRegBinary(L"SYSTEM\\CurrentControlSet\\Services\\UsoSvc", 
                       // L"FailureActions", failureActions, sizeof(failureActions));
// }

// FailureActions для WaaSMedicSvc (увімкнений стан)
bool SetWaaSMedicFailureActions() {
    BYTE failureActions[] = {
        0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xc0, 0xd4, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
        0xe0, 0x93, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    return SetRegBinary(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", 
                       L"FailureActions", failureActions, sizeof(failureActions));
}

// FailureActions для wuauserv (увімкнений стан)
// bool SetWuauservFailureActions() {
    // BYTE failureActions[] = {
        // 0x80, 0x51, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        // 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0x60, 0xea, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        // 0xc0, 0xd4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        // 0x00, 0x00, 0x00, 0x00
    // };
    // return SetRegBinary(L"SYSTEM\\CurrentControlSet\\Services\\wuauserv", 
                       // L"FailureActions", failureActions, sizeof(failureActions));
// }

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
    return SetRegDWORD(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 
        L"FlightSettingsMaxPauseDays", maxDays);
}

std::wstring CalculateFutureDate100Years() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    st.wYear += 100;
    st.wMonth = 12;
    st.wDay = 31;
    st.wHour = 16;
    st.wMinute = 15;
    st.wSecond = 25;
    
    wchar_t result[32];
    swprintf_s(result, L"%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return std::wstring(result);
}

bool ApplyPause() {
    const DWORD maxDays = 36500; // 100 years
    SetMaxPauseDays(maxDays);
    
    const std::wstring startTime = GetCurrentTimeString();
    const std::wstring endTime = CalculateFutureDate100Years();
    
    bool success = true;
    
    // Set all pause registry values
    success &= SetRegString(L"PauseUpdatesStartTime", startTime);
    success &= SetRegString(L"PauseUpdatesExpiryTime", endTime);
    success &= SetRegString(L"PauseFeatureUpdatesStartTime", startTime);
    success &= SetRegString(L"PauseFeatureUpdatesEndTime", endTime);
    success &= SetRegString(L"PauseQualityUpdatesStartTime", startTime);
    success &= SetRegString(L"PauseQualityUpdatesEndTime", endTime);
    
    // UpdatePolicy Settings
    const wchar_t* updatePolicyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    success &= SetRegDWORD(updatePolicyKey, L"PausedFeatureStatus", 1);
    success &= SetRegDWORD(updatePolicyKey, L"PausedQualityStatus", 1);
    success &= SetRegString(updatePolicyKey, L"PausedQualityDate", endTime);
    success &= SetRegString(updatePolicyKey, L"PausedFeatureDate", endTime);
    
    // Additional settings
	if (IsWindows10Only()) {
		SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 4);
		SetDisabledFailureActions(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc");
	}

    SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 4);
	SetDisabledFailureActions(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc");
	
	// SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\wuauserv", L"Start", 4);
	// SetDisabledFailureActions(L"SYSTEM\\CurrentControlSet\\Services\\wuauserv");
	
	// SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\DoSvc", L"Start", 4);
    // SetDisabledFailureActions(L"SYSTEM\\CurrentControlSet\\Services\\DoSvc");

    // Вимикаємо службу Update Orchestrator
    // SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\UsoSvc", L"Start", 4);
    // SetDisabledFailureActions(L"SYSTEM\\CurrentControlSet\\Services\\UsoSvc");
	
    SetRegDWORD(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", L"ExcludeWUDriversInQualityUpdate", 1);
    SetRegDWORD(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"SearchOrderConfig", 0);
    SetRegDWORD(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"DontSearchWindowsUpdate", 1);
	
	SetRegDWORD(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Device Metadata", L"PreventDeviceMetadataFromNetwork", 1);
    
    // Windows Update Policies
    const wchar_t* wuPolicyKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    SetRegString(wuPolicyKey, L"WUServer", L" ");
    SetRegString(wuPolicyKey, L"WUStatusServer", L" ");
    SetRegString(wuPolicyKey, L"UpdateServiceUrlAlternate", L" ");
    SetRegDWORD(wuPolicyKey, L"DisableWindowsUpdateAccess", 1);
    SetRegDWORD(wuPolicyKey, L"DisableOSUpgrade", 1);
    SetRegDWORD(wuPolicyKey, L"SetDisableUXWUAccess", 1);
    SetRegDWORD(wuPolicyKey, L"DoNotConnectToWindowsUpdateInternetLocations", 1);
    
    // Вимкнути автоматичне завантаження оновлень через лімітні підключення
    SetRegDWORD(wuPolicyKey, L"AllowAutoWindowsUpdateDownloadOverMeteredNetwork", 0);
    
    // AutoUpdate policies
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    SetRegDWORD(auKey, L"NoAutoUpdate", 1);
    SetRegDWORD(auKey, L"UseWUServer", 1);
	SetRegDWORD(auKey, L"AUOptions", 2);
	SetRegDWORD(auKey, L"NoAutoRebootWithLoggedOnUsers", 1);
    SetRegDWORD(auKey, L"AllowMUUpdateService", 0);
    
    return success;
}

bool RemovePause() {
    bool success = true;
    
    // Delete pause values
    success &= DeleteRegValue(L"PauseUpdatesStartTime");
    success &= DeleteRegValue(L"PauseUpdatesExpiryTime");
    success &= DeleteRegValue(L"PauseFeatureUpdatesEndTime");
    success &= DeleteRegValue(L"PauseQualityUpdatesEndTime");
    success &= DeleteRegValue(L"PauseFeatureUpdatesStartTime");
    success &= DeleteRegValue(L"PauseQualityUpdatesStartTime");
    
    // Reset UpdatePolicy Settings
    const wchar_t* updatePolicyKey = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UpdatePolicy\\Settings";
    success &= SetRegDWORD(updatePolicyKey, L"PausedFeatureStatus", 0);
    success &= SetRegDWORD(updatePolicyKey, L"PausedQualityStatus", 0);
    success &= DeleteRegValue(updatePolicyKey, L"PausedQualityDate");
    success &= DeleteRegValue(updatePolicyKey, L"PausedFeatureDate");
    
    // Restore services
	if (IsWindows10Only()) {
		SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"Start", 2);
		DeleteRegValue(L"SYSTEM\\CurrentControlSet\\Services\\uhssvc", L"FailureActions");
	}	
	
    // SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\wuauserv", L"Start", 3);
    // SetWuauservFailureActions();
    
    // WaaSMedicSvc - оновлюємо Start на 3 та відновлюємо FailureActions
    SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\WaaSMedicSvc", L"Start", 3);
    SetWaaSMedicFailureActions();

    // Відновлюємо службу Delivery Optimization
    // SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\DoSvc", L"Start", 3);
    // SetDoSvcFailureActions();

    // Відновлюємо службу Update Orchestrator
    // SetRegDWORD(L"SYSTEM\\CurrentControlSet\\Services\\UsoSvc", L"Start", 2);
    // SetUsoSvcFailureActions();
	
    SetRegDWORD(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", L"ExcludeWUDriversInQualityUpdate", 0);
    DeleteRegValue(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"SearchOrderConfig");
    DeleteRegValue(L"SOFTWARE\\Policies\\Microsoft\\Windows\\DriverSearching", L"DontSearchWindowsUpdate");
	
	DeleteRegValue(L"SOFTWARE\\Policies\\Microsoft\\Windows\\Device Metadata", L"PreventDeviceMetadataFromNetwork");
    
    // Delete policies
    const wchar_t* wuPolicyKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    DeleteRegValue(wuPolicyKey, L"WUServer");
    DeleteRegValue(wuPolicyKey, L"WUStatusServer");
    DeleteRegValue(wuPolicyKey, L"UpdateServiceUrlAlternate");
    DeleteRegValue(wuPolicyKey, L"DisableWindowsUpdateAccess");
    DeleteRegValue(wuPolicyKey, L"DisableOSUpgrade");
    DeleteRegValue(wuPolicyKey, L"SetDisableUXWUAccess");
    DeleteRegValue(wuPolicyKey, L"DoNotConnectToWindowsUpdateInternetLocations");

    DeleteRegValue(wuPolicyKey, L"AllowAutoWindowsUpdateDownloadOverMeteredNetwork");
    
    const wchar_t* auKey = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
    DeleteRegValue(auKey, L"NoAutoUpdate");
    DeleteRegValue(auKey, L"UseWUServer");
	DeleteRegValue(auKey, L"AUOptions");
	DeleteRegValue(auKey, L"NoAutoRebootWithLoggedOnUsers");
    DeleteRegValue(auKey, L"AllowMUUpdateService");
    
    // Clear max pause days
    DeleteRegValue(L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", L"FlightSettingsMaxPauseDays");
    
    return success;
}

void TogglePause() {
    if (g_app.isOperationInProgress) return;
    g_app.isOperationInProgress = true;
    g_app.originalStatusMessage.clear();

    bool wasPaused = g_app.isPaused;
    bool success = false;

    if (wasPaused) {
        success = RemovePause() && !IsPaused();
        if (success) {
            g_app.statusMessage = L"⚠️ Restart required to resume updates";
            g_app.isPaused = false;
            PlaySystemSound(true);
            // Відкриваємо налаштування
            OpenWindowsUpdateSettings();
            // Не перезавантажуємо — чекаємо на натискання кнопки
        } else {
            g_app.statusMessage = L"❌ Failed to resume updates";
            PlaySystemSound(false);
        }
    } else {
        success = ApplyPause() && IsPaused();
        if (success) {
            g_app.statusMessage = L"⚠️ Restart required to apply pause";
            g_app.isPaused = true;
            PlaySystemSound(true);
            // Відкриваємо налаштування
            OpenWindowsUpdateSettings();
            // Не перезавантажуємо — чекаємо на натискання кнопки
        } else {
            g_app.statusMessage = L"❌ Failed to pause updates";
            PlaySystemSound(false);
        }
    }

    g_app.isOperationInProgress = false;
    InvalidateRect(g_app.hWnd, nullptr, FALSE);
}

// ==================================================================
// WINDOW STYLE
// ==================================================================
void EnableModernWindowStyle(HWND hWnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    
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

void ApplyDpiTransform() {
    if (!g_d2d.renderTarget) return;

    float dpiScale = g_app.dpi / 96.0f;
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Scale(dpiScale, dpiScale);
    g_d2d.renderTarget->SetTransform(transform);
}

// ==================================================================
// WINDOW PROCEDURE
// ==================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
	case WM_CREATE: {
		g_app.hWnd = hWnd;
		g_app.dpi = GetDpiForWindow(hWnd);
		g_app.isPaused = IsPaused();

		// Initialize colors
		g_app.buttonColor.SetCurrent(Colors::Accent);
		g_app.statusColor.SetCurrent(Colors::Card);
		g_app.cardColor.SetCurrent(Colors::Card);
		UpdateButtonTargetColor();
		UpdateStatusTargetColor();
		UpdateCardTargetColor();
		InitializeDirect2D(hWnd);
		EnableModernWindowStyle(hWnd);
		SetTimer(hWnd, TIMER_ID, TIMER_INTERVAL, nullptr);
		SetTimer(hWnd, TIMER_ANIMATION_ID, TIMER_ANIMATION_INTERVAL, nullptr);
		return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        PaintWindow(hWnd);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        ResizeDirect2D(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

	case WM_TIMER:
		if (wParam == TIMER_ID) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(hWnd, &pt);

			// Check button hover
			D2D1_RECT_F buttonRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
			bool newHover = PointInRect(buttonRect, pt);
			if (newHover != g_app.btnHover) {
				g_app.btnHover = newHover;
				UpdateButtonTargetColor();
				UpdateCardTargetColor();
				InvalidateRect(hWnd, nullptr, FALSE);
			}

			// Check status hover
			D2D1_RECT_F statusRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 160.0f, STATUS_WIDTH, STATUS_HEIGHT);
			bool newStatusHover = PointInRect(statusRect, pt);
			if (newStatusHover != g_app.statusHover) {
				g_app.statusHover = newStatusHover;
				UpdateStatusTargetColor();

				// Перевіряємо, чи зараз не активний стан "Restart required"
				bool isRestartMode = (g_app.statusMessage.find(L"Restart required") != std::wstring::npos);

				if (g_app.statusHover && !isRestartMode) {
					// Зберігаємо оригінальний статус лише якщо не в режимі перезавантаження
					if (g_app.originalStatusMessage.empty()) {
						g_app.originalStatusMessage = g_app.statusMessage;
					}
					g_app.statusMessage = L"🔗 Open author's GitHub repositories";
				} else if (!g_app.statusHover) {
					// Повертаємо оригінальний статус, якщо не в режимі перезавантаження
					if (!g_app.originalStatusMessage.empty() && !isRestartMode) {
						g_app.statusMessage = g_app.originalStatusMessage;
						g_app.originalStatusMessage.clear();
					}
					// Якщо в режимі перезавантаження — нічого не робимо, статус залишається "⚠️ Restart required..."
				}
				InvalidateRect(hWnd, nullptr, FALSE);
			}
		}
        else if (wParam == TIMER_ANIMATION_ID) {
            bool needsRedraw = false;
            needsRedraw |= g_app.buttonColor.Update(COLOR_TRANSITION_SPEED);
            needsRedraw |= g_app.statusColor.Update(COLOR_TRANSITION_SPEED);
            needsRedraw |= g_app.cardColor.Update(COLOR_TRANSITION_SPEED);
            
            if (needsRedraw) {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;

	case WM_SETCURSOR: {
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hWnd, &pt);

		D2D1_RECT_F buttonRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
		D2D1_RECT_F statusRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 160.0f, STATUS_WIDTH, STATUS_HEIGHT);

		if (PointInRect(buttonRect, pt) || PointInRect(statusRect, pt)) {
			SetCursor(LoadCursor(nullptr, IDC_HAND));
			return TRUE;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	case WM_LBUTTONDOWN: {
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		D2D1_RECT_F buttonRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
		D2D1_RECT_F statusRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 160.0f, STATUS_WIDTH, STATUS_HEIGHT);

		if (PointInRect(buttonRect, pt)) {
			g_app.btnPressed = true;
			UpdateButtonTargetColor();
			UpdateCardTargetColor();
			SetCapture(hWnd);
			InvalidateRect(hWnd, nullptr, FALSE);
		}
		else if (PointInRect(statusRect, pt)) {
			g_app.statusPressed = true;
			UpdateStatusTargetColor();
			SetCapture(hWnd);
			InvalidateRect(hWnd, nullptr, FALSE);
			PlaySoundW(L"SystemDefault", nullptr, SND_ALIAS | SND_ASYNC);
		}
		return 0;
	}

	case WM_LBUTTONUP: {
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		D2D1_RECT_F buttonRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 95.0f, BUTTON_WIDTH, BUTTON_HEIGHT);
		D2D1_RECT_F statusRect = GetScaledCenteredRect(WINDOW_WIDTH / 2.0f, 160.0f, STATUS_WIDTH, STATUS_HEIGHT); // 👈 додано
		if (g_app.btnPressed) {
			ReleaseCapture();
			g_app.btnPressed = false;
			UpdateButtonTargetColor();
			UpdateCardTargetColor();
			if (PointInRect(buttonRect, pt)) {
				// Перевіряємо: чи потрібно перезавантаження?
				if (g_app.statusMessage.find(L"Restart required") != std::wstring::npos) {
					PerformSystemRestart(); 
				} else {
					TogglePause();
				}
			}
			InvalidateRect(hWnd, nullptr, TRUE);
		}
		else if (g_app.statusPressed) {
			ReleaseCapture();
			g_app.statusPressed = false;
			UpdateStatusTargetColor();
			if (PointInRect(statusRect, pt)) { 
				ShowAboutDialog(hWnd);
			}
			InvalidateRect(hWnd, nullptr, FALSE);
		}
		return 0;
	}

	case WM_DPICHANGED: {
		g_app.dpi = LOWORD(wParam);
		
		g_app.dpiScale = static_cast<float>(g_app.dpi) / 96.0f;

		// Отримуємо нові рекомендовані розміри вікна з lParam
		RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);

		// Обчислюємо бажаний розмір CLIENT AREA (логічний) — завжди 455x245
		int clientWidth = MulDiv(WINDOW_WIDTH, g_app.dpi, 96);
		int clientHeight = MulDiv(WINDOW_HEIGHT, g_app.dpi, 96);

		// Обчислюємо розмір вікна (з рамкою) на основі client area
		RECT windowRect = { 0, 0, clientWidth, clientHeight };
		DWORD style = GetWindowLong(hWnd, GWL_STYLE);
		DWORD exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
		AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, g_app.dpi);
		int windowWidth = windowRect.right - windowRect.left;
		int windowHeight = windowRect.bottom - windowRect.top;

		// Застосовуємо новий розмір вікна
		SetWindowPos(hWnd,
			nullptr,
			prcNewWindow->left,  // Залишаємо позицію як рекомендовано
			prcNewWindow->top,
			windowWidth,          // Новий ширина вікна (з рамкою)
			windowHeight,         // Нова висота вікна (з рамкою)
			SWP_NOZORDER | SWP_NOACTIVATE);

		// Оновлюємо розмір render target
		ResizeDirect2D(hWnd);

		// Застосовуємо глобальну трансформацію масштабування
		// ApplyDpiTransform();

		// Інвалідуємо вікно для перерендеру
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID);
        KillTimer(hWnd, TIMER_ANIMATION_ID);
        g_d2d.Release();
        ReleaseSingleInstance(); // Звільняємо mutex
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ==================================================================
// ENTRY POINT
// ==================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Check for single instance FIRST
    if (!CheckSingleInstance()) {
        // Другий екземпляр - виходимо
        return 0;
    }
	
	// Check Windows version
    if (!CheckWindowsVersion()) {
		ReleaseSingleInstance();
        return 1;
    }

    // Check admin privileges
    if (!IsRunningAsAdmin()) {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, szPath, MAX_PATH) == 0) {
            MessageBoxW(nullptr,
                L"Failed to get application path.",
                L"Error",
                MB_OK | MB_ICONERROR);
			ReleaseSingleInstance();
            return 1;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei)) {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED) {
                MessageBoxW(nullptr,
                    L"Administrator privileges are required to run this application.",
                    L"Administrator Privileges Required",
                    MB_OK | MB_ICONWARNING);
            }
			ReleaseSingleInstance();
            return 1;
        }
		ReleaseSingleInstance();
        return 0;
    }

    // Initialize COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

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
        CoUninitialize();
		ReleaseSingleInstance();
        return 1;
    }

    // Отримуємо DPI
    UINT dpi = 96;
    HMONITOR hMon = MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY);
    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpi, &dpi);
    
    // ВАЖЛИВО: Встановити DPI перед створенням вікна
    g_app.dpi = dpi;
	
	g_app.dpiScale = static_cast<float>(dpi) / 96.0f;
    
    // Обчислюємо бажаний розмір CLIENT AREA (не вікна!)
    int clientWidth = MulDiv(WINDOW_WIDTH, dpi, 96);
    int clientHeight = MulDiv(WINDOW_HEIGHT, dpi, 96);
    
    // Обчислюємо розмір вікна на основі client area
    RECT windowRect = { 0, 0, clientWidth, clientHeight };
    
    // Стиль вікна
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOPMOST;
    
    // AdjustWindowRectExForDpi обчислює розмір вікна з рамкою для заданого DPI
    AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, dpi);
    
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    
    // Центруємо вікно
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMon, &mi);
    int windowX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - windowWidth) / 2;
    int windowY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - windowHeight) / 2;

    // Create window з правильними розмірами
    HWND hWnd = CreateWindowExW(
        exStyle,
        CLASS_NAME,
        L"WUP",
        style,
        windowX,
        windowY,
        windowWidth,    // Розмір вікна (з рамкою)
        windowHeight,   // Розмір вікна (з рамкою)
        nullptr, 
        nullptr, 
        hInstance, 
        nullptr
    );

    if (!hWnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        ReleaseSingleInstance();
        return 1;
    }

    // Тепер не потрібно викликати CenterWindowOnMonitor - вікно вже центроване
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
	ReleaseSingleInstance();
    return static_cast<int>(msg.wParam);
}
