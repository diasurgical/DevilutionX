#include "utils/paths.h"

#include <optional>
#include <string>
#include <string_view>

#include <SDL.h>

#include "appfat.h"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/sdl_ptrs.h"

#ifdef __IPHONEOS__
#include "platform/ios/ios_paths.h"
#endif

#ifdef NXDK
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

namespace paths {

namespace {

std::optional<std::string> basePath;
std::optional<std::string> prefPath;
std::optional<std::string> configPath;
std::optional<std::string> assetsPath;

void AddTrailingSlash(std::string &path)
{
	if (!path.empty() && path.back() != DirectorySeparator)
		path += DirectorySeparator;
}

std::string FromSDL(char *s)
{
	SDLUniquePtr<char> pinned(s);
	std::string result = (s != nullptr ? s : "");
	if (s == nullptr) {
		Log("{}", SDL_GetError());
		SDL_ClearError();
	}
	return result;
}

#ifdef NXDK
const std::string &NxdkGetPrefPath()
{
	static const std::string Path = []() {
		const char *path = "E:\\UDATA\\devilutionx\\";
		if (CreateDirectoryA(path, nullptr) == FALSE && ::GetLastError() != ERROR_ALREADY_EXISTS) {
			DirErrorDlg(path);
		}
		return path;
	}();
	return Path;
}
#endif

} // namespace

const std::string &BasePath()
{
	if (!basePath) {
		basePath = FromSDL(SDL_GetBasePath());
	}
	return *basePath;
}

const std::string &PrefPath()
{
	if (!prefPath) {
#if defined(__IPHONEOS__)
		prefPath = FromSDL(IOSGetPrefPath());
#elif defined(NXDK)
		prefPath = NxdkGetPrefPath();
#else
		prefPath = FromSDL(SDL_GetPrefPath("diasurgical", "devilution"));
#if !defined(__amigaos__)
		if (FileExistsAndIsWriteable("diablo.ini")) {
			prefPath = std::string();
		}
#endif
#endif
	}
	return *prefPath;
}

const std::string &ConfigPath()
{
	if (!configPath) {
#if defined(__IPHONEOS__)
		configPath = FromSDL(IOSGetPrefPath());
#elif defined(NXDK)
		configPath = NxdkGetPrefPath();
#else
		configPath = FromSDL(SDL_GetPrefPath("diasurgical", "devilution"));
#if !defined(__amigaos__)
		if (FileExistsAndIsWriteable("diablo.ini")) {
			configPath = std::string();
		}
#endif
#endif
	}
	return *configPath;
}

const std::string &AssetsPath()
{
	if (!assetsPath) {
#if __EMSCRIPTEN__
		assetsPath.emplace("assets/");
#elif defined(NXDK)
		assetsPath.emplace("D:\\assets\\");
#elif defined(__3DS__) || defined(__SWITCH__)
		assetsPath.emplace("romfs:/");
#elif defined(__APPLE__) && defined(USE_SDL1)
		// In `Info.plist` we have
		//
		//    <key>SDL_FILESYSTEM_BASE_DIR_TYPE</key>
		//    <string>resource</string>
		//
		// This means `SDL_GetBasePath()` returns exedir for non-bundled
		// and the `app_dir.app/Resources/` for bundles.
		//
		// Our built-in resources are directly in the `devilutionx.app/Resources` directory
		// and are normally looked up via a relative lookup in `FindAsset`.
		// In SDL2, this is implemented by calling `SDL_OpenFPFromBundleOrFallback`
		// from `SDL_RWFromFile` but SDL1 doesn't do it, so we set the directory explicitly.
		//
		// Note that SDL3 reverts to SDL1 behaviour!
		// https://github.com/libsdl-org/SDL/blob/962268ca21ed10b9cee31198c22681099293f20a/docs/README-migration.md?plain=1#L1623
		assetsPath.emplace(FromSDL(SDL_GetBasePath()));
#else
		assetsPath.emplace(FromSDL(SDL_GetBasePath()) + ("assets" DIRECTORY_SEPARATOR_STR));
#endif
	}
	return *assetsPath;
}

void SetBasePath(const std::string &path)
{
	basePath = path;
	AddTrailingSlash(*basePath);
}

void SetPrefPath(const std::string &path)
{
	prefPath = path;
	AddTrailingSlash(*prefPath);
}

void SetConfigPath(const std::string &path)
{
	configPath = path;
	AddTrailingSlash(*configPath);
}

void SetAssetsPath(const std::string &path)
{
	assetsPath = path;
	AddTrailingSlash(*assetsPath);
}

} // namespace paths

} // namespace devilution
