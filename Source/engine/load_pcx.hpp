#pragma once

#include <cstdint>
#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#else
#include <SDL.h>
#endif

#include "engine/clx_sprite.hpp"

#ifdef UNPACKED_MPQS
#define DEVILUTIONX_PCX_EXT ".clx"
#else
#define DEVILUTIONX_PCX_EXT ".pcx"
#endif

namespace devilution {

/**
 * @brief Loads a PCX file as a CLX sprite list.
 *
 * @param filename
 * @param numFramesOrFrameHeight Pass a positive value with the number of frames, or the frame height as a negative value.
 * @param transparentColor
 * @param outPalette
 * @return OptionalOwnedClxSpriteList
 */
OptionalOwnedClxSpriteList LoadPcxSpriteList(const char *filename, int numFramesOrFrameHeight, std::optional<uint8_t> transparentColor = std::nullopt, SDL_Color *outPalette = nullptr, bool logError = true);

/**
 * @brief Loads a PCX file as a CLX sprite list with a single sprite.
 *
 * @param filename
 * @param transparentColor
 * @param outPalette
 * @return OptionalOwnedClxSpriteList
 */
inline OptionalOwnedClxSpriteList LoadPcx(const char *filename, std::optional<uint8_t> transparentColor = std::nullopt, SDL_Color *outPalette = nullptr, bool logError = true)
{
	return LoadPcxSpriteList(filename, 1, transparentColor, outPalette, logError);
}

} // namespace devilution
