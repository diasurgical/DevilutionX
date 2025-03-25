/**
 * @file dx.h
 *
 * Interface of functions setting up the graphics pipeline.
 */
#pragma once

#include <SDL.h>

#include "engine/surface.hpp"

namespace devilution {

/** Whether we render directly to the screen surface, i.e. `PalSurface == GetOutputSurface()` */
extern bool RenderDirectlyToOutputSurface;

extern SDL_Surface *PalSurface;

extern float CurrentZoomLevel;
extern float MinZoom;
extern float MaxZoom;

Surface GlobalBackBuffer();

void dx_init();
void dx_cleanup();
void CreateBackBuffer();
void InitPalette();
void BltFast(SDL_Rect *srcRect, SDL_Rect *dstRect);
void Blit(SDL_Surface *src, SDL_Rect *srcRect, SDL_Rect *dstRect);
void RenderPresent();
void PaletteGetEntries(int dwNumEntries, SDL_Color *lpEntries);
void UpdateZoomLimits();

} // namespace devilution
