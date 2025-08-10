#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/lighting_defs.hpp"
#include "engine/point.hpp"
#include "levels/gendung_defs.hpp"

namespace devilution {

class Lightmap {
public:
	explicit Lightmap(const uint8_t *outBuffer, std::span<const uint8_t> lightmapBuffer, uint16_t pitch,
	    std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables,
	    const uint8_t *fullyLitLightTable, const uint8_t *fullyDarkLightTable)
	    : Lightmap(outBuffer, pitch, lightmapBuffer, pitch, lightTables, fullyLitLightTable, fullyDarkLightTable)
	{
	}

	explicit Lightmap(const uint8_t *outBuffer, uint16_t outPitch,
	    std::span<const uint8_t> lightmapBuffer, uint16_t lightmapPitch,
	    std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables,
	    const uint8_t *fullyLitLightTable, const uint8_t *fullyDarkLightTable);

	[[nodiscard]] uint8_t adjustColor(uint8_t color, uint8_t lightLevel) const
	{
		return lightTables[lightLevel][color];
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

	[[nodiscard]] bool isFullyLitLightTable(const uint8_t *lightTable) const { return lightTable == fullyLitLightTable_; }
	[[nodiscard]] bool isFullyDarkLightTable(const uint8_t *lightTable) const { return lightTable == fullyDarkLightTable_; }

	static Lightmap build(bool perPixelLighting, Point tilePosition, Point targetBufferPosition,
	    int viewportWidth, int viewportHeight, int rows, int columns,
	    const uint8_t *outBuffer, uint16_t outPitch,
	    std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables,
	    const uint8_t *fullyLitLightTable, const uint8_t *fullyDarkLightTable,
	    const uint8_t tileLights[MAXDUNX][MAXDUNY],
	    uint_fast8_t microTileLen);

	static Lightmap bleedUp(bool perPixelLighting, const Lightmap &source, Point targetBufferPosition, std::span<uint8_t> lightmapBuffer);

private:
	const uint8_t *outBuffer;
	const uint16_t outPitch;

	std::span<const uint8_t> lightmapBuffer;
	const uint16_t lightmapPitch;

	std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables;
	const uint8_t *fullyLitLightTable_;
	const uint8_t *fullyDarkLightTable_;
};

} // namespace devilution
