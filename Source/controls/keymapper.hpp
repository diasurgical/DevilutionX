#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

namespace devilution {

void KeymapperPress(SDL_Keycode key);
void KeymapperRelease(SDL_Keycode key);

} // namespace devilution
