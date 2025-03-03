#pragma once

#include "engine/point.hpp"

namespace devilution {

void BuildLightmap(Point tilePosition, Point targetBufferPosition, int rows, int columns);
uint8_t AdjustColor(uint8_t color, uint8_t lightLevel);
const uint8_t *GetLightmapAt(const uint8_t *gbbLoc);

} // namespace devilution