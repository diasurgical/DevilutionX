#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/shadow_fix.hpp"
#include "engine/shadow_mask.hpp"
#include "engine/shadow_rule.hpp"
#include "engine/surface.hpp"
#include "utils/clx_decode.hpp"
#include "utils/clx_encode.hpp"
#include "utils/clx_shadow.hpp"
#include "utils/endian_write.hpp"
#include "utils/palette_blending.hpp"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

/**
 * A sprite as rows of characters, top row first:
 * '.' is transparent, '#' is black (palette index 0), any other character is that palette index.
 */
using Picture = std::vector<std::string>;

void AppendSprite(const Picture &picture, std::vector<uint8_t> &out)
{
	const size_t headerPos = out.size();
	out.resize(out.size() + ClxFrameHeaderSize);
	WriteLE16(&out[headerPos], ClxFrameHeaderSize);
	WriteLE16(&out[headerPos + 2], static_cast<uint16_t>(picture[0].size()));
	WriteLE16(&out[headerPos + 4], static_cast<uint16_t>(picture.size()));
	for (size_t i = picture.size(); i-- > 0;) {
		for (const char c : picture[i]) {
			if (c == '.') {
				AppendClxTransparentRun(1, out);
			} else {
				const uint8_t color = c == '#' ? 0 : static_cast<uint8_t>(c);
				AppendClxPixelsRun(&color, 1, out);
			}
		}
	}
}

void AppendList(const std::vector<Picture> &pictures, std::vector<uint8_t> &out)
{
	const size_t begin = out.size();
	out.resize(out.size() + 4 * (2 + pictures.size()));
	WriteLE32(&out[begin], static_cast<uint32_t>(pictures.size()));
	for (size_t i = 0; i < pictures.size(); ++i) {
		WriteLE32(&out[begin + 4 * (1 + i)], static_cast<uint32_t>(out.size() - begin));
		AppendSprite(pictures[i], out);
	}
	WriteLE32(&out[begin + 4 * (1 + pictures.size())], static_cast<uint32_t>(out.size() - begin));
}

OwnedClxSpriteListOrSheet MakeList(const std::vector<Picture> &pictures)
{
	std::vector<uint8_t> data;
	AppendList(pictures, data);
	std::unique_ptr<uint8_t[]> buffer { new uint8_t[data.size()] };
	std::copy(data.begin(), data.end(), buffer.get());
	return OwnedClxSpriteListOrSheet { std::move(buffer), 0 };
}

OwnedClxSpriteListOrSheet MakeSheet(const std::vector<std::vector<Picture>> &lists)
{
	std::vector<uint8_t> data(lists.size() * 4);
	for (size_t i = 0; i < lists.size(); ++i) {
		WriteLE32(&data[i * 4], static_cast<uint32_t>(data.size()));
		AppendList(lists[i], data);
	}
	std::unique_ptr<uint8_t[]> buffer { new uint8_t[data.size()] };
	std::copy(data.begin(), data.end(), buffer.get());
	return OwnedClxSpriteListOrSheet { std::move(buffer), static_cast<uint16_t>(lists.size()) };
}

Picture ToPicture(ClxSprite sprite)
{
	const size_t width = sprite.width();
	Picture result(sprite.height(), std::string(width, '.'));
	size_t pos = 0;
	const auto put = [&](uint8_t color) {
		result[sprite.height() - 1 - pos / width][pos % width] = color == 0 ? '#' : static_cast<char>(color);
		++pos;
	};
	const uint8_t *src = sprite.pixelData();
	const uint8_t *end = src + sprite.pixelDataSize();
	while (src < end) {
		const uint8_t control = *src++;
		if (!IsClxOpaque(control)) {
			pos += control;
		} else if (IsClxOpaqueFill(control)) {
			const uint8_t color = *src++;
			for (unsigned i = GetClxOpaqueFillWidth(control); i != 0; --i)
				put(color);
		} else {
			for (unsigned i = GetClxOpaquePixelsWidth(control); i != 0; --i)
				put(*src++);
		}
	}
	return result;
}

/** The mask drawn as '#' on a transparent picture of the given size. */
Picture ToPicture(ShadowMask mask, size_t width, size_t height)
{
	Picture result(height, std::string(width, '.'));
	mask.forEachSpan([&](uint16_t rowFromBottom, ShadowSpan span) {
		for (unsigned x = span.x; x < static_cast<unsigned>(span.x) + span.width; ++x)
			result[height - 1 - rowFromBottom][x] = '#';
	});
	return result;
}

constexpr ShadowRule FirstFrame { ShadowRuleType::FirstFrame, 0 };
constexpr ShadowRule EachFrame { ShadowRuleType::EachFrame, 0 };

// A 12 pixel tall bar with its baseline on row 11.
// Rows 9..11 and 6..8 shadow the bar itself, rows 3..5 shadow (5, 7) and rows 0..2 shadow (4, 6).
const Picture Bar = {
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"....#.a.",
	".....#a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
};
const Picture BarWithoutShadow = {
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
	"......a.",
};
const Picture BarShadow = {
	"........",
	"........",
	"........",
	"........",
	"........",
	"........",
	"....#...",
	".....#..",
	"......#.",
	"......#.",
	"........",
	"........",
};

TEST(ClxShadowTest, ExtractsShadow)
{
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());

	ASSERT_FALSE(result->isSheet());
	const ClxSpriteList list = result->list();
	ASSERT_EQ(list.numSprites(), 1U);
	EXPECT_EQ(list[0].width(), 8);
	EXPECT_EQ(list[0].height(), 12);
	EXPECT_EQ(ToPicture(list[0]), BarWithoutShadow);

	// The mask includes the shadow underneath the sprite.
	ASSERT_TRUE(HasShadowMask(list[0]));
	const ShadowMask mask = GetShadowMask(list[0]);
	EXPECT_EQ(mask.bottomOffset(), 2);
	EXPECT_EQ(mask.numRows(), 4);
	EXPECT_EQ(ToPicture(mask, 8, 12), BarShadow);
	// 2 bytes of header and one span for each of the 4 rows.
	EXPECT_EQ(list[0].headerSize(), ClxSprite::BaseHeaderSize + 2 + 4 * 2);
}

TEST(ClxShadowTest, KeepsBlackPixelsThatAreNotShadow)
{
	Picture picture = Bar;
	// A black pixel of the bar. It casts the same shadow as the pixel it replaces.
	picture[2][6] = '#';
	// Its shadow would fall outside of the frame, so it cannot be ruled out as a body pixel.
	picture[0][0] = '#';
	// Not in any shadow, and its own shadow would fall on a transparent pixel.
	picture[3][3] = '#';
	Picture expected = BarWithoutShadow;
	expected[2][6] = '#';
	expected[0][0] = '#';
	expected[3][3] = '#';

	const OwnedClxSpriteListOrSheet sprites = MakeList({ picture });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), expected);
	EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 8, 12), BarShadow);
}

TEST(ClxShadowTest, RecognisesBlackBodyPixelInsideAShadow)
{
	// The baseline is row 23. 'b' at (7, 0) shadows (2, 14), so the black pixel there looks like
	// a shadow. It must be a body pixel, because nothing else casts the black pixel at (0, 18).
	Picture picture(24, std::string(8, '.'));
	picture[0][7] = 'b';
	picture[14][2] = '#';
	picture[18][0] = '#';
	picture[23][7] = 'a';
	Picture expected = picture;
	expected[18][0] = '.';

	const OwnedClxSpriteListOrSheet sprites = MakeList({ picture });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), expected);
}

