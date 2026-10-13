# GPU Shader Scalers for DevilutionX

This fork of [DevilutionX](https://github.com/diasurgical/DevilutionX) adds
**GPU shader-based upscaling filters** — ScaleFX (2x/3x/4x) and HQ2x/HQ3x/HQ4x —
implemented on the **SDL3 GPU API** (`SDL_GPU`, backed by Vulkan, Direct3D 12,
or Metal). Unlike the stock SDL_Renderer path, which simply stretches the
frame, these filters reconstruct pixel art detail on the GPU at up to 4x
resolution, producing sharper edges, smoother diagonal lines, and cleaner
corner transitions than plain linear scaling.

The feature is a drop-in addition: nothing about the stock rendering path
changes unless a Shader Filter is selected, and the game falls back to the
stock renderer automatically if a compatible GPU device cannot be created.

## Requirements

- A build with `USE_SDL3=ON` (SDL3 is only available on current master; the
  1.5.5 stable branch does not support SDL3). `USE_GPU_SCALER` is a dependent
  option that defaults to ON whenever `USE_SDL3` is ON and `USE_SDL1` is OFF.
- A GPU with Vulkan, Direct3D 12, or Metal support, and a driver new enough
  for one of those APIs. On Windows 8.1+ the D3D12 shaders are compiled at
  runtime via `d3dcompiler_47.dll` (Win7 falls back to `d3dcompiler_43` and
  shader model 5.0).
- The in-game **Upscale** option must be enabled (it is by default).

## Usage

1. Start the game, open **Main Menu → Options → Graphics Options**.
2. Scroll to **Shader Filter** and pick one of:
   `Off`, `HQ2x`, `HQ3x`, `HQ4x`, `ScaleFX 2x`, `ScaleFX 3x`, `ScaleFX 4x`.
3. Confirm — the renderer is recreated and the filter applies immediately
   from the menu. The option is intentionally disabled during an ongoing game
   (it forces a renderer recreation).
4. The choice is persisted to `diablo.ini` under the `Shader Filter` key and
   restored on the next launch. If the GPU device cannot be created on the
   next start (driver update, remote desktop, VM without GPU), the game logs
   the failure once and continues with the stock renderer.
5. **Filter confirmation label**: a white dialog-size label is permanently
   shown in the bottom-right corner — e.g. `Shader Filter: HQ2x (Vulkan)`,
   `Shader Filter: Off`, or `Shader Filter: HQ3x (unavailable)` if the
   scaler could not start. The text updates whenever the filter, its
   availability, or the GPU backend changes. The label is drawn into the
   game frame itself, so it is upscaled by the active filter and each
   switch also lets you compare text sharpness between filters. Filter
   changes are also logged to `gpu-scaler.log` (`osd:` and `Reinit:` lines).

## What each filter does

**ScaleFX** (by Sp00kyFox, MIT license) is a directional, multi-pass
interpolation filter designed for pixel art. It analyzes each pixel's
neighborhood to classify edges and corners, then reconstructs subpixel
geometry. This implementation runs it as a 5-pass chain: metric extraction
(half-float), corner strength, edge flags, a 3x3 subpixel mapping pass, and a
final render pass that scales by the requested integer factor. For window
sizes that do not match the requested factor exactly, the subpixel grid
collapses gracefully (`min(fp, 2)`, matching the original CG shader behavior).

**HQx** (by Maxim Steppin, Adam Zemek and Blok; LGPL 2.1) blends pixels using
a large weight table selected from an edge-pattern index — the same LUT-based
approach libretro's slang shaders use. It runs as two passes: an edge-index
pass, then a blend pass that samples a `256 x (16 * S^2)` LUT texture (the
palette-indexed LUT data is embedded in the binary and expanded at
initialization).

## Technical notes

- The game still renders internally at its native 640x480/960x540 8-bit
  paletted surface; the palette-blit into an RGBA texture is unchanged. When
  a Shader Filter is active, the RGBA frame is uploaded to a GPU texture each
  frame and pushed through the filter chain, and the final pass letterboxes
  into the swapchain viewport (1:1 when it fits, linear sampling otherwise).
  The **Integer Scaling** option is honored by clamping the effective scale.
- Shader sources are maintained in three dialects: Vulkan GLSL (compiled to
  SPIR-V), HLSL (D3D12), and MSL (Metal). SPIR-V blobs are committed
  prebuilt (`Source/engine/render/gpu/gpu_shader_spv_prebuilt.hpp`); if
  `glslangValidator` is found at configure time, they are regenerated at
  build time instead (custom target `gpu_scaler_spv`). HLSL and MSL are
  compiled at runtime by the platform (d3dcompiler / SDL's Metal shader
  compiler).
- The SDL_GPU device, swapchain, and pipelines are created lazily and torn
  down on `Shutdown` and renderer reinitialization, so switching to
  `Off` (or losing the GPU) restores the exact stock path.

## Building

Linux (native):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SDL3=ON \
  -DDEVILUTIONX_SYSTEM_SDL3=OFF -DDEVILUTIONX_SYSTEM_LIBSODIUM=OFF \
  -DBUILD_TESTING=OFF
cmake --build build -j$(nproc)
```

Windows (MinGW-w64 cross build from Linux, fully vendored deps, static SDL3;
the bundled `CMake/platforms/mingwcc64.toolchain.cmake` understands
`CROSS_PREFIX`):

```sh
cmake -S . -B build-win -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=CMake/platforms/mingwcc64.toolchain.cmake \
  -DCROSS_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DUSE_SDL3=ON \
  -DDEVILUTIONX_SYSTEM_SDL3=OFF -DDEVILUTIONX_SYSTEM_LIBSODIUM=OFF \
  -DDEVILUTIONX_SYSTEM_ZLIB=OFF -DDEVILUTIONX_SYSTEM_BZIP2=OFF \
  -DDEVILUTIONX_SYSTEM_LIBPNG=OFF -DBUILD_TESTING=OFF
cmake --build build-win -j$(nproc)
```

On GCC 14-based toolchains, add
`-DCMAKE_C_FLAGS="-Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -Wno-error=int-conversion"`
to keep the vendored ZeroTier/miniupnpc C sources building (GCC 14 promotes
those warnings to errors; upstream CI uses MSVC so this is mingw-specific).

A GitHub Actions workflow is included (`.github/workflows/gpu-shaders.yml`)
that builds **Windows x64 and Linux x64 only** (no macOS job) on every push,
on manual dispatch, and on releases. The Windows job reproduces the verified
local cross-build exactly: MinGW-w64 cross toolchain, all dependencies
vendored, GCC14+ diagnostic relaxation for the vendored C sources, and
**fully static linking** (`-static -static-libgcc -static-libstdc++`) so the
resulting `devilutionx.exe` depends on Windows system DLLs only and launches
on a clean Windows install with no extra runtime DLLs. The workflow fails the
build if a MinGW runtime DLL (libstdc++-6.dll, libgcc_s_seh-1.dll,
libwinpthread-1.dll, ...) ever appears in the exe imports again — keep the
`CMAKE_EXE_LINKER_FLAGS` line in the workflow when editing it.

The upstream CI workflows (3ds, Android, PS4/PS5, Xbox, Vita, Switch, macOS,
stock Linux/Windows variants, etc. — 31 files) are intentionally removed from
this fork so only the GPU shader scaler pipeline runs. If your fork still has
them, delete everything under `.github/workflows/` **except**
`gpu-shaders.yml` — otherwise every push also builds stock devilutionX for a
dozen platforms. Restore them from diasurgical/DevilutionX if you rebase onto
a newer upstream and want stock CI back.

To regenerate the SPIR-V blobs by hand: install `glslang-tools` (or place the
Khronos glslang release on `PATH` / set `GLSLANG_DIR`), configure, and build
the `gpu_scaler_spv` target.

## Troubleshooting

The GPU scaler writes its own log file, **`gpu-scaler.log`, next to
`devilutionx.exe`**. It records every initialization step (backend in use,
swapchain format, shader/pipeline creation), the first presented frame, and
any failure with the underlying SDL error text. If a filter misbehaves,
include that file in your bug report.

If the GPU path hits an unrecoverable error (shader compilation failure,
pipeline creation failure, device loss), the scaler tears itself down and the
game **automatically reverts to the regular SDL_Renderer renderer for the
rest of the session** — the game keeps running, it just loses the shader
filter. The failure is recorded in `gpu-scaler.log`.

### Window claim failures ("Access is denied" / 0x80070005)

On Windows, DXGI permits only one flip-model swapchain per window and refuses
to create a new one from a different device even after the previous owner
released its swapchain. Enabling a filter therefore hands the game a
**freshly recreated window** before the D3D12 device claims it (and the same
is done in reverse when switching back to the regular renderer), so the
window switch is expected and costs a brief flicker. If the D3D12 claim still
fails, the scaler automatically retries once with the **Vulkan** backend
(shaders ship with prebuilt SPIR-V, no recompilation needed) before falling
back to the regular renderer. All of these steps are recorded in
`gpu-scaler.log`.

To get back to a clean state manually, set `Shader Filter=0` under the
`[Graphics]` section of `diablo.ini`.

### Crash while enabling a filter

The scaler logs every initialization step to `gpu-scaler.log` (`ok: shader
'hqx-index' created`, `ok: pipeline 'sfx-pass0' created`, ...). If the process
dies outright while enabling a filter, the last `ok:` line identifies the
exact call that triggered it — almost always a GPU driver crash inside its
pipeline-state compiler. The log also records fatal native exceptions
(`CRASH #1: access violation (code=0xC0000005, ...)` lines) with the faulting
address, thread name, faulting instruction pointer and a raw stack backtrace
as `devilutionx.exe+0x...` offsets, plus SDL's own messages including the GPU
adapter name and driver version — all of which should be attached to a bug
report (the offsets are resolvable to file/line against the exact binary).

The present loop also writes a `heartbeat:` line to the log every 5 seconds
(frames rendered, frames skipped, active filter), and the single-player load
path writes `ShowProgress:` phase lines. If the game appears frozen, the log
shows whether the render loop was still running (heartbeats continue) or the
main thread was stuck elsewhere (heartbeats stop after the last phase line).

If a sound effect cannot be decoded, the failure line includes the asset
size and a hex dump of its first bytes, to distinguish unsupported codecs
from corrupted or mis-read MPQ data.

### Sound effects fail with "Audio data is in unknown/unsupported/corrupt format" (fixed in this fork)

The vendored SDL3_mixer 3.2.0 shipped with a WAV decoder regression: it
treats a zero-length RIFF chunk inside a WAV file as end-of-file, then
rejects the file for having no data chunk. 1996-era WAVs — like every sound
effect in `DIABDAT.MPQ` — can contain empty metadata chunks, so menu sounds
(`sfx\items\titlemov.wav` / `titlslct.wav`) failed to load and the stock
"fatal error" dialog locked the UI. This fork carries two fixes:

1. A patch to the vendored SDL3_mixer
   (`3rdParty/SDL3_mixer/0001-WAV-decoder-skip-zero-length-chunks.patch`)
   that skips zero-length chunks instead of aborting the chunk walk.
2. A fallback in `SoundSample::SetChunk`: if SDL3_mixer still cannot load a
   sound effect, it is decoded with SDL's own `SDL_LoadWAV_IO` (the loader
   SDL2_mixer used for years) and handed to the mixer as a canonical WAV.
   A `SoundSample::SetChunk: recovered audio via SDL_LoadWAV_IO` line in
   `gpu-scaler.log` records when this path triggers.

