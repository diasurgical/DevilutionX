#include "levels/reencode_dun_cels.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "levels/dun_tile.hpp"
#include "utils/endian_read.hpp"

namespace devilution {
namespace {

/** A 32x32 dungeon frame, top row first. 0 is transparent. */
using Frame = std::array<std::array<uint8_t, DunFrameWidth>, DunFrameHeight>;

/** Encodes a frame as a `TransparentSquare`: bottom row first, in runs that don't cross rows. */
std::vector<uint8_t> EncodeTransparentSquare(const Frame &frame)
{
	std::vector<uint8_t> out;
	for (int y = DunFrameHeight - 1; y >= 0; --y) {
		for (int x = 0; x < DunFrameWidth;) {
			const bool solid = frame[y][x] != 0;
			int run = 0;
			while (x + run < DunFrameWidth && (frame[y][x + run] != 0) == solid)
				++run;
			if (solid) {
				out.push_back(static_cast<uint8_t>(run));
				out.insert(out.end(), &frame[y][x], &frame[y][x] + run);
			} else {
				out.push_back(static_cast<uint8_t>(-run));
			}
			x += run;
		}
	}
	return out;
}

/** Decodes the top `height` rows of a frame from a `TransparentSquare`. */
Frame DecodeTransparentSquare(const uint8_t *src, int height)
{
	Frame frame {};
	for (int y = height - 1; y >= 0; --y) {
		for (int x = 0; x < DunFrameWidth;) {
			const auto run = static_cast<int8_t>(*src++);
			if (run > 0) {
				std::memcpy(&frame[y][x], src, run);
				src += run;
				x += run;
			} else {
				x -= run;
			}
		}
	}
	return frame;
}

TEST(ReencodeDunCelsTest, FloorFoliageKeepsItsPosition)
{
	Frame frame {};
	for (int y = 16; y < DunFrameHeight; ++y) // the floor
		for (int x = 0; x < DunFrameWidth; ++x)
			frame[y][x] = 100;
	// Foliage outside the left floor triangle (which covers x >= 32 - 2y in rows 1-15), in runs that end before
	// the end of the row and at it
	frame[7][2] = 10;
	frame[7][3] = 11;
	frame[7][4] = 12;
	frame[7][6] = 13;
	frame[10][0] = 14;
	frame[10][1] = 15;
	frame[0][30] = 16; // row 0 is above the triangle, so this run can reach the end of the row
	frame[0][31] = 17;

	const std::vector<uint8_t> encoded = EncodeTransparentSquare(frame);
	const size_t size = 12 + encoded.size();
	std::unique_ptr<std::byte[]> cels { new std::byte[size] };
	auto *data = reinterpret_cast<uint8_t *>(cels.get());
	const uint32_t header[] = { 1, 12, static_cast<uint32_t>(size) }; // one frame, its offset, the file size
	for (size_t i = 0; i < 3; ++i) {
		for (size_t b = 0; b < 4; ++b)
			data[i * 4 + b] = static_cast<uint8_t>(header[i] >> (8 * b));
	}
	std::memcpy(data + 12, encoded.data(), encoded.size());

	std::pair<uint16_t, DunFrameInfo> frames[] = {
		{ 1, DunFrameInfo { 0, TileType::TransparentSquare, TileProperties::None } }, // a left floor frame
	};
	ReencodeDungeonCels(cels, frames);

	const auto *out = reinterpret_cast<const uint8_t *>(cels.get());
	const Frame foliage = DecodeTransparentSquare(out + LoadLE32(out + 4) + ReencodedTriangleFrameSize, 16);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < DunFrameWidth - 2 * y; ++x) {
			EXPECT_EQ(foliage[y][x], frame[y][x]) << "at x=" << x << ", y=" << y;
		}
	}
}

} // namespace
} // namespace devilution
