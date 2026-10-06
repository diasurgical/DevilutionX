/**
 * @file cursor_defs.hpp
 *
 * Cursor and selection region enums with no heavy dependencies.
 */
#pragma once

#include <cstdint>

#include "utils/enum_traits.h"

namespace devilution {

enum class SelectionRegion : uint8_t {
	None = 0,
	Bottom = 1U << 0,
	Middle = 1U << 1,
	Top = 1U << 2,
};
use_enum_as_flags(SelectionRegion);

enum cursor_id : uint8_t {
	CURSOR_NONE,
	CURSOR_HAND,
	CURSOR_IDENTIFY,
	CURSOR_REPAIR,
	CURSOR_RECHARGE,
	CURSOR_DISARM,
	CURSOR_OIL,
	CURSOR_TELEKINESIS,
	CURSOR_RESURRECT,
	CURSOR_TELEPORT,
	CURSOR_HEALOTHER,
	CURSOR_HOURGLASS,
	CURSOR_FIRSTITEM,
};

} // namespace devilution
