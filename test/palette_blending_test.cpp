#include "utils/palette_blending.hpp"

#include <algorithm>
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
	SDL_Color palette[256];
	GeneratePalette(palette);

	GenerateBlendedLookupTable(palette, /*skipFrom=*/-1, /*skipTo=*/-1);

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
}

} // namespace
} // namespace devilution
