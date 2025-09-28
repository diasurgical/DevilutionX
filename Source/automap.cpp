/**
 * @file automap.cpp
 *
 * Implementation of the in-game map overlay.
 */
#include "automap.h"

#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "engine/load_file.hpp"
#include "engine/palette.h"
#include "engine/render/automap_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "player.h"
#include "utils/language.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

namespace {
Point Automap;

enum MapColors : uint8_t {
	/** color used to draw the player's arrow */
	MapColorsPlayer = (PAL8_ORANGE + 1),
	/** color for bright map lines (doors, stairs etc.) */
	MapColorsBright = PAL8_YELLOW,
	/** color for dim map lines/dots */
	MapColorsDim = (PAL16_YELLOW + 8),
	/** color for items on automap */
	MapColorsItem = (PAL8_BLUE + 1),
};

struct AutomapTile {
	/** The general shape of the tile */
	enum class Types : uint8_t {
		None,
		Diamond,
		Vertical,
		Horizontal,
		Cross,
		FenceVertical,
		FenceHorizontal,
		Corner,
		CaveHorizontalCross,
		CaveVerticalCross,
		CaveHorizontal,
		CaveVertical,
		CaveCross,
		Bridge,
		River,
		RiverCornerEast,
		RiverCornerNorth,
		RiverCornerSouth,
		RiverCornerWest,
		RiverForkIn,
		RiverForkOut,
		RiverLeftIn,
		RiverLeftOut,
		RiverRightIn,
		RiverRightOut,
	};

	Types type;

	/** Additional details about the given tile */
	enum class Flags : uint8_t {
		// clang-format off
		VerticalDoor      = 1 << 0,
		HorizontalDoor    = 1 << 1,
		VerticalArch      = 1 << 2,
		HorizontalArch    = 1 << 3,
		VerticalGrate     = 1 << 4,
		HorizontalGrate   = 1 << 5,
		VerticalPassage   = VerticalDoor | VerticalArch | VerticalGrate,
		HorizontalPassage = HorizontalDoor | HorizontalArch | HorizontalGrate,
		Dirt              = 1 << 6,
		Stairs            = 1 << 7,
		// clang-format on
	};

	Flags flags;

	constexpr bool HasFlag(Flags test) const
	{
		return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
	}
};

/**
 * Maps from tile_id to automap type.
 */
std::array<AutomapTile, 256> AutomapTypeTiles;

void DrawDiamond(const Surface &out, Point center, uint8_t color)
{
	const Point left { center.x - AmLine(16), center.y };
	const Point top { center.x, center.y - AmLine(8) };
	const Point bottom { center.x, center.y + AmLine(8) };

	DrawMapLineNE(out, left, AmLine(8), color);
	DrawMapLineSE(out, left, AmLine(8), color);
	DrawMapLineSE(out, top, AmLine(8), color);
	DrawMapLineNE(out, bottom, AmLine(8), color);
}

void DrawMapVerticalDoor(const Surface &out, Point center, uint8_t colorBright, uint8_t colorDim)
{
	if (leveltype != DTYPE_CATACOMBS) {
		DrawMapLineNE(out, { center.x + AmLine(8), center.y - AmLine(4) }, AmLine(4), colorDim);
		DrawMapLineNE(out, { center.x - AmLine(16), center.y + AmLine(8) }, AmLine(4), colorDim);
		DrawDiamond(out, center, colorBright);
	} else {
		DrawMapLineNE(out, { center.x - AmLine(8), center.y + AmLine(4) }, AmLine(8), colorDim);
		DrawMapLineNE(out, { center.x - AmLine(16), center.y + AmLine(8) }, AmLine(4), colorDim);
		DrawDiamond(out, { center.x + AmLine(16), center.y - AmLine(8) }, colorBright);
	}
}

void DrawMapHorizontalDoor(const Surface &out, Point center, uint8_t colorBright, uint8_t colorDim)
{
	if (leveltype != DTYPE_CATACOMBS) {
		DrawMapLineSE(out, { center.x - AmLine(16), center.y - AmLine(8) }, AmLine(4), colorDim);
		DrawMapLineSE(out, { center.x + AmLine(8), center.y + AmLine(4) }, AmLine(4), colorDim);
		DrawDiamond(out, center, colorBright);
	} else {
		DrawMapLineSE(out, { center.x - AmLine(8), center.y - AmLine(4) }, AmLine(8), colorDim);
		DrawMapLineSE(out, { center.x + AmLine(8), center.y + AmLine(4) }, AmLine(4), colorDim);
		DrawDiamond(out, { center.x - AmLine(16), center.y - AmLine(8) }, colorBright);
	}
}

void DrawDirt(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x + AmLine(8) - AmLine(32), center.y + AmLine(4) }, color);

	out.SetPixel({ center.x - AmLine(16), center.y }, color);
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x, center.y - AmLine(8) }, color);
	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawBridge(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel(center, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverRightIn(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverCornerSouth(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);
}

void DrawRiverCornerNorth(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x, center.y - AmLine(8) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
}

void DrawRiverLeftOut(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x + AmLine(8) - AmLine(32), center.y + AmLine(4) }, color);

	out.SetPixel({ center.x - AmLine(16), center.y }, color);
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverLeftIn(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(16), center.y }, color);
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x, center.y - AmLine(8) }, color);
	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);
}