TEST(ClxShadowTest, BaselineRules)
{
	// The second sprite ends 3 rows higher, with the shadow that its own baseline (row 8) gives.
	const Picture shortBar = {
		"......a.",
		"......a.",
		"......a.",
		"......a.",
		".....#a.",
		"......a.",
		"......a.",
		"......a.",
		"......a.",
		"........",
		"........",
		"........",
	};
	Picture shortBarWithoutShadow = shortBar;
	shortBarWithoutShadow[4][5] = '.';
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar, shortBar });

	const std::optional<OwnedClxSpriteListOrSheet> eachFrame = ExtractClxShadows(sprites, EachFrame);
	ASSERT_TRUE(eachFrame.has_value());
	EXPECT_EQ(ToPicture(eachFrame->list()[0]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(eachFrame->list()[1]), shortBarWithoutShadow);
	EXPECT_EQ(GetShadowMask(eachFrame->list()[1]).bottomOffset(), 5);
	EXPECT_EQ(GetShadowMask(eachFrame->list()[1]).numRows(), 3);

	// With the baseline of the first sprite (row 11) that black pixel is not a shadow.
	const std::optional<OwnedClxSpriteListOrSheet> firstFrame = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(firstFrame.has_value());
	EXPECT_EQ(ToPicture(firstFrame->list()[0]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(firstFrame->list()[1]), shortBar);
}

TEST(ClxShadowTest, KeepsBlackPixelsWhenTheFirstFrameHasNoBaseline)
{
	// The first sprite has no pixels, so `FirstFrame` has no baseline for the rest of the list.
	const Picture empty(Bar.size(), std::string(Bar[0].size(), '.'));
	const OwnedClxSpriteListOrSheet sprites = MakeList({ empty, Bar });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), empty);
	EXPECT_EQ(ToPicture(result->list()[1]), Bar);
	EXPECT_FALSE(HasShadowMask(result->list()[1]));
}

TEST(ClxShadowTest, BaselineOffset)
{
	// A flying sprite. Its lowest opaque row (row 5) is its shadow.
	const Picture picture = {
		"......a.",
		"......a.",
		"......a.",
		"........",
		"........",
		"....#...",
	};
	Picture expected = picture;
	expected[5][4] = '.';
	const OwnedClxSpriteListOrSheet sprites = MakeList({ picture });

	// With the baseline on row 5 the sprite only shadows itself, at (6, 2).
	const std::optional<OwnedClxSpriteListOrSheet> noOffset = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(noOffset.has_value());
	EXPECT_EQ(ToPicture(noOffset->list()[0]), picture);

	// With the baseline on row 10, rows 0 and 1 shadow (4, 5). The shadow of row 2 is outside the frame.
	const std::optional<OwnedClxSpriteListOrSheet> withOffset = ExtractClxShadows(sprites, ShadowRule { ShadowRuleType::FirstFrame, 5 });
	ASSERT_TRUE(withOffset.has_value());
	EXPECT_EQ(ToPicture(withOffset->list()[0]), expected);
	const ShadowMask mask = GetShadowMask(withOffset->list()[0]);
	EXPECT_EQ(mask.bottomOffset(), 0);
	EXPECT_EQ(mask.numRows(), 1);
	EXPECT_EQ(ToPicture(mask, 8, 6), (Picture { "........", "........", "........", "........", "........", "....#..." }));
}

TEST(ClxShadowTest, PreservesSheetStructure)
{
	const Picture empty(12, std::string(8, '.'));
	const OwnedClxSpriteListOrSheet sprites = MakeSheet({ { Bar, Bar }, { empty }, { Bar } });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());

	ASSERT_TRUE(result->isSheet());
	const ClxSpriteSheet sheet = result->sheet();
	ASSERT_EQ(sheet.numLists(), 3);
	ASSERT_EQ(sheet[0].numSprites(), 2U);
	ASSERT_EQ(sheet[1].numSprites(), 1U);
	ASSERT_EQ(sheet[2].numSprites(), 1U);
	EXPECT_EQ(ToPicture(sheet[0][1]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(sheet[1][0]), empty);
	EXPECT_EQ(ToPicture(sheet[2][0]), BarWithoutShadow);

	EXPECT_EQ(ToPicture(GetShadowMask(sheet[0][1]), 8, 12), BarShadow);
	// A sprite without a shadow has a plain header.
	EXPECT_FALSE(HasShadowMask(sheet[1][0]));
	EXPECT_EQ(sheet[1][0].headerSize(), ClxSprite::BaseHeaderSize);
	EXPECT_EQ(ToPicture(GetShadowMask(sheet[2][0]), 8, 12), BarShadow);
}

