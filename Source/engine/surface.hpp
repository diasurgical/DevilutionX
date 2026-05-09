#pragma once

#include <cstddef>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL_version.h>

#if SDL_VERSION_ATLEAST(2, 0, 0)
#include <SDL_rect.h>
#include <SDL_surface.h>
#else
#include "utils/sdl2_to_1_2_backports.h"
#include <SDL_video.h>
#endif
#endif

#include "engine/point.hpp"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"

namespace devilution {

/**
 * @brief 8-bit surface.
 */
struct Surface {
	SDL_Surface *surface;
	SDL_Rect region;

	Surface()
	    : surface(nullptr)
	    , region(SDL_Rect { 0, 0, 0, 0 })
	{
	}

	explicit Surface(SDL_Surface *surface)
	    : surface(surface)
	    , region(MakeSdlRect(0, 0, surface->w, surface->h))
	{
	}

	Surface(SDL_Surface *surface, SDL_Rect region)
	    : surface(surface)
	    , region(region)
	{
	}

	Surface(const Surface &other) = default;
	Surface &operator=(const Surface &other) = default;

	int w() const
	{
		return region.w;
	}
	int h() const
	{
		return region.h;
	}

	std::uint8_t &operator[](Point p) const
	{
		return *at(p.x, p.y);
	}

	std::uint8_t *at(int x, int y) const
	{
		return static_cast<uint8_t *>(surface->pixels) + region.x + x + surface->pitch * (region.y + y);
	}

	std::uint8_t *begin() const
	{
		return at(0, 0);
	}
	std::uint8_t *end() const
	{
		return at(0, region.h);
	}

	/**
	 * @brief Set the value of a single pixel if it is in bounds.
	 * @param position Target buffer coordinate
	 * @param col Color index from current palette
	 */
	void SetPixel(Point position, std::uint8_t col) const
	{
		if (InBounds(position))
			(*this)[position] = col;
	}

	/**
	 * @brief Line width of the raw underlying byte buffer.
	 * May be wider than its logical width (for power-of-2 alignment).
	 */
	[[nodiscard]] uint16_t pitch() const
	{
		return surface->pitch;
	}

	bool InBounds(Point position) const
	{
		return position.x >= 0 && position.y >= 0 && position.x < region.w && position.y < region.h;
	}

	/**
	 * @brief Returns a subregion of the given buffer.
	 */
	Surface subregion(int x, int y, int w, int h) const
	{
		return Surface(surface, MakeSdlRect(region.x + x, region.y + y, w, h));
	}

	/**
	 * @brief Returns a buffer that starts at `x` of width `w`.
	 */
	Surface subregionX(int x, int w) const
	{
		SDL_Rect subregion = region;
		subregion.x += static_cast<decltype(SDL_Rect {}.x)>(x);
		subregion.w = static_cast<decltype(SDL_Rect {}.w)>(w);
		return Surface(surface, subregion);
	}

	/**
	 * @brief Returns a buffer that starts at `y` of height `h`.
	 */
	Surface subregionY(int y, int h) const
	{
		SDL_Rect subregion = region;
		subregion.y += static_cast<decltype(SDL_Rect {}.y)>(y);
		subregion.h = static_cast<decltype(SDL_Rect {}.h)>(h);
		return Surface(surface, subregion);
	}

	/**
	 * @brief Clips srcRect and targetPosition to this output buffer.
	 */
	void Clip(SDL_Rect *srcRect, Point *targetPosition) const
	{
		if (targetPosition->x < 0) {
			srcRect->x -= targetPosition->x;
			srcRect->w += targetPosition->x;
			targetPosition->x = 0;
		}
		if (targetPosition->y < 0) {
			srcRect->y -= targetPosition->y;
			srcRect->h += targetPosition->y;
			targetPosition->y = 0;
		}
		if (targetPosition->x + srcRect->w > region.w) {
			srcRect->w = region.w - targetPosition->x;
		}
		if (targetPosition->y + srcRect->h > region.h) {
			srcRect->h = region.h - targetPosition->y;
		}
	}

	/**
	 * @brief Copies the `srcRect` portion of the given buffer to this buffer at `targetPosition`.
	 */
	void BlitFrom(const Surface &src, SDL_Rect srcRect, Point targetPosition) const;

	/**
	 * @brief Copies the `srcRect` portion of the given buffer to this buffer at `targetPosition`.
	 * Source pixels with index 0 are not copied.
	 */
	void BlitFromSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) const;
};

class OwnedSurface : public Surface {
	SDLSurfaceUniquePtr pinnedSurface;

public:
	explicit OwnedSurface(SDLSurfaceUniquePtr surface)
	    : Surface(surface.get())
	    , pinnedSurface(std::move(surface))
	{
	}

	OwnedSurface(int width, int height)
	    : OwnedSurface(SDLWrap::CreateRGBSurfaceWithFormat(0, width, height, 8, SDL_PIXELFORMAT_INDEX8))
	{
	}

	explicit OwnedSurface(Size size)
	    : OwnedSurface(size.width, size.height)
	{
	}
};

} // namespace devilution
