#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>
#else
#include <SDL.h>
#endif

#include "engine/assets.hpp"

#ifndef USE_SDL3
#ifdef __cplusplus
extern "C" {
#endif

const int IMG_INIT_PNG = 0x00000002;

int IMG_Init(int flags);
void IMG_Quit(void);
int IMG_isPNG(SDL_RWops *src);
SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *src);
int IMG_SavePNG(SDL_Surface *surface, const char *file);
int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst);

inline SDL_Surface *IMG_LoadPNG(const char *file)
{
	SDL_RWops *src = SDL_RWFromFile(file, "rb");
	return IMG_LoadPNG_RW(src);
}

#ifdef __cplusplus
}
#endif
#endif

namespace devilution {

#ifndef USE_SDL3
inline int InitPNG()
{
	return IMG_Init(IMG_INIT_PNG);
}

inline void QuitPNG()
{
	IMG_Quit();
}
#endif

inline SDL_Surface *LoadPNG(const char *file)
{
	auto *rwops = OpenAssetAsSdlRwOps(file);
#ifdef USE_SDL3
	SDL_Surface *surface = IMG_LoadPNG_IO(rwops);
	SDL_CloseIO(rwops);
#else
	SDL_Surface *surface = IMG_LoadPNG_RW(rwops);
	SDL_RWclose(rwops);
#endif
	return surface;
}

} // namespace devilution
