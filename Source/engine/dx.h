/**
 * @file dx.h
 *
 * Interface of functions setting up the graphics pipeline.
 */
#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "engine/surface.hpp"

namespace devilution {

/** Whether we render directly to the screen surface, i.e. `PalSurface == GetOutputSurface()` */
extern bool RenderDirectlyToOutputSurface;

extern SDL_Surface *PalSurface;

Surface GlobalBackBuffer();

void dx_init();
void dx_cleanup();
void CreateBackBuffer();
void BltFast(SDL_Rect *srcRect, SDL_Rect *dstRect);
void Blit(SDL_Surface *src, SDL_Rect *srcRect, SDL_Rect *dstRect);
void RenderPresent();

} // namespace devilution
