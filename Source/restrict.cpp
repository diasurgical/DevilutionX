/**
 * @file restrict.cpp
 *
 * Implementation of functionality for checking if the game will be able run on the system.
 */

#include <string>

#include "appfat.h"
#include "utils/file_util.h"
#include "utils/paths.h"

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>
#endif

#include "utils/sdl_compat.h"

namespace devilution {

void ReadOnlyTest()
{
#ifdef __DREAMCAST__
	// On Dreamcast, VMU filesystem has been verified in InitDreamcast().
	// SDL_IOFromFile doesn't work reliably with KOS's /vmu/ paths,
	// but direct file operations do work (as shown by "VMUFS: file written").
	// Skip this test - saves will fail gracefully if VMU is unavailable.
	return;
#endif
	const std::string path = paths::PrefPath() + "Diablo1ReadOnlyTest.foo";
	SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "w");
	if (file == nullptr) {
		DirErrorDlg(paths::PrefPath());
	}
	SDL_CloseIO(file);
	RemoveFile(path.c_str());
}

} // namespace devilution
