#include "utils/palette_blending.hpp"

#include <cstdint>
#include <limits>

#include <SDL.h>

namespace devilution {

// This array is read from a lot on every frame.
// We do not use `std::array` here to improve debug build performance.
// In a debug build, `std::array` accesses are function calls.
uint8_t paletteTransparencyLookup[256][256];

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
uint16_t paletteTransparencyLookupBlack16[65536];
#endif

namespace {

struct RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

uint8_t FindBestMatchForColor(SDL_Color palette[256], RGB color, int skipFrom, int skipTo)
{
	uint8_t best;
	uint32_t bestDiff = std::numeric_limits<uint32_t>::max();
	for (int i = 0; i < 256; i++) {
		if (i >= skipFrom && i <= skipTo)
			continue;
		const int diffr = palette[i].r - color.r;
		const int diffg = palette[i].g - color.g;
		const int diffb = palette[i].b - color.b;
		const uint32_t diff = diffr * diffr + diffg * diffg + diffb * diffb;

		if (bestDiff > diff) {
			best = i;
			bestDiff = diff;
		}
	}
	return best;
}

RGB BlendColors(const SDL_Color &a, const SDL_Color &b)
{
	return RGB {
		.r = static_cast<uint8_t>((static_cast<int>(a.r) + static_cast<int>(b.r)) / 2),
		.g = static_cast<uint8_t>((static_cast<int>(a.g) + static_cast<int>(b.g)) / 2),
		.b = static_cast<uint8_t>((static_cast<int>(a.b) + static_cast<int>(b.b)) / 2),
	};
}

} // namespace

void GenerateBlendedLookupTable(SDL_Color palette[256], int skipFrom, int skipTo)
{
	for (unsigned i = 0; i < 256; i++) {
		for (unsigned j = 0; j < 256; j++) {
			if (i == j) { // No need to calculate transparency between 2 identical colors
				paletteTransparencyLookup[i][j] = j;
				continue;
			}
			if (i > j) { // Half the blends will be mirror identical ([i][j] is the same as [j][i]), so simply copy the existing combination.
				paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i];
				continue;
			}
			const uint8_t best = FindBestMatchForColor(palette, BlendColors(palette[i], palette[j]), skipFrom, skipTo);
			paletteTransparencyLookup[i][j] = best;
		}
	}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	for (unsigned i = 0; i < 256; ++i) {
		for (unsigned j = 0; j < 256; ++j) {
			const uint16_t index = i | (j << 8U);
			paletteTransparencyLookupBlack16[index] = paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8);
		}
	}
#endif
}

void UpdateBlendedLookupTableSingleColor(unsigned i, SDL_Color palette[256], int skipFrom, int skipTo)
{
	// Update blended transparency, but only for the color that was updated
	for (unsigned j = 0; j < 256; j++) {
		if (i == j) { // No need to calculate transparency between 2 identical colors
			paletteTransparencyLookup[i][j] = j;
			continue;
		}
		const uint8_t best = FindBestMatchForColor(palette, BlendColors(palette[i], palette[j]), skipFrom, skipTo);
		paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i] = best;
	}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	UpdateTransparencyLookupBlack16(i, i);
#endif
}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
void UpdateTransparencyLookupBlack16(unsigned from, unsigned to)
{
	for (unsigned i = from; i <= to; i++) {
		for (unsigned j = 0; j < 256; j++) {
			const uint16_t index = i | (j << 8U);
			const uint16_t reverseIndex = j | (i << 8U);
			paletteTransparencyLookupBlack16[index] = paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8);
			paletteTransparencyLookupBlack16[reverseIndex] = paletteTransparencyLookup[0][j] | (paletteTransparencyLookup[0][i] << 8);
		}
	}
}
#endif

} // namespace devilution