TEST(ClxShadowTest, SplitsLongRunsAndKeepsEmptyRows)
{
	// The baseline is row 8. 'a' (row 0) shadows row 4 shifted left by 1, 'b' (rows 6..8) shadows row 6,
	// and nothing shadows row 5.
	Picture picture(9, std::string(200, '.'));
	for (size_t x = 1; x < 200; ++x)
		picture[0][x] = 'a';
	for (size_t x = 0; x < 199; ++x)
		picture[4][x] = '#';
	picture[8][10] = 'b';
	picture[7][10] = 'b';
	picture[6][10] = 'b';
	Picture expectedSprite = picture;
	expectedSprite[4].assign(200, '.');
	Picture expectedMask(9, std::string(200, '.'));
	expectedMask[4].replace(0, 199, std::string(199, '#'));
	expectedMask[6][10] = '#';

	const OwnedClxSpriteListOrSheet sprites = MakeList({ picture });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), expectedSprite);

	const ShadowMask mask = GetShadowMask(result->list()[0]);
	EXPECT_EQ(mask.bottomOffset(), 2);
	EXPECT_EQ(mask.numRows(), 3);
	EXPECT_EQ(ToPicture(mask, 200, 9), expectedMask);
	// Row 6: one span. Row 5: an empty span. Row 4: 199 pixels as spans of 127 and 72.
	const std::vector<uint8_t> expectedRows = { 10, 0x80 | 1, 0, 0x80 | 0, 0, 127, 127, 0x80 | 72 };
	EXPECT_EQ(std::vector<uint8_t>(mask.rows(), mask.rows() + expectedRows.size()), expectedRows);
}

/** An 8-bit surface filled with a pattern that has no black in it. */
SDLSurfaceUniquePtr MakeBackground(int width, int height)
{
	SDLSurfaceUniquePtr surface = SDLWrap::CreateRGBSurfaceWithFormat(/*flags=*/0, width, height, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	const Surface out { surface.get() };
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x)
			*out.at(x, y) = static_cast<uint8_t>(1 + (x * 7 + y * 13) % 200);
	}
	return surface;
}

std::vector<uint8_t> Pixels(const Surface &out)
{
	std::vector<uint8_t> result;
	for (int y = 0; y < out.h(); ++y)
		result.insert(result.end(), out.at(0, y), out.at(0, y) + out.w());
	return result;
}

// A sprite whose shadow reaches the left edge, with a black body pixel and a body pixel inside the shadow.
const Picture Walker = {
	"...........aaa..",
	"...........a#a..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
	"........###aaa..",
	"........###aaa..",
	".........##aaa..",
	"...........aaa..",
	"...........aaa..",
	"...........aaa..",
};

