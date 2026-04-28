#pragma once

#include <cstdint>
#include <type_traits>

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#else
#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

#include "engine/render/renderer.h"
#include "utils/attributes.h"
#include "utils/log.hpp"
#include "utils/sdl_ptrs.h"
#include "utils/ui_fwd.h"

namespace devilution {

extern SDL_Window *window;
extern SDL_Window *ghMainWnd;

extern DVL_API_FOR_TEST Size forceResolution;

#ifdef USE_SDL1
void SetVideoMode(int width, int height, int bpp, uint32_t flags);
void SetVideoModeToPrimary(bool fullscreen, int width, int height);
#endif

bool IsFullScreen();

#ifndef USE_SDL1
void ReinitializeTexture();
#endif

void AdjustToScreenGeometry(Size windowSize);

#ifdef DEVILUTIONX_GL1_RENDERER
void SwitchRenderer();
#endif

// Convert from output coordinates to logical (resolution-independent) coordinates.
template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void OutputToLogical(T *x, T *y)
{
	int ix = static_cast<int>(*x);
	int iy = static_cast<int>(*y);
	GetRenderer().MapWindowToLogical(&ix, &iy);
	*x = static_cast<T>(ix);
	*y = static_cast<T>(iy);
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void LogicalToOutput(T *x, T *y)
{
	int ix = static_cast<int>(*x);
	int iy = static_cast<int>(*y);
	GetRenderer().MapLogicalToWindow(&ix, &iy);
	*x = static_cast<T>(ix);
	*y = static_cast<T>(iy);
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
SDL_DisplayMode GetNearestDisplayMode(Size preferredSize,
#ifdef USE_SDL3
    SDL_PixelFormat preferredPixelFormat = SDL_PIXELFORMAT_UNKNOWN
#else
    SDL_PixelFormatEnum preferredPixelFormat = SDL_PIXELFORMAT_UNKNOWN
#endif
);
#endif

} // namespace devilution
