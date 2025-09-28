#pragma once

#include <cstdint>

#include "appfat.h"
#include "utils/attributes.h"

namespace devilution {

[[nodiscard]] constexpr bool IsClxOpaque(uint8_t control)
{
	constexpr uint8_t ClxOpaqueMin = 0x80;
	return control >= ClxOpaqueMin;
}

[[nodiscard]] constexpr uint8_t GetClxOpaquePixelsWidth(uint8_t control)
{
	return -static_cast<int8_t>(control);
}

[[nodiscard]] constexpr bool IsClxOpaqueFill(uint8_t control)
{
	constexpr uint8_t ClxFillMax = 0xBE;
	return control <= ClxFillMax;
}

[[nodiscard]] constexpr uint8_t GetClxOpaqueFillWidth(uint8_t control)
{
	constexpr uint8_t ClxFillEnd = 0xBF;
	return ClxFillEnd - control;
}

struct SkipSize {
	int_fast16_t wholeLines;
	int_fast16_t xOffset;
};

// Returns the number of lines and the x-offset by which the rendering has overrun
// the current line (when a CLX command overruns the current line).
//
// Requires: remainingWidth <= 0.
DVL_ALWAYS_INLINE SkipSize GetSkipSize(int_fast16_t remainingWidth, int_fast16_t srcWidth)
{
	// If `remainingWidth` is negative, `-remainingWidth` is the overrun.
	// Otherwise, `remainingWidth` is always 0.

	// Remaining width of 0 (= no overrun) is a common case.
	// The calculation below would result in the same result.
	// However, checking for 0 and skipping it entirely turns out to be faster.
	if (remainingWidth == 0) return { 1, 0 };

	const auto overrun = static_cast<uint_fast16_t>(-remainingWidth);
	const uint_fast16_t overrunLines = overrun / srcWidth + 1;
	const uint_fast16_t xOffset = overrun % srcWidth;
	return { static_cast<int_fast16_t>(overrunLines), static_cast<int_fast16_t>(xOffset) };
}

} // namespace devilution
