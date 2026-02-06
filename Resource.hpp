#ifndef RESOURCE_HPP
#define RESOURCE_HPP

// ==========================================
// SECTION 1: RC COMPATIBLE (C Preprocessor)
// ==========================================

// Version - single source of truth
#define VER_MAJOR       2
#define VER_MINOR       0
#define VER_PATCH       1
#define VER_BUILD       0

#define VER_FILEVERSION VER_MAJOR,VER_MINOR,VER_PATCH,VER_BUILD

// Stringify macros
#define VER_STR(s)      #s
#define VER_XSTR(s)     VER_STR(s)
#define VER_FILEVERSION_STR VER_XSTR(VER_MAJOR) "." VER_XSTR(VER_MINOR) "." VER_XSTR(VER_PATCH)

// Resource IDs - використовуємо IDI_ префікс щоб уникнути конфліктів з Windows headers
#define IDI_MAIN_ICON       101

// String constants for RC
#define RES_COMPANY_NAME    "EXLOUD"
#define RES_PRODUCT_NAME    "Windows Update Pauser"
#define RES_COPYRIGHT       "EXLOUD 2026"
#define RES_INTERNAL_NAME   "WindowsUpdatePauser"
#define RES_ORIGINAL_NAME   "WindowsUpdatePauser.exe"
#define RES_CLASS_NAME      "WUP"
#define RES_MUTEX_NAME      "Global\\EXLOUD_WUP_2010_2026_SingleInstance"

// ==========================================
// SECTION 2: C++23 ONLY (Excluded from RC)
// ==========================================
#ifndef RC_INVOKED

#include <cstdint>
#include <string_view>
#include <format>

namespace resources {
    namespace icons {
        inline constexpr std::uint16_t MAIN = IDI_MAIN_ICON;
    }
    
    namespace strings {
        inline constexpr std::wstring_view CLASS_NAME = L"" RES_CLASS_NAME;
        inline constexpr std::wstring_view MUTEX_NAME = L"" RES_MUTEX_NAME;
        inline constexpr std::wstring_view WINDOW_TITLE = L"WUP";
        inline constexpr std::wstring_view COMPANY = L"" RES_COMPANY_NAME;
        inline constexpr std::wstring_view PRODUCT = L"" RES_PRODUCT_NAME;
    }
    
    namespace ui {
        inline constexpr int WINDOW_WIDTH = 445;
        inline constexpr int WINDOW_HEIGHT = 210;
        inline constexpr float BUTTON_WIDTH = 340.0F;
        inline constexpr float BUTTON_HEIGHT = 35.0F;
        inline constexpr float STATUS_WIDTH = 390.0F;
        inline constexpr float STATUS_HEIGHT = 40.0F;
        inline constexpr float CARD_WIDTH = 390.0F;
        inline constexpr float CARD_HEIGHT = 70.0F;
        inline constexpr int TIMER_ID = 1;
        inline constexpr int TIMER_INTERVAL = 50;
        inline constexpr int TIMER_ANIMATION_ID = 2;
        inline constexpr int TIMER_ANIMATION_INTERVAL = 16;
        inline constexpr float COLOR_TRANSITION_SPEED = 0.08F;
    }
    
    namespace version {
        inline constexpr int MAJOR = VER_MAJOR;
        inline constexpr int MINOR = VER_MINOR;
        inline constexpr int PATCH = VER_PATCH;
        inline constexpr int BUILD = VER_BUILD;
        
        [[nodiscard]] inline std::wstring string() {
            return std::format(L"{}.{}.{}.{}", VER_MAJOR, VER_MINOR, VER_PATCH, VER_BUILD);
        }
    }
    
    // Compile-time verification
    static_assert(icons::MAIN == 101);
}

#endif // RC_INVOKED
#endif // RESOURCE_HPP