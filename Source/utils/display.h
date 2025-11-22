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

#include "utils/attributes.h"
#include "utils/log.hpp"
#include "utils/sdl_ptrs.h"
#include "utils/ui_fwd.h"

namespace devilution {

extern int refreshDelay; // Screen refresh rate in nanoseconds
extern SDL_Window *window;
extern SDL_Window *ghMainWnd;
extern SDL_Renderer *renderer;
#ifndef USE_SDL1
extern SDLTextureUniquePtr texture;
#endif

extern SDLPaletteUniquePtr Palette;
extern SDL_Surface *PalSurface;
extern DVL_API_FOR_TEST Size forceResolution;

#ifdef USE_SDL1
void SetVideoMode(int width, int height, int bpp, uint32_t flags);
void SetVideoModeToPrimary(bool fullscreen, int width, int height);
#endif

bool IsFullScreen();

// Returns:
// SDL1: Video surface.
// SDL2, no upscale: Window surface.
// SDL2, upscale: Renderer texture surface.
SDL_Surface *GetOutputSurface();

// Whether the output surface requires software scaling.
// Always returns false on SDL2.
bool OutputRequiresScaling();

// Scales rect if necessary.
void ScaleOutputRect(SDL_Rect *rect);

// If the output requires software scaling, replaces the given surface with a scaled one.
SDLSurfaceUniquePtr ScaleSurfaceToOutput(SDLSurfaceUniquePtr surface);

// Convert from output coordinates to logical (resolution-independent) coordinates.
template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void OutputToLogical(T *x, T *y)
{
#ifdef USE_SDL3
	if (renderer == nullptr) return;
	float outX, outY;
	if (!SDL_RenderCoordinatesFromWindow(renderer, *x, *y, &outX, &outY)) {
		LogError("SDL_RenderCoordinatesFromWindow: {}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	*x = static_cast<T>(outX);
	*y = static_cast<T>(outY);
#elif !defined(USE_SDL1)
	if (renderer == nullptr) return;
	float scaleX;
	SDL_RenderGetScale(renderer, &scaleX, nullptr);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<T>(*x / scale);
	*y = static_cast<T>(*y / scale);

	SDL_Rect view;
	SDL_RenderGetViewport(renderer, &view);
	*x -= view.x;
	*y -= view.y;
#else
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * gnScreenWidth / surface->w;
	*y = *y * gnScreenHeight / surface->h;
#endif
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void LogicalToOutput(T *x, T *y)
{
#ifdef USE_SDL3
	if (renderer == nullptr) return;
	float outX, outY;
	if (!SDL_RenderCoordinatesToWindow(renderer, *x, *y, &outX, &outY)) {
		LogError("SDL_RenderCoordinatesFromWindow: {}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	*x = static_cast<T>(outX);
	*y = static_cast<T>(outY);
#elif !defined(USE_SDL1)
	if (renderer == nullptr) return;
	SDL_Rect view;
	SDL_RenderGetViewport(renderer, &view);
	*x += view.x;
	*y += view.y;

	float scaleX;
	SDL_RenderGetScale(renderer, &scaleX, nullptr);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<T>(*x * scale);
	*y = static_cast<T>(*y * scale);
#else
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * surface->w / gnScreenWidth;
	*y = *y * surface->h / gnScreenHeight;
#endif
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
