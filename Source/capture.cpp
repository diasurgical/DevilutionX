/**
 * @file capture.cpp
 *
 * Implementation of the screenshot function.
 */
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>

#include "utils/sdl_compat.h"
#endif

#include <expected.hpp>

#define DEVILUTIONX_SCREENSHOT_FORMAT_PCX 0
#define DEVILUTIONX_SCREENSHOT_FORMAT_PNG 1

#if DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PCX
#include "utils/surface_to_pcx.hpp"
#endif
#if DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PNG
#include "utils/surface_to_png.hpp"
#endif

#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "engine/palette.h"
#include "engine/render/scrollrt.h"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

SDL_IOStream *CaptureFile(std::string *dstPath)
{
	const char *ext =
#if DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PCX
	    ".pcx";
#elif DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PNG
	    ".png";
#endif
	const std::time_t tt = std::time(nullptr);
	const std::tm *tm = std::localtime(&tt);
	const std::string filename = tm != nullptr
	    ? StrCat("Screenshot from ",
	          LeftPad(tm->tm_year + 1900, 4, '0'), "-", LeftPad(tm->tm_mon + 1, 2, '0'), "-", LeftPad(tm->tm_mday, 2, '0'), "-",
	          LeftPad(tm->tm_hour, 2, '0'), "-", LeftPad(tm->tm_min, 2, '0'), "-", LeftPad(tm->tm_sec, 2, '0'))
	    : "Screenshot";
	*dstPath = StrCat(paths::PrefPath(), filename, ext);
	int i = 0;
	while (FileExists(dstPath->c_str())) {
		i++;
		*dstPath = StrCat(paths::PrefPath(), filename, "-", i, ext);
	}
	return SDL_IOFromFile(dstPath->c_str(), "wb");
}

/**
 * @brief Make a red version of the given palette and apply it to the screen.
 */
void RedPalette()
{
	for (int i = 0; i < 256; i++) {
		system_palette[i].g = 0;
		system_palette[i].b = 0;
	}
	SystemPaletteUpdated();
	BltFast(nullptr, nullptr);
	RenderPresent();
}

} // namespace

void CaptureScreen()
{
	std::string fileName;
	const uint32_t startTime = SDL_GetTicks();

	auto *outStream = CaptureFile(&fileName);
	if (outStream == nullptr) {
		LogError("Failed to open {} for writing: {}", fileName, SDL_GetError());
		SDL_ClearError();
		return;
	}
	DrawAndBlit();

	const std::array<SDL_Color, 256> origSystemPalette = system_palette;
	RedPalette();

	system_palette = origSystemPalette;
	SystemPaletteUpdated();

	const tl::expected<void, std::string> result =
#if DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PCX
	    WriteSurfaceToFilePcx(GlobalBackBuffer(), outStream);
#elif DEVILUTIONX_SCREENSHOT_FORMAT == DEVILUTIONX_SCREENSHOT_FORMAT_PNG
	    WriteSurfaceToFilePng(GlobalBackBuffer(), outStream);
#endif

	if (!result.has_value()) {
		LogError("Failed to save screenshot at {}: ", fileName, result.error());
		RemoveFile(fileName.c_str());
	} else {
		Log("Screenshot saved at {}", fileName);
	}
	const uint32_t timePassed = SDL_GetTicks() - startTime;
	if (timePassed < 300) {
		SDL_Delay(300 - timePassed);
	}
	RedrawEverything();
}

} // namespace devilution
