#include "levels/reencode_dun_cels.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <SDL_endian.h>

#include "levels/dun_tile.hpp"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"
#include "utils/format_int.hpp"
#include "utils/log.hpp"

namespace devilution {
namespace {

DVL_ALWAYS_INLINE void ReencodeDungeonCelsLeftTriangleLower(uint8_t *&dst, const uint8_t *&src)
{
	unsigned width = 0;
	for (unsigned i = 0; i < 8; ++i) {
		src += 2; // Skips the two zero bytes (aka bloat).
		width += 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
		width += 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
	}
}

DVL_ALWAYS_INLINE void ReencodeDungeonCelsLeftTriangle(uint8_t *&dst, const uint8_t *&src)
{
	ReencodeDungeonCelsLeftTriangleLower(dst, src);
	unsigned width = DunFrameWidth;
	for (unsigned i = 0; i < 7; ++i) {
		src += 2; // Skips the two zero bytes (aka bloat).
		width -= 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
		width -= 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
	}
	src += 2; // Skips the two zero bytes (aka bloat).
	width -= 2;
	std::memcpy(dst, src, width);
	dst += width;
}

DVL_ALWAYS_INLINE void ReencodeDungeonCelsRightTriangleLower(uint8_t *&dst, const uint8_t *&src)
{
	unsigned width = 0;
	for (unsigned i = 0; i < 8; ++i) {
		width += 2;
		std::memcpy(dst, src, width);
		src += width + 2; // Skips the two zero bytes (aka bloat).
		dst += width;
		width += 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
	}
}

DVL_ALWAYS_INLINE void ReencodeDungeonCelsRightTriangle(uint8_t *&dst, const uint8_t *&src)
{
	ReencodeDungeonCelsRightTriangleLower(dst, src);
	unsigned width = DunFrameWidth;
	for (unsigned i = 0; i < 7; ++i) {
		width -= 2;
		std::memcpy(dst, src, width);
		src += width + 2; // Skips the two zero bytes (aka bloat).
		dst += width;
		width -= 2;
		std::memcpy(dst, src, width);
		src += width;
		dst += width;
	}
	width -= 2;
	std::memcpy(dst, src, width);
	dst += width;
}

DVL_ALWAYS_INLINE void ReencodeDungeonCelsLeftTrapezoid(uint8_t *&dst, const uint8_t *&src)
{
	ReencodeDungeonCelsLeftTriangleLower(dst, src);
	std::memcpy(dst, src, DunFrameWidth * 16);
	dst += DunFrameWidth * 16;
}

DVL_ALWAYS_INLINE void ReencodeDungeonCelsRightTrapezoid(uint8_t *&dst, const uint8_t *&src)
{
	ReencodeDungeonCelsRightTriangleLower(dst, src);
	std::memcpy(dst, src, DunFrameWidth * 16);
	dst += DunFrameWidth * 16;
}

DVL_ALWAYS_INLINE void RenderTransparentSquare(uint8_t *dst, const uint8_t *src)
{
	for (unsigned i = 0; i < DunFrameHeight; ++i, dst -= 2 * DunFrameWidth) {
		uint_fast8_t drawWidth = DunFrameWidth;
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				std::memcpy(dst, src, v);
				src += v;
			} else {
				v = static_cast<int8_t>(-v);
			}
			dst += v;
			drawWidth -= v;
		}
	}
}

DVL_ALWAYS_INLINE void ExtractFoliageLeftTriangle(uint8_t *&dst, uint8_t *src)
{
	for (int w = 2, y = 31; y >= 16; --y, w += 2, src -= DunFrameWidth) {
		std::memcpy(dst, src + (DunFrameWidth - w), w);
		std::memset(src + (DunFrameWidth - w), 0, w);
		dst += w;
	}
	for (int w = 30, y = 15; y > 0; --y, w -= 2, src -= DunFrameWidth) {
		std::memcpy(dst, src + (DunFrameWidth - w), w);
		std::memset(src + (DunFrameWidth - w), 0, w);
		dst += w;
	}
}

