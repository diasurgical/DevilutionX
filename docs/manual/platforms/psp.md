# DevilutionX (Diablo 1) for PlayStation Portable

## Installation

1. Download and unzip [devilutionx-psp.zip](https://github.com/diasurgical/devilutionX/releases/latest/download/devilutionx-psp.zip).
2. Copy `EBOOT.PBP` and the `assets` and `mods` folders to `PSP/GAME/DevilutionX/` on the Memory Stick.
3. Copy `DIABDAT.MPQ` from your CD (or GoG install folder) to `PSP/GAME/DevilutionX/`.
   For Hellfire, also copy `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq`, and `hfvoice.mpq`.

The settings file (`diablo.ini`) and save games are stored in the same folder.

## Usage

Launch DevilutionX from the Game menu of the PSP.

## Resolution

The Resolution setting offers two logical resolutions, both scaled to fit the PSP's 480×272 screen:

- 640×480 (4:3)
- 848×480 (Widescreen): shows more of the game world instead of stretching the 4:3 image

## Known issues

- Only the PSP-3000 has been tested on real hardware.
- The shareware `spawn.mpq` from the [DevilutionX assets release](https://github.com/diasurgical/devilutionx-assets/releases/latest) uses MP3 audio, which currently powers off the PSP.
  Use the original WAV-audio `spawn.mpq` from Blizzard's [shareware installer](http://ftp.blizzard.com/pub/demos/diablosw.exe) instead.
- Multiplayer is not supported.

# Building from Source

Install the [PSPDEV](https://pspdev.github.io/) toolchain, then run:

```bash
cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE=${PSPDEV}/psp/share/pspdev.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(getconf _NPROCESSORS_ONLN)
```

`EBOOT.PBP` and the `assets` and `mods` folders will be generated in the build folder.
