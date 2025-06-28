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

extern std::array<SDL_Color, 256> logical_palette;

namespace {

PaletteKdTree CurrentPaletteKdTree;

using RGB = std::array<uint8_t, 3>;

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

void GenerateBlendedLookupTable(int skipFrom, int skipTo)
{
	const SDL_Color *palette = logical_palette.data();
	CurrentPaletteKdTree = PaletteKdTree { palette, skipFrom, skipTo };
	for (unsigned i = 0; i < 256; i++) {
		paletteTransparencyLookup[i][i] = i;
		unsigned j = 0;
		for (; j < i; j++) {
			paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i];
		}
		++j;
		for (; j < 256; j++) {
			paletteTransparencyLookup[i][j] = CurrentPaletteKdTree.findNearestNeighbor(BlendColors(palette[i], palette[j]));
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

void UpdateBlendedLookupTableSingleColor(unsigned i)
{
	const SDL_Color *palette = logical_palette.data();
	for (unsigned j = 0; j < 256; j++) {
		if (i == j) { // No need to calculate transparency between 2 identical colors
			paletteTransparencyLookup[i][j] = j;
			continue;
		}
		const uint8_t best = CurrentPaletteKdTree.findNearestNeighbor(BlendColors(palette[i], palette[j]));
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