DVL_ALWAYS_INLINE void ExtractFoliageRightTriangle(uint8_t *&dst, uint8_t *src)
{
	for (int w = 2, y = 31; y >= 16; --y, w += 2, src -= DunFrameWidth) {
		std::memcpy(dst, src, w);
		std::memset(src, 0, w);
		dst += w;
	}
	for (int w = 30, y = 15; y > 0; --y, w -= 2, src -= DunFrameWidth) {
		std::memcpy(dst, src, w);
		std::memset(src, 0, w);
		dst += w;
	}
}

DVL_ALWAYS_INLINE void ExtractFoliageTransparentSquare(uint8_t *&dst, const uint8_t *src)
{
	// The bottom 16 lines are always transparent, foliage only
	// applies to the upper half of the tile.
	src -= DunFrameHeight * 16;
	for (int y = 16; y > 0; --y, src -= 2 * DunFrameWidth) {
		unsigned transparentRun = 0;
		unsigned solidRun = 0;
		for (int x = 0; x < DunFrameWidth; ++x) {
			if (*src++ != 0) {
				if (transparentRun != 0) {
					*dst++ = static_cast<uint8_t>(-static_cast<int8_t>(transparentRun));
					transparentRun = 0;
				}
				++solidRun;
			} else {
				if (solidRun != 0) {
					*dst++ = solidRun;
					std::memcpy(dst, src - solidRun, solidRun);
					dst += solidRun;
					solidRun = 0;
				}
				++transparentRun;
			}
		}
		if (transparentRun != 0) {
			*dst++ = static_cast<uint8_t>(-static_cast<int8_t>(transparentRun));
		} else if (solidRun != 0) {
			*dst++ = solidRun;
			std::memcpy(dst, src - solidRun, solidRun);
			dst += solidRun;
		}
	}
}

DVL_ALWAYS_INLINE void ReencodeFloorWithFoliage(uint8_t *&dst, const uint8_t *&src, TileType tileType)
{
	uint8_t surface[DunFrameWidth * DunFrameHeight] {};
	uint8_t *surfaceLastLine = &surface[DunFrameWidth * (DunFrameHeight - 1)];
	RenderTransparentSquare(surfaceLastLine, src);
	if (tileType == TileType::LeftTriangle) {
		ExtractFoliageLeftTriangle(dst, surfaceLastLine);
	} else {
		ExtractFoliageRightTriangle(dst, surfaceLastLine);
	}
	ExtractFoliageTransparentSquare(dst, surfaceLastLine);
}

size_t GetReencodedSize(const uint8_t *dungeonCels, std::span<std::pair<uint16_t, DunFrameInfo>> frames)
{
	size_t result = (2 + frames.size()) * 4;
	const auto *srcOffsets = reinterpret_cast<const uint32_t *>(dungeonCels);
	for (const auto &[frame, info] : frames) {
		size_t frameSize;
		switch (info.type) {
		case TileType::TransparentSquare: {
			const uint32_t srcFrameBegin = SDL_SwapLE32(srcOffsets[frame]);
			if (info.isFloor()) {
				uint8_t out[1024];
				uint8_t *outIt = out;
				const uint8_t *src = &dungeonCels[srcFrameBegin];
				const TileType newType = info.isFloorLeft() ? TileType::LeftTriangle : TileType::RightTriangle;
				ReencodeFloorWithFoliage(outIt, src, newType);
				frameSize = outIt - out;
			} else {
				const uint32_t srcFrameEnd = SDL_SwapLE32(srcOffsets[frame + 1]);
				frameSize = srcFrameEnd - srcFrameBegin;
			}
		} break;
		case TileType::Square: {
			frameSize = DunFrameWidth * DunFrameHeight;
		} break;
		case TileType::LeftTriangle:
		case TileType::RightTriangle:
			frameSize = ReencodedTriangleFrameSize;
			break;
		case TileType::LeftTrapezoid:
		case TileType::RightTrapezoid:
			frameSize = ReencodedTrapezoidFrameSize;
			break;
		}
		result += frameSize;
	}
	return result;
}

} // namespace

