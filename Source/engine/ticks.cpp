#include "engine/ticks.hpp"

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

namespace devilution {

uint32_t GetAnimationFrame(uint32_t frames, uint32_t fps)
{
	return (SDL_GetTicks() / fps) % frames;
}

} // namespace devilution
