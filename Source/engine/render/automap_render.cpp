/**
 * @file automap_render.cpp
 *
 * Line drawing routines for the automap.
 */
#include "engine/render/automap_render.hpp"

#include <cstdint>

#include "automap.h"
#include "engine/render/renderer.h"

namespace devilution {
namespace {

enum class DirectionX : int8_t {
	EAST = 1,
	WEST = -1,
};

enum class DirectionY : int8_t {
	SOUTH = 1,
	NORTH = -1,
};

template <DirectionX DirX, DirectionY DirY>
void DrawMapLine(Point from, int height, std::uint8_t colorIndex)
{
	while (height-- > 0) {
		SetMapPixel({ from.x, from.y + 1 }, 0);
		SetMapPixel(from, colorIndex);
		from.x += static_cast<int>(DirX);
		SetMapPixel({ from.x, from.y + 1 }, 0);
		SetMapPixel(from, colorIndex);
		from.x += static_cast<int>(DirX);
		from.y += static_cast<int>(DirY);
	}
	SetMapPixel({ from.x, from.y + 1 }, 0);
	SetMapPixel(from, colorIndex);
}

template <DirectionX DirX, DirectionY DirY>
void DrawMapLineSteep(Point from, int width, std::uint8_t colorIndex)
{
	while (width-- > 0) {
		SetMapPixel({ from.x, from.y + 1 }, 0);
		SetMapPixel(from, colorIndex);
		from.y += static_cast<int>(DirY);
		SetMapPixel({ from.x, from.y + 1 }, 0);
		SetMapPixel(from, colorIndex);
		from.y += static_cast<int>(DirY);
		from.x += static_cast<int>(DirX);
	}
	SetMapPixel({ from.x, from.y + 1 }, 0);
	SetMapPixel(from, colorIndex);
}

} // namespace

void DrawMapLineNS(Point from, int height, std::uint8_t colorIndex)
{
	if (from.x < 0 || height <= 0)
		return;

	if (from.y < 0) {
		height += from.y;
		from.y = 0;
	}

	for (int i = 0; i < height; ++i) {
		SetMapPixel({ from.x, from.y + i }, colorIndex);
	}
}

void DrawMapLineWE(Point from, int width, std::uint8_t colorIndex)
{
	if (from.y < 0 || width <= 0)
		return;

	if (from.x < 0) {
		width += from.x;
		from.x = 0;
	}

	for (int i = 0; i < width; ++i) {
		SetMapPixel({ from.x + i, from.y }, colorIndex);
	}
}

void DrawMapLineNE(Point from, int height, std::uint8_t colorIndex)
{
	DrawMapLine<DirectionX::EAST, DirectionY::NORTH>(from, height, colorIndex);
}

void DrawMapLineSE(Point from, int height, std::uint8_t colorIndex)
{
	DrawMapLine<DirectionX::EAST, DirectionY::SOUTH>(from, height, colorIndex);
}

void DrawMapLineNW(Point from, int height, std::uint8_t colorIndex)
{
	DrawMapLine<DirectionX::WEST, DirectionY::NORTH>(from, height, colorIndex);
}

void DrawMapLineSW(Point from, int height, std::uint8_t colorIndex)
{
	DrawMapLine<DirectionX::WEST, DirectionY::SOUTH>(from, height, colorIndex);
}

void DrawMapLineSteepNE(Point from, int width, std::uint8_t colorIndex)
{
	DrawMapLineSteep<DirectionX::EAST, DirectionY::NORTH>(from, width, colorIndex);
}

void DrawMapLineSteepSE(Point from, int width, std::uint8_t colorIndex)
{
	DrawMapLineSteep<DirectionX::EAST, DirectionY::SOUTH>(from, width, colorIndex);
}

void DrawMapLineSteepNW(Point from, int width, std::uint8_t colorIndex)
{
	DrawMapLineSteep<DirectionX::WEST, DirectionY::NORTH>(from, width, colorIndex);
}

void DrawMapLineSteepSW(Point from, int width, std::uint8_t colorIndex)
{
	DrawMapLineSteep<DirectionX::WEST, DirectionY::SOUTH>(from, width, colorIndex);
}

/**
 * @brief Draws a line from first point to second point, unrestricted to the standard automap angles. Doesn't include shadow.
 */
void DrawMapFreeLine(Point from, Point to, uint8_t colorIndex)
{
	const int dx = std::abs(to.x - from.x);
	const int dy = std::abs(to.y - from.y);
	const int sx = from.x < to.x ? 1 : -1;
	const int sy = from.y < to.y ? 1 : -1;
	int err = dx - dy;

	while (true) {
		SetMapPixel(from, colorIndex);

		if (from.x == to.x && from.y == to.y) {
			break;
		}

		const int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			from.x += sx;
		}
		if (e2 < dx) {
			err += dx;
			from.y += sy;
		}
	}
}

void SetMapPixel(Point position, uint8_t color)
{
	if (GetAutomapType() == AutomapType::Minimap && !MinimapRect.contains(position))
		return;

	if (GetAutomapType() == AutomapType::Transparent) {
		GetRenderer().DrawBlendedPixel(position, color);
	} else {
		GetRenderer().DrawPixel(position, color);
	}
}

} // namespace devilution
