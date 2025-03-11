<p align="center">
<img width="554" src="https://user-images.githubusercontent.com/204594/113575181-c946a400-961d-11eb-8347-a8829fa3830c.png">
</p>

---

<a href="README.md">English</a>
&nbsp;&nbsp;| &nbsp;&nbsp;
Русский
&nbsp;&nbsp;| &nbsp;&nbsp;
<a href="README-ua.md">Українська</a>

[![Discord Channel](https://img.shields.io/discord/518540764754608128?color=%237289DA&logo=discord&logoColor=%23FFFFFF)](https://discord.gg/devilutionx)
[![Downloads](https://img.shields.io/github/downloads/diasurgical/devilutionX/total.svg)](https://github.com/diasurgical/devilutionX/releases/latest)
[![Codecov](https://codecov.io/gh/diasurgical/devilutionX/branch/master/graph/badge.svg)](https://codecov.io/gh/diasurgical/devilutionX)

<p align="center">
<img width="838" src="https://github.com/user-attachments/assets/e3a16315-2368-4a4d-a161-69afac246c33">
</p>

<sub>*(Индикаторы здоровья врага и опыта по умолчанию отключены, но могут быть включены в [настройках игры](https://github.com/diasurgical/devilutionX/wiki/DevilutionX-diablo.ini-configuration-guide). Широкоэкранный режим также можно отключить.)*</sub>

# Что такое DevilutionX

DevilutionX - это порт Diablo и Hellfire, который стремится упростить запуск игры, одновременно улучшая движок, исправляя ошибки и некоторые дополнительные функции, улучшающие качество жизни.

Ознакомьтесь с [руководством пользователя](https://github.com/diasurgical/devilutionX/wiki), чтобы узнать о доступных функциях и о том, как ими воспользоваться.

Полный список изменений можно посмотреть в нашем [журнале изменений](CHANGELOG.md).

# Как установить

Примечание: Вам потребуется доступ к данным из оригинальной игры. Если у вас нет оригинального CD диска, вы можете [купить Diablo в GoG.com](https://www.gog.com/game/diablo) или Battle.net. В качестве альтернативы вы можете использовать "spawn.mpq" из [условно-бесплатной](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq) [[2]](http://ftp.blizzard.com/pub/demos/diablosw.exe) версии вместо "DIABDAT.MPQ", чтобы играть в условно-бесплатную часть игры.

Загрузите последнюю версию [DevilutionX](https://github.com/diasurgical/devilutionX/releases/latest) и извлеките содержимое в любое место на ваш выбор или [скомпилируйте из исходного кода](#building-from-source).

- Скопируйте `DIABDAT.MPQ` с CD диска или установочного диска Diablo (или [извлеките его из установщика GoG](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)) в папку DevilutionX.
- Чтобы запустить дополнение Diablo: Hellfire, вам также потребуется скопировать файлы `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq` и `hfvoice.mpq`.
Для получения более подробных инструкций: [Инструкции по установке](installing.md).

# Участие

Мы всегда ищем новых людей, которые могли бы помочь с [написанием кода](CONTRIBUTING.md), [документацией](https://github.com/diasurgical/devilutionX/wiki), [тестированием последних сборок](#test-builds), распространением информации или просто общением на нашем [Discord сервере](https://discord.gg/devilutionx).

# Моды

Мы надеемся стать хорошей отправной точкой для создания модов. В дополнение к полному исходному коду Devilution, мы также предоставляем инструменты для моддинга. Ознакомьтесь со списком известных [модов, основанных на DevilutionX](https://github.com/diasurgical/devilutionX/wiki/Mods).

# Тестовые сборки (билды)

Если вы хотите помочь протестировать последнюю версию разработки (обязательно создайте резервную копию своих файлов, так как билды могут содержать ошибки), вы можете получить артефакт тестовой сборки с одного из серверов сборки:

* Примечание: Для загрузки вложений вы должны быть авторизованы на GitHub!*

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

# Компиляция из кода

Хотите скомпилировать программу самостоятельно? Отлично! Просто следуйте [инструкциям по сборке](building.md).

# Благодареость

- Оригинальному проекту Devilution: [Devilution](https://github.com/diasurgical/devilution#credits)
- [Всем](https://github.com/diasurgical/devilutionX/graphs/contributors), кто работал над Devilution/DevilutionX
- [Николаю Попову](https://www.instagram.com/nikolaypopovz/) за работу над пользовательским интерфейсом и графикой
- [WiAParker](https://wiaparker.pl/projekty/diablo-hellfire/) за польскую озвучку
- И спасибо всем, кто поддерживает проект, сообщает об ошибках и помогает распространять информацию ❤️

# Юридическая информация

DevilutionX является общедоступным и выпускается под Sustainable Use License (см. [ЛИЦЕНЗИЯ](LICENSE.md)).

Исходный код в этом репозитории предназначен только для некоммерческого использования. Если вы используете исходный код, вы не имеете права взимать плату с других лиц за доступ к нему или за любые производные от него работы.

Diablo® - Авторское право © 1996 Blizzard Entertainment, Inc. Все права защищены. Diablo и Blizzard Entertainment являются торговыми марками или зарегистрированными товарными знаками Blizzard Entertainment, Inc. в США и/или других странах.

DevilutionX и кто-либо из его сопровождающих никоим образом не связаны с Blizzard Entertainment® и не одобрены ею.