TEST(ClxShadowTest, DrawingShadowThenSpriteMatchesTheOriginal)
{
	constexpr int Width = 24, Height = 20;
	for (const Picture &picture : { Bar, Walker }) {
		const OwnedClxSpriteListOrSheet original = MakeList({ picture });
		const std::optional<OwnedClxSpriteListOrSheet> extracted = ExtractClxShadows(original, FirstFrame);
		ASSERT_TRUE(extracted.has_value());
		ASSERT_TRUE(HasShadowMask(extracted->list()[0]));
		// The shadow must really have been removed, or this test proves nothing.
		ASSERT_NE(ToPicture(extracted->list()[0]), picture);

		// Every position, including those where the sprite is clipped or entirely outside.
		const int spriteWidth = static_cast<int>(picture[0].size());
		const int spriteHeight = static_cast<int>(picture.size());
		for (int y = -2; y < Height + spriteHeight + 2; ++y) {
			for (int x = -spriteWidth - 2; x < Width + 2; ++x) {
				const SDLSurfaceUniquePtr expectedSurface = MakeBackground(Width, Height);
				const Surface expected { expectedSurface.get() };
				ClxDraw(expected, { x, y }, original.list()[0]);

				const SDLSurfaceUniquePtr actualSurface = MakeBackground(Width, Height);
				const Surface actual { actualSurface.get() };
				ClxDrawShadow(actual, { x, y }, extracted->list()[0]);
				ClxDraw(actual, { x, y }, extracted->list()[0]);

				ASSERT_EQ(Pixels(actual), Pixels(expected)) << "at " << x << ", " << y;
			}
		}
	}
}

TEST(ClxShadowTest, OpaqueRunsThatContinueOnTheNextRow)
{
	// CLX sprites must not have opaque runs that continue on the next row, but sprites that were
	// converted before `Cl2ToClx` stopped producing them do. Such a run is taken to continue on the
	// next row, as in the image that it was made from, and the result has no such runs.
	// A 6x4 sprite, from the bottom row upwards:
	//   "....ab" and the run goes on with "cd"
	//   "cd#..."
	//   "....ee"
	//   "....e#"
	std::vector<uint8_t> data(4 * 3 + ClxFrameHeaderSize);
	WriteLE32(&data[0], 1);
	WriteLE32(&data[4], 12);
	WriteLE16(&data[12], ClxFrameHeaderSize);
	WriteLE16(&data[14], 6);
	WriteLE16(&data[16], 4);
	const auto transparent = [&](unsigned width) { AppendClxTransparentRun(width, data); };
	const auto pixels = [&](std::initializer_list<uint8_t> colors) { AppendClxPixelsRun(colors.begin(), static_cast<unsigned>(colors.size()), data); };
	transparent(4);
	pixels({ 'a', 'b', 'c', 'd' });
	pixels({ 0 });
	transparent(3 + 4);
	pixels({ 'e', 'e' });
	transparent(4);
	pixels({ 'e', 0 });
	WriteLE32(&data[8], static_cast<uint32_t>(data.size()));
	std::unique_ptr<uint8_t[]> buffer { new uint8_t[data.size()] };
	std::copy(data.begin(), data.end(), buffer.get());
	const OwnedClxSpriteListOrSheet original { std::move(buffer), 0 };

	const std::optional<OwnedClxSpriteListOrSheet> extracted = ExtractClxShadows(original, FirstFrame);
	ASSERT_TRUE(extracted.has_value());
	const ClxSprite sprite = extracted->list()[0];
	EXPECT_EQ(ToPicture(sprite), (Picture { "....e#", "....ee", "cd#...", "....ab" }));

	// No opaque command of the result goes past the end of its row.
	const uint8_t *src = sprite.pixelData();
	const uint8_t *end = src + sprite.pixelDataSize();
	unsigned x = 0;
	while (src < end) {
		const uint8_t control = *src++;
		if (!IsClxOpaque(control)) {
			x += control;
		} else {
			const unsigned width = IsClxOpaqueFill(control) ? GetClxOpaqueFillWidth(control) : GetClxOpaquePixelsWidth(control);
			src += IsClxOpaqueFill(control) ? 1 : width;
			EXPECT_LE(x + width, 6U);
			x += width;
		}
		x %= 6;
	}
}

TEST(ClxShadowTest, DrawsBlendedShadow)
{
	// A recognisable "blend with black" table: every colour maps to itself plus 50.
	for (unsigned i = 0; i < 256; ++i)
		paletteTransparencyLookup[0][i] = static_cast<uint8_t>(i + 50);
#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	for (unsigned i = 0; i < 256; ++i) {
		for (unsigned j = 0; j < 256; ++j)
			paletteTransparencyLookupBlack16[i | (j << 8U)] = static_cast<uint16_t>(paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8U));
	}
