<p align="center">
<img width="554" src="https://user-images.githubusercontent.com/204594/113575181-c946a400-961d-11eb-8347-a8829fa3830c.png">
</p>

---

<a href="README.md">English</a>
&nbsp;&nbsp;| &nbsp;&nbsp;
<a href="README-ru.md">Русский</a>
&nbsp;&nbsp;| &nbsp;&nbsp;
Українська

[![Discord Channel](https://img.shields.io/discord/518540764754608128?color=%237289DA&logo=discord&logoColor=%23FFFFFF)](https://discord.gg/devilutionx)
[![Downloads](https://img.shields.io/github/downloads/diasurgical/devilutionX/total.svg)](https://github.com/diasurgical/devilutionX/releases/latest)
[![Codecov](https://codecov.io/gh/diasurgical/devilutionX/branch/master/graph/badge.svg)](https://codecov.io/gh/diasurgical/devilutionX)

<p align="center">
<img width="838" src="https://github.com/user-attachments/assets/bf3f7f0d-7319-45a6-9615-94537198b3da">
</p>

<sub>*(Індикатори здоров'я ворога і досвіду вимкнені за замовчуванням, але можуть бути ввімкнені в [налаштуваннях гри](https://github.com/diasurgical/devilutionX/wiki/DevilutionX-diablo.ini-configuration-guide). Широкоекранний режим також можна вимкнути.)*</sub>

# Що таке DevilutionX

DevilutionX - це порт Diablo та Hellfire, який прагне полегшити запуск гри, одночасно вдосконалюючи двигун, виправляючи помилки та деякі додаткові функції, що покращують якість життя.

Ознайомтеся з [керівництвом користувача](https://github.com/diasurgical/devilutionX/wiki), щоб дізнатися про доступні функції та як їх використовувати.

Повний список змін можна переглянути в нашому [журналі змін](CHANGELOG.MD).

# Як встановити

Примітка: вам буде необхідний доступ до даних з оригінальної гри. Якщо у вас немає оригінального CD диска, ви можете [купити Diablo в GoG.com](https://www.gog.com/game/diablo) або Battle.net. В якості альтернативи ви можете використовувати "spawn.mpq" з [умовно-безкоштовної](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq) [[2]](http://ftp.blizzard.com/pub/demos/diablosw.exe) версії замість "DIABDAT.MPQ", щоб грати в умовно-безкоштовну частину гри.

Завантажте останню версію [DevilutionX](https://github.com/diasurgical/devilutionX/releases/latest) та розпакуйте вміст у будь-яку теку на ваш вибір або [скомпілюйте з вихідного коду](#building-from-source).

- Скопіюйте `DIABDAT.MPQ` з CD диска або інсталяційного диска Diablo (або [витягніть його з інсталятора GoG](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)) в папку DevilutionX.
- Щоб запустити аддон Diablo: Hellfire, вам також потрібно скопіювати файли `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq` і `hfvoice.mpq`.
Для отримання більш детальних інструкцій: [інструкції з встановлення](installing.md).

# Участь

Ми завжди шукаємо нових людей, які могли б допомогти з [написанням коду](CONTRIBUTING.md), [документацією](https://github.com/diasurgical/devilutionX/wiki), [тестуванням останніх збірок](#test-builds), поширенням інформації або просто для спілкуванням на нашому [Discord сервері](https://discord.gg/devilutionx).

# Мода

Ми сподіваємося стати гарною відправною точкою для створення модів. Ми також надаємо інструменти для моддингу, на додаток до повного вихідного коду Devilution. Перегляньте список відомих [модів на основі DevilutionX](https://github.com/diasurgical/devilutionX/wiki/Mods).

# Тестові збірки (білди)

Якщо ви бажаєте допомогти протестувати останню версію розробки (обов'язково створіть резервну копію своїх файлів, оскільки білди можуть містити помилки), ви можете отримати артефакт тестової збірки з одного із серверів збірки:

* Примітка: для завантаження вкладень ви повинні бути авторизовані на GitHub!*

[![Linux x86_64](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64.yml?query=branch%3Amaster)
[![Linux AArch64](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_aarch64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_aarch64.yml?query=branch%3Amaster)
[![Linux x86](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86.yml?query=branch%3Amaster)
[![Linux x86_64 SDL1](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64_SDL1.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Linux_x86_64_SDL1.yml?query=branch%3Amaster)
[![macOS x86_64](https://github.com/diasurgical/devilutionX/actions/workflows/macOS_x86_64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/macOS_x86_64.yml?query=branch%3Amaster)
[![Windows MSVC x64](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MSVC_x64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MSVC_x64.yml?query=branch%3Amaster)
[![Windows MinGW x64](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x64.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x64.yml?query=branch%3Amaster)
[![Windows MinGW x86](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x86.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Windows_MinGW_x86.yml?query=branch%3Amaster)
[![Android](https://github.com/diasurgical/devilutionX/actions/workflows/Android.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/Android.yml?query=branch%3Amaster)
[![iOS](https://github.com/diasurgical/devilutionX/actions/workflows/iOS.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/iOS.yml?query=branch%3Amaster)
[![PS4](https://github.com/diasurgical/devilutionX/actions/workflows/PS4.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/PS4.yml?query=branch%3Amaster)
[![Original Xbox](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_nxdk.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_nxdk.yml?query=branch%3Amaster)
[![Xbox One/Series](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_one.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/xbox_one.yml?query=branch%3Amaster)
[![Nintendo Switch](https://github.com/diasurgical/devilutionX/actions/workflows/switch.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/switch.yml)
[![Sony PlayStation Vita](https://github.com/diasurgical/devilutionX/actions/workflows/vita.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/vita.yml)
[![Nintendo 3DS](https://github.com/diasurgical/devilutionX/actions/workflows/3ds.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/3ds.yml)
[![Amiga M68K](https://github.com/diasurgical/devilutionX/actions/workflows/amiga-m68k.yml/badge.svg)](https://github.com/diasurgical/devilutionX/actions/workflows/amiga-m68k.yml)

# Компіляція з коду

Хочете скомпілювати програму самостійно? Чудово! Просто дотримуйтесь [інструкцій по збірці](building.md).

# Подяка

- Оригінальному проекту Devilution: [Devilution](https://github.com/diasurgical/devilution#credits)
- [Всім](https://github.com/diasurgical/devilutionX/graphs/contributors), хто працював над Devilution/DevilutionX
- [Миколаю Попову](https://www.instagram.com/nikolaypopovz/) за роботу над інтерфейсом користувача та графікою
- [WiAParker](https://wiaparker.pl/projekty/diablo-hellfire/) за польську озвучку
- І дякую всім, хто підтримує проект, повідомляє про помилки та допомагає поширювати інформацію ❤️

# Юридична інформація

DevilutionX є загальнодоступним і випускається під Sustainable Use License (див. [ЛІЦЕНЗІЯ](LICENSE.md)).

Вихідний код у цьому сховищі призначений лише для некомерційного використання. Якщо ви використовуєте вихідний код, ви не маєте права стягувати плату з інших осіб за доступ до нього або за будь-які похідні від нього роботи.

Diablo® - авторське право © 1996 Blizzard Entertainment, Inc. Всі права захищені. Diablo і Blizzard Entertainment є торговими марками або зареєстрованими товарними знаками Blizzard Entertainment, Inc. у США та/або інших країнах.

DevilutionX і будь-хто з його супроводжуючих жодним чином не пов'язані з Blizzard Entertainment® і не схвалені нею.
