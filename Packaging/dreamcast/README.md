# Dreamcast Build

This folder contains the Dreamcast packaging flow for DevilutionX.

## Prerequisites

- [KallistiOS](http://gamedev.allusion.net/softprj/kos/) (KOS) with kos-ports (SDL1, zlib, bzip2)
- [`mkdcdisc`](https://gitlab.com/simulant/mkdcdisc) for CDI disc image creation

No external `fmt` patch is required. The build applies a bundled SH4 `libfmt` patch automatically.

## Game Data (Required)

- You must provide your own MPQ files. Diablo data files are proprietary assets owned by Blizzard.
- Copy `DIABDAT.MPQ` to `Packaging/dreamcast/cd_root/`.
- `spawn.mpq` is optional for shareware mode.
- For extraction methods, see [Extracting MPQs from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer).

## Build

From repo root:

```sh
./Packaging/dreamcast/build.sh
```

## Output

- Intermediate ELF: `build-dreamcast/devilutionx.elf`
- Bootable CDI: `Packaging/dreamcast/devilutionx-playable.cdi`

## Notes

- `build.sh` deletes and recreates `build-dreamcast/`.
- Override tool paths with env vars if needed:
  - `KOS_BASE`
  - `KOS_ENV`
  - `MKDCDISC`
