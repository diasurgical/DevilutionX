#pragma once

#include <cstdint>

#include <SDL.h>

#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

/**
 * @brief Loads a PCX file as a CLX sprite.
 *
 * @param handle A non-null SDL_RWops handle. Closed by this function.
 * @param numFramesOrFrameHeight Pass a positive value with the number of frames, or the frame height as a negative value.
 * @param transparentColor The PCX palette index of the transparent color.
 */
OptionalOwnedClxSpriteList PcxToClx(AssetHandle &handle, size_t fileSize, int numFramesOrFrameHeight = 1, std::optional<uint8_t> transparentColor = std::nullopt, SDL_Color *outPalette = nullptr);

} // namespace devilution
