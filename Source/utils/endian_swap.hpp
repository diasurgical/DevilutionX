#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_endian.h>
#else
#include <SDL_endian.h>
#endif

namespace devilution {

inline uint16_t Swap16LE(uint16_t val)
{
#ifdef USE_SDL3
	return SDL_Swap16LE(val);
#else
	return SDL_SwapLE16(val);
#endif
}

inline uint16_t Swap16BE(uint16_t val)
{
#ifdef USE_SDL3
	return SDL_Swap16BE(val);
#else
	return SDL_SwapBE16(val);
#endif
}

inline uint32_t Swap32LE(uint32_t val)
{
#ifdef USE_SDL3
	return SDL_Swap32LE(val);
#else
	return SDL_SwapLE32(val);
#endif
}

inline int32_t SwapSigned32LE(int32_t val)
{
	return static_cast<int32_t>(Swap32LE(static_cast<uint32_t>(val)));
}

inline uint32_t Swap32BE(uint32_t val)
{
#ifdef USE_SDL3
	return SDL_Swap32BE(val);
#else
	return SDL_SwapBE32(val);
#endif
}

inline uint64_t Swap64LE(uint64_t val)
{
#ifdef USE_SDL3
	return SDL_Swap64LE(val);
#else
	return SDL_SwapLE64(val);
#endif
}

inline uint64_t Swap64BE(uint64_t val)
{
#ifdef USE_SDL3
	return SDL_Swap64BE(val);
#else
	return SDL_SwapBE64(val);
#endif
}

} // namespace devilution
