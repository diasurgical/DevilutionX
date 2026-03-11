#pragma once
// Backports for older versions of SDL 2.

#include <SDL.h>

#ifndef SDL_MAX_UINT32
#define SDL_MAX_UINT32 ((Uint32)0xFFFFFFFFu)
#endif

#ifndef SDL_TICKS_PASSED
#define SDL_TICKS_PASSED(A, B) ((Sint32)((B) - (A)) <= 0)
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 4)
inline SDL_bool SDL_PointInRect(const SDL_Point *p, const SDL_Rect *r)
{
	return ((p->x >= r->x) && (p->x < (r->x + r->w))
	           && (p->y >= r->y) && (p->y < (r->y + r->h)))
	    ? SDL_TRUE
	    : SDL_FALSE;
}
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 5)
inline SDL_Surface *
SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth,
    Uint32 format)
{
	int bpp;
	Uint32 rmask, gmask, bmask, amask;
	if (!SDL_PixelFormatEnumToMasks(format, &bpp, &rmask, &gmask, &bmask, &amask))
		return NULL;
	return SDL_CreateRGBSurface(flags, width, height, bpp, rmask, gmask, bmask, amask);
}

// From SDL 2.0.9.
inline SDL_Surface *
SDL_CreateRGBSurfaceWithFormatFrom(void *pixels,
    int width, int height, int depth, int pitch,
    Uint32 format)
{
	SDL_Surface *surface;

	surface = SDL_CreateRGBSurfaceWithFormat(0, 0, 0, depth, format);
	if (surface != NULL) {
		surface->flags |= SDL_PREALLOC;
		surface->pixels = pixels;
		surface->w = width;
		surface->h = height;
		surface->pitch = pitch;
		SDL_SetClipRect(surface, NULL);
	}
	return surface;
}
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 14)
inline SDL_bool
SDL_GameControllerHasButton(SDL_GameController *gamecontroller, SDL_GameControllerButton button)
{
	SDL_GameControllerButtonBind bind = SDL_GameControllerGetBindForButton(gamecontroller, button);
	return (bind.bindType != SDL_CONTROLLER_BINDTYPE_NONE) ? SDL_TRUE : SDL_FALSE;
}
#endif
