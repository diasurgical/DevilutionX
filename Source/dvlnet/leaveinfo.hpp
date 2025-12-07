#pragma once

#include <cstdint>

namespace devilution {
namespace net {

enum class leaveinfo_t : uint32_t {
	LEAVE_EXIT = 3,
	LEAVE_ENDING = 0x40000004,
	LEAVE_DROP = 0x40000006,
};

} // namespace net
} // namespace devilution
