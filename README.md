
<div align="center">
  
# Windows Update Pauser

### 👀 Repository Views  
<img alt="count" src="https://count.getloli.com/get/@:EXLOUD-WUP?theme=rule34" />  

**⭐ If this tool helped you, please consider giving it a star! ⭐**  

---

<img src="assets/preview.png" alt="Office Privacy and Telemetry Disabler Logo">

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Windows](https://img.shields.io/badge/Windows-10%2F11-blue.svg)

</div>

<details>
<summary>Українська (UA)</summary>

Ця програма дозволяє **призупиняти або поновлювати оновлення Windows** (як функціональні, так і якісні) за допомогою сучасного графічного інтерфейсу.

### Що робить ця утиліта?
- Призупиняє оновлення до **12 грудня 4750 року**
- Дозволяє поновити оновлення будь-коли
- Темна тема, плавна анімація, візуальні та звукові ефекти
- Відкриває параметри оновлення Windows після дії
- Підтримує Windows 10 та 11

### Як користуватись?
1. Запустіть `.exe` з правами адміністратора
2. Натисніть кнопку **⏸️ Pause Until 4750** або **▶️ Resume Updates**
3. Програма:
   - Змінить записи в реєстрі
   - Відобразить статус виконання
   - Програє звуковий сигнал
   - Відкриє `ms-settings:windowsupdate`

### Увага!
- Необхідні **права адміністратора**
- Використовуйте на власний ризик (резервна копія рекомендується)
- Може не працювати в корпоративних мережах

</details>

## English (EN)

This GUI tool allows you to **pause or resume Windows Updates** (both feature and quality updates) using a modern, dark-themed interface.

---

### 🔧 Features

- **Long-term Pause**: Pauses Windows Updates until **December 12, 4750**
- **Modern UI**: Dark-themed interface with animations and hover effects
- **One-Click Toggle**: Easily switch between pause and resume
- **Live Feedback**: Real-time success/failure status with color-coded messages
- **System Integration**: Opens Windows Update settings after each action
- **Sound Feedback**: Audio confirmation after successful operations
- **Full Compatibility**: Works with both Windows 10 and 11

---

### 🖥️ Screenshots

> UI features a modern dark design:
- Card-based layout with animations
- Color-coded status (✅ green = success, ❌ red = error)
- Large pause/resume toggle buttons

_(Add screenshots in `/screenshots` folder if available)_

---

## 🚀 How It Works

This app changes several Windows registry values to override update behavior.

### 🔑 Registry Keys Used

```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\WindowsUpdate\UX\Settings
├── PauseUpdatesExpiryTime
├── PauseFeatureUpdatesEndTime
├── PauseQualityUpdatesEndTime
├── PauseFeatureUpdatesStartTime
├── PauseQualityUpdatesStartTime
├── FlightSettingsMaxPauseDays
```

### When Pausing:
- Sets expiration date to **12/12/4750**
- Applies large pause duration (~2000 years)
- Sets current time as start time

### When Resuming:
- Clears all pause-related registry values
- Restores default update behavior

---

## 💻 System Requirements

| Component        | Requirement                      |
|------------------|----------------------------------|
| OS               | Windows 10 or 11                 |
| Architecture     | x86 / x64                        |
| Privileges       | Administrator access required    |

---

## ⚠️ Important Notes

- This tool only modifies the **Windows registry**
- **Does not disable services** or block updates permanently
- **Use with caution** – your system will not get updates while paused
- Ideal for temporary control, not indefinite disabling

---

## 🛠️ Build Instructions

### Prerequisites

- Visual Studio 2019 or later
- Windows SDK
- Linked libraries:
  - `comctl32.lib`, `dwmapi.lib`, `uxtheme.lib`, `winmm.lib`, `version.lib`

### Build Steps

1. Clone the repository
2. Open the solution in Visual Studio
3. Make sure you have:
   - `icon.ico` (32x32)
   - `icon_small.ico` (16x16)
4. Build in **Release mode**

```
WindowsUpdatePauser/
├── UnifiedWindowsUpdateControl.cpp
├── resource.h
├── Resource.rc
├── icon.ico
├── icon_small.ico
└── README.md
```

---

## 🐞 Troubleshooting

### Common Issues

| Error                      | Solution                                                  |
|----------------------------|-----------------------------------------------------------|
| Failed to apply pause      | Run as Administrator; check Update service is running     |
| Unsupported Windows Version| Use on Windows 10 or 11 only                              |
| Registry Access Denied     | Right-click → Run as Admin; verify UAC settings           |

---

## 🧩 Contributing

Contributions are welcome!

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on Windows 10/11
5. Submit a pull request

---

## 📄 License

Licensed under the [MIT License](LICENSE)

---

## ❗ Disclaimer

This software modifies the **Windows registry**. Use it **at your own risk**. The authors are **not responsible** for any system damage, loss of updates, or security issues that may result from prolonged update pausing.

---

**Version**: 1.0.0.0  
**Author**: EXLOUD  
**Compatibility**: Windows 10 / 11  
**Architecture**: x86 & x64  
