#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "controller_buttons.h"
#include "utils/static_vector.hpp"

namespace devilution {

// Must be called exactly once at the start of each SDL input event.
void UnlockControllerState(const SDL_Event &event);

StaticVector<ControllerButtonEvent, 4> ToControllerButtonEvents(const SDL_Event &event);

bool IsControllerButtonPressed(ControllerButton button);
bool IsControllerButtonComboPressed(ControllerButtonCombo combo);

bool HandleControllerAddedOrRemovedEvent(const SDL_Event &event);

} // namespace devilution
