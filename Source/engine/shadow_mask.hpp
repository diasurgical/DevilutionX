#pragma once
/**
 * @file shadow_mask.hpp
 *
 * @brief Shadow masks: the shadows extracted from sprites, stored as horizontal spans.
 *
 * A shadow is a single-colour shape that is drawn by darkening what is underneath it,
 * so it is stored as spans rather than as pixels.
 *
 * Shadow mask of a single sprite:
 *
 *  Bytes |  Type   | Value
 * :-----:|:-------:|------------------------------------------------------
 *    0   | uint8_t | number of sprite rows below the lowest shadow row
 *    1   | uint8_t | number of shadow rows
 *   2..  |  rows   | from the lowest row upwards
 *
 * Each row is a sequence of spans sorted by x. A span is 2 bytes:
 *
 *  Byte | Bits | Value
 * :----:|:----:|--------------------------------------------
 *   0   | 0..7 | x coordinate of the first pixel
 *   1   | 0..6 | width
 *   1   |  7   | set if this is the last span of the row
 *
 * A row without shadow pixels is a single span of width 0.
 * Runs longer than `MaxShadowSpanWidth` are stored as several spans.
 *
 * Rows keep their own spans, with absolute x coordinates, instead of letting runs flow from
 * one row into the next like CLX does: that way a span can be drawn or clipped on its own,
 * without tracking where the previous one ended.
 *
 * Like CLX sprites, masks are stored bottom-up, so that a mask can be drawn knowing only the
 * position of the bottom-left corner of its sprite.
 *
 * The mask of a sprite is stored in the CLX frame header of that sprite, after the height,
 * so it is available wherever the sprite is. Sprites without a shadow have a plain header.
 */

#include <cstddef>
#include <cstdint>

#include "appfat.h"
#include "engine/clx_sprite.hpp"

namespace devilution {

constexpr size_t ShadowMaskHeaderSize = 2;

/** The largest sprite that a shadow mask can describe. */
constexpr unsigned MaxShadowMaskSpriteWidth = 256;
constexpr unsigned MaxShadowMaskSpriteHeight = 255;

/** The largest shadow mask, in bytes: it has to fit in a CLX frame header. */
constexpr size_t MaxShadowMaskSize = 0xFFFF - ClxSprite::BaseHeaderSize;

constexpr uint8_t MaxShadowSpanWidth = 0x7F;
constexpr uint8_t ShadowSpanEndOfRow = 0x80;

struct ShadowSpan {
	uint8_t x;
	uint8_t width;
};

/**
 * @brief The shadow mask of a single sprite.
 */
class ShadowMask {
public:
	explicit constexpr ShadowMask(const uint8_t *data)
	    : data_(data)
	{
		assert(data != nullptr);
	}

	/** @brief The number of sprite rows below the lowest shadow row. */
	[[nodiscard]] constexpr uint8_t bottomOffset() const
	{
		return data_[0];
	}

	[[nodiscard]] constexpr uint8_t numRows() const
	{
		return data_[1];
	}

	[[nodiscard]] constexpr bool empty() const
	{
		return numRows() == 0;
	}

	/** @brief The rows, from the lowest one upwards. */
	[[nodiscard]] constexpr const uint8_t *rows() const
	{
		return &data_[ShadowMaskHeaderSize];
	}

	/**
	 * @brief Calls `fn(rowFromBottom, span)` for every span.
	 *
	 * `rowFromBottom` is the number of sprite rows below the span.
	 */
	template <typename F>
	constexpr void forEachSpan(F &&fn) const
	{
		const uint8_t *src = rows();
		uint16_t row = bottomOffset();
		for (unsigned i = numRows(); i != 0; --i, ++row) {
			uint8_t widthAndFlag;
			do {
				widthAndFlag = src[1];
				const uint8_t width = widthAndFlag & MaxShadowSpanWidth;
				if (width != 0) fn(row, ShadowSpan { src[0], width });
				src += 2;
			} while ((widthAndFlag & ShadowSpanEndOfRow) == 0);
		}
	}

private:
	const uint8_t *data_;
};

/** @brief Whether the sprite carries a shadow mask. */
[[nodiscard]] constexpr bool HasShadowMask(ClxSprite sprite)
{
	return sprite.headerSize() > ClxSprite::BaseHeaderSize;
}

/**
 * @brief The shadow mask of the sprite.
 *
 * Requires `HasShadowMask(sprite)`.
 */
[[nodiscard]] constexpr ShadowMask GetShadowMask(ClxSprite sprite)
{
	assert(HasShadowMask(sprite));
	return ShadowMask { sprite.headerExtension() };
}

} // namespace devilution
