#pragma once

// Guarded runtime shader compilation with d3dcompiler, shared by the GPU
// scaler's baked-in D3D12 shaders and the runtime libretro slang preset
// loader. Includes the vectored exception handler that contains faults
// inside d3dcompiler and logs fatal GPU-path crashes for offline
// diagnosis; see gpu_d3dcompile.cpp for the full design notes.

#ifdef _WIN32

#include <cstdint>
#include <vector>

namespace devilution {
namespace gpu_d3dcompile {

/**
 * @brief Loads the system d3dcompiler by full path and keeps it loaded for
 * the whole process. Must run before any renderer or GPU device is created
 * so every later bare-name load of d3dcompiler_47.dll resolves to the
 * genuine system copy instead of a stale DLL shipped next to the game.
 */
void PreloadSystemCompiler();

/**
 * @brief Installs the vectored exception handler (idempotent). The handler
 * converts faults inside guarded D3DCompile calls into compile failures via
 * longjmp, and writes fatal native exceptions to the GPU scaler diagnostic
 * log before they kill the process.
 */
void InstallCrashGuard();

/**
 * @brief Compiles HLSL source with the guarded D3DCompile.
 * @return the DXBC blob, or an empty vector on failure (reason logged).
 */
std::vector<uint8_t> Compile(const char *source, const char *target);

} // namespace gpu_d3dcompile
} // namespace devilution

#else // _WIN32

// Non-Windows stubs so callers stay unconditional.
#include <cstdint>
#include <vector>

namespace devilution {
namespace gpu_d3dcompile {

inline void PreloadSystemCompiler()
{
}
inline void InstallCrashGuard()
{
}
inline std::vector<uint8_t> Compile(const char *, const char *)
{
	return {};
}

} // namespace gpu_d3dcompile
} // namespace devilution

#endif // _WIN32
