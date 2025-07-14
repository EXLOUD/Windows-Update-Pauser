# Windows Update Pause Registry Tweaks

<details>
<summary>Українська (UA)</summary>

Цей файл реєстру містить налаштування Windows, які дозволяють призупинити оновлення системи (функціональні та якісні) на дуже довгий термін — до 2750 року. Це дає змогу повністю контролювати процес оновлень або тимчасово їх відключити.

### Що робить цей файл реєстру?

- Вимикає службу Microsoft Update для запобігання автоматичним оновленням.
- Встановлює максимально можливий термін паузи для оновлень — понад 263 тисячі днів.
- Встановлює дату початку паузи — 14 липня 2025 року.
- Встановлює дату закінчення паузи — 14 липня 2750 року (фактично нескінченність).
- Охоплює функціональні та якісні оновлення.

### Як використовувати?

1. Збережіть файл з розширенням `.reg`.
2. Зробіть резервну копію реєстру або створіть точку відновлення системи.
3. Запустіть файл подвійним кліком і підтвердіть зміни.
4. Перезавантажте комп’ютер для застосування налаштувань.

### Увага!

- Використання цього файлу **блокує оновлення системи** на дуже довгий час, що може призвести до вразливостей безпеки.
- Рекомендується застосовувати лише якщо ви усвідомлюєте наслідки.
- Налаштування можна скасувати вручну або через відновлення системи.

</details>


## English (EN)

This registry file contains Windows settings that allow you to pause system updates (both feature and quality updates) for a very long time — until the year 2750. This gives you full control over the update process or temporarily disables updates.

### What does this registry file do?

- Disables the Microsoft Update service to prevent automatic updates.
- Sets the maximum possible pause duration for updates — over 263 thousand days.
- Sets the pause start date to July 14, 2025.
- Sets the pause end date to July 14, 2750 (effectively indefinite).
- Covers both feature and quality updates.

### How to use?

1. Save the file with `.reg` extension.
2. Backup your registry or create a system restore point.
3. Run the file by double-clicking and confirm the changes.
4. Restart your computer to apply the settings.

### Warning!

- Using this file **blocks system updates** for a very long time, which may cause security vulnerabilities.
- Recommended only if you fully understand the consequences.
- Settings can be reverted manually or via system restore.

---
