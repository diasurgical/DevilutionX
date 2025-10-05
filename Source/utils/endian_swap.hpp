#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_endian.h>
#else
#include <SDL_endian.h>
#endif

namespace devilution {

constexpr uint16_t Swap16LE(uint16_t val)
{
#ifdef USE_SDL3
	return SDL_Swap16LE(val);
#else
	return SDL_SwapLE16(val);
#endif
}

constexpr uint32_t Swap32LE(uint32_t val)
{
#ifdef USE_SDL3
	return SDL_Swap32LE(val);
#else
	return SDL_SwapLE32(val);
#endif
}

} // namespace devilution
