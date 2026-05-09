#pragma once
/**
 * @brief std::unique_ptr specializations for SDL types.
 */

#include <memory>
#include <type_traits>

#ifdef USE_SDL3
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

namespace devilution {

/**
 * @brief Deletes the SDL surface using `SDL_FreeSurface`.
 */
struct SDLSurfaceDeleter {
	void operator()(SDL_Surface *surface) const
	{
#ifdef USE_SDL3
		SDL_DestroySurface(surface);
#else
		SDL_FreeSurface(surface);
#endif
	}
};

using SDLSurfaceUniquePtr = std::unique_ptr<SDL_Surface, SDLSurfaceDeleter>;

#if SDL_VERSION_ATLEAST(2, 0, 0)
struct SDLCursorDeleter {
	void operator()(SDL_Cursor *cursor) const
	{
#ifdef USE_SDL3
		SDL_DestroyCursor(cursor);
#else
		SDL_FreeCursor(cursor);
#endif
	}
};

using SDLCursorUniquePtr = std::unique_ptr<SDL_Cursor, SDLCursorDeleter>;

struct SDLTextureDeleter {
	void operator()(SDL_Texture *texture) const
	{
		SDL_DestroyTexture(texture);
	}
};

using SDLTextureUniquePtr = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;
#endif

struct SDLPaletteDeleter {
	void operator()(SDL_Palette *palette) const
	{
#ifdef USE_SDL3
		SDL_DestroyPalette(palette);
#else
		SDL_FreePalette(palette);
#endif
	}
};

using SDLPaletteUniquePtr = std::unique_ptr<SDL_Palette, SDLPaletteDeleter>;

/**
 * @brief Deletes the object using `SDL_free`.
 */
template <typename T>
struct SDLFreeDeleter {
	static_assert(!std::is_same<T, SDL_Surface>::value,
	    "SDL_Surface should use SDLSurfaceUniquePtr instead.");

	void operator()(T *obj) const
	{
		SDL_free(obj);
	}
};

/**
 * @brief A unique pointer to T that is deleted with SDL_free.
 */
template <typename T>
using SDLUniquePtr = std::unique_ptr<T, SDLFreeDeleter<T>>;

} // namespace devilution
