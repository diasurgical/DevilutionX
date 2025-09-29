#pragma once

#include <cstdint>
#include <cstring>

#include <SDL_endian.h>

namespace devilution {

inline void WriteLE16(void *out, uint16_t val)
{
	const uint16_t littleEndian = SDL_SwapLE16(val);
	memcpy(out, &littleEndian, 2);
}

inline void WriteLE32(void *out, uint32_t val)
{
	const uint32_t littleEndian = SDL_SwapLE32(val);
	memcpy(out, &littleEndian, 4);
}

} // namespace devilution
