/**
 * @file engine.cpp
 *
 * Implementation of basic engine helper functions:
 * - Sprite blitting
 * - Drawing
 * - Angle calculation
 * - RNG
 * - Memory allocation
 * - File loading
 * - Video playback
 */

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "lighting.h"
#include "movie.h"
#include "options.h"

namespace devilution {

Direction GetDirection(Point start, Point destination)
{
	Direction md;

	int mx = destination.x - start.x;
	int my = destination.y - start.y;
	if (mx >= 0) {
		if (my >= 0) {
			if (5 * mx <= (my * 2)) // mx/my <= 0.4, approximation of tan(22.5)
				return Direction::SouthWest;
			md = Direction::South;
		} else {
			my = -my;
			if (5 * mx <= (my * 2))
				return Direction::NorthEast;
			md = Direction::East;
		}
		if (5 * my <= (mx * 2)) // my/mx <= 0.4
			md = Direction::SouthEast;
	} else {
		mx = -mx;
		if (my >= 0) {
			if (5 * mx <= (my * 2))
				return Direction::SouthWest;
			md = Direction::West;
		} else {
			my = -my;
			if (5 * mx <= (my * 2))
				return Direction::NorthEast;
			md = Direction::North;
		}
		if (5 * my <= (mx * 2))
			md = Direction::NorthWest;
	}
	return md;
}

int CalculateWidth2(int width)
{
	return (width - 64) / 2;
}

} // namespace devilution
