// Compatibility wrappers for SDL 1 & 2.
#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#ifndef USE_SDL1
#define SDLC_KEYSTATE_LEFTCTRL SDL_SCANCODE_LCTRL
#define SDLC_KEYSTATE_RIGHTCTRL SDL_SCANCODE_RCTRL
#define SDLC_KEYSTATE_LEFTSHIFT SDL_SCANCODE_LSHIFT
#define SDLC_KEYSTATE_RIGHTSHIFT SDL_SCANCODE_RSHIFT
#define SDLC_KEYSTATE_LALT SDL_SCANCODE_LALT
#define SDLC_KEYSTATE_RALT SDL_SCANCODE_RALT
#define SDLC_KEYSTATE_UP SDL_SCANCODE_UP
#define SDLC_KEYSTATE_DOWN SDL_SCANCODE_DOWN
#define SDLC_KEYSTATE_LEFT SDL_SCANCODE_LEFT
#define SDLC_KEYSTATE_RIGHT SDL_SCANCODE_RIGHT
#else
#define SDLC_KEYSTATE_LEFTCTRL SDLK_LCTRL
#define SDLC_KEYSTATE_RIGHTCTRL SDLK_RCTRL
#define SDLC_KEYSTATE_LEFTSHIFT SDLK_LSHIFT
#define SDLC_KEYSTATE_RIGHTSHIFT SDLK_LSHIFT
#define SDLC_KEYSTATE_LALT SDLK_LALT
#define SDLC_KEYSTATE_RALT SDLK_RALT
#define SDLC_KEYSTATE_UP SDLK_UP
#define SDLC_KEYSTATE_DOWN SDLK_DOWN
#define SDLC_KEYSTATE_LEFT SDLK_LEFT
#define SDLC_KEYSTATE_RIGHT SDLK_RIGHT
#endif

#ifdef USE_SDL3
inline const bool *SDLC_GetKeyState()
#else
inline const Uint8 *SDLC_GetKeyState()
#endif
{
#ifndef USE_SDL1
	return SDL_GetKeyboardState(nullptr);
#else
	return SDL_GetKeyState(nullptr);
#endif
}

// Sets the palette's colors and:
// SDL3 and SDL2: Points the surface's palette to the given palette if necessary.
// SDL1: Sets the surface's colors.
inline bool SDLC_SetSurfaceAndPaletteColors(SDL_Surface *surface, SDL_Palette *palette, SDL_Color *colors, int firstcolor, int ncolors)
{
#if defined(USE_SDL1)
	if (ncolors > (palette->ncolors - firstcolor)) {
		SDL_SetError("ncolors > (palette->ncolors - firstcolor)");
		return false;
	}
	if (colors != (palette->colors + firstcolor))
		SDL_memcpy(palette->colors + firstcolor, colors, ncolors * sizeof(*colors));

#if SDL1_VIDEO_MODE_BPP == 8
	// When the video surface is 8bit, we need to set the output palette as well.
	SDL_Surface *videoSurface = SDL_GetVideoSurface();
	SDL_SetColors(videoSurface, colors, firstcolor, ncolors);
	if (videoSurface == surface) return true;
#endif
	// In SDL1, the surface always has its own distinct palette, so we need to
	// update it as well.
	return SDL_SetPalette(surface, SDL_LOGPAL, colors, firstcolor, ncolors) == 0;
#elif defined(USE_SDL3)
	if (!SDL_SetPaletteColors(palette, colors, firstcolor, ncolors)) return false;
	if (SDL_GetSurfacePalette(surface) != palette) {
		if (!SDL_SetSurfacePalette(surface, palette)) return false;
	}
	return true;
#else
	if (SDL_SetPaletteColors(palette, colors, firstcolor, ncolors) < 0)
		return false;
	if (surface->format->palette != palette)
		return SDL_SetSurfacePalette(surface, palette);
	return true;
#endif
}
