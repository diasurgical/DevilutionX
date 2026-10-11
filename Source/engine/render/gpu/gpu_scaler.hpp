#pragma once

#include "options.h"

#ifndef USE_SDL1

#include <cstdint>

struct SDL_Surface;
struct SDL_Window;

namespace devilution {

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

} // namespace gpu_scaler

} // namespace devilution

#endif // !USE_SDL1
