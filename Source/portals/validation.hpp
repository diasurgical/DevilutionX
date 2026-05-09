/**
 * @file portals/validation.hpp
 *
 * Interface of functions for validation of portal data.
 */
#pragma once

#include <cstdint>

#include "engine/world_tile.hpp"

namespace devilution {

bool IsPortalDeltaValid(WorldTilePosition location, uint8_t level, uint8_t levelType, bool isOnSetLevel);

} // namespace devilution
