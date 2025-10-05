#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>
#endif

#include <expected.hpp>

#include "engine/surface.hpp"

namespace devilution {

tl::expected<void, std::string>
WriteSurfaceToFilePcx(const Surface &buf,
#ifdef USE_SDL3
    SDL_IOStream *
#else
    SDL_RWops *
#endif
        outStream);

} // namespace devilution
