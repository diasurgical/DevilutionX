# What is Diablo Access

Diablo Access is a DevilutionX-based mod focused on accessibility and quality-of-life improvements.

Upstream project: https://github.com/diasurgical/devilutionX

For a full list of changes, see our [changelog](docs/CHANGELOG.md).

# Features

- Screen reader / TTS integration (Windows: NVDA/JAWS via Tolk; Linux: speech-dispatcher).
- Proximity audio cues (items, monsters, doors/chests, interactable targets).
- Spoken announcements for many UI elements and interactions.
- Tracker navigation that speaks the tracked target and directions to reach it.
- Low HP warning sound that starts at 50% HP and speeds up every 10% down to 0% (stops on death).

# Keybinds (defaults)

Keybinds are configurable, but these are the defaults most players will use:

## Navigation / tracking

- `T` - cycle tracker target (items / chests / doors / shrines / objects / breakables / monsters).
- `N` - tracker directions to the nearest target (speaks target name + directions).
- `Shift`+`N` - cycle to the next target in the current tracker category (speaks name only; duplicates get ordinal numbers).
- `Ctrl`+`N` - clear the tracker target.
- `H` - speak nearest unexplored space.
- `E` - speak nearest exit (hold `Shift` for quest entrances; in town: `Ctrl`+`E` cycles dungeon entrances).
- `P` - speak nearest open town portal (town only).
- `,` - speak nearest stairs up.
- `.` - speak nearest stairs down.
- `L` - speak current dungeon + floor.
- `Z` - speak player health percentage.
- `X` - speak experience remaining to next level.

## Keyboard controls

- Arrow keys - walk one tile (north/south/east/west).
- `A` - primary action (attack / talk / lift items).
- `D` - secondary action (open chests / doors / pick up items).
- `W` - spell action (cast active spell).

## Town NPCs

- `F4` - list town NPCs.
- `PageUp` / `PageDown` - select previous / next town NPC.
- `End` - speak selected town NPC.
- `Home` - walk to selected town NPC (town only).

# How to Install

Note: You'll need access to the data from the original game. If you don't have an original CD, you can [buy Diablo from GoG.com](https://www.gog.com/game/diablo) or Battle.net. Alternatively, you can use `spawn.mpq` from the [shareware](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/spawn.mpq) [[2]](http://ftp.blizzard.com/pub/demos/diablosw.exe) version, in place of `DIABDAT.MPQ`, to play the shareware portion of the game.

Download the latest [Diablo Access release](https://github.com/mojsior/diablo-access/releases/latest) and extract the contents to a location of your choosing or [build from source](#building-from-source).

- Copy `DIABDAT.MPQ` from the CD or Diablo installation (or [extract it from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)) to the Diablo Access folder.
- To run the Diablo: Hellfire expansion, you will also need to copy `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq`, and `hfvoice.mpq`.

For more detailed instructions: [Installation Instructions](./docs/installing.md).

# Contributing

We are always looking for more people to help with [coding](docs/CONTRIBUTING.md), [documentation](https://github.com/diasurgical/devilutionX/wiki), [testing the latest builds](#test-builds), spreading the word, or simply just hanging out on our [Discord server](https://discord.gg/devilutionx).

# Mods

We hope to provide a good starting point for mods. In addition to the full Devilution source code, we also provide modding tools. Check out the list of known [mods based on DevilutionX](https://github.com/diasurgical/devilutionX/wiki/Mods).

# Test Builds

If you want to help test the latest development version (make sure to back up your files, as these may contain bugs), you can fetch the test build artifact from one of the build servers:

*Note: You must be logged into GitHub to download the attachments!*

[![Linux x86_64](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86_64.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86_64.yml?query=branch%3Amaster)
[![Linux AArch64](https://github.com/mojsior/diablo-access/actions/workflows/Linux_aarch64.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Linux_aarch64.yml?query=branch%3Amaster)
[![Linux x86](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86.yml?query=branch%3Amaster)
[![Linux x86_64 SDL1](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86_64_SDL1.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Linux_x86_64_SDL1.yml?query=branch%3Amaster)
[![macOS x86_64](https://github.com/mojsior/diablo-access/actions/workflows/macOS_x86_64.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/macOS_x86_64.yml?query=branch%3Amaster)
[![Windows MSVC x64](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MSVC_x64.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MSVC_x64.yml?query=branch%3Amaster)
[![Windows MinGW x64](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MinGW_x64.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MinGW_x64.yml?query=branch%3Amaster)
[![Windows MinGW x86](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MinGW_x86.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Windows_MinGW_x86.yml?query=branch%3Amaster)
[![Android](https://github.com/mojsior/diablo-access/actions/workflows/Android.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/Android.yml?query=branch%3Amaster)
[![iOS](https://github.com/mojsior/diablo-access/actions/workflows/iOS.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/iOS.yml?query=branch%3Amaster)
[![PS4](https://github.com/mojsior/diablo-access/actions/workflows/PS4.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/PS4.yml?query=branch%3Amaster)
[![Original Xbox](https://github.com/mojsior/diablo-access/actions/workflows/xbox_nxdk.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/xbox_nxdk.yml?query=branch%3Amaster)
[![Xbox One/Series](https://github.com/mojsior/diablo-access/actions/workflows/xbox_one.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/xbox_one.yml?query=branch%3Amaster)
[![Nintendo Switch](https://github.com/mojsior/diablo-access/actions/workflows/switch.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/switch.yml)
[![Sony PlayStation Vita](https://github.com/mojsior/diablo-access/actions/workflows/vita.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/vita.yml)
[![Nintendo 3DS](https://github.com/mojsior/diablo-access/actions/workflows/3ds.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/3ds.yml)
[![Amiga M68K](https://github.com/mojsior/diablo-access/actions/workflows/amiga-m68k.yml/badge.svg)](https://github.com/mojsior/diablo-access/actions/workflows/amiga-m68k.yml)

# Building from Source

Want to compile the program by yourself? Great! Simply follow the [build instructions](./docs/building.md).

# Credits

- The original Devilution project: [Devilution](https://github.com/diasurgical/devilution#credits)
- [Everyone](https://github.com/diasurgical/devilutionX/graphs/contributors) who worked on Devilution/DevilutionX
- [Nikolay Popov](https://www.instagram.com/nikolaypopovz/) for UI and graphics
- [WiAParker](https://wiaparker.pl/projekty/diablo-hellfire/) for the Polish voice pack
- And thanks to all who support the project, report bugs, and help spread the word ❤️

# Legal

DevilutionX is made publicly available and released under the Sustainable Use License (see [LICENSE](LICENSE.md)).

The source code in this repository is for non-commercial use only. If you use the source code, you may not charge others for access to it or any derivative work thereof.

Diablo® - Copyright © 1996 Blizzard Entertainment, Inc. All rights reserved. Diablo and Blizzard Entertainment are trademarks or registered trademarks of Blizzard Entertainment, Inc. in the U.S. and/or other countries.

DevilutionX and any of its maintainers are in no way associated with or endorsed by Blizzard Entertainment®.
