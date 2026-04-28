/**
 * @file dx.cpp
 *
 * Implementation of functions setting up the graphics pipeline.
 */
#include "engine/dx.h"

#ifdef USE_SDL3
#include <SDL3/SDL_video.h>
#else
#include <SDL.h>
#endif

#include "engine/palette.h"
#include "engine/render/renderer.h"

#include "init.hpp"
#include "utils/display.h"

namespace devilution {

void dx_init()
{
#ifndef USE_SDL1
	SDL_RaiseWindow(ghMainWnd);
	SDL_ShowWindow(ghMainWnd);
#endif

	GetRenderer().InitPalette();
	palette_init();

	GetRenderer().Init(ghMainWnd);
}

void dx_cleanup()
{
#ifndef USE_SDL1
	if (ghMainWnd != nullptr)
		SDL_HideWindow(ghMainWnd);
#endif

	GetRenderer().Shutdown();

	SDL_DestroyWindow(ghMainWnd);
}

} // namespace devilution
