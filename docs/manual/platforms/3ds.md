# DevilutionX (Diablo 1) for Nintendo 3DS

## Installation

Look for the latest release on the
[Releases](https://github.com/diasurgical/devilutionX/releases/latest) page.

Installation instructions can be found on the [Installing](/docs/installing.md) page.

## Usage

* Launch Diablo from your 3DS Homemenu.

## Controls

* Circle-Pad or D-Pad: move hero
* A: attack nearby enemies, talk to townspeople and merchants, pickup/place items in the inventory, OK while in main menu
* B: select spell, back while in menus
* X: pickup items, open nearby chests and doors, use item in the inventory
* Y: cast spell, delete character while in main menu
* L: use health item from belt
* R: use mana potion from belt
* Start + ↑: game menu (alt: Start + Select)
* Start + ↓: toggle automap
* Start + ←: character sheet (alt: ZL or Start + L)
* Start + →: inventory (alt: ZR or Start + R)
* Start + X: toggle zoom
* Select + D-Pad: move automap or simulate mouse
* Select + A/B/X/Y: Spell hotkeys
* C-stick: move automap or simulate mouse
* Select + ZL: quest log (alt: Start + Y)
* Select + ZR: spell book (alt: Start + B)

## Touchpad

* Single finger drag: move the mouse pointer (pointer jumps to finger)
* Single short tap: left mouse click

## Multiplayer

The 3DS currently supports cross-platform Multiplayer in TCP mode.
To play, you will need to enable Wi-Fi on the console and set up a network connection in System Settings.
For more general information about Multiplayer and how to set up your network for TCP games,
refer to the [DevilutionX Multiplayer guide](https://github.com/diasurgical/devilutionX/wiki/Multiplayer).

When playing Multiplayer, guests will experience better performance than hosts.
When playing cross-platform, it is recommended to host your game session
on another platform with more capable hardware.

## Translations

The 3DS version of the game will attempt to detect the appropriate
language based on your 3DS console's language setting.
Chinese, Korean, and Japanese users will need to download
[fonts.mpq](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/fonts.mpq)
or the text will be missing.

It is currently not recommended to use the Chinese, Korean, or Japanese translations on old 3DS models.
The game will load additional symbols into system memory as needed while you continue to play the game.
There is not enough memory in the old 3DS models to hold all the symbols in memory.
It is therefore possible to run out of memory on the console and crash the game simply by using these translations.

To change the language used by the game, you will need to
[modify diablo.ini](https://github.com/diasurgical/devilutionX/wiki/DevilutionX-diablo.ini-configuration-guide#language).
The config folder path for 3DS is the same as the data folder for the MPQs (`/3ds/devilutionx` on your SD card).

## Performance tips

New 3DS models have significantly improved hardware compared to old models.
To improve the performance of the game on old 3DS models, you will want to
[modify diablo.ini](https://github.com/diasurgical/devilutionX/wiki/DevilutionX-diablo.ini-configuration-guide).
The config folder path for 3DS is the same as the data folder for the MPQs (`/3ds/devilutionx` on your SD card).

In particular, you will see a significant performance improvement
if you set the game to Diablo's original resolution of 640x480.

```ini
[Graphics]
Width=640
Height=480
```

After making this change, if you would like the game to stretch to fit the full area of the top screen,
use `Fit to Screen=1`. If instead you do not like the display to be stretched, use `Fit to Screen=0`.

Because 3DS uses SDL1, many of the graphics settings do not apply.
The following represents the full list of applicable settings.

* Width
* Height
* Fit to Screen
* Blended Transparency
* Gamma Correction
* Color Cycling
* FPS Limiter

## Resources

* Discord: https://discord.gg/devilutionx-518540764754608128
* GitHub: https://github.com/diasurgical/devilutionX
