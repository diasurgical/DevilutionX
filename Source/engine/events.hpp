#pragma once

#include <cstdint>

#include <SDL.h>

#include "engine/point.hpp"

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

struct EventHandler {
	void (*handle)(const SDL_Event &event, uint16_t modState);
	int (*poll)(SDL_Event *event);
};

/** @brief The current input handler function */
extern EventHandler CurrentEventHandler;

EventHandler SetEventHandler(EventHandler NewHandler);

bool FetchMessage(SDL_Event *event, uint16_t *modState,
    int (*poll)(SDL_Event *event) = SDL_PollEvent);

void HandleMessage(const SDL_Event &event, uint16_t modState);

} // namespace devilution
