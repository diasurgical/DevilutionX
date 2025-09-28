/**
 * @file engine.h
 *
 *  of basic engine helper functions:
 * - Sprite blitting
 * - Drawing
 * - Angle calculation
 * - Memory allocation
 * - File loading
 * - Video playback
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <tuple>
#include <utility>

// We include `cinttypes` here so that it is included before `inttypes.h`
// to work around a bug in older GCC versions on some platforms,
// where including `inttypes.h` before `cinttypes` leads to missing
// defines for `PRIuMAX` et al. SDL transitively includes `inttypes.h`.
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97044
#include <cinttypes>

#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#include "appfat.h"
#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/cstddef.hpp"

#define TILE_WIDTH 64
#define TILE_HEIGHT 32

namespace devilution {

#if __cplusplus >= 201703L
template <typename V, typename X, typename... Xs>
constexpr bool IsAnyOf(const V &v, X x, Xs... xs)
{
	return v == x || ((v == xs) || ...);
}

template <typename V, typename X, typename... Xs>
constexpr bool IsNoneOf(const V &v, X x, Xs... xs)
{
	return v != x && ((v != xs) && ...);
}
#else
template <typename V, typename X>
constexpr bool IsAnyOf(const V &v, X x)
{
	return v == x;
}

template <typename V, typename X, typename... Xs>
constexpr bool IsAnyOf(const V &v, X x, Xs... xs)
{
	return IsAnyOf(v, x) || IsAnyOf(v, xs...);
}

template <typename V, typename X>
constexpr bool IsNoneOf(const V &v, X x)
{
	return v != x;
}

template <typename V, typename X, typename... Xs>
constexpr bool IsNoneOf(const V &v, X x, Xs... xs)
{
	return IsNoneOf(v, x) && IsNoneOf(v, xs...);
}
#endif

/**
 * @brief Calculate the best fit direction between two points
 * @param start Tile coordinate
 * @param destination Tile coordinate
 * @return A value from the direction enum
 */
Direction GetDirection(Point start, Point destination);

/**
 * @brief Calculate Width2 from the orginal Width
 * Width2 is needed for savegame compatiblity and to render animations centered
 * @return Returns Width2
 */
int CalculateWidth2(int width);

inline int GetAnimationFrame(int frames, int fps = 60)
{
	int frame = (SDL_GetTicks() / fps) % frames;
	return frame > frames ? 0 : frame;
}

} // namespace devilution