void DrawRiverCornerWest(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x + AmLine(8) - AmLine(32), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(16), center.y }, color);
}

void DrawRiverCornerEast(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);
	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverRightOut(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiver(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverForkIn(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x - AmLine(16), center.y }, color);
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x, center.y - AmLine(8) }, color);
	out.SetPixel(center, color);
	out.SetPixel({ center.x, center.y + AmLine(8) }, color);
	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y - AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(4) }, color);
	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x + AmLine(16), center.y }, color);
	out.SetPixel({ center.x + AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8) + AmLine(32), center.y + AmLine(4) }, color);
}

void DrawRiverForkOut(const Surface &out, Point center, uint8_t color)
{
	out.SetPixel({ center.x + AmLine(8) - AmLine(32), center.y + AmLine(4) }, color);

	out.SetPixel({ center.x - AmLine(16), center.y }, color);
	out.SetPixel({ center.x - AmLine(16), center.y + AmLine(8) }, color);

	out.SetPixel({ center.x - AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);

	out.SetPixel({ center.x, center.y + AmLine(16) }, color);

	out.SetPixel({ center.x + AmLine(8), center.y + AmLine(16) - AmLine(4) }, color);
}

void DrawStairs(const Surface &out, Point center, uint8_t color)
{
	constexpr int NumStairSteps = 4;
	const Displacement offset = { -AmLine(8), AmLine(4) };
	Point p = { center.x - AmLine(8), center.y - AmLine(8) - AmLine(4) };
	for (int i = 0; i < NumStairSteps; ++i) {
		DrawMapLineSE(out, p, AmLine(16), color);
		p += offset;
	}
}

/**
 * Left-facing obstacle
 */
void DrawHorizontal(const Surface &out, Point center, AutomapTile tile, uint8_t colorBright, uint8_t colorDim)
{
	if (!tile.HasFlag(AutomapTile::Flags::HorizontalPassage)) {
		DrawMapLineSE(out, { center.x, center.y - AmLine(16) }, AmLine(16), colorDim);
		return;
	}
	if (tile.HasFlag(AutomapTile::Flags::HorizontalDoor)) {
		DrawMapHorizontalDoor(out, { center.x + AmLine(16), center.y - AmLine(8) }, colorBright, colorDim);
	}
	if (tile.HasFlag(AutomapTile::Flags::HorizontalGrate)) {
		DrawMapLineSE(out, { center.x + AmLine(16), center.y - AmLine(8) }, AmLine(8), colorDim);
		DrawDiamond(out, { center.x, center.y - AmLine(8) }, colorDim);
	} else if (tile.HasFlag(AutomapTile::Flags::HorizontalArch)) {
		DrawDiamond(out, { center.x, center.y - AmLine(8) }, colorDim);
	}
}

/**
 * Right-facing obstacle
 */
void DrawVertical(const Surface &out, Point center, AutomapTile tile, uint8_t colorBright, uint8_t colorDim)
{
	if (!tile.HasFlag(AutomapTile::Flags::VerticalPassage)) {
		DrawMapLineNE(out, { center.x - AmLine(32), center.y }, AmLine(16), colorDim);
		return;
	}
	if (tile.HasFlag(AutomapTile::Flags::VerticalDoor)) { // two wall segments with a door in the middle
		DrawMapVerticalDoor(out, { center.x - AmLine(16), center.y - AmLine(8) }, colorBright, colorDim);
	}
	if (tile.HasFlag(AutomapTile::Flags::VerticalGrate)) { // right-facing half-wall
		DrawMapLineNE(out, { center.x - AmLine(32), center.y }, AmLine(8), colorDim);
		DrawDiamond(out, { center.x, center.y - AmLine(8) }, colorDim);
	} else if (tile.HasFlag(AutomapTile::Flags::VerticalArch)) { // window or passable column
		DrawDiamond(out, { center.x, center.y - AmLine(8) }, colorDim);
	}
}

/**
 * For caves the horizontal/vertical flags are swapped
 */
void DrawCaveHorizontal(const Surface &out, Point center, AutomapTile tile, uint8_t colorBright, uint8_t colorDim)
{
	if (tile.HasFlag(AutomapTile::Flags::VerticalDoor)) {
		DrawMapHorizontalDoor(out, { center.x - AmLine(16), center.y + AmLine(8) }, colorBright, colorDim);
	} else {
		DrawMapLineSE(out, { center.x - AmLine(32), center.y }, AmLine(16), colorDim);
	}
}

/**
 * For caves the horizontal/vertical flags are swapped
 */
void DrawCaveVertical(const Surface &out, Point center, AutomapTile tile, uint8_t colorBright, uint8_t colorDim)
{
	if (tile.HasFlag(AutomapTile::Flags::HorizontalDoor)) {
		DrawMapVerticalDoor(out, { center.x + AmLine(16), center.y + AmLine(8) }, colorBright, colorDim);
	} else {
		DrawMapLineNE(out, { center.x, center.y + AmLine(16) }, AmLine(16), colorDim);
	}
}

/**
 * @brief Check if a given tile has the provided AutomapTile flag
 */
bool HasAutomapFlag(Point position, AutomapTile::Flags type)
{
	if (position.x < 0 || position.x >= DMAXX || position.y < 0 || position.y >= DMAXX) {
		return false;
	}

	return AutomapTypeTiles[dungeon[position.x][position.y]].HasFlag(type);
}

/**
 * @brief Returns the automap shape at the given coordinate.
 */
AutomapTile GetAutomapType(Point position)
{
	if (position.x < 0 || position.x >= DMAXX || position.y < 0 || position.y >= DMAXX) {
		return {};
	}

	AutomapTile tile = AutomapTypeTiles[dungeon[position.x][position.y]];
	if (tile.type == AutomapTile::Types::Corner) {
		if (HasAutomapFlag({ position.x - 1, position.y }, AutomapTile::Flags::HorizontalArch)) {
			if (HasAutomapFlag({ position.x, position.y - 1 }, AutomapTile::Flags::VerticalArch)) {
				tile.type = AutomapTile::Types::Diamond;
			}
		}
	}

	return tile;
}

/**
 * @brief Returns the automap shape at the given coordinate.
 */
AutomapTile GetAutomapTypeView(Point map)
{
	if (map.x == -1 && map.y >= 0 && map.y < DMAXY && AutomapView[0][map.y] != MAP_EXP_NONE) {
		if (HasAutomapFlag({ 0, map.y + 1 }, AutomapTile::Flags::Dirt) && HasAutomapFlag({ 0, map.y }, AutomapTile::Flags::Dirt) && HasAutomapFlag({ 0, map.y - 1 }, AutomapTile::Flags::Dirt)) {
			return {};
		}
		return { AutomapTile::Types::None, AutomapTile::Flags::Dirt };
	}

	if (map.y == -1 && map.x >= 0 && map.x < DMAXY && AutomapView[map.x][0] != MAP_EXP_NONE) {
		if (HasAutomapFlag({ map.x + 1, 0 }, AutomapTile::Flags::Dirt) && HasAutomapFlag({ map.x, 0 }, AutomapTile::Flags::Dirt) && HasAutomapFlag({ map.x - 1, 0 }, AutomapTile::Flags::Dirt)) {
			return {};
		}
		return { AutomapTile::Types::None, AutomapTile::Flags::Dirt };
	}

	if (map.x < 0 || map.x >= DMAXX) {
		return {};
	}
	if (map.y < 0 || map.y >= DMAXX) {
		return {};
	}
	if (AutomapView[map.x][map.y] == MAP_EXP_NONE) {
		return {};
	}

	return GetAutomapType(map);
}

/**
 * @brief Renders the given automap shape at the specified screen coordinates.
 */
void DrawAutomapTile(const Surface &out, Point center, Point map)
{
	AutomapTile tile = GetAutomapTypeView(map);
	uint8_t colorBright = MapColorsBright;
	uint8_t colorDim = MapColorsDim;
	MapExplorationType explorationType = static_cast<MapExplorationType>(AutomapView[clamp(map.x, 0, DMAXX - 1)][clamp(map.y, 0, DMAXY - 1)]);

	switch (explorationType) {
	case MAP_EXP_SHRINE:
		colorDim = PAL16_GRAY + 11;
		colorBright = PAL16_GRAY + 3;
		break;
	case MAP_EXP_OTHERS:
		colorDim = PAL16_BEIGE + 10;
		colorBright = PAL16_BEIGE + 2;
		break;
	case MAP_EXP_SELF:
	case MAP_EXP_NONE:
	case MAP_EXP_OLD:
		break;
	}

	if (tile.HasFlag(AutomapTile::Flags::Dirt)) {
		DrawDirt(out, center, colorDim);
	}

	if (tile.HasFlag(AutomapTile::Flags::Stairs)) {
		DrawStairs(out, center, colorBright);
	}

	switch (tile.type) {
	case AutomapTile::Types::Diamond: // stand-alone column or other unpassable object
		DrawDiamond(out, { center.x, center.y - AmLine(8) }, colorDim);
		break;
	case AutomapTile::Types::Vertical:
	case AutomapTile::Types::FenceVertical:
		DrawVertical(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::Horizontal:
	case AutomapTile::Types::FenceHorizontal:
		DrawHorizontal(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::Cross:
		DrawVertical(out, center, tile, colorBright, colorDim);
		DrawHorizontal(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveHorizontalCross:
		DrawVertical(out, center, tile, colorBright, colorDim);
		DrawCaveHorizontal(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveVerticalCross:
		DrawHorizontal(out, center, tile, colorBright, colorDim);
		DrawCaveVertical(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveHorizontal:
		DrawCaveHorizontal(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveVertical:
		DrawCaveVertical(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveCross:
		DrawCaveHorizontal(out, center, tile, colorBright, colorDim);
		DrawCaveVertical(out, center, tile, colorBright, colorDim);
		break;
	case AutomapTile::Types::Corner:
	case AutomapTile::Types::None:
		break;
	case AutomapTile::Types::Bridge:
		DrawBridge(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::River:
		DrawRiver(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverCornerEast:
		DrawRiverCornerEast(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverCornerNorth:
		DrawRiverCornerNorth(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverCornerSouth:
		DrawRiverCornerSouth(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverCornerWest:
		DrawRiverCornerWest(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverForkIn:
		DrawRiverForkIn(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverForkOut:
		DrawRiverForkOut(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverLeftIn:
		DrawRiverLeftIn(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverLeftOut:
		DrawRiverLeftOut(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverRightIn:
		DrawRiverRightIn(out, center, MapColorsItem);
		break;
	case AutomapTile::Types::RiverRightOut:
		DrawRiverRightOut(out, center, MapColorsItem);
		break;
	}
}

void SearchAutomapItem(const Surface &out, const Displacement &myPlayerOffset, int searchRadius, tl::function_ref<bool(Point position)> highlightTile)
{
	const Player &player = *MyPlayer;
	Point tile = player.position.tile;
	if (player._pmode == PM_WALK_SIDEWAYS) {
		tile = player.position.future;
		if (player._pdir == Direction::West)
			tile.x++;
		else
			tile.y++;
	}

	const int startX = clamp(tile.x - searchRadius, 0, MAXDUNX);
	const int startY = clamp(tile.y - searchRadius, 0, MAXDUNY);

	const int endX = clamp(tile.x + searchRadius, 0, MAXDUNX);
	const int endY = clamp(tile.y + searchRadius, 0, MAXDUNY);

	for (int i = startX; i < endX; i++) {
		for (int j = startY; j < endY; j++) {
			if (!highlightTile({ i, j }))
				continue;

			int px = i - 2 * AutomapOffset.deltaX - ViewPosition.x;
			int py = j - 2 * AutomapOffset.deltaY - ViewPosition.y;

			Point screen = {
				(myPlayerOffset.deltaX * AutoMapScale / 100 / 2) + (px - py) * AmLine(16) + gnScreenWidth / 2,
				(myPlayerOffset.deltaY * AutoMapScale / 100 / 2) + (px + py) * AmLine(8) + (gnScreenHeight - GetMainPanel().size.height) / 2
			};

			if (CanPanelsCoverView()) {
				if (IsRightPanelOpen())
					screen.x -= 160;
				if (IsLeftPanelOpen())
					screen.x += 160;
			}
			screen.y -= AmLine(8);
			DrawDiamond(out, screen, MapColorsItem);
		}
	}
}

/**
 * @brief Renders an arrow on the automap, centered on and facing the direction of the player.
 */
void DrawAutomapPlr(const Surface &out, const Displacement &myPlayerOffset, int playerId)
{
	int playerColor = MapColorsPlayer + (8 * playerId) % 128;

	Player &player = Players[playerId];
	Point tile = player.position.tile;
	if (player._pmode == PM_WALK_SIDEWAYS) {
		tile = player.position.future;
	}

	int px = tile.x - 2 * AutomapOffset.deltaX - ViewPosition.x;
	int py = tile.y - 2 * AutomapOffset.deltaY - ViewPosition.y;

	Displacement playerOffset = {};
	if (player.isWalking())
		playerOffset = GetOffsetForWalking(player.AnimInfo, player._pdir);

	Point base = {
		((playerOffset.deltaX + myPlayerOffset.deltaX) * AutoMapScale / 100 / 2) + (px - py) * AmLine(16) + gnScreenWidth / 2,
		((playerOffset.deltaY + myPlayerOffset.deltaY) * AutoMapScale / 100 / 2) + (px + py) * AmLine(8) + (gnScreenHeight - GetMainPanel().size.height) / 2
	};

	if (CanPanelsCoverView()) {
		if (IsRightPanelOpen())
			base.x -= gnScreenWidth / 4;
		if (IsLeftPanelOpen())
			base.x += gnScreenWidth / 4;
	}
	base.y -= AmLine(16);

	switch (player._pdir) {
	case Direction::North: {
		const Point point { base.x, base.y - AmLine(16) };
		DrawVerticalLine(out, point, AmLine(16), playerColor);
		DrawMapLineSteepNE(out, { point.x - AmLine(4), point.y + 2 * AmLine(4) }, AmLine(4), playerColor);
		DrawMapLineSteepNW(out, { point.x + AmLine(4), point.y + 2 * AmLine(4) }, AmLine(4), playerColor);
	} break;
	case Direction::NorthEast: {
		const Point point { base.x + AmLine(16), base.y - AmLine(8) };
		DrawHorizontalLine(out, { point.x - AmLine(8), point.y }, AmLine(8), playerColor);
		DrawMapLineNE(out, { point.x - 2 * AmLine(8), point.y + AmLine(8) }, AmLine(8), playerColor);
		DrawMapLineSteepSW(out, point, AmLine(4), playerColor);
	} break;
	case Direction::East: {
		const Point point { base.x + AmLine(16), base.y };
		DrawMapLineNW(out, point, AmLine(4), playerColor);
		DrawHorizontalLine(out, { point.x - AmLine(16), point.y }, AmLine(16), playerColor);
		DrawMapLineSW(out, point, AmLine(4), playerColor);
	} break;
	case Direction::SouthEast: {
		const Point point { base.x + AmLine(16), base.y + AmLine(8) };
		DrawMapLineSteepNW(out, point, AmLine(4), playerColor);
		DrawMapLineSE(out, { point.x - 2 * AmLine(8), point.y - AmLine(8) }, AmLine(8), playerColor);
		DrawHorizontalLine(out, { point.x - (AmLine(8) + 1), point.y }, AmLine(8) + 1, playerColor);
	} break;
	case Direction::South: {
		const Point point { base.x, base.y + AmLine(16) };
		DrawVerticalLine(out, { point.x, point.y - AmLine(16) }, AmLine(16), playerColor);
		DrawMapLineSteepSW(out, { point.x + AmLine(4), point.y - 2 * AmLine(4) }, AmLine(4), playerColor);
		DrawMapLineSteepSE(out, { point.x - AmLine(4), point.y - 2 * AmLine(4) }, AmLine(4), playerColor);
	} break;
	case Direction::SouthWest: {
		const Point point { base.x - AmLine(16), base.y + AmLine(8) };
		DrawMapLineSteepNE(out, point, AmLine(4), playerColor);
		DrawMapLineSW(out, { point.x + 2 * AmLine(8), point.y - AmLine(8) }, AmLine(8), playerColor);
		DrawHorizontalLine(out, point, AmLine(8) + 1, playerColor);
	} break;
	case Direction::West: {
		const Point point { base.x - AmLine(16), base.y };
		DrawMapLineNE(out, point, AmLine(4), playerColor);
		DrawHorizontalLine(out, point, AmLine(16) + 1, playerColor);
		DrawMapLineSE(out, point, AmLine(4), playerColor);
	} break;
	case Direction::NorthWest: {
		const Point point { base.x - AmLine(16), base.y - AmLine(8) };
		DrawMapLineNW(out, { point.x + 2 * AmLine(8), point.y + AmLine(8) }, AmLine(8), playerColor);
		DrawHorizontalLine(out, point, AmLine(8) + 1, playerColor);
		DrawMapLineSteepSE(out, point, AmLine(4), playerColor);
	} break;
	case Direction::NoDirection:
		break;
	}
}

/**
 * @brief Renders game info, such as the name of the current level, and in multi player the name of the game and the game password.
 */
void DrawAutomapText(const Surface &out)
{
	Point linePosition { 8, 8 };

	if (gbIsMultiplayer) {
		if (GameName != "0.0.0.0" && !IsLoopback) {
			std::string description = std::string(_("Game: "));
			description.append(GameName);
			DrawString(out, description, linePosition);
			linePosition.y += 15;
		}

		std::string description;
		if (IsLoopback) {
			description = std::string(_("Offline Game"));
		} else if (!PublicGame) {
			description = std::string(_("Password: "));
			description.append(GamePassword);
		} else {
			description = std::string(_("Public Game"));
		}
		DrawString(out, description, linePosition);
		linePosition.y += 15;
	}

	if (setlevel) {
		DrawString(out, _(QuestLevelNames[setlvlnum]), linePosition);
		return;
	}

	std::string description;
	switch (leveltype) {
	case DTYPE_NEST:
		description = fmt::format(fmt::runtime(_("Level: Nest {:d}")), currlevel - 16);
		break;
	case DTYPE_CRYPT:
		description = fmt::format(fmt::runtime(_("Level: Crypt {:d}")), currlevel - 20);
		break;
	case DTYPE_TOWN:
		description = std::string(_("Town"));
		break;
	default:
		description = fmt::format(fmt::runtime(_("Level: {:d}")), currlevel);
		break;
	}

	DrawString(out, description, linePosition);
	linePosition.y += 15;
	string_view difficulty;
	switch (sgGameInitInfo.nDifficulty) {
	case DIFF_NORMAL:
		difficulty = _("Normal");
		break;
	case DIFF_NIGHTMARE:
		difficulty = _("Nightmare");
		break;
	case DIFF_HELL:
		difficulty = _("Hell");
		break;
	}

	std::string difficultyString = fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} means: Game Difficulty. */ "Difficulty: {:s}")), difficulty);
	DrawString(out, difficultyString, linePosition);
}

std::unique_ptr<AutomapTile[]> LoadAutomapData(size_t &tileCount)
{
	switch (leveltype) {
	case DTYPE_TOWN:
		return LoadFileInMem<AutomapTile>("levels\\towndata\\automap.amp", &tileCount);
	case DTYPE_CATHEDRAL:
		return LoadFileInMem<AutomapTile>("levels\\l1data\\l1.amp", &tileCount);
	case DTYPE_CATACOMBS:
		return LoadFileInMem<AutomapTile>("levels\\l2data\\l2.amp", &tileCount);
	case DTYPE_CAVES:
		return LoadFileInMem<AutomapTile>("levels\\l3data\\l3.amp", &tileCount);
	case DTYPE_HELL:
		return LoadFileInMem<AutomapTile>("levels\\l4data\\l4.amp", &tileCount);
	case DTYPE_NEST:
		return LoadFileInMem<AutomapTile>("nlevels\\l6data\\l6.amp", &tileCount);
	case DTYPE_CRYPT:
		return LoadFileInMem<AutomapTile>("nlevels\\l5data\\l5.amp", &tileCount);
	default:
		return nullptr;
	}
}

} // namespace

bool AutomapActive;
uint8_t AutomapView[DMAXX][DMAXY];
int AutoMapScale;
Displacement AutomapOffset;

void InitAutomapOnce()
{
	AutomapActive = false;
	AutoMapScale = 50;
}

void InitAutomap()
{
	size_t tileCount = 0;
	std::unique_ptr<AutomapTile[]> tileTypes = LoadAutomapData(tileCount);
	for (unsigned i = 0; i < tileCount; i++) {
		AutomapTypeTiles[i + 1] = tileTypes[i];
	}

	memset(AutomapView, 0, sizeof(AutomapView));

	for (auto &column : dFlags)
		for (auto &dFlag : column)
			dFlag &= ~DungeonFlag::Explored;
}

void StartAutomap()
{
	AutomapOffset = { 0, 0 };
	AutomapActive = true;
}

void AutomapUp()
{
	AutomapOffset.deltaX--;
	AutomapOffset.deltaY--;
}

void AutomapDown()
{
	AutomapOffset.deltaX++;
	AutomapOffset.deltaY++;
}

void AutomapLeft()
{
	AutomapOffset.deltaX--;
	AutomapOffset.deltaY++;
}

void AutomapRight()
{
	AutomapOffset.deltaX++;
	AutomapOffset.deltaY--;
}

void AutomapZoomIn()
{
	if (AutoMapScale >= 200)
		return;

	AutoMapScale += 5;
}

void AutomapZoomOut()
{
	if (AutoMapScale <= 50)
		return;

	AutoMapScale -= 5;
}

void DrawAutomap(const Surface &out)
{
	Automap = { (ViewPosition.x - 8) / 2, (ViewPosition.y - 8) / 2 };
	if (leveltype != DTYPE_TOWN) {
		Automap += { -4, -4 };
	}
	while (Automap.x + AutomapOffset.deltaX < 0)
		AutomapOffset.deltaX++;
	while (Automap.x + AutomapOffset.deltaX >= DMAXX)
		AutomapOffset.deltaX--;

	while (Automap.y + AutomapOffset.deltaY < 0)
		AutomapOffset.deltaY++;
	while (Automap.y + AutomapOffset.deltaY >= DMAXY)
		AutomapOffset.deltaY--;

	Automap += AutomapOffset;

	const Player &myPlayer = *MyPlayer;
	Displacement myPlayerOffset = {};
	if (myPlayer.isWalking())
		myPlayerOffset = GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);
	myPlayerOffset += Displacement { -1, (leveltype != DTYPE_CAVES) ? TILE_HEIGHT - 1 : -1 };

	int d = (AutoMapScale * 64) / 100;
	int cells = 2 * (gnScreenWidth / 2 / d) + 1;
	if (((gnScreenWidth / 2) % d) != 0)
		cells++;
	if (((gnScreenWidth / 2) % d) >= (AutoMapScale * 32) / 100)
		cells++;
	if ((myPlayerOffset.deltaX + myPlayerOffset.deltaY) != 0)
		cells++;

	Point screen {
		gnScreenWidth / 2,
		(gnScreenHeight - GetMainPanel().size.height) / 2
	};
	if ((cells & 1) != 0) {
		screen.x -= AmLine(64) * ((cells - 1) / 2);
		screen.y -= AmLine(32) * ((cells + 1) / 2);
	} else {
		screen.x -= AmLine(64) * (cells / 2) - AmLine(32);
		screen.y -= AmLine(32) * (cells / 2) + AmLine(16);
	}
	if ((ViewPosition.x & 1) != 0) {
		screen.x -= AmLine(16);
		screen.y -= AmLine(8);
	}
	if ((ViewPosition.y & 1) != 0) {
		screen.x += AmLine(16);
		screen.y -= AmLine(8);
	}

	screen.x += AutoMapScale * myPlayerOffset.deltaX / 100 / 2;
	screen.y += AutoMapScale * myPlayerOffset.deltaY / 100 / 2;

	if (CanPanelsCoverView()) {
		if (IsRightPanelOpen()) {
			screen.x -= gnScreenWidth / 4;
		}
		if (IsLeftPanelOpen()) {
			screen.x += gnScreenWidth / 4;
		}
	}

	Point map = { Automap.x - cells, Automap.y - 1 };

	for (int i = 0; i <= cells + 1; i++) {
		Point tile1 = screen;
		for (int j = 0; j < cells; j++) {
			DrawAutomapTile(out, tile1, { map.x + j, map.y - j });
			tile1.x += AmLine(64);
		}
		map.y++;

		Point tile2 { screen.x - AmLine(32), screen.y + AmLine(16) };
		for (int j = 0; j <= cells; j++) {
			DrawAutomapTile(out, tile2, { map.x + j, map.y - j });
			tile2.x += AmLine(64);
		}
		map.x++;
		screen.y += AmLine(32);
	}

	if (leveltype == DTYPE_CAVES)
		myPlayerOffset.deltaY += TILE_HEIGHT;
	for (size_t playerId = 0; playerId < Players.size(); playerId++) {
		Player &player = Players[playerId];
		if (player.isOnActiveLevel() && player.plractive && !player._pLvlChanging && (&player == MyPlayer || player.friendlyMode)) {
			DrawAutomapPlr(out, myPlayerOffset, playerId);
		}
	}

	myPlayerOffset.deltaY -= TILE_HEIGHT / 2;
	if (AutoMapShowItems)
		SearchAutomapItem(out, myPlayerOffset, 8, [](Point position) { return dItem[position.x][position.y] != 0; });
#ifdef _DEBUG
	if (IsDebugAutomapHighlightNeeded())
		SearchAutomapItem(out, myPlayerOffset, std::max(MAXDUNX, MAXDUNY), ShouldHighlightDebugAutomapTile);
#endif

	DrawAutomapText(out);
}

void UpdateAutomapExplorer(Point map, MapExplorationType explorer)
{
	if (AutomapView[map.x][map.y] < explorer)
		AutomapView[map.x][map.y] = explorer;
}

void SetAutomapView(Point position, MapExplorationType explorer)
{
	const Point map { (position.x - 16) / 2, (position.y - 16) / 2 };

	if (map.x < 0 || map.x >= DMAXX || map.y < 0 || map.y >= DMAXY) {
		return;
	}

	UpdateAutomapExplorer(map, explorer);

	AutomapTile tile = GetAutomapType(map);
	bool solid = tile.HasFlag(AutomapTile::Flags::Dirt);

	switch (tile.type) {
	case AutomapTile::Types::Vertical:
		if (solid) {
			auto tileSW = GetAutomapType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
		} else if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
		}
		break;
	case AutomapTile::Types::Horizontal:
		if (solid) {
			auto tileSE = GetAutomapType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x + 1, map.y }, explorer);
		} else if (HasAutomapFlag({ map.x, map.y - 1 }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x, map.y - 1 }, explorer);
		}
		break;
	case AutomapTile::Types::Cross:
		if (solid) {
			auto tileSW = GetAutomapType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
			auto tileSE = GetAutomapType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x + 1, map.y }, explorer);
		} else {
			if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
			if (HasAutomapFlag({ map.x, map.y - 1 }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y - 1 }, explorer);
			if (HasAutomapFlag({ map.x - 1, map.y - 1 }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x - 1, map.y - 1 }, explorer);
		}
		break;
	case AutomapTile::Types::FenceVertical:
		if (solid) {
			if (HasAutomapFlag({ map.x, map.y - 1 }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y - 1 }, explorer);
			auto tileSW = GetAutomapType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
		} else if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
		}
		break;
	case AutomapTile::Types::FenceHorizontal:
		if (solid) {
			if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
			auto tileSE = GetAutomapType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.HasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x + 1, map.y }, explorer);
		} else if (HasAutomapFlag({ map.x, map.y - 1 }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x, map.y - 1 }, explorer);
		}
		break;
	default:
		break;
	}
}

void AutomapZoomReset()
{
	AutomapOffset = { 0, 0 };
}

} // namespace devilution
