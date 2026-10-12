#pragma once

#include "options.h"

#include "engine/point.hpp"

#ifndef USE_SDL1

#include <cstdint>
#include <string_view>

struct SDL_Surface;
struct SDL_Window;

namespace devilution {

class Surface;

/**
 * @brief GPU-based shader scaler built on the SDL_GPU API.
 *
 * Replaces the SDL_Renderer presentation path when a shader filter is
 * selected in the graphics options. The game still renders into its 8-bit
 * paletted backbuffer as usual; the converted RGBA frame is uploaded to a
 * GPU texture every frame and pushed through a chain of shader passes
 * (ScaleFX: metric -> strength -> edge flags -> subpixel map -> render,
 * HQx: edge indices -> LUT blend) before being presented with letterboxing.
 */
namespace gpu_scaler {

/**
 * @brief Returns true when a shader filter is selected in the options,
 * regardless of whether the scaler initialized successfully. Callers use
 * this to keep the backbuffer setup (upscale-style) consistent.
 */
bool ScalerWanted();

/**
 * @brief Returns true when the GPU scaler owns presentation for the main
 * window (device created, window claimed and filter initialized).
 */
bool IsActive();

/**
 * @brief Returns true when the GPU scaler hit an unrecoverable error this
 * session. The caller should tear it down and revert to the regular
 * renderer; Reinit() will keep refusing until the process restarts.
 */
bool HasFailed();

/**
 * @brief Returns true when the GPU device successfully claimed the given
 * window at some point this session (even if the scaler has since been shut
 * down). On Windows the underlying HWND keeps its DXGI flip-model swapchain
 * history, which prevents other presentation APIs from creating their own
 * swapchain on it; recreate the window before handing it back to the
 * regular SDL_Renderer (see RecreateMainRenderWindow in display.cpp).
 */
bool WindowHasSwapchainHistory(SDL_Window *window);

/**
 * @brief (Re)initializes the GPU scaler for the given window.
 * @return true on success; on failure the caller must fall back to the
 * regular SDL_Renderer path.
 */
bool Reinit(SDL_Window *window);

/** @brief Releases all GPU scaler resources. Safe to call anytime. */
void Shutdown();

/**
 * @brief Presents the given RGBA frame (game resolution) with the active
 * shader filter. Blocks for vsync when configured.
 */
void PresentFrame(const SDL_Surface &frame);

/**
 * @brief Appends a timestamped line to the GPU scaler diagnostic log
 * (gpu-scaler.log next to the executable). Used by load/present phase
 * instrumentation to diagnose remote hangs and crashes.
 */
void LogPhase(std::string_view line);

/**
 * @brief Refreshes the persistent filter OSD label confirming which shader
 * filter the presentation path is actually running (e.g. "Shader Filter:
 * HQ2x (Vulkan)", "Shader Filter: Off"). Call after the renderer
 * (re)initialization has settled. The label is permanently visible in the
 * bottom-right corner of the game frame; this call only updates its text,
 * which changes when the selected filter, its availability, or the active
 * GPU backend changes. Every text change is also logged to gpu-scaler.log.
 */
void ShowFilterOsd();

/**
 * @brief The current OSD label, or an empty view before the first
 * initialization. Consumed by DrawFilterOsd.
 */
std::string_view FilterOsdText();

/**
 * @brief Draws the persistent filter OSD label (if any) into the 8-bit
 * game frame, right-aligned in the bottom-right corner, so it flows
 * through the scaler (or the regular renderer) like the rest of the
 * image. A no-op before the first ShowFilterOsd call.
 */
void DrawFilterOsd(const Surface &out);

} // namespace gpu_scaler

} // namespace devilution

#endif // !USE_SDL1
