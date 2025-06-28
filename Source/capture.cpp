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

#include <SDL.h>
#include <expected.hpp>
#include <fmt/format.h>

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
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

SDL_RWops *CaptureFile(std::string *dstPath)
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
	    ? fmt::format("Screenshot from {:04}-{:02}-{:02} {:02}-{:02}-{:02}",
	          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec)
	    : "Screenshot";
	*dstPath = StrCat(paths::PrefPath(), filename, ext);
	int i = 0;
	while (FileExists(dstPath->c_str())) {
		i++;
		*dstPath = StrCat(paths::PrefPath(), filename, "-", i, ext);
	}
	return SDL_RWFromFile(dstPath->c_str(), "wb");
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

	SDL_RWops *outStream = CaptureFile(&fileName);
	if (outStream == nullptr) {
		LogError("Failed to open {} for writing: {}", fileName, SDL_GetError());
		SDL_ClearError();
		return;
	}
	DrawAndBlit();

	std::array<SDL_Color, 256> origSystemPalette = system_palette;
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
