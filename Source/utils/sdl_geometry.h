/**
 * @file sdl_geometry.h
 * Helpers for SDL geometry types
 */
#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#else
#include <SDL_version.h>

#if SDL_VERSION_ATLEAST(2, 0, 0)
#include <SDL_rect.h>
#else
#include <SDL_video.h>
#endif
#endif

#include "engine/rectangle.hpp"

namespace devilution {

/**
 * @brief Same as constructing directly but avoids type conversion warnings.
 */
inline SDL_Rect MakeSdlRect(
    decltype(SDL_Rect {}.x) x, decltype(SDL_Rect {}.y) y,
    decltype(SDL_Rect {}.w) w, decltype(SDL_Rect {}.h) h)
{
	return SDL_Rect { x, y, w, h };
}

inline SDL_Rect MakeSdlRect(Rectangle rect)
{
	return MakeSdlRect(rect.position.x, rect.position.y, rect.size.width, rect.size.height);
}

constexpr Rectangle MakeRectangle(SDL_Rect sdlRect)
{
	return { Point { sdlRect.x, sdlRect.y }, Size { sdlRect.w, sdlRect.h } };
}
} // namespace devilution
