#pragma once

#include <cstddef>

namespace devilution {

#define MAXLIGHTS 32
#define MAXVISION 4
#define NO_LIGHT -1

constexpr char LightsMax = 15;

/** @brief Number of supported light levels */
constexpr size_t NumLightingLevels = LightsMax + 1;

} // namespace devilution
