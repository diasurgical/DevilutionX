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

namespace devilution {

void ReadOnlyTest()
{
	const std::string path = paths::PrefPath() + "Diablo1ReadOnlyTest.foo";
#ifdef USE_SDL3
	SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "w");
#else
	SDL_RWops *file = SDL_RWFromFile(path.c_str(), "w");
#endif
	if (file == nullptr) {
		DirErrorDlg(paths::PrefPath());
	}

#ifdef USE_SDL3
	SDL_CloseIO(file);
#else
	SDL_RWclose(file);
#endif
	RemoveFile(path.c_str());
}

} // namespace devilution
