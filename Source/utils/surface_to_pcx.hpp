#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>

#include "utils/sdl_compat.h"
#endif

#include <expected.hpp>

#include "engine/surface.hpp"

namespace devilution {

tl::expected<void, std::string>
WriteSurfaceToFilePcx(const Surface &buf, SDL_IOStream *outStream);

} // namespace devilution
