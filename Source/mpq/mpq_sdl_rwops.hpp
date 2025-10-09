#pragma once

#include <cstdint>
#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>

#include "utils/sdl_compat.h"
#endif

#include "mpq/mpq_reader.hpp"

namespace devilution {

SDL_IOStream *SDL_RWops_FromMpqFile(MpqArchive &mpqArchive, uint32_t fileNumber, std::string_view filename, bool threadsafe);

} // namespace devilution
