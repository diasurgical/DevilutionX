#pragma once

#include <array>
#include <cstdint>

#include <SDL_version.h>

#if SDL_VERSION_ATLEAST(2, 0, 0)
#include <SDL_surface.h>
#else
#include <SDL_video.h>
#endif

namespace devilution {

/**
 * @brief Bilinear 32-bit scaling.
 * Requires `src` and `dst` to have the same pixel format (ARGB8888 or RGBA8888).
 */
void BilinearScale32(SDL_Surface *src, SDL_Surface *dst);

/**
 * @brief Streamlined bilinear downscaling using blended transparency table.
 * Requires `src` and `dst` to have the same pixel format (INDEX8).
 */
void BilinearDownscaleByHalf8(const SDL_Surface *src, const Uint8 paletteBlendingTable[256][256], SDL_Surface *dst, uint8_t transparentIndex);

} // namespace devilution
