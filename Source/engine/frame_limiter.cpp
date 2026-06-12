/**
 * @file frame_limiter.cpp
 *
 * CPU sleep-based frame limiter, independent of renderer vsync.
 */
#include "engine/frame_limiter.h"

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <cstdint>

#include "options.h"

namespace devilution {

int refreshDelay;

void LimitFrameRate()
{
	if (*GetOptions().Graphics.frameRateControl != FrameRateControl::CPUSleep)
		return;
	static uint32_t frameDeadline;
	const uint32_t tc = SDL_GetTicks() * 1000;
	uint32_t v = 0;
	if (frameDeadline > tc) {
		v = tc % refreshDelay;
		SDL_Delay((v / 1000) + 1); // ceil
	}
	frameDeadline = tc + v + refreshDelay;
}

} // namespace devilution
