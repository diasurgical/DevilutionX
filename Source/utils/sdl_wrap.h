#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

#include "appfat.h"
#include "utils/sdl_ptrs.h"

#define NonNull(x) NullErrDlg(x, __FILE__, __LINE__)

namespace devilution {

namespace SDLWrap {

template <typename T>
T NullErrDlg(T x, const char *file, int line)
{
	if (x == nullptr)
		ErrDlg("SDL Error", SDL_GetError(), file, line);
	return x;
}

inline SDLSurfaceUniquePtr CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
#ifdef USE_SDL3
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateSurface(width, height, SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask))) };
#else
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateRGBSurface(flags, width, height, depth, Rmask, Gmask, Bmask, Amask)) };
#endif
}

#ifdef USE_SDL3
inline SDLSurfaceUniquePtr CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, SDL_PixelFormat format)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateSurface(width, height, format)) };
}
#else
inline SDLSurfaceUniquePtr CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateRGBSurfaceWithFormat(flags, width, height, depth, format)) };
}
#endif

#ifdef USE_SDL3
inline SDLSurfaceUniquePtr CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth, int pitch, SDL_PixelFormat format)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateSurfaceFrom(width, height, format, pixels, pitch)) };
}
#else
inline SDLSurfaceUniquePtr CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth, int pitch, Uint32 format)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, depth, pitch, format)) };
}
#endif

#ifndef USE_SDL1
inline SDLSurfaceUniquePtr ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags)
#else
inline SDLSurfaceUniquePtr ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, Uint32 flags)
#endif
{
#ifdef USE_SDL3
	return SDLSurfaceUniquePtr { NonNull(SDL_ConvertSurface(src, *fmt)) };
#else
	return SDLSurfaceUniquePtr { NonNull(SDL_ConvertSurface(src, fmt, flags)) };
#endif
}

#ifndef USE_SDL1
#ifdef USE_SDL3
inline SDLSurfaceUniquePtr ConvertSurfaceFormat(SDL_Surface *src, SDL_PixelFormat format, Uint32 flags)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_ConvertSurface(src, format)) };
}
#else
inline SDLSurfaceUniquePtr ConvertSurfaceFormat(SDL_Surface *src, Uint32 format, Uint32 flags)
{
	return SDLSurfaceUniquePtr { NonNull(SDL_ConvertSurfaceFormat(src, format, flags)) };
}
#endif

#ifdef USE_SDL3
inline SDLTextureUniquePtr CreateTexture(SDL_Renderer *renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h)
{
	return SDLTextureUniquePtr { NonNull(SDL_CreateTexture(renderer, format, access, w, h)) };
}
#else
inline SDLTextureUniquePtr CreateTexture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h)
{
	return SDLTextureUniquePtr { NonNull(SDL_CreateTexture(renderer, format, access, w, h)) };
}
#endif
#endif

inline SDLPaletteUniquePtr AllocPalette(int ncolors = 256)
{
#ifdef USE_SDL3
	return SDLPaletteUniquePtr { NonNull(SDL_CreatePalette(ncolors)) };
#else
	return SDLPaletteUniquePtr { NonNull(SDL_AllocPalette(ncolors)) };
#endif
}

} // namespace SDLWrap

} // namespace devilution
