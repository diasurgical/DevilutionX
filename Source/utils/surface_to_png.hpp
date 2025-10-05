#include <cstdio>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include <expected.hpp>

#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Writes the given surface to `dst` as PNG.
 *
 * Takes ownership of `dst` and closes it when done.
 */
tl::expected<void, std::string>
WriteSurfaceToFilePng(const Surface &buf,
#ifdef USE_SDL3
    SDL_IOStream *
#else
    SDL_RWops *
#endif
        dst);

} // namespace devilution
