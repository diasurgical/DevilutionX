#include "utils/palette_blending.hpp"

#include <array>
#include <cstdint>
#include <limits>

#include <SDL.h>

#include "utils/palette_kd_tree.hpp"

namespace devilution {

// This array is read from a lot on every frame.
// We do not use `std::array` here to improve debug build performance.
// In a debug build, `std::array` accesses are function calls.
uint8_t paletteTransparencyLookup[256][256];

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
uint16_t paletteTransparencyLookupBlack16[65536];
#endif

namespace {

using RGB = std::array<uint8_t, 3>;

uint8_t FindBestMatchForColor(SDL_Color palette[256], RGB color, int skipFrom, int skipTo)
{
	uint8_t best;
	uint32_t bestDiff = std::numeric_limits<uint32_t>::max();
	for (int i = 0; i < 256; i++) {
		if (i >= skipFrom && i <= skipTo)
			continue;
		const uint32_t diff = GetColorDistance(palette[i], color);
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
		static_cast<uint8_t>((static_cast<int>(a.r) + static_cast<int>(b.r)) / 2),
		static_cast<uint8_t>((static_cast<int>(a.g) + static_cast<int>(b.g)) / 2),
		static_cast<uint8_t>((static_cast<int>(a.b) + static_cast<int>(b.b)) / 2),
	};
}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
void SetPaletteTransparencyLookupBlack16(unsigned i, unsigned j)
{
	paletteTransparencyLookupBlack16[i | (j << 8U)] = paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8U);
}
#endif

} // namespace

void GenerateBlendedLookupTable(SDL_Color palette[256], int skipFrom, int skipTo)
{
	const PaletteKdTree kdTree { palette, skipFrom, skipTo };
	for (unsigned i = 0; i < 256; i++) {
		paletteTransparencyLookup[i][i] = i;
		unsigned j = 0;
		for (; j < i; j++) {
			paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i];
		}
		++j;
		for (; j < 256; j++) {
			paletteTransparencyLookup[i][j] = kdTree.findNearestNeighbor(BlendColors(palette[i], palette[j]));
		}
	}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	for (unsigned i = 0; i < 256; ++i) {
		SetPaletteTransparencyLookupBlack16(i, i);
		for (unsigned j = 0; j < i; ++j) {
			SetPaletteTransparencyLookupBlack16(i, j);
			SetPaletteTransparencyLookupBlack16(j, i);
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
			SetPaletteTransparencyLookupBlack16(i, j);
			SetPaletteTransparencyLookupBlack16(j, i);
		}
	}
}
#endif

} // namespace devilution
