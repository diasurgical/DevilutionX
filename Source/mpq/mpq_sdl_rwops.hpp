#pragma once

#include <string_view>

#include "mpq/mpq_reader.hpp"

// Forward-declare the SDL type for the active SDL version.
#ifdef USE_SDL3
struct SDL_IOStream;
using SdlRwopsType = SDL_IOStream;
#else
struct SDL_RWops;
using SdlRwopsType = SDL_RWops;
#endif

namespace devilution {

SdlRwopsType *SDL_RWops_FromMpqFile(MpqArchive &archive,
    uint32_t hashIndex,
    std::string_view filename,
    bool threadsafe);

} // namespace devilution
