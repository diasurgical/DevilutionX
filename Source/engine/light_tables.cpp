#include "engine/light_tables.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

#include "levels/dun_tile_data.hpp"
#include "utils/is_of.hpp"

namespace devilution {

std::array<std::array<uint8_t, LightTableSize>, NumLightingLevels> LightTables;
uint8_t *FullyLitLightTable = nullptr;
uint8_t *FullyDarkLightTable = nullptr;

uint8_t LightFalloffs[NumLightRadiuses][128];
uint8_t LightConeInterpolations[8][8][16][16];

void MakeLightTable()
{
	// Generate 16 gradually darker translation tables for doing lighting
	uint8_t shade = 0;
	constexpr uint8_t Black = 0;
	constexpr uint8_t White = 255;
	for (auto &lightTable : LightTables) {
		uint8_t colorIndex = 0;
		for (const uint8_t steps : { 16, 16, 16, 16, 16, 16, 16, 16, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16 }) {
			const uint8_t shading = shade * steps / 16;
			const uint8_t shadeStart = colorIndex;
			const uint8_t shadeEnd = shadeStart + steps - 1;
			for (uint8_t step = 0; step < steps; step++) {
				if (colorIndex == Black) {
					lightTable[colorIndex++] = Black;
					continue;
				}
				int color = shadeStart + step + shading;
				if (color > shadeEnd || color == White)
					color = Black;
				lightTable[colorIndex++] = color;
			}
		}
		shade++;
	}

	LightTables[15] = {}; // Make last shade pitch black
	FullyLitLightTable = LightTables[0].data();
	FullyDarkLightTable = LightTables[LightsMax].data();

	if (leveltype == DTYPE_HELL) {
		// Blood wall lighting
		const auto shades = static_cast<int>(LightTables.size() - 1);
		for (int i = 0; i < shades; i++) {
			auto &lightTable = LightTables[i];
			constexpr int Range = 16;
			for (int j = 0; j < Range; j++) {
				uint8_t color = ((Range - 1) << 4) / shades * (shades - i) / Range * (j + 1);
				color = 1 + (color >> 4);
				int idx = j + 1;
				lightTable[idx] = color;
				idx = 31 - j;
				lightTable[idx] = color;
			}
		}
		FullyLitLightTable = nullptr; // A color map is used for the ceiling animation, so even fully lit tiles have a color map
	} else if (IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		// Make the lava fully bright
		for (auto &lightTable : LightTables)
			std::iota(lightTable.begin(), lightTable.begin() + 16, uint8_t { 0 });
		LightTables[15][0] = 0;
		std::fill_n(LightTables[15].begin() + 1, 15, 1);
		FullyDarkLightTable = nullptr; // Tiles in Hellfire levels are never completely black
	}

	// Verify that fully lit and fully dark light table optimizations are correctly enabled/disabled (nullptr = disabled)
	assert((FullyLitLightTable != nullptr) == (LightTables[0][0] == 0 && std::adjacent_find(LightTables[0].begin(), LightTables[0].end() - 1, [](auto x, auto y) { return (x + 1) != y; }) == LightTables[0].end() - 1));
	assert((FullyDarkLightTable != nullptr) == (std::all_of(LightTables[LightsMax].begin(), LightTables[LightsMax].end(), [](auto x) { return x == 0; })));

	// Generate light falloffs ranges
	const float maxDarkness = 15;
	const float maxBrightness = 0;
	for (unsigned radius = 0; radius < NumLightRadiuses; radius++) {
		const unsigned maxDistance = (radius + 1) * 8;
		for (unsigned distance = 0; distance < 128; distance++) {
			if (distance > maxDistance) {
				LightFalloffs[radius][distance] = 15;
			} else {
				const float factor = static_cast<float>(distance) / static_cast<float>(maxDistance);
				float scaled;
				if (IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
					// quardratic falloff with over exposure
					const float brightness = static_cast<float>(radius) * 1.25F;
					scaled = factor * factor * brightness + (maxDarkness - brightness);
					scaled = std::max(maxBrightness, scaled);
				} else {
					// Leaner falloff
					scaled = factor * maxDarkness;
				}
				scaled += 0.5F; // Round up
				LightFalloffs[radius][distance] = static_cast<uint8_t>(scaled);
			}
		}
	}

	// Generate the light cone interpolations
	for (int offsetY = 0; offsetY < 8; offsetY++) {
		for (int offsetX = 0; offsetX < 8; offsetX++) {
			for (int y = 0; y < 16; y++) {
				for (int x = 0; x < 16; x++) {
					const int a = ((8 * x) - offsetX);
					const int b = ((8 * y) - offsetY);
					LightConeInterpolations[offsetX][offsetY][x][y] = static_cast<uint8_t>(sqrt((a * a) + (b * b)));
				}
			}
		}
	}
}

} // namespace devilution