void ReencodeDungeonCels(std::unique_ptr<std::byte[]> &dungeonCels, std::span<std::pair<uint16_t, DunFrameInfo>> frames)
{
	const auto *srcData = reinterpret_cast<const uint8_t *>(dungeonCels.get());
	const auto *srcOffsets = reinterpret_cast<const uint32_t *>(srcData);

	int numFoliage = 0;
	LogVerbose("Re-encoding dungeon CELs: {} frames, {} bytes",
	    FormatInteger(SDL_SwapLE32(srcOffsets[0])),
	    FormatInteger(SDL_SwapLE32(srcOffsets[SDL_SwapLE32(srcOffsets[0]) + 1])));

	const size_t outSize = GetReencodedSize(srcData, frames);
	std::unique_ptr<std::byte[]> result { new std::byte[outSize] };
	auto *const resultPtr = reinterpret_cast<uint8_t *>(result.get());
	WriteLE32(resultPtr, static_cast<uint32_t>(frames.size()));
	uint8_t *lookup = resultPtr + 4;
	uint8_t *out = resultPtr + (2 + frames.size()) * 4; // number of frames, frame offsets, file size
	for (const auto &[frame, info] : frames) {
		WriteLE32(lookup, static_cast<uint32_t>(out - resultPtr));
		lookup += 4;
		const uint32_t srcFrameBegin = SDL_SwapLE32(srcOffsets[frame]);
		const uint8_t *src = &srcData[srcFrameBegin];
		switch (info.type) {
		case TileType::TransparentSquare: {
			if (info.isFloor()) {
				const TileType newType = info.isFloorLeft() ? TileType::LeftTriangle : TileType::RightTriangle;
				ReencodeFloorWithFoliage(out, src, newType);
				++numFoliage;
			} else {
				const uint32_t srcFrameEnd = SDL_SwapLE32(srcOffsets[frame + 1]);
				const uint32_t size = srcFrameEnd - srcFrameBegin;
				std::memcpy(out, src, size);
				out += size;
			}
		} break;
		case TileType::Square:
			std::memcpy(out, src, DunFrameWidth * DunFrameHeight);
			out += DunFrameWidth * DunFrameHeight;
			break;
		case TileType::LeftTriangle:
			ReencodeDungeonCelsLeftTriangle(out, src);
			break;
		case TileType::RightTriangle:
			ReencodeDungeonCelsRightTriangle(out, src);
			break;
		case TileType::LeftTrapezoid:
			ReencodeDungeonCelsLeftTrapezoid(out, src);
			break;
		case TileType::RightTrapezoid:
			ReencodeDungeonCelsRightTrapezoid(out, src);
			break;
		}
	}
	WriteLE32(lookup, static_cast<uint32_t>(outSize));

	const auto *dstOffsets = reinterpret_cast<const uint32_t *>(resultPtr);
	LogVerbose(" Re-encoded dungeon CELs: {} frames, {} bytes. Extracted {} foliage tiles.",
	    FormatInteger(SDL_SwapLE32(dstOffsets[0])),
	    FormatInteger(SDL_SwapLE32(dstOffsets[SDL_SwapLE32(dstOffsets[0]) + 1])),
	    FormatInteger(numFoliage));

	dungeonCels = std::move(result);
}

std::vector<std::pair<uint16_t, uint16_t>> ComputeCelBlockAdjustments(std::span<std::pair<uint16_t, DunFrameInfo>> frames)
{
	std::vector<std::pair<uint16_t, uint16_t>> celBlockAdjustments;
	uint16_t lastFrameIndex = 0;
	uint16_t adjustment = 0;
	for (auto &[frame, info] : frames) {
		uint16_t diff = frame - lastFrameIndex - 1;
		if (diff > 0) celBlockAdjustments.emplace_back(frame, adjustment);
		adjustment += diff;
		lastFrameIndex = frame;
	}
	if (adjustment > 0) {
		celBlockAdjustments.emplace_back(std::numeric_limits<uint16_t>::max(), adjustment);
	}
	return celBlockAdjustments;
}

} // namespace devilution