#endif

	constexpr int Width = 24, Height = 20;
	const std::optional<OwnedClxSpriteListOrSheet> extracted = ExtractClxShadows(MakeList({ Walker }), FirstFrame);
	ASSERT_TRUE(extracted.has_value());
	const ClxSprite sprite = extracted->list()[0];
	const Picture body = ToPicture(sprite);
	const Picture shadow = ToPicture(GetShadowMask(sprite), Walker[0].size(), Walker.size());

	// Positions that clip the shadow on every side, with odd and even span starts.
	for (const Point position : { Point { 3, 16 }, Point { 4, 16 }, Point { -9, 16 }, Point { 12, 10 }, Point { 2, 25 } }) {
		const SDLSurfaceUniquePtr backgroundSurface = MakeBackground(Width, Height);
		const std::vector<uint8_t> background = Pixels(Surface { backgroundSurface.get() });

		const SDLSurfaceUniquePtr surface = MakeBackground(Width, Height);
		const Surface out { surface.get() };
		ClxDrawShadowBlended(out, position, sprite);
		ClxDraw(out, position, sprite);

		std::vector<uint8_t> expected = background;
		for (int spriteY = 0; spriteY < static_cast<int>(Walker.size()); ++spriteY) {
			for (int spriteX = 0; spriteX < static_cast<int>(Walker[0].size()); ++spriteX) {
				const int x = position.x + spriteX;
				const int y = position.y - (static_cast<int>(Walker.size()) - 1 - spriteY);
				if (x < 0 || x >= Width || y < 0 || y >= Height) continue;
				uint8_t &pixel = expected[static_cast<size_t>(y) * Width + x];
				if (body[spriteY][spriteX] != '.') {
					pixel = body[spriteY][spriteX] == '#' ? 0 : static_cast<uint8_t>(body[spriteY][spriteX]);
				} else if (shadow[spriteY][spriteX] == '#') {
					pixel = static_cast<uint8_t>(pixel + 50);
				}
			}
		}
		EXPECT_EQ(Pixels(out), expected) << "at " << position.x << ", " << position.y;
	}
}

TEST(ClxShadowTest, NothingToExtractWithoutAKnownRule)
{
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar });
	EXPECT_FALSE(ExtractClxShadows(sprites, ShadowRule { ShadowRuleType::Unknown, 0 }).has_value());
	EXPECT_FALSE(ExtractClxShadows(sprites, ShadowRule { ShadowRuleType::None, 0 }).has_value());
	EXPECT_FALSE(ExtractClxShadows(sprites, ShadowRule { ShadowRuleType::Baked, 0 }).has_value());
}

/**
 * The corrections of a sprite, drawn over it: 'B' is a pixel that belongs to the sprite,
 * 'S' is a pixel that is shadow, anything else is not corrected. Empty for no corrections.
 */
struct FixedPicture {
	Picture picture;
	Picture fix;
};

void AppendFixMask(const Picture &fix, char kind, std::vector<uint8_t> &out)
{
	const int height = static_cast<int>(fix.size());
	int lowest = -1;
	int highest = height;
	for (int y = 0; y < height; ++y) {
		if (fix[y].find(kind) == std::string::npos) continue;
		lowest = y;
		if (highest == height) highest = y;
	}
	if (lowest < 0) return;
	out.push_back(static_cast<uint8_t>(height - 1 - lowest));
	out.push_back(static_cast<uint8_t>(lowest - highest + 1));
	for (int y = lowest; y >= highest; --y) {
		const size_t rowBegin = out.size();
		for (size_t x = 0; x < fix[y].size(); ++x) {
			if (fix[y][x] != kind) continue;
			out.push_back(static_cast<uint8_t>(x));
			out.push_back(1);
		}
		if (out.size() == rowBegin) {
			out.push_back(0);
			out.push_back(0);
		}
		out.back() |= ShadowSpanEndOfRow;
	}
}

