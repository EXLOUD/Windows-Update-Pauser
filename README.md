<div align="center">
  
# Windows Update Pauser

### 👀 Repository Views  
<img alt="count" src="https://count.getloli.com/get/@:EXLOUD-WUP?theme=rule34" />  

**⭐ If this tool helped you, please consider giving it a star! ⭐**  

---

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Windows](https://img.shields.io/badge/Windows-10%2F11-blue.svg)

</div>

<details>
<summary>Українська (UA)</summary>

Ця програма дозволяє **призупиняти або поновлювати оновлення Windows** (як функціональні, так і якісні) за допомогою сучасного графічного інтерфейсу.

### Що робить ця утиліта?
- Призупиняє оновлення до 4750 року (через зміну реєстру Windows)
- Надає змогу знову дозволити оновлення у будь-який момент
- Має темний стиль, чистий та адаптивний інтерфейс
- Автоматично відкриває `ms-settings:windowsupdate` після дії

### Як користуватись?
1. Запустіть `.exe` з правами адміністратора
2. Натисніть кнопку "Pause" або "Resume"
3. Оновлення буде призупинено або дозволено
4. Вікно оновлень відкриється автоматично

### Увага!
- Працює лише на **Windows 10 та 11**
- Зміни застосовуються через **реєстр**, тому для ефекту потрібні права адміністратора
- Після перезавантаження або вручну можна відновити оновлення

</details>

## English (EN)

This GUI tool allows you to **pause or resume Windows Updates** (both feature and quality updates) using a modern, dark-themed interface.

### What does this app do?
- Pauses updates until the year 4750 via registry tweaks
- Lets you resume updates anytime with one click
- Clean, dark-mode interface using modern Win32 drawing
- Automatically opens `ms-settings:windowsupdate` after changes

### How to use?
1. Run the `.exe` as Administrator
2. Click "Pause" or "Resume"
3. Updates will be paused or resumed
4. Windows Update settings will open automatically

### Warning!
- Works only on **Windows 10 or 11**
- Requires administrator privileges
- Settings affect the Windows registry (use at your own risk)

---

## Build Instructions

To build from source:

1. Open `UnifiedWindowsUpdateControl.cpp` in Visual Studio
2. Link the following libraries: `comctl32`, `dwmapi`, `uxtheme`, `winmm`, `version`
3. Compile as a Windows Desktop Application
4. Run with Administrator rights

## License

MIT License — see `LICENSE` for details.
