#include "platform/ctr/display.hpp"
#include <SDL.h>

#include <cstdint>
uint32_t Get3DSScalingFlag(bool fitToScreen, int width, int height)
{
	if (fitToScreen)
		return SDL_FULLSCREEN;
	if (width * 3 < height * 5)
		return SDL_FITHEIGHT;
	return SDL_FITWIDTH;
}
