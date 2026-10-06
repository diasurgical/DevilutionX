/**
 * @file light_tables.hpp
 *
 * Declarations for light table data and generation.
 */
#pragma once

#include <array>
#include <cstdint>

#include "engine/lighting_defs.hpp"
#include "utils/attributes.h"

namespace devilution {

constexpr size_t NumLightRadiuses = 16;

extern DVL_API_FOR_TEST std::array<std::array<uint8_t, LightTableSize>, NumLightingLevels> LightTables;
/** @brief Contains a pointer to a light table that is fully lit (no color mapping is required). Can be null in hell. */
extern DVL_API_FOR_TEST uint8_t *FullyLitLightTable;
/** @brief Contains a pointer to a light table that is fully dark (every color result to 0/black). Can be null in hellfire levels. */
extern DVL_API_FOR_TEST uint8_t *FullyDarkLightTable;
extern uint8_t LightFalloffs[NumLightRadiuses][128];
extern uint8_t LightConeInterpolations[8][8][16][16];

void MakeLightTable();

} // namespace devilution
