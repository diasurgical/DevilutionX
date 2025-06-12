#pragma once

#include <SDL.h>

#include "controls/controller.h"
#include "controls/controller_motion.h"

namespace devilution {

inline int PollEventCustom(SDL_Event *event, int (*poll)(SDL_Event *event))
{
	int result = poll(event);
	if (result != 0) {
		UnlockControllerState(*event);
		ProcessControllerMotion(*event);
	}

	return result;
}

inline int PollEvent(SDL_Event *event)
{
	return PollEventCustom(event, SDL_PollEvent);
}

} // namespace devilution
