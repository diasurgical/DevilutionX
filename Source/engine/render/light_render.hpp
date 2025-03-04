#pragma once

#include "engine/point.hpp"

namespace devilution {

class Lightmap {
public:
	explicit Lightmap(const uint8_t *outBuffer, const uint8_t *lightmapBuffer, const uint8_t *lightTables, size_t lightTableSize);

	uint8_t adjustColor(uint8_t color, uint8_t lightLevel) const
	{
		size_t offset = lightLevel * lightTableSize + color;
		return lightTables[offset];
	}

	const uint8_t *getLightingAt(const uint8_t *outLoc) const
	{
		return lightmapBuffer + (outLoc - outBuffer);
	}

	static Lightmap build(Point tilePosition, Point targetBufferPosition,
	    int viewportWidth, int viewportHeight, int rows, int columns,
	    const uint8_t *outBuffer, const uint8_t *lightTables, size_t lightTableSize);

private:
	const uint8_t *outBuffer;
	const uint8_t *lightmapBuffer;
	const uint8_t *lightTables;
	const size_t lightTableSize;
};

} // namespace devilution
