#include "engine/load_pcx.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#ifdef DEBUG_PCX_TO_CL2_SIZE
#include <iostream>
#endif

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#else
#include <SDL.h>
#endif

#include "mpq/mpq_common.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifdef UNPACKED_MPQS
#include "engine/load_clx.hpp"
#include "engine/load_file.hpp"
#else
#include "engine/assets.hpp"
#include "utils/pcx.hpp"
#include "utils/pcx_to_clx.hpp"
#endif

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

OptionalOwnedClxSpriteList LoadPcxSpriteList(const char *filename, int numFramesOrFrameHeight, std::optional<uint8_t> transparentColor, SDL_Color *outPalette, bool logError)
{
	char path[MaxMpqPathSize];
	char *pathEnd = BufCopy(path, filename, DEVILUTIONX_PCX_EXT);
	*pathEnd = '\0';
#ifdef UNPACKED_MPQS
	OptionalOwnedClxSpriteList result = LoadOptionalClx(path);
	if (!result) {
		if (logError)
			LogError("Missing file: {}", path);
		return result;
	}
	if (outPalette != nullptr) {
		std::memcpy(pathEnd - 3, "pal", 3);
		std::array<uint8_t, 256 * 3> palette;
		LoadFileInMem(path, palette);
		for (unsigned i = 0; i < 256; i++) {
			outPalette[i].r = palette[i * 3];
			outPalette[i].g = palette[i * 3 + 1];
			outPalette[i].b = palette[i * 3 + 2];
#ifndef USE_SDL1
			outPalette[i].a = SDL_ALPHA_OPAQUE;
#endif
		}
	}
	return result;
#else
	size_t fileSize;
	AssetHandle handle = OpenAsset(path, fileSize);
	if (!handle.ok()) {
		if (logError)
			LogError("Missing file: {}", path);
		return std::nullopt;
	}
#ifdef DEBUG_PCX_TO_CL2_SIZE
	std::cout << filename;
#endif
	OptionalOwnedClxSpriteList result = PcxToClx(handle, fileSize, numFramesOrFrameHeight, transparentColor, outPalette);
	if (!result)
		return std::nullopt;
	return result;
#endif
}

} // namespace devilution
