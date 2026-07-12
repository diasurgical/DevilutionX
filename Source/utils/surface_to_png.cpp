#include "utils/surface_to_png.hpp"

#include <expected>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>
#else
#include <SDL.h>

#include "utils/sdl_compat.h"
#endif

#include "engine/surface.hpp"

namespace devilution {

#ifndef USE_SDL3
extern "C" int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst);
#endif

std::expected<void, std::string>
WriteSurfaceToFilePng(const Surface &buf, SDL_IOStream *dst)
{
#ifdef USE_SDL3
	const bool ok = IMG_SavePNG_IO(buf.surface, dst, /*closeio=*/true);
#else
	const bool ok = IMG_SavePNG_RW(buf.surface, dst, /*freedst=*/1) == 0;
#endif
	if (!ok) {
		std::expected<void, std::string> result = std::unexpected(std::string(SDL_GetError()));
		SDL_ClearError();
		return result;
	}
	return {};
}

} // namespace devilution
