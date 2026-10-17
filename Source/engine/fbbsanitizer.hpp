#pragma once

#include <cstdint>
#include <string_view>

#include "utils/stdcompat/shared_ptr_array.hpp"

// Forward-declare the SDL type for the active SDL version.
#ifdef USE_SDL3
struct SDL_IOStream;
using SdlRwopsType = SDL_IOStream;
#else
struct SDL_RWops;
using SdlRwopsType = SDL_RWops;
#endif

namespace devilution {

bool IsFbbAsset(std::string_view filename);
bool FbbAssetNeedsSanitizer();
SdlRwopsType *OpenSanitizedFbbAsSdlRwOps(bool threadsafe = false);
ArraySharedPtr<std::uint8_t> LoadSanitizedFbbAsset(size_t &size);

} // namespace devilution