std::vector<uint8_t> MakeFixes(const std::vector<std::vector<FixedPicture>> &lists)
{
	std::vector<uint8_t> out { 'S', 'H', 'F', 'X', ShadowFixVersion, 0, 0, 0 };
	WriteLE16(&out[6], static_cast<uint16_t>(lists.size()));
	const auto append16 = [&out](size_t value) {
		out.resize(out.size() + 2);
		WriteLE16(&out[out.size() - 2], static_cast<uint16_t>(value));
	};
	for (const std::vector<FixedPicture> &list : lists) {
		append16(list.size());
		for (const FixedPicture &frame : list) {
			if (frame.fix.empty()) {
				append16(0);
				continue;
			}
			const size_t width = frame.picture[0].size();
			std::vector<uint32_t> blackPixels;
			for (size_t y = 0; y < frame.picture.size(); ++y) {
				for (size_t x = 0; x < width; ++x) {
					if (frame.picture[y][x] == '#') blackPixels.push_back(static_cast<uint32_t>(y * width + x));
				}
			}
			std::vector<uint8_t> body;
			std::vector<uint8_t> shadow;
			AppendFixMask(frame.fix, 'B', body);
			AppendFixMask(frame.fix, 'S', shadow);
			append16(6 + body.size() + shadow.size());
			out.resize(out.size() + 4);
			WriteLE32(&out[out.size() - 4], ShadowFixGuard(static_cast<uint16_t>(width), static_cast<uint16_t>(frame.picture.size()), blackPixels));
			append16(body.size());
			out.insert(out.end(), body.begin(), body.end());
			out.insert(out.end(), shadow.begin(), shadow.end());
		}
	}
	return out;
}

/** A picture of the given size with the given characters at `(x, y)`. */
Picture FixAt(const Picture &like, std::initializer_list<std::tuple<int, int, char>> pixels)
{
	Picture fix(like.size(), std::string(like[0].size(), '.'));
	for (const auto &[x, y, c] : pixels)
		fix[y][x] = c;
	return fix;
}

TEST(ClxShadowTest, FixReturnsShadowPixelToTheSprite)
{
	// The pixel at (4, 6) is covered by the shadow of the bar, yet it is part of the sprite.
	const std::vector<uint8_t> fixes = MakeFixes({ { { Bar, FixAt(Bar, { { 4, 6, 'B' } }) } } });
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame, fixes);
	ASSERT_TRUE(result.has_value());
	Picture expected = BarWithoutShadow;
	expected[6][4] = '#';
	EXPECT_EQ(ToPicture(result->list()[0]), expected);
	// The mask still has the shadow underneath the pixel.
	EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 8, 12), BarShadow);
}

TEST(ClxShadowTest, FixTurnsSpritePixelIntoShadow)
{
	Picture picture = Bar;
	// Not in any shadow, and its own shadow would fall on a transparent pixel, so the rule keeps it.
	picture[3][3] = '#';
	const std::vector<uint8_t> fixes = MakeFixes({ { { picture, FixAt(picture, { { 3, 3, 'S' } }) } } });
	const OwnedClxSpriteListOrSheet sprites = MakeList({ picture });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame, fixes);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), BarWithoutShadow);
	Picture expectedShadow = BarShadow;
	expectedShadow[3][3] = '#';
	EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 8, 12), expectedShadow);
}

TEST(ClxShadowTest, FixOnlyAppliesToBlackPixels)
{
	// A correction on a coloured pixel and one on a transparent pixel.
	const std::vector<uint8_t> fixes = MakeFixes({ { { Bar, FixAt(Bar, { { 6, 0, 'S' }, { 0, 0, 'S' }, { 1, 1, 'B' } }) } } });
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame, fixes);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(ToPicture(result->list()[0]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 8, 12), BarShadow);
}

