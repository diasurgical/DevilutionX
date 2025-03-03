#pragma once

#include "engine/point.hpp"

namespace devilution {

class Lightmap {
public:
	Lightmap(uint8_t *outBuffer);

	uint8_t adjustColor(uint8_t color, uint8_t lightLevel) const
	{
		size_t offset = lightLevel * lightTableSize + color;
		return lightTables[offset];
	}

	const uint8_t *getLightingAt(const uint8_t *outLoc) const
	{
		return lightmapBuffer + (outLoc - outBuffer);
	}

private:
	uint8_t *outBuffer;
	uint8_t *lightmapBuffer;
	uint8_t *lightTables;
	size_t lightTableSize;
};

void BuildLightmap(Point tilePosition, Point targetBufferPosition, int rows, int columns);

} // namespace devilution