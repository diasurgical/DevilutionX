/**
 * @file init.cpp
 *
 * Implementation of routines for initializing the environment, disable screen saver, load MPQ.
 */
#include "init.hpp"

#include <cstdint>
#include <string>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#else
#include <SDL.h>
#endif

#include <config.h>

#include "DiabloUI/diabloui.h"
#include "engine/assets.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "engine/events.hpp"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "pfile.h"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/str_split.hpp"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

#ifndef UNPACKED_MPQS
#include "mpq/mpq_common.hpp"
#include "mpq/mpq_reader.hpp"
#endif

#ifdef __vita__
// increase default allowed heap size on Vita
int _newlib_heap_size_user = 100 * 1024 * 1024;
#endif

namespace devilution {

bool gbActive;

namespace {

constexpr char DevilutionXMpqVersion[] = "1\n";
constexpr char ExtraFontsVersion[] = "1\n";

bool AssetContentsEq(AssetRef &&ref, std::string_view expected)
{
	const size_t size = ref.size();
	AssetHandle handle = OpenAsset(std::move(ref), false);
	if (!handle.ok()) return false;
	const std::unique_ptr<char[]> contents { new char[size] };
	if (!handle.read(contents.get(), size)) return false;
	return std::string_view { contents.get(), size } == expected;
}

bool CheckDevilutionXMpqVersion(AssetRef &&ref)
{
	return !AssetContentsEq(std::move(ref), DevilutionXMpqVersion);
}

bool CheckExtraFontsVersion(AssetRef &&ref)
{
	return !AssetContentsEq(std::move(ref), ExtraFontsVersion);
}

} // namespace

bool IsDevilutionXMpqOutOfDate()
{
	AssetRef ref = FindAsset("ASSETS_VERSION");
	if (!ref.ok())
		return true;
	return CheckDevilutionXMpqVersion(std::move(ref));
}

#ifdef UNPACKED_MPQS
bool AreExtraFontsOutOfDate(std::string_view path)
{
	const std::string versionPath = StrCat(path, "fonts" DIRECTORY_SEPARATOR_STR "VERSION");
	if (versionPath.size() + 1 > AssetRef::PathBufSize)
		app_fatal("Path too long");
	AssetRef ref;
	*BufCopy(ref.path, versionPath) = '\0';
	return CheckExtraFontsVersion(std::move(ref));
}
#else
bool AreExtraFontsOutOfDate(MpqArchive &archive)
{
	const char filename[] = "fonts\\VERSION";
	const MpqFileHash fileHash = CalculateMpqFileHash(filename);
	uint32_t fileNumber;
	if (!archive.GetFileNumber(fileHash, fileNumber))
		return true;
	AssetRef ref;
	ref.archive = &archive;
	ref.fileNumber = fileNumber;
	ref.filename = filename;
	return CheckExtraFontsVersion(std::move(ref));
}
#endif

bool AreExtraFontsOutOfDate()
{
	const auto it = MpqArchives.find(FontMpqPriority);
	return it != MpqArchives.end() && AreExtraFontsOutOfDate(it->second);
}

void init_cleanup()
{
	if (gbIsMultiplayer && gbRunGame) {
		pfile_write_hero(/*writeGameData=*/false);
		sfile_write_stash();
	}

	MpqArchives.clear();
	HasHellfireMpq = false;

	NetClose();
}

void init_create_window()
{
	if (!SpawnWindow(PROJECT_NAME))
		app_fatal(_("Unable to create main window"));
	dx_init();
	gbActive = true;
#ifndef USE_SDL1
	SDL_DisableScreenSaver();
#endif
}

void MainWndProc(const SDL_Event &event)
{
#ifndef USE_SDL1
#ifdef USE_SDL3
	switch (event.type)
#else
	if (event.type != SDL_WINDOWEVENT) return;
	switch (event.window.event)
#endif
	{
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_HIDDEN:
	case SDL_EVENT_WINDOW_MINIMIZED:
#else
	case SDL_WINDOWEVENT_HIDDEN:
	case SDL_WINDOWEVENT_MINIMIZED:
#endif
		gbActive = false;
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_SHOWN:
	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESTORED:
#else
	case SDL_WINDOWEVENT_SHOWN:
	case SDL_WINDOWEVENT_EXPOSED:
	case SDL_WINDOWEVENT_RESTORED:
#endif
		gbActive = true;
		RedrawEverything();
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
#else
	case SDL_WINDOWEVENT_SIZE_CHANGED:
#endif
		ReinitializeHardwareCursor();
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
#else
	case SDL_WINDOWEVENT_LEAVE:
#endif
		sgbMouseDown = CLICK_NONE;
		LastPlayerAction = PlayerActionType::None;
		RedrawEverything();
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
#else
	case SDL_WINDOWEVENT_CLOSE:
#endif
		diablo_quit(0);
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_FOCUS_LOST:
#else
	case SDL_WINDOWEVENT_FOCUS_LOST:
#endif
		if (*GetOptions().Gameplay.pauseOnFocusLoss)
			diablo_focus_pause();
		break;
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
#else
	case SDL_WINDOWEVENT_FOCUS_GAINED:
#endif
		if (*GetOptions().Gameplay.pauseOnFocusLoss)
			diablo_focus_unpause();
		break;
#ifdef USE_SDL3
	default:
		break;
#else
	case SDL_WINDOWEVENT_MOVED:
	case SDL_WINDOWEVENT_RESIZED:
	case SDL_WINDOWEVENT_MAXIMIZED:
	case SDL_WINDOWEVENT_ENTER:
	case SDL_WINDOWEVENT_TAKE_FOCUS:
		break;
	default:
		LogVerbose("Unhandled SDL_WINDOWEVENT event: {:d}", event.window.event);
		break;
#endif
	}
#else
	if (event.type != SDL_ACTIVEEVENT)
		return;
	if ((event.active.state & SDL_APPINPUTFOCUS) != 0) {
		if (event.active.gain == 0)
			diablo_focus_pause();
		else
			diablo_focus_unpause();
	}
#endif
}

} // namespace devilution
