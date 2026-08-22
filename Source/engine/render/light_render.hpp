#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/lighting_defs.hpp"
#include "engine/point.hpp"
#include "engine/render/lighting_mode.hpp"
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
	    const uint8_t *fullyLitLightTable, const uint8_t *fullyDarkLightTable,
	    bool perPixel = false);

	[[nodiscard]] bool isPerPixel() const { return perPixel_; }

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
			return lightmapBuffer.data() + std::min<ptrdiff_t>(rowOffset + modOffset, lightmapPitch - 1);
		}

		const ptrdiff_t row = outDist / outPitch;
		const ptrdiff_t col = std::min<ptrdiff_t>(rowOffset, lightmapPitch - 1);
		return lightmapBuffer.data() + std::min<ptrdiff_t>(row * lightmapPitch + col, static_cast<ptrdiff_t>(lightmapBuffer.size()) - 1);
	}

	[[nodiscard]] bool isFullyLitLightTable(const uint8_t *lightTable) const { return lightTable == fullyLitLightTable_; }
	[[nodiscard]] bool isFullyDarkLightTable(const uint8_t *lightTable) const { return lightTable == fullyDarkLightTable_; }

private:
	friend Lightmap LightmapBuild(LightingMode, Point, Point, int, int, int, int,
	    const uint8_t *, uint16_t,
	    std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels>,
	    const uint8_t *, const uint8_t *,
	    const uint8_t (*)[MAXDUNY], uint_fast8_t);
	friend Lightmap LightmapBleedUp(const Lightmap &, Point, std::span<uint8_t>);
	const uint8_t *outBuffer;
	const uint16_t outPitch;

	std::span<const uint8_t> lightmapBuffer;
	const uint16_t lightmapPitch;

	std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables;
	const uint8_t *fullyLitLightTable_;
	const uint8_t *fullyDarkLightTable_;
	bool perPixel_ = false;
};

} // namespace devilution