Additionally, a sound effect that cannot be loaded for any reason no longer
raises a fatal error dialog: the game logs the failure and continues without
that sound.

As a workaround for a broken D3D12 or Vulkan driver, the backend can be
forced with an environment variable, e.g.:

    set DEVILUTIONX_GPU_BACKEND=vulkan   (cmd.exe)
    $env:DEVILUTIONX_GPU_BACKEND="vulkan"   (PowerShell)

Supported values are the SDL GPU driver names: `direct3d12` and `vulkan`
(`metal` on macOS). **Note:** on Windows, Vulkan is currently the more
robust choice — if the D3D12 shader-compilation path hits a driver crash,
the scaler automatically retries with the Vulkan backend and continues the
session. HLSL shaders are compiled at runtime with Shader Model 5.0 targets
(5.1 fallback); if `d3dcompiler_47.dll` cannot compile a shader, the exact
compiler error is recorded in `gpu-scaler.log`.

## Known limitations

- **macOS/MSL is untested.** The Metal shaders were written conservatively
  and compile-time validation on a Mac host was not possible during
  development. If Metal shader compilation fails at runtime, the game falls
  back to the stock renderer cleanly. Feedback from macOS users is needed.
- The virtual gamepad overlay is not drawn in GPU mode; touch devices
  continue to use the stock renderer path (GPU scaler is a desktop feature).
- Video playback goes through the final blit pass rather than the filter
  chain; it works, but its aspect-fit behavior differs slightly.
- With an HQx filter selected and a window size that is not an exact
  multiple of the game resolution, the filter renders at native scale and
  the result is then downscaled to fit (ScaleFX behaves the same but looks
  better at fractional scales thanks to its subpixel grid).
- The option cannot be changed while a game session is active (by design,
  since it recreates the renderer).

## Credits

- ScaleFX: Sp00kyFox (MIT) — ported from the original ReShade/CG shader.
- HQ2x/HQ3x/HQ4x: Maxim Steppin, Adam Zemek, Blok (LGPL 2.1) — LUT data
  derived from libretro's slang-shaders implementations.
- DevilutionX: Diasurgical and contributors.
