#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/pcx_to_clx.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"
#include "utils/surface_to_pcx.hpp"

namespace devilution {
namespace {

// An odd width, with some colors above 0xBF (these must be escaped in PCX).
const std::vector<std::string> Pixels = {
	"ab\xC8\xC8\xC8",
	"\xFF\xFF"
	"cde",
	"fffff",
};

std::expected<OwnedClxSpriteList, std::string> LoadPcxFromMemory(const std::vector<uint8_t> &data, int numFramesOrFrameHeight = 1, SDL_Color *outPalette = nullptr)
{
	AssetHandle handle { SDL_IOFromConstMem(data.data(), static_cast<int>(data.size())) };
	return PcxToClx(handle, data.size(), numFramesOrFrameHeight, /*transparentColor=*/std::nullopt, outPalette);
}

SDL_Palette *GetOrCreatePalette(SDL_Surface *surface)
{
#ifdef USE_SDL3
	// In SDL3, indexed surfaces do not have a palette by default.
	SDL_Palette *palette = SDL_GetSurfacePalette(surface);
	if (palette == nullptr)
		palette = SDL_CreateSurfacePalette(surface);
	return palette;
#else
	return surface->format->palette;
#endif
}

/** @brief Draws the sprite and returns its pixels, row by row. */
std::vector<std::string> Draw(const ClxSprite sprite)
{
	const SDLSurfaceUniquePtr surface = SDLWrap::CreateRGBSurfaceWithFormat(/*flags=*/0, sprite.width(), sprite.height(), /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	const Surface out { surface.get() };
	ClxDraw(out, { 0, out.h() - 1 }, sprite);
	std::vector<std::string> rows;
	for (int y = 0; y < out.h(); ++y)
		rows.emplace_back(out.at(0, y), out.at(0, y) + out.w());
	return rows;
}

/**
 * @brief Encodes the pixels as an 8-bit PCX with the given number of bytes per line.
 *
 * Each pixel is encoded individually (no runs).
 */
std::vector<uint8_t> MakePcx(const std::vector<std::string> &rows, uint16_t bytesPerLine)
{
	const auto width = static_cast<uint16_t>(rows[0].size());
	const auto height = static_cast<uint16_t>(rows.size());
	std::vector<uint8_t> data(128);
	data[0] = 10; // Manufacturer
	data[1] = 5;  // Version
	data[2] = 1;  // Encoding
	data[3] = 8;  // BitsPerPixel
	data[8] = static_cast<uint8_t>(width - 1);
	data[9] = static_cast<uint8_t>((width - 1) >> 8);
	data[10] = static_cast<uint8_t>(height - 1);
	data[11] = static_cast<uint8_t>((height - 1) >> 8);
	data[65] = 1; // NPlanes
	data[66] = static_cast<uint8_t>(bytesPerLine);
	data[67] = static_cast<uint8_t>(bytesPerLine >> 8);
	for (const std::string &row : rows) {
		for (size_t x = 0; x < bytesPerLine; ++x) {
			const uint8_t pixel = x < row.size() ? static_cast<uint8_t>(row[x]) : 0;
			if (pixel > 0xBF)
				data.push_back(0xC1); // A run of length 1.
			data.push_back(pixel);
		}
	}
	data.push_back(0x0C); // Palette separator
	data.resize(data.size() + 256 * 3);
	return data;
}

TEST(PcxToClxTest, ScreenshotWithOddWidthRoundTrips)
{
	const SDLSurfaceUniquePtr surface = SDLWrap::CreateRGBSurfaceWithFormat(/*flags=*/0, /*width=*/5, /*height=*/3, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	const Surface out { surface.get() };
	for (int y = 0; y < out.h(); ++y) {
		for (int x = 0; x < out.w(); ++x)
			*out.at(x, y) = static_cast<uint8_t>(Pixels[y][x]);
	}
	SDL_Palette *surfacePalette = GetOrCreatePalette(surface.get());
	ASSERT_NE(surfacePalette, nullptr) << SDL_GetError();
	ASSERT_EQ(surfacePalette->ncolors, 256);
	SDL_Color *colors = surfacePalette->colors;
	for (int i = 0; i < 256; ++i) {
		colors[i].r = static_cast<uint8_t>(i);
		colors[i].g = static_cast<uint8_t>(255 - i);
		colors[i].b = static_cast<uint8_t>(i / 2);
	}

	const std::string path = ::testing::TempDir() + "pcx_to_clx_test.pcx";
	SDL_IOStream *stream = SDL_IOFromFile(path.c_str(), "wb");
	ASSERT_NE(stream, nullptr) << SDL_GetError();
	const std::expected<void, std::string> written = WriteSurfaceToFilePcx(out, stream);
	ASSERT_TRUE(written.has_value()) << written.error();
	std::ifstream input(path, std::ios::binary);
	const std::vector<uint8_t> data { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	input.close();
	std::remove(path.c_str());

	// The PCX specification requires the bytes per line to be even.
	ASSERT_GE(data.size(), 128U);
	EXPECT_EQ(data[66] | (data[67] << 8), 6);

	SDL_Color palette[256];
	const std::expected<OwnedClxSpriteList, std::string> list = LoadPcxFromMemory(data, /*numFramesOrFrameHeight=*/1, palette);
	ASSERT_TRUE(list.has_value()) << list.error();
	EXPECT_EQ(Draw((*list)[0]), Pixels);
	for (int i = 0; i < 256; ++i) {
		EXPECT_EQ(palette[i].r, colors[i].r);
		EXPECT_EQ(palette[i].g, colors[i].g);
		EXPECT_EQ(palette[i].b, colors[i].b);
	}
}

#ifdef USE_SDL3
TEST(PcxToClxTest, ScreenshotRequiresPalette)
{
	const SDLSurfaceUniquePtr surface = SDLWrap::CreateRGBSurfaceWithFormat(/*flags=*/0, /*width=*/5, /*height=*/3, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	ASSERT_EQ(SDL_GetSurfacePalette(surface.get()), nullptr);
	uint8_t buffer[1024];
	SDL_IOStream *stream = SDL_IOFromMem(buffer, sizeof(buffer));
	ASSERT_NE(stream, nullptr) << SDL_GetError();
	const std::expected<void, std::string> written = WriteSurfaceToFilePcx(Surface { surface.get() }, stream);
	SDL_CloseIO(stream);
	EXPECT_EQ(written.error_or(""), "surface has no palette");
}
#endif

TEST(PcxToClxTest, LoadsOddWidthWithoutPadding)
{
	// Older versions of DevilutionX wrote screenshots with an odd width without padding.
	const std::expected<OwnedClxSpriteList, std::string> list = LoadPcxFromMemory(MakePcx(Pixels, /*bytesPerLine=*/5));
	ASSERT_TRUE(list.has_value()) << list.error();
	EXPECT_EQ(Draw((*list)[0]), Pixels);
}

TEST(PcxToClxTest, LoadsOddWidthWithPadding)
{
	const std::expected<OwnedClxSpriteList, std::string> list = LoadPcxFromMemory(MakePcx(Pixels, /*bytesPerLine=*/6));
	ASSERT_TRUE(list.has_value()) << list.error();
	EXPECT_EQ(Draw((*list)[0]), Pixels);
}

TEST(PcxToClxTest, RejectsInvalidData)
{
	const std::vector<uint8_t> valid = MakePcx(Pixels, /*bytesPerLine=*/6);

	std::vector<uint8_t> truncated = valid;
	truncated.resize(140);
	EXPECT_EQ(LoadPcxFromMemory(truncated).error_or(""), "PCX pixel data is truncated");

	std::vector<uint8_t> headerOnly = valid;
	headerOnly.resize(100);
	EXPECT_EQ(LoadPcxFromMemory(headerOnly).error_or(""), "PCX file is too small");

	std::vector<uint8_t> runTooLong = valid;
	runTooLong[128] = 0xC7; // A run of 7 pixels.
	EXPECT_EQ(LoadPcxFromMemory(runTooLong).error_or(""), "PCX run crosses a scanline boundary");

	std::vector<uint8_t> narrowBytesPerLine = valid;
	narrowBytesPerLine[66] = 4;
	EXPECT_EQ(LoadPcxFromMemory(narrowBytesPerLine).error_or(""), "Invalid PCX header: bytes per line is less than the image width");

	std::vector<uint8_t> fourBit = valid;
	fourBit[3] = 4;
	EXPECT_EQ(LoadPcxFromMemory(fourBit).error_or(""), "Only 8-bit single-plane PCX images are supported");

	EXPECT_EQ(LoadPcxFromMemory(valid, /*numFramesOrFrameHeight=*/0).error_or(""), "The number of frames must not be 0");
	EXPECT_EQ(LoadPcxFromMemory(valid, /*numFramesOrFrameHeight=*/4).error_or(""), "PCX image is too small for the requested frame size or count");

	std::vector<uint8_t> noPalette = valid;
	noPalette.resize(noPalette.size() - 769);
	SDL_Color palette[256];
	EXPECT_EQ(LoadPcxFromMemory(noPalette, /*numFramesOrFrameHeight=*/1, palette).error_or(""), "PCX has no palette");
}

} // namespace
} // namespace devilution
