#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/cl2_to_clx.hpp"
#include "utils/clx_decode.hpp"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

/** A CL2 with a single 4 pixel wide frame that has the given commands. */
std::vector<uint8_t> MakeCl2(const std::vector<uint8_t> &commands)
{
	constexpr size_t ListHeaderSize = 12; // The frame count, the offset of the frame and the size.
	constexpr size_t FrameHeaderSize = 10;
	std::vector<uint8_t> data(ListHeaderSize + FrameHeaderSize);
	data[0] = 1;
	data[4] = ListHeaderSize;
	data[ListHeaderSize] = FrameHeaderSize;
	data.insert(data.end(), commands.begin(), commands.end());
	data[8] = static_cast<uint8_t>(data.size());
	return data;
}

TEST(Cl2ToClxTest, OpaqueRunsEndWithTheirLine)
{
	// From the bottom line upwards: "..ab" then "cd..".
	// The opaque pixels that end the first line are followed directly by those that begin the next.
	const std::vector<uint8_t> cl2 = MakeCl2({ 2, 0xFE, 'a', 'b', 0xFE, 'c', 'd', 2 });
	std::vector<uint8_t> clxData;
	ASSERT_EQ(Cl2ToClx(cl2.data(), cl2.size(), PointerOrValue<uint16_t> { static_cast<uint16_t>(4) }, clxData), 0);
	const ClxSpriteList list { clxData.data() };
	ASSERT_EQ(list.numSprites(), 1U);
	const ClxSprite sprite = list[0];
	ASSERT_EQ(sprite.width(), 4);
	ASSERT_EQ(sprite.height(), 2);

	// In CLX, only transparent runs can continue on the next line.
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
			EXPECT_LE(x + width, 4U);
			x += width;
		}
		x %= 4;
	}

	// The sprite is drawn as it is in the CL2: nothing to the right of it.
	const SDLSurfaceUniquePtr surface = SDLWrap::CreateRGBSurfaceWithFormat(/*flags=*/0, /*width=*/8, /*height=*/4, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	const Surface out { surface.get() };
	for (int y = 0; y < out.h(); ++y) {
		for (int px = 0; px < out.w(); ++px)
			*out.at(px, y) = '.';
	}
	ClxDraw(out, { 1, 2 }, sprite);
	const auto row = [&](int y) { return std::string(out.at(0, y), out.at(0, y) + out.w()); };
	EXPECT_EQ(row(0), "........");
	EXPECT_EQ(row(1), ".cd.....");
	EXPECT_EQ(row(2), "...ab...");
	EXPECT_EQ(row(3), "........");
}

} // namespace
} // namespace devilution
