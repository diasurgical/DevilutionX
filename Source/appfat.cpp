/**
 * @file appfat.cpp
 *
 * Implementation of error dialogs.
 */

#include <config.h>

#ifdef USE_SDL3
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#include "utils/sdl_compat.h"
#endif

#include <fmt/format.h>

#include "diablo.h"
#include "multi.h"
#include "storm/storm_net.hpp"
#include "utils/language.h"
#include "utils/sdl_thread.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

/** Set to true when a fatal error is encountered and the application should shut down. */
bool Terminating = false;

/** Thread id of the last callee to FreeDlg(). */
SDL_ThreadID CleanupThreadId;

/**
 * @brief Cleans up after a fatal application error.
 */
void FreeDlg()
{
	if (Terminating && CleanupThreadId != this_sdl_thread::get_id())
		SDL_Delay(20000);

	Terminating = true;
	CleanupThreadId = this_sdl_thread::get_id();

	if (gbIsMultiplayer) {
		if (SNetLeaveGame(3))
			SDL_Delay(2000);
	}

	SNetDestroy();
}

} // namespace

void DisplayFatalErrorAndExit(std::string_view title, std::string_view body)
{
	FreeDlg();
	UiErrorOkDialog(title, body);
	diablo_quit(1);
}

void app_fatal(std::string_view str)
{
	DisplayFatalErrorAndExit(_("Error"), str);
}

#ifdef _DEBUG
void assert_fail(int nLineNo, const char *pszFile, const char *pszFail)
{
	app_fatal(StrCat("assertion failed (", pszFile, ":", nLineNo, ")\n", pszFail));
}
#endif

void ErrDlg(const char *title, std::string_view error, std::string_view logFilePath, int logLineNr)
{
	DisplayFatalErrorAndExit(
	    title,
	    fmt::format(fmt::runtime(_(/* TRANSLATORS: Error message that displays relevant information for bug report */ "{:s}\n\nThe error occurred at: {:s} line {:d}")),
	        error, logFilePath, logLineNr));
}

void InsertCDDlg(std::string_view archiveName)
{
	DisplayFatalErrorAndExit(_("Data File Error"),
	    fmt::format(fmt::runtime(_("Unable to open main data archive ({:s}).\n"
	                               "\n"
	                               "Make sure that it is in the game folder.")),
	        archiveName));
}

void DirErrorDlg(std::string_view error)
{
	DisplayFatalErrorAndExit(
	    _("Read-Only Directory Error"),
	    fmt::format(fmt::runtime(_(/* TRANSLATORS: Error when Program is not allowed to write data */ "Unable to write to location:\n{:s}")),
	        error));
}

} // namespace devilution
