#pragma once

#include <cstdint>
#include <cstring>

#include "utils/endian_swap.hpp"

namespace devilution {

inline void WriteLE16(void *out, uint16_t val)
{
	const uint16_t littleEndian = Swap16LE(val);
	memcpy(out, &littleEndian, 2);
}

inline void WriteLE32(void *out, uint32_t val)
{
	const uint32_t littleEndian = Swap32LE(val);
	memcpy(out, &littleEndian, 4);
}

} // namespace devilution
