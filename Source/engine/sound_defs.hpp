#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_version.h>
#else
#include <SDL_version.h>
#endif

#define VOLUME_MIN -1600
#define VOLUME_MAX 0
#define VOLUME_STEPS 64

#define ATTENUATION_MIN -6400
#define ATTENUATION_MAX 0

#define PAN_MIN -6400
#define PAN_MAX 6400

#if SDL_VERSION_ATLEAST(2, 0, 7) && defined(DEVILUTIONX_RESAMPLER_SDL)
#define DVL_AULIB_SUPPORTS_SDL_RESAMPLER
#endif
