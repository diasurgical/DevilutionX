#pragma once

#include <span>

#include "engine/point.hpp"

namespace devilution {

class Lightmap {
public:
	explicit Lightmap(const uint8_t *outBuffer, std::span<const uint8_t> lightmapBuffer, uint16_t pitch, const uint8_t *lightTables, size_t lightTableSize)
	    : Lightmap(outBuffer, pitch, lightmapBuffer, pitch, lightTables, lightTableSize)
	{
	}

	explicit Lightmap(const uint8_t *outBuffer, uint16_t outPitch,
	    std::span<const uint8_t> lightmapBuffer, uint16_t lightmapPitch,
	    const uint8_t *lightTables, size_t lightTableSize);

	uint8_t adjustColor(uint8_t color, uint8_t lightLevel) const
	{
		size_t offset = lightLevel * lightTableSize + color;
		return lightTables[offset];
	}

	const uint8_t *getLightingAt(const uint8_t *outLoc) const
	{
		const ptrdiff_t outDist = outLoc - outBuffer;
		const ptrdiff_t rowOffset = outDist % outPitch;

		if (outDist < 0) {
			// In order to support "bleed up" for wall tiles,
			// reuse the first row whenever outLoc is out of bounds
			const int modOffset = rowOffset < 0 ? outPitch : 0;
			return lightmapBuffer.data() + rowOffset + modOffset;
		}

		const ptrdiff_t row = outDist / outPitch;
		return lightmapBuffer.data() + row * lightmapPitch + rowOffset;
	}

	static Lightmap build(Point tilePosition, Point targetBufferPosition,
	    int viewportWidth, int viewportHeight, int rows, int columns,
	    const uint8_t *outBuffer, uint16_t outPitch,
	    const uint8_t *lightTables, size_t lightTableSize);

	static Lightmap bleedUp(const Lightmap &source, Point targetBufferPosition, std::span<uint8_t> lightmapBuffer);

private:
	const uint8_t *outBuffer;
	const uint16_t outPitch;

	std::span<const uint8_t> lightmapBuffer;
	const uint16_t lightmapPitch;

	const uint8_t *lightTables;
	const size_t lightTableSize;
};

} // namespace devilution