TEST(ClxShadowTest, FixesAloneDescribeTheShadowOfASpriteWithoutARule)
{
	const Picture pot = {
		"..aa..",
		".a##a.",
		"##a#a.",
		".###..",
	};
	const Picture fix = {
		"......",
		"......",
		"SS....",
		".S....",
	};
	const std::vector<uint8_t> fixes = MakeFixes({ { { pot, fix } } });
	const OwnedClxSpriteListOrSheet sprites = MakeList({ pot });
	for (const ShadowRule rule : { ShadowRule { ShadowRuleType::Baked }, ShadowRule { ShadowRuleType::Unknown } }) {
		const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, rule, fixes);
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(ToPicture(result->list()[0]), (Picture { "..aa..", ".a##a.", "..a#a.", "..##.." }));
		ASSERT_TRUE(HasShadowMask(result->list()[0]));
		EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 6, 4), (Picture { "......", "......", "##....", ".#...." }));
	}
}

TEST(ClxShadowTest, FixesOfASheetFollowItsLists)
{
	Picture other = Bar;
	other[7][5] = '.';
	const std::vector<uint8_t> fixes = MakeFixes({
	    { { other, {} }, { Bar, {} } },
	    { { Bar, {} }, { Bar, FixAt(Bar, { { 5, 7, 'B' } }) } },
	});
	const OwnedClxSpriteListOrSheet sprites = MakeSheet({ { other, Bar }, { Bar, Bar } });
	const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame, fixes);
	ASSERT_TRUE(result.has_value());
	ASSERT_TRUE(result->isSheet());
	Picture expected = BarWithoutShadow;
	expected[7][5] = '#';
	EXPECT_EQ(ToPicture(result->sheet()[0][0]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(result->sheet()[0][1]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(result->sheet()[1][0]), BarWithoutShadow);
	EXPECT_EQ(ToPicture(result->sheet()[1][1]), expected);
}

TEST(ClxShadowTest, FixesMadeForOtherSpritesAreIgnored)
{
	const Picture fix = FixAt(Bar, { { 4, 6, 'B' } });
	const OwnedClxSpriteListOrSheet sprites = MakeList({ Bar });
	const auto expectPlainExtraction = [&](const std::vector<uint8_t> &fixes) {
		const std::optional<OwnedClxSpriteListOrSheet> result = ExtractClxShadows(sprites, FirstFrame, fixes);
		ASSERT_TRUE(result.has_value());
		EXPECT_EQ(ToPicture(result->list()[0]), BarWithoutShadow);
		EXPECT_EQ(ToPicture(GetShadowMask(result->list()[0]), 8, 12), BarShadow);
	};

	// The black pixels are elsewhere.
	Picture moved = Bar;
	moved[7][5] = '.';
	moved[7][4] = '#';
	expectPlainExtraction(MakeFixes({ { { moved, fix } } }));
	// Another number of sprites, or of lists.
	expectPlainExtraction(MakeFixes({ { { Bar, fix }, { Bar, fix } } }));
	expectPlainExtraction(MakeFixes({ { { Bar, fix } }, { { Bar, fix } } }));

	// Damaged files.
	const std::vector<uint8_t> good = MakeFixes({ { { Bar, fix } } });
	for (size_t size = 0; size < good.size(); ++size)
		expectPlainExtraction(std::vector<uint8_t>(good.begin(), good.begin() + size));
	std::vector<uint8_t> wrongVersion = good;
	wrongVersion[4] = ShadowFixVersion + 1;
	expectPlainExtraction(wrongVersion);
	// A span that reaches outside of the sprite.
	std::vector<uint8_t> outside = good;
	outside[outside.size() - 2] = 7;
	outside[outside.size() - 1] = 2 | ShadowSpanEndOfRow;
	expectPlainExtraction(outside);

	// Corrections that cannot be used leave a sprite without a rule alone.
	EXPECT_FALSE(ExtractClxShadows(sprites, ShadowRule { ShadowRuleType::Baked }, wrongVersion).has_value());
}

} // namespace
} // namespace devilution
