#pragma once

#include <cstddef>

namespace devilution {

#define MAXLIGHTS 32
#define MAXVISION 4
#define NO_LIGHT -1

constexpr char LightsMax = 15;

/** @brief A light table maps palette indices, so its size is the same as the palette size. */
constexpr size_t LightTableSize = 256;

/** @brief Number of supported light levels */
constexpr size_t NumLightingLevels = LightsMax + 1;

} // namespace devilution
