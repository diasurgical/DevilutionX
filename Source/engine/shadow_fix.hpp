#pragma once
/**
 * @file shadow_fix.hpp
 *
 * @brief Shadow corrections: where the shadow of a sprite differs from what its shadow rule finds.
 *
 * A shadow rule (see `shadow_rule.hpp`) cannot tell a black pixel of the sprite from its shadow
 * where the two overlap. Corrections list the pixels that it gets wrong. They are made offline,
 * from information that the game does not have when it loads a sprite, such as the other
 * equipment variants of the same character.
 *
 * The corrections of a sprite file are stored next to it, in a file with the same path and the
 * extension `.shadowfix`:
 *
 *    Bytes   |   Type   | Value
 * :---------:|:--------:|------------------------------------------------------------
 *    0..3    |  char[4] | "SHFX"
 *      4     |  uint8_t | version (1)
 *      5     |  uint8_t | reserved (0)
 *    6..7    | uint16_t | number of sprite lists (1 for a file that is a single list)
 *    8..     |  lists   |
 *
 * Each list:
 *
 *    Bytes   |   Type   | Value
 * :---------:|:--------:|------------------------------------------------------------
 *    0..1    | uint16_t | number of sprites in the list
 *    2..     |  frames  | one per sprite
 *
 * Each frame:
 *
 *    Bytes   |   Type   | Value
 * :---------:|:--------:|------------------------------------------------------------
 *    0..1    | uint16_t | size of the rest of the frame (0 if it has no corrections)
 *    2..5    | uint32_t | guard (see `ShadowFixGuard`)
 *    6..7    | uint16_t | size of the body mask
 *    8..     |   mask   | body mask: pixels taken for shadow that belong to the sprite
 *     ..     |   mask   | shadow mask: pixels kept in the sprite that are shadow
 *
 * Both masks have the format of a shadow mask (see `shadow_mask.hpp`). An empty mask has no bytes.
 * All integers are little-endian.
 *
 * Corrections only ever apply to black pixels, and only to a sprite whose black pixels are where
 * they were when the corrections were made: the guard of a frame is a hash of the size of the
 * sprite and of the positions of its black pixels. The corrections of a frame whose guard does
 * not match are skipped, and so is a file whose lists or sprite counts do not match the sprites.
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/shadow_mask.hpp"
#include "utils/endian_read.hpp"

namespace devilution {

constexpr char ShadowFixMagic[4] = { 'S', 'H', 'F', 'X' };
constexpr uint8_t ShadowFixVersion = 1;
constexpr size_t ShadowFixHeaderSize = 8;
constexpr char ShadowFixExtension[] = ".shadowfix";

/**
 * @brief Hashes what the corrections of a frame depend on.
 *
 * @param blackPixels the indices (`y * width + x`, rows top-down) of the black pixels, in order.
 */
[[nodiscard]] constexpr uint32_t ShadowFixGuard(uint16_t width, uint16_t height, std::span<const uint32_t> blackPixels)
{
	// FNV-1a, a value at a time instead of a byte at a time.
	constexpr uint32_t Prime = 16777619U;
	uint32_t hash = 2166136261U;
	hash = (hash ^ width) * Prime;
	hash = (hash ^ height) * Prime;
	for (const uint32_t index : blackPixels)
		hash = (hash ^ index) * Prime;
	return hash;
}

/** @brief The corrections of a single sprite. */
struct ShadowFixFrame {
	uint32_t guard = 0;
	/** Shadow masks, including their headers. Empty if there is nothing to correct. */
	std::span<const uint8_t> body;
	std::span<const uint8_t> shadow;

	[[nodiscard]] constexpr bool empty() const
	{
		return body.empty() && shadow.empty();
	}
};

/**
 * @brief Whether the data is a well-formed shadow mask that stays within a sprite of the given size.
 */
[[nodiscard]] constexpr bool IsValidShadowMask(std::span<const uint8_t> data, unsigned width, unsigned height)
{
	if (data.size() < ShadowMaskHeaderSize) return false;
	const unsigned numRows = data[1];
	if (data[0] + numRows > height) return false;
	size_t pos = ShadowMaskHeaderSize;
	for (unsigned row = 0; row < numRows; ++row) {
		uint8_t widthAndFlag = 0;
		do {
			if (pos + 2 > data.size()) return false;
			widthAndFlag = data[pos + 1];
			if (data[pos] + static_cast<unsigned>(widthAndFlag & MaxShadowSpanWidth) > width) return false;
			pos += 2;
		} while ((widthAndFlag & ShadowSpanEndOfRow) == 0);
	}
	return pos == data.size();
}

/**
 * @brief Reads the corrections of a sprite file, list by list and sprite by sprite, in order.
 *
 * Once anything does not match or is malformed, the reader fails and returns no further corrections.
 */
class ShadowFixReader {
public:
	/** @param numLists the number of lists of the sprite file (1 for a single list). */
	constexpr ShadowFixReader(std::span<const uint8_t> data, uint16_t numLists)
	    : data_(data)
	{
		ok_ = data.size() >= ShadowFixHeaderSize
		    && data[0] == ShadowFixMagic[0] && data[1] == ShadowFixMagic[1] && data[2] == ShadowFixMagic[2] && data[3] == ShadowFixMagic[3]
		    && data[4] == ShadowFixVersion
		    && LoadLE16(&data[6]) == numLists;
		pos_ = ShadowFixHeaderSize;
	}

	[[nodiscard]] constexpr bool ok() const
	{
		return ok_;
	}

	/** @brief Starts the next list, which must have the given number of sprites. */
	constexpr bool beginList(uint32_t numSprites)
	{
		if (!ok_ || framesLeft_ != 0 || pos_ + 2 > data_.size() || LoadLE16(&data_[pos_]) != numSprites) return ok_ = false;
		pos_ += 2;
		framesLeft_ = numSprites;
		return true;
	}

	/**
	 * @brief The corrections of the next sprite of the list, which has the given size.
	 *
	 * Empty if there are none or if the reader has failed.
	 */
	constexpr ShadowFixFrame nextFrame(unsigned width, unsigned height)
	{
		constexpr size_t FrameHeaderSize = 6;
		if (!ok_ || framesLeft_ == 0 || pos_ + 2 > data_.size()) {
			ok_ = false;
			return {};
		}
		--framesLeft_;
		const size_t size = LoadLE16(&data_[pos_]);
		pos_ += 2;
		if (size == 0) return {};
		if (size < FrameHeaderSize || pos_ + size > data_.size()) {
			ok_ = false;
			return {};
		}
		const std::span<const uint8_t> record = data_.subspan(pos_, size);
		pos_ += size;
		const size_t bodySize = LoadLE16(&record[4]);
		if (bodySize > size - FrameHeaderSize) {
			ok_ = false;
			return {};
		}
		ShadowFixFrame result;
		result.guard = LoadLE32(record.data());
		result.body = record.subspan(FrameHeaderSize, bodySize);
		result.shadow = record.subspan(FrameHeaderSize + bodySize);
		if ((!result.body.empty() && !IsValidShadowMask(result.body, width, height))
		    || (!result.shadow.empty() && !IsValidShadowMask(result.shadow, width, height))) {
			ok_ = false;
			return {};
		}
		return result;
	}

private:
	std::span<const uint8_t> data_;
	size_t pos_ = 0;
	uint32_t framesLeft_ = 0;
	bool ok_ = false;
};

} // namespace devilution
