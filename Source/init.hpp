/**
 * @file init.hpp
 *
 * Interface of routines for initializing the environment, disable screen saver, load MPQ.
 */
#pragma once

// Unused here but must be included before SDL.h, see:
// https://github.com/bebbo/amiga-gcc/issues/413
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#ifdef UNPACKED_MPQS
#include <string_view>
#else
#include "mpq/mpq_reader.hpp"
#endif

namespace devilution {

/** True if the game is the current active window */
extern bool gbActive;

[[nodiscard]] bool IsDevilutionXMpqOutOfDate();

#ifdef UNPACKED_MPQS
bool AreExtraFontsOutOfDate(std::string_view path);
#else
bool AreExtraFontsOutOfDate(MpqArchive &archive);
#endif

[[nodiscard]] bool AreExtraFontsOutOfDate();

void init_cleanup();
void init_create_window();
void MainWndProc(const SDL_Event &event);

} // namespace devilution
