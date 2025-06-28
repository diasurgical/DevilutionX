#include "utils/palette_blending.hpp"

#include <algorithm>
#include <array>
#include <iostream>

#include <SDL.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "utils/str_cat.hpp"

void PrintTo(const SDL_Color &color, std::ostream *os)
{
	*os << "("
	    << static_cast<int>(color.r) << ", "
	    << static_cast<int>(color.g) << ", "
	    << static_cast<int>(color.b) << ")";
}

namespace devilution {

std::array<SDL_Color, 256> logical_palette;

namespace {

MATCHER_P3(ColorIs, r, g, b,
    StrCat(negation ? "isn't" : "is", " (", r, ", ", g, ", ", b, ")"))
{
	return arg.r == r && arg.g == g && arg.b == b;
}

void GeneratePalette(SDL_Color palette[256])
{
	for (unsigned j = 0; j < 4; ++j) {
		for (unsigned i = 0; i < 64; ++i) {
			palette[j * 64 + i].r = i * std::max(j, 1U);
			palette[j * 64 + i].g = i * j;
			palette[j * 64 + i].b = i * 2;
#ifndef USE_SDL1
			palette[j * 64 + i].a = SDL_ALPHA_OPAQUE;
#endif
		}
	}
}

TEST(GenerateBlendedLookupTableTest, BasicTest)
{
	SDL_Color *palette = logical_palette.data();
	GeneratePalette(palette);

	GenerateBlendedLookupTable();

	EXPECT_EQ(paletteTransparencyLookup[100][100], 100);

	EXPECT_THAT(palette[17], ColorIs(17, 0, 34));
	EXPECT_THAT(palette[150], ColorIs(44, 44, 44));
	EXPECT_THAT(palette[86], ColorIs(22, 22, 44));
	EXPECT_EQ(paletteTransparencyLookup[17][150], 86);
	EXPECT_EQ(paletteTransparencyLookup[150][17], 86);

	EXPECT_THAT(palette[27], ColorIs(27, 0, 54));
	EXPECT_THAT(palette[130], ColorIs(4, 4, 4));
	EXPECT_THAT(palette[15], ColorIs(15, 0, 30));
	EXPECT_EQ(paletteTransparencyLookup[27][130], 15);
	EXPECT_EQ(paletteTransparencyLookup[130][27], 15);

	EXPECT_THAT(palette[0], ColorIs(0, 0, 0));
	EXPECT_THAT(palette[100], ColorIs(36, 36, 72));
	EXPECT_THAT(palette[82], ColorIs(18, 18, 36));
	EXPECT_EQ(paletteTransparencyLookup[0][100], 82);
	EXPECT_EQ(paletteTransparencyLookup[100][0], 82);

	EXPECT_THAT(palette[0], ColorIs(0, 0, 0));
	EXPECT_THAT(palette[200], ColorIs(24, 24, 16));
	EXPECT_THAT(palette[196], ColorIs(12, 12, 8));
	EXPECT_EQ(paletteTransparencyLookup[0][200], 196);
	EXPECT_EQ(paletteTransparencyLookup[200][0], 196);

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	EXPECT_EQ(paletteTransparencyLookupBlack16[100 | (100 << 8)], 82 | (82 << 8));
	EXPECT_EQ(paletteTransparencyLookupBlack16[100 | (200 << 8)], 82 | (196 << 8));
#endif
}

} // namespace
} // namespace devilution
