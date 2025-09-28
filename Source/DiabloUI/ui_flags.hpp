#pragma once

#include <cstdint>

#include "utils/enum_traits.h"

namespace devilution {

enum class UiFlags : uint32_t {
	// clang-format off
	None               = 0,

	FontSize12         = 1 << 0,
	FontSize24         = 1 << 1,
	FontSize30         = 1 << 2,
	FontSize42         = 1 << 3,
	FontSize46         = 1 << 4,
	FontSizeDialog     = 1 << 5,

	ColorUiGold        = 1 << 6,
	ColorUiSilver      = 1 << 7,
	ColorUiGoldDark    = 1 << 8,
	ColorUiSilverDark  = 1 << 9,
	ColorDialogWhite   = 1 << 10,
	ColorDialogYellow  = 1 << 11,
	ColorDialogRed     = 1 << 12,
	ColorYellow        = 1 << 13,
	ColorGold          = 1 << 14,
	ColorBlack         = 1 << 15,
	ColorWhite         = 1 << 16,
	ColorWhitegold     = 1 << 17,
	ColorRed           = 1 << 18,
	ColorBlue          = 1 << 19,
	ColorOrange        = 1 << 20,
	ColorButtonface    = 1 << 21,
	ColorButtonpushed  = 1 << 22,

	AlignCenter        = 1 << 23,
	AlignRight         = 1 << 24,
	VerticalCenter     = 1 << 25,

	KerningFitSpacing  = 1 << 26,

	ElementDisabled    = 1 << 27,
	ElementHidden      = 1 << 28,

	PentaCursor        = 1 << 29,
	Outlined           = 1 << 30,

	/** @brief Ensures that the if current element is active that the next element is also visible. */
	NeedsNextElement   = 1U << 31U,
	// clang-format on
};
use_enum_as_flags(UiFlags);

} // namespace devilution
