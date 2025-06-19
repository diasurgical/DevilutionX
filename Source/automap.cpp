/**
 * @file automap.cpp
 *
 * Implementation of the in-game map overlay.
 */
#include "automap.h"

#include <algorithm>
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
#include "utils/attributes.h"
#include "utils/enum_traits.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#include "lighting.h"
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
	/** color for activated pentragram on automap */
	MapColorsPentagramOpen = (PAL8_RED + 2),
	/** color for cave lava on automap */
	MapColorsLava = (PAL8_ORANGE + 2),
	/** color for cave water on automap */
	MapColorsWater = (PAL8_BLUE + 2),
	/** color for hive acid on automap */
	MapColorsAcid = (PAL8_YELLOW + 4),
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
		CaveHorizontalWoodCross,
		CaveVerticalWoodCross,
		CaveLeftCorner,
		CaveRightCorner,
		CaveBottomCorner,
		CaveHorizontalWood,
		CaveVerticalWood,
		CaveWoodCross,
		CaveRightWoodCross,
		CaveLeftWoodCross,
		HorizontalLavaThin,
		VerticalLavaThin,
		BendSouthLavaThin,
		BendWestLavaThin,
		BendEastLavaThin,
		BendNorthLavaThin,
		VerticalWallLava,
		HorizontalWallLava,
		SELava,
		SWLava,
		NELava,
		NWLava,
		SLava,
		WLava,
		ELava,
		NLava,
		Lava,
		CaveHorizontalWallLava,
		CaveVerticalWallLava,
		HorizontalBridgeLava,
		VerticalBridgeLava,
		VerticalDiamond,
		HorizontalDiamond,
		PentagramClosed,
		PentagramOpen,
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

	Flags flags = {};

	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool hasFlag(Flags test) const
	{
		return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
	}

	template <typename... Args>
	[[nodiscard]] DVL_ALWAYS_INLINE constexpr bool hasAnyFlag(Flags flag, Args... flags)
	{
		return (static_cast<uint8_t>(this->flags)
		           & (static_cast<uint8_t>(flag) | ... | static_cast<uint8_t>(flags)))
		    != 0;
	}
};

/**
 * Maps from tile_id to automap type.
 */
std::array<AutomapTile, 256> AutomapTypeTiles;

/**
 * @brief Draw a diamond on top tile.
 */
void DrawDiamond(const Surface &out, Point center, uint8_t color)
{
	DrawMapLineNE(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::FullTile), color);
	DrawMapLineSE(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::FullTile), color);
	DrawMapLineSE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileUp), AmLine(AmLineLength::FullTile), color);
	DrawMapLineNE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::None), AmLine(AmLineLength::FullTile), color);
}

/**
 * @brief Draws a bright diamond and a line, orientation depending on the tileset.
 */
void DrawMapVerticalDoor(const Surface &out, Point center, AutomapTile neTile, uint8_t colorBright, uint8_t colorDim)
{
	AmWidthOffset lWidthOffset;
	AmHeightOffset lHeightOffset;
	AmWidthOffset dWidthOffset;
	AmHeightOffset dHeightOffset;
	AmLineLength length;

	switch (leveltype) {
	case DTYPE_CATHEDRAL:
	case DTYPE_CRYPT:
		lWidthOffset = AmWidthOffset::QuarterTileLeft;
		lHeightOffset = AmHeightOffset::QuarterTileUp;

		dWidthOffset = AmWidthOffset::HalfTileLeft;
		dHeightOffset = AmHeightOffset::HalfTileDown;

		length = AmLineLength::HalfTile;
		break;
	case DTYPE_CATACOMBS:
		lWidthOffset = AmWidthOffset::ThreeQuartersTileLeft;
		lHeightOffset = AmHeightOffset::QuarterTileDown;

		dWidthOffset = AmWidthOffset::None;
		dHeightOffset = AmHeightOffset::None;

		length = AmLineLength::FullTile;
		break;
	case DTYPE_CAVES:
		lWidthOffset = AmWidthOffset::QuarterTileLeft;
		lHeightOffset = AmHeightOffset::ThreeQuartersTileDown;

		dWidthOffset = AmWidthOffset::HalfTileRight;
		dHeightOffset = AmHeightOffset::HalfTileDown;

		length = AmLineLength::FullTile;
		break;
	default:
		app_fatal("Invalid leveltype");
	}
	if (!(neTile.hasFlag(AutomapTile::Flags::VerticalPassage) && leveltype == DTYPE_CATHEDRAL))
		DrawMapLineNE(out, center + AmOffset(lWidthOffset, lHeightOffset), AmLine(length), colorDim);
	DrawDiamond(out, center + AmOffset(dWidthOffset, dHeightOffset), colorBright);
}

/**
 * @brief Draws a bright diamond and a line, orientation depending on the tileset.
 */
void DrawMapHorizontalDoor(const Surface &out, Point center, AutomapTile nwTile, uint8_t colorBright, uint8_t colorDim)
{
	AmWidthOffset lWidthOffset;
	AmHeightOffset lHeightOffset;
	AmWidthOffset dWidthOffset;
	AmHeightOffset dHeightOffset;
	AmLineLength length;

	switch (leveltype) {
	case DTYPE_CATHEDRAL:
	case DTYPE_CRYPT:
		lWidthOffset = AmWidthOffset::None;
		lHeightOffset = AmHeightOffset::HalfTileUp;

		dWidthOffset = AmWidthOffset::HalfTileRight;
		dHeightOffset = AmHeightOffset::HalfTileDown;

		length = AmLineLength::HalfTile;
		break;
	case DTYPE_CATACOMBS:
		lWidthOffset = AmWidthOffset::QuarterTileRight;
		lHeightOffset = AmHeightOffset::QuarterTileUp;

		dWidthOffset = AmWidthOffset::None;
		dHeightOffset = AmHeightOffset::None;

		length = AmLineLength::FullTile;
		break;
	case DTYPE_CAVES:
		lWidthOffset = AmWidthOffset::QuarterTileLeft;
		lHeightOffset = AmHeightOffset::QuarterTileDown;

		dWidthOffset = AmWidthOffset::HalfTileLeft;
		dHeightOffset = AmHeightOffset::HalfTileDown;

		length = AmLineLength::FullTile;
		break;
		break;
	default:
		app_fatal("Invalid leveltype");
	}
	if (!(nwTile.hasFlag(AutomapTile::Flags::HorizontalPassage) && leveltype == DTYPE_CATHEDRAL))
		DrawMapLineSE(out, center + AmOffset(lWidthOffset, lHeightOffset), AmLine(length), colorDim);
	DrawDiamond(out, center + AmOffset(dWidthOffset, dHeightOffset), colorBright);
}

/**
 * @brief Draw 16 individual pixels equally spaced apart, used to communicate OOB area to the player.
 */
void DrawDirt(const Surface &out, Point center, AutomapTile nwTile, AutomapTile neTile, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);
	// Prevent the top dirt pixel from appearing inside arch diamonds
	if (!nwTile.hasAnyFlag(AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::HorizontalGrate)
	    && !neTile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::VerticalGrate))
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), color);
	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawBridge(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center, color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverRightIn(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverCornerSouth(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);
}

void DrawRiverCornerNorth(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
}

void DrawRiverLeftOut(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverLeftIn(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), color);
	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);
}

void DrawRiverCornerWest(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
}

void DrawRiverCornerEast(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverRightOut(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiver(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverForkIn(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), color);
	SetMapPixel(out, center, color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color);
}

void DrawRiverForkOut(const Surface &out, Point center, uint8_t color)
{
	SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
	SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color);

	SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);
}

template <Direction TDir1, Direction TDir2>
void DrawLavaRiver(const Surface &out, Point center, uint8_t color, bool hasBridge)
{
	// First row (y = 0)
	if constexpr (IsAnyOf(Direction::NorthWest, TDir1, TDir2)) {
		if (!(hasBridge && IsAnyOf(TDir1, Direction::NorthWest))) {
			SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color);
			SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);
		}
	}

	// Second row (y = 1)
	if constexpr (IsAnyOf(Direction::NorthEast, TDir1, TDir2)) {
		if (!(hasBridge && IsAnyOf(Direction::NorthEast, TDir1, TDir2)))
			SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color);
	}
	if constexpr (IsAnyOf(Direction::NorthWest, TDir1, TDir2) || IsAnyOf(Direction::NorthEast, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::None), color);
	}
	if constexpr (IsAnyOf(Direction::SouthWest, TDir1, TDir2) || IsAnyOf(Direction::NorthWest, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color);
	}
	if constexpr (IsAnyOf(Direction::SouthWest, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);
	}

	// Third row (y = 2)
	if constexpr (IsAnyOf(Direction::NorthEast, TDir1, TDir2)) {
		if (!(hasBridge && IsAnyOf(Direction::NorthEast, TDir1, TDir2)))
			SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);
	}
	if constexpr (IsAnyOf(Direction::NorthEast, TDir1, TDir2) || IsAnyOf(Direction::SouthEast, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color);
	}
	if constexpr (IsAnyOf(Direction::SouthWest, TDir1, TDir2) || IsAnyOf(Direction::SouthEast, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color);
	}
	if constexpr (IsAnyOf(Direction::SouthWest, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color);
	}

	// Fourth row (y = 3)
	if constexpr (IsAnyOf(Direction::SouthEast, TDir1, TDir2)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color);
	}
}

template <Direction TDir>
void DrawLava(const Surface &out, Point center, uint8_t color)
{
	if constexpr (IsAnyOf(TDir, Direction::NorthWest, Direction::North, Direction::NorthEast, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), color); // north corner
	}
	if constexpr (IsNoneOf(TDir, Direction::South, Direction::SouthEast, Direction::East)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileUp), color); // northwest edge
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), color);             // northwest edge
	}
	if constexpr (IsAnyOf(TDir, Direction::SouthWest, Direction::West, Direction::NorthWest, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), color); // west corner
	}
	if constexpr (IsAnyOf(TDir, Direction::South, Direction::SouthWest, Direction::West, Direction::NorthWest, Direction::SouthEast, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), color);             // southwest edge
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), color); // southwest edge
	}
	if constexpr (IsAnyOf(TDir, Direction::South, Direction::SouthWest, Direction::SouthEast, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), color); // south corner
	}
	if constexpr (IsAnyOf(TDir, Direction::South, Direction::SouthWest, Direction::NorthEast, Direction::East, Direction::SouthEast, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), color);             // southeast edge
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), color); // southeast edge
	}
	if constexpr (IsAnyOf(TDir, Direction::NorthEast, Direction::East, Direction::SouthEast, Direction::NoDirection)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), color); // east corner
	}
	if constexpr (IsNoneOf(TDir, Direction::South, Direction::SouthWest, Direction::West)) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileUp), color); // northeast edge
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), color);             // northeast edge
	}
	if constexpr (TDir != Direction::South) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::None), color); // north center
	}
	if constexpr (TDir != Direction::East) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown), color); // west center
	}
	if constexpr (TDir != Direction::West) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::QuarterTileDown), color); // east center
	}
	if constexpr (TDir != Direction::North) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), color); // south center
	}
}

/**
 * @brief Draw 4 south-east facing lines, used to communicate trigger locations to the player.
 */
void DrawStairs(const Surface &out, Point center, uint8_t color)
{
	constexpr int NumStairSteps = 4;
	const Displacement offset = AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::QuarterTileDown);
	AmWidthOffset w = AmWidthOffset::QuarterTileLeft;
	AmHeightOffset h = AmHeightOffset::QuarterTileUp;

	if (IsAnyOf(leveltype, DTYPE_CATACOMBS, DTYPE_HELL)) {
		w = AmWidthOffset::QuarterTileLeft;
		h = AmHeightOffset::ThreeQuartersTileUp;
	}

	// Initial point based on the 'center' position.
	Point p = center + AmOffset(w, h);

	for (int i = 0; i < NumStairSteps; ++i) {
		DrawMapLineSE(out, p, AmLine(AmLineLength::DoubleTile), color);
		p += offset;
	}
}
/**
 * @brief Redraws the bright line of the door diamond that gets overwritten by later drawn lines.
 */
void FixHorizontalDoor(const Surface &out, Point center, AutomapTile nwTile, uint8_t colorBright)
{
	if (leveltype != DTYPE_CATACOMBS && nwTile.hasFlag(AutomapTile::Flags::HorizontalDoor)) {
		DrawMapLineNE(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::FullTile), colorBright);
	}
}

/**
 * @brief Redraws the bright line of the door diamond that gets overwritten by later drawn lines.
 */
void FixVerticalDoor(const Surface &out, Point center, AutomapTile neTile, uint8_t colorBright)
{
	if (leveltype != DTYPE_CATACOMBS && neTile.hasFlag(AutomapTile::Flags::VerticalDoor)) {
		DrawMapLineSE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileUp), AmLine(AmLineLength::FullTile), colorBright);
	}
}

/**
 * @brief Draw half-tile length lines to connect walls to any walls to the north-west and/or north-east
 */
void DrawWallConnections(const Surface &out, Point center, AutomapTile tile, AutomapTile nwTile, AutomapTile neTile, uint8_t colorBright, uint8_t colorDim)
{
	if (tile.hasFlag(AutomapTile::Flags::HorizontalDoor) && nwTile.hasFlag(AutomapTile::Flags::HorizontalDoor)) {
		//  fix missing lower half of the line connecting door pairs in Lazarus' level
		DrawMapLineSE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::HalfTile), colorDim);
	}
	if (IsAnyOf(nwTile.type, AutomapTile::Types::HorizontalWallLava, AutomapTile::Types::Horizontal, AutomapTile::Types::HorizontalDiamond, AutomapTile::Types::FenceHorizontal, AutomapTile::Types::Cross, AutomapTile::Types::CaveVerticalWoodCross, AutomapTile::Types::CaveRightCorner)) {
		DrawMapLineSE(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileUp), AmLine(AmLineLength::HalfTile), colorDim);
		FixHorizontalDoor(out, center, nwTile, colorBright);
	}
	if (IsAnyOf(neTile.type, AutomapTile::Types::VerticalWallLava, AutomapTile::Types::Vertical, AutomapTile::Types::VerticalDiamond, AutomapTile::Types::FenceVertical, AutomapTile::Types::Cross, AutomapTile::Types::CaveHorizontalWoodCross, AutomapTile::Types::CaveLeftCorner)) {
		DrawMapLineNE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::HalfTile), colorDim);
		FixVerticalDoor(out, center, neTile, colorBright);
	}
}

/**
 * @brief Draws a dotted line to represent a wall grate.
 */
void DrawMapVerticalGrate(const Surface &out, Point center, uint8_t colorDim)
{
	Point pos1 = center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None) + AmOffset(AmWidthOffset::EighthTileRight, AmHeightOffset::EighthTileUp);
	Point pos2 = center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None);
	Point pos3 = center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None) + AmOffset(AmWidthOffset::EighthTileLeft, AmHeightOffset::EighthTileDown);

	SetMapPixel(out, pos1 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos2 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos3 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos1, colorDim);
	SetMapPixel(out, pos2, colorDim);
	SetMapPixel(out, pos3, colorDim);
}

/**
 * @brief Draws a dotted line to represent a wall grate.
 */
void DrawMapHorizontalGrate(const Surface &out, Point center, uint8_t colorDim)
{
	Point pos1 = center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None) + AmOffset(AmWidthOffset::EighthTileLeft, AmHeightOffset::EighthTileUp);
	Point pos2 = center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None);
	Point pos3 = center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None) + AmOffset(AmWidthOffset::EighthTileRight, AmHeightOffset::EighthTileDown);

	SetMapPixel(out, pos1 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos2 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos3 + Displacement { 0, 1 }, 0);
	SetMapPixel(out, pos1, colorDim);
	SetMapPixel(out, pos2, colorDim);
	SetMapPixel(out, pos3, colorDim);
}

/**
 * Left-facing obstacle
 */
void DrawHorizontal(const Surface &out, Point center, AutomapTile tile, AutomapTile nwTile, AutomapTile neTile, AutomapTile seTile, uint8_t colorBright, uint8_t colorDim)
{
	AmWidthOffset w = AmWidthOffset::None;
	AmHeightOffset h = AmHeightOffset::HalfTileUp;
	AmLineLength l = AmLineLength::FullAndHalfTile;

	// Draw a diamond in the top tile
	if (neTile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::VerticalGrate)                                                                           // NE tile has an arch, so add a diamond for visual consistency
	    || nwTile.hasAnyFlag(AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::HorizontalGrate)                                                                    // NW tile has an arch, so add a diamond for visual consistency
	    || tile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::VerticalGrate, AutomapTile::Flags::HorizontalGrate) // Current tile has an arch, add a diamond
	    || tile.type == AutomapTile::Types::HorizontalDiamond) {                                                                                                         // wall ending in hell that should end with a diamond
		w = AmWidthOffset::QuarterTileRight;
		h = AmHeightOffset::QuarterTileUp;
		l = AmLineLength::FullTile; // shorten line to avoid overdraw
		DrawDiamond(out, center, colorDim);
		FixHorizontalDoor(out, center, nwTile, colorBright);
		FixVerticalDoor(out, center, neTile, colorBright);
	}
	// Shorten line to avoid overdraw
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)
	    && IsAnyOf(tile.type, AutomapTile::Types::CaveVerticalCross, AutomapTile::Types::CaveVerticalWoodCross)
	    && !(IsAnyOf(seTile.type, AutomapTile::Types::Horizontal, AutomapTile::Types::CaveVerticalCross, AutomapTile::Types::CaveVerticalWoodCross, AutomapTile::Types::Corner))) {
		l = AmLineLength::FullTile;
	}
	// Draw the wall line if the wall is solid
	if (!tile.hasFlag(AutomapTile::Flags::HorizontalPassage)) {
		DrawMapLineSE(out, center + AmOffset(w, h), AmLine(l), colorDim);
		return;
	}
	// Draw door or grate
	if (tile.hasFlag(AutomapTile::Flags::HorizontalDoor)) {
		DrawMapHorizontalDoor(out, center, nwTile, colorBright, colorDim);
	} else if (tile.hasFlag(AutomapTile::Flags::HorizontalGrate)) {
		DrawMapHorizontalGrate(out, center, colorDim);
	}
}

/**
 * Right-facing obstacle
 */
void DrawVertical(const Surface &out, Point center, AutomapTile tile, AutomapTile nwTile, AutomapTile neTile, AutomapTile swTile, uint8_t colorBright, uint8_t colorDim)
{
	AmWidthOffset w = AmWidthOffset::ThreeQuartersTileLeft;
	AmHeightOffset h = AmHeightOffset::QuarterTileDown;
	AmLineLength l = AmLineLength::FullAndHalfTile;

	// Draw a diamond in the top tile
	if (neTile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::VerticalGrate)                                                                           // NE tile has an arch, so add a diamond for visual consistency
	    || nwTile.hasAnyFlag(AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::HorizontalGrate)                                                                    // NW tile has an arch, so add a diamond for visual consistency
	    || tile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::VerticalGrate, AutomapTile::Flags::HorizontalGrate) // Current tile has an arch, add a diamond
	    || tile.type == AutomapTile::Types::VerticalDiamond) {                                                                                                           // wall ending in hell that should end with a diamond
		l = AmLineLength::FullTile;                                                                                                                                      // shorten line to avoid overdraw
		DrawDiamond(out, center, colorDim);
		FixVerticalDoor(out, center, nwTile, colorBright);
		FixVerticalDoor(out, center, neTile, colorBright);
	}
	// Shorten line to avoid overdraw and adjust offset to match
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)
	    && IsAnyOf(tile.type, AutomapTile::Types::CaveHorizontalCross, AutomapTile::Types::CaveHorizontalWoodCross)
	    && !(IsAnyOf(swTile.type, AutomapTile::Types::Vertical, AutomapTile::Types::CaveHorizontalCross, AutomapTile::Types::CaveHorizontalWoodCross, AutomapTile::Types::Corner))) {
		w = AmWidthOffset::HalfTileLeft;
		h = AmHeightOffset::None;
		l = AmLineLength::FullTile;
	}
	// Draw the wall line if the wall is solid
	if (!tile.hasFlag(AutomapTile::Flags::VerticalPassage)) {
		DrawMapLineNE(out, center + AmOffset(w, h), AmLine(l), colorDim);
		return;
	}
	// Draw door or grate
	if (tile.hasFlag(AutomapTile::Flags::VerticalDoor)) {
		DrawMapVerticalDoor(out, center, neTile, colorBright, colorDim);
	} else if (tile.hasFlag(AutomapTile::Flags::VerticalGrate)) {
		DrawMapVerticalGrate(out, center, colorDim);
	}
}

/**
 * @brief Draw half-tile length lines to connect walls to any walls to the south-west and/or south-east
 * (For caves the horizontal/vertical flags are swapped)
 */
void DrawCaveWallConnections(const Surface &out, Point center, AutomapTile sTile, AutomapTile swTile, AutomapTile seTile, uint8_t colorDim)
{
	if (IsAnyOf(swTile.type, AutomapTile::Types::CaveVerticalWallLava, AutomapTile::Types::CaveVertical, AutomapTile::Types::CaveVerticalWood, AutomapTile::Types::CaveCross, AutomapTile::Types::CaveWoodCross, AutomapTile::Types::CaveRightWoodCross, AutomapTile::Types::CaveLeftWoodCross, AutomapTile::Types::CaveRightCorner)) {
		DrawMapLineNE(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), AmLine(AmLineLength::HalfTile), colorDim);
	}
	if (IsAnyOf(seTile.type, AutomapTile::Types::CaveHorizontalWallLava, AutomapTile::Types::CaveHorizontal, AutomapTile::Types::CaveHorizontalWood, AutomapTile::Types::CaveCross, AutomapTile::Types::CaveWoodCross, AutomapTile::Types::CaveRightWoodCross, AutomapTile::Types::CaveLeftWoodCross, AutomapTile::Types::CaveLeftCorner)) {
		DrawMapLineSE(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown), AmLine(AmLineLength::HalfTile), colorDim);
	}
}
void DrawCaveHorizontalDirt(const Surface &out, Point center, AutomapTile tile, AutomapTile swTile, uint8_t colorDim)
{
	if (swTile.hasFlag(AutomapTile::Flags::Dirt) || (leveltype != DTYPE_TOWN && IsNoneOf(tile.type, AutomapTile::Types::CaveHorizontalWood, AutomapTile::Types::CaveHorizontalWoodCross, AutomapTile::Types::CaveWoodCross, AutomapTile::Types::CaveLeftWoodCross))) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::ThreeQuartersTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), colorDim);
	}
}

/**
 * For caves the horizontal/vertical flags are swapped
 */
void DrawCaveHorizontal(const Surface &out, Point center, AutomapTile tile, AutomapTile nwTile, AutomapTile swTile, uint8_t colorBright, uint8_t colorDim)
{
	if (tile.hasFlag(AutomapTile::Flags::VerticalDoor)) {
		DrawMapHorizontalDoor(out, center, nwTile, colorBright, colorDim);
	} else {
		AmWidthOffset w;
		AmHeightOffset h;
		AmLineLength l;

		if (IsAnyOf(tile.type, AutomapTile::Types::CaveHorizontalCross, AutomapTile::Types::CaveHorizontalWoodCross)) {
			w = AmWidthOffset::HalfTileLeft;
			h = AmHeightOffset::None;
			l = AmLineLength::FullTile;
		} else {
			w = AmWidthOffset::ThreeQuartersTileLeft;
			h = AmHeightOffset::QuarterTileUp;
			l = AmLineLength::FullAndHalfTile;
		}
		DrawCaveHorizontalDirt(out, center, tile, swTile, colorDim);
		DrawMapLineSE(out, center + AmOffset(w, h), AmLine(l), colorDim);
	}
}

void DrawCaveVerticalDirt(const Surface &out, Point center, AutomapTile tile, AutomapTile seTile, uint8_t colorDim)
{
	if (seTile.hasFlag(AutomapTile::Flags::Dirt) || (leveltype != DTYPE_TOWN && IsNoneOf(tile.type, AutomapTile::Types::CaveVerticalWood, AutomapTile::Types::CaveVerticalWoodCross, AutomapTile::Types::CaveWoodCross, AutomapTile::Types::CaveRightWoodCross))) {
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::QuarterTileRight, AmHeightOffset::ThreeQuartersTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), colorDim);
		SetMapPixel(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileRight, AmHeightOffset::QuarterTileDown), colorDim);
	}
}

/**
 * For caves the horizontal/vertical flags are swapped
 */
void DrawCaveVertical(const Surface &out, Point center, AutomapTile tile, AutomapTile neTile, AutomapTile seTile, uint8_t colorBright, uint8_t colorDim)
{
	if (tile.hasFlag(AutomapTile::Flags::HorizontalDoor)) {
		DrawMapVerticalDoor(out, center, neTile, colorBright, colorDim);
	} else {
		AmLineLength l;

		if (IsAnyOf(tile.type, AutomapTile::Types::CaveVerticalCross, AutomapTile::Types::CaveVerticalWoodCross)) {
			l = AmLineLength::FullTile;
		} else {
			l = AmLineLength::FullAndHalfTile;
		}
		DrawCaveVerticalDirt(out, center, tile, seTile, colorDim);
		DrawMapLineNE(out, { center + AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown) }, AmLine(l), colorDim);
	}
}

void DrawCaveLeftCorner(const Surface &out, Point center, uint8_t colorDim)
{
	DrawMapLineSE(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileUp), AmLine(AmLineLength::HalfTile), colorDim);
	DrawMapLineNE(out, center + AmOffset(AmWidthOffset::ThreeQuartersTileLeft, AmHeightOffset::QuarterTileDown), AmLine(AmLineLength::HalfTile), colorDim);
}

void DrawCaveRightCorner(const Surface &out, Point center, uint8_t colorDim)
{
	DrawMapLineSE(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), AmLine(AmLineLength::HalfTile), colorDim);
	DrawMapLineNE(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None), AmLine(AmLineLength::HalfTile), colorDim);
}

void DrawMapEllipse(const Surface &out, Point from, int radius, uint8_t colorIndex)
{
	const int a = radius;
	const int b = radius / 2;

	int x = 0;
	int y = b;

	// Offset ellipse so the center of the ellipse is the center of our megatile on the x plane
	from.x -= radius;

	// Initial point
	SetMapPixel(out, { from.x, from.y + b }, colorIndex);
	SetMapPixel(out, { from.x, from.y - b }, colorIndex);

	// Initialize the parameters
	int p1 = (b * b) - (a * a * b) + (a * a) / 4;

	// Region 1
	while ((b * b * x) < (a * a * y)) {
		x++;
		if (p1 < 0) {
			p1 += (2 * b * b * x) + (b * b);
		} else {
			y--;
			p1 += (2 * b * b * x) - (2 * a * a * y) + (b * b);
		}

		SetMapPixel(out, { from.x + x, from.y + y }, colorIndex);
		SetMapPixel(out, { from.x - x, from.y + y }, colorIndex);
		SetMapPixel(out, { from.x + x, from.y - y }, colorIndex);
		SetMapPixel(out, { from.x - x, from.y - y }, colorIndex);
	}

	// Initialize the second parameter for Region 2
	int p2 = (b * b * ((x + 1) * (x + 1))) + (a * a * ((y - 1) * (y - 1))) - (a * a * b * b);

	// Region 2
	while (y > 0) {
		y--;
		if (p2 > 0) {
			p2 += (-2 * a * a * y) + (a * a);
		} else {
			x++;
			p2 += (2 * b * b * x) - (2 * a * a * y) + (a * a);
		}

		SetMapPixel(out, { from.x + x, from.y + y }, colorIndex);
		SetMapPixel(out, { from.x - x, from.y + y }, colorIndex);
		SetMapPixel(out, { from.x + x, from.y - y }, colorIndex);
		SetMapPixel(out, { from.x - x, from.y - y }, colorIndex);
	}
}

void DrawMapStar(const Surface &out, Point from, int radius, uint8_t color)
{
	const int scaleFactor = 128;
	Point anchors[5];

	// Offset star so the center of the star is the center of our megatile on the x plane
	from.x -= radius;

	anchors[0] = { from.x - (121 * radius / scaleFactor), from.y + (19 * radius / scaleFactor) }; // Left Point
	anchors[1] = { from.x + (121 * radius / scaleFactor), from.y + (19 * radius / scaleFactor) }; // Right Point
	anchors[2] = { from.x, from.y + (64 * radius / scaleFactor) };                                // Bottom Point
	anchors[3] = { from.x - (75 * radius / scaleFactor), from.y - (51 * radius / scaleFactor) };  // Top Left Point
	anchors[4] = { from.x + (75 * radius / scaleFactor), from.y - (51 * radius / scaleFactor) };  // Top Right Point

	// Draw lines between the anchors to form a star
	DrawMapFreeLine(out, anchors[3], anchors[1], color); // Connect Top Left -> Right
	DrawMapFreeLine(out, anchors[1], anchors[0], color); // Connect Right -> Left
	DrawMapFreeLine(out, anchors[0], anchors[4], color); // Connect Left -> Top Right
	DrawMapFreeLine(out, anchors[4], anchors[2], color); // Connect Top Right -> Bottom
	DrawMapFreeLine(out, anchors[2], anchors[3], color); // Connect Bottom -> Top Left
}

/**
 * @brief Check if a given tile has the provided AutomapTile flag
 */
bool HasAutomapFlag(Point position, AutomapTile::Flags type)
{
	if (position.x < 0 || position.x >= DMAXX || position.y < 0 || position.y >= DMAXX) {
		return false;
	}

	return AutomapTypeTiles[dungeon[position.x][position.y]].hasFlag(type);
}

/**
 * @brief Returns the automap shape at the given coordinate.
 */
AutomapTile GetAutomapTileType(Point position)
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

	return GetAutomapTileType(map);
}

/**
 * @brief Renders the given automap shape at the specified screen coordinates.
 */
void DrawAutomapTile(const Surface &out, Point center, Point map)
{
	uint8_t colorBright = MapColorsBright;
	uint8_t colorDim = MapColorsDim;
	MapExplorationType explorationType = static_cast<MapExplorationType>(AutomapView[std::clamp(map.x, 0, DMAXX - 1)][std::clamp(map.y, 0, DMAXY - 1)]);

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

	bool noConnect = false;
	AutomapTile tile = GetAutomapTypeView(map + Direction::NoDirection);
	AutomapTile nwTile = GetAutomapTypeView(map + Direction::NorthWest);
	AutomapTile neTile = GetAutomapTypeView(map + Direction::NorthEast);

#ifdef _DEBUG
	if (DebugVision) {
		if (IsTileLit(map.megaToWorld()))
			DrawDiamond(out, center, PAL8_ORANGE + 1);
		if (IsTileLit(map.megaToWorld() + Direction::South))
			DrawDiamond(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), PAL8_ORANGE + 1);
		if (IsTileLit(map.megaToWorld() + Direction::SouthWest))
			DrawDiamond(out, center + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), PAL8_ORANGE + 1);
		if (IsTileLit(map.megaToWorld() + Direction::SouthEast))
			DrawDiamond(out, center + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), PAL8_ORANGE + 1);
	}
#endif

	// If the tile is an arch, grate, or diamond, we draw a diamond and therefore don't want connection lines
	if (tile.hasAnyFlag(AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::VerticalArch, AutomapTile::Flags::HorizontalGrate, AutomapTile::Flags::VerticalGrate)
	    || nwTile.hasAnyFlag(AutomapTile::Flags::HorizontalArch, AutomapTile::Flags::HorizontalGrate)
	    || neTile.hasAnyFlag(AutomapTile::Flags::VerticalArch, AutomapTile::Flags::VerticalGrate)
	    || tile.type == AutomapTile::Types::Diamond) {
		noConnect = true;
	}

	// These tilesets have doors where the connection lines would be drawn
	if (IsAnyOf(leveltype, DTYPE_CATACOMBS, DTYPE_CAVES) && (tile.hasFlag(AutomapTile::Flags::HorizontalDoor) || tile.hasFlag(AutomapTile::Flags::VerticalDoor)))
		noConnect = true;

	const AutomapTile swTile = GetAutomapTypeView(map + Direction::SouthWest);
	const AutomapTile sTile = GetAutomapTypeView(map + Direction::South);
	const AutomapTile seTile = GetAutomapTypeView(map + Direction::SouthEast);
	const AutomapTile nTile = GetAutomapTypeView(map + Direction::North);
	const AutomapTile wTile = GetAutomapTypeView(map + Direction::West);
	const AutomapTile eTile = GetAutomapTypeView(map + Direction::East);

	if ((leveltype == DTYPE_TOWN && tile.hasFlag(AutomapTile::Flags::Dirt))
	    || (tile.hasFlag(AutomapTile::Flags::Dirt)
	        && (tile.type != AutomapTile::Types::None
	            || swTile.type != AutomapTile::Types::None
	            || sTile.type != AutomapTile::Types::None
	            || seTile.type != AutomapTile::Types::None
	            || IsAnyOf(nwTile.type, AutomapTile::Types::CaveCross, AutomapTile::Types::CaveVertical, AutomapTile::Types::CaveVerticalCross, AutomapTile::Types::CaveVerticalWallLava, AutomapTile::Types::CaveLeftWoodCross)
	            || IsAnyOf(nTile.type, AutomapTile::Types::CaveCross)
	            || IsAnyOf(neTile.type, AutomapTile::Types::CaveCross, AutomapTile::Types::CaveHorizontal, AutomapTile::Types::CaveHorizontalCross, AutomapTile::Types::CaveHorizontalWallLava, AutomapTile::Types::CaveRightWoodCross)
	            || IsAnyOf(wTile.type, AutomapTile::Types::CaveVerticalCross)
	            || IsAnyOf(eTile.type, AutomapTile::Types::CaveHorizontalCross)))) {
		DrawDirt(out, center, nwTile, neTile, colorDim);
	}

	if (tile.hasFlag(AutomapTile::Flags::Stairs)) {
		DrawStairs(out, center, colorBright);
	}

	if (!noConnect) {
		if (IsAnyOf(leveltype, DTYPE_TOWN, DTYPE_CAVES, DTYPE_NEST)) {
			DrawCaveWallConnections(out, center, sTile, swTile, seTile, colorDim);
		}
		DrawWallConnections(out, center, tile, nwTile, neTile, colorBright, colorDim);
	}

	uint8_t lavaColor = MapColorsLava;
	if (leveltype == DTYPE_NEST) {
		lavaColor = MapColorsAcid;
	} else if (setlevel && setlvlnum == Quests[Q_PWATER]._qslvl) {
		if (Quests[Q_PWATER]._qactive != QUEST_DONE) {
			lavaColor = MapColorsAcid;
		} else {
			lavaColor = MapColorsWater;
		}
	}

	switch (tile.type) {
	case AutomapTile::Types::Diamond: // stand-alone column or other unpassable object
		DrawDiamond(out, center, colorDim);
		break;
	case AutomapTile::Types::Vertical:
	case AutomapTile::Types::FenceVertical:
	case AutomapTile::Types::VerticalDiamond:
		DrawVertical(out, center, tile, nwTile, neTile, swTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::Horizontal:
	case AutomapTile::Types::FenceHorizontal:
	case AutomapTile::Types::HorizontalDiamond:
		DrawHorizontal(out, center, tile, nwTile, neTile, seTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::Cross:
		DrawVertical(out, center, tile, nwTile, neTile, swTile, colorBright, colorDim);
		DrawHorizontal(out, center, tile, nwTile, neTile, seTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveHorizontalCross:
	case AutomapTile::Types::CaveHorizontalWoodCross:
		DrawVertical(out, center, tile, nwTile, neTile, swTile, colorBright, colorDim);
		DrawCaveHorizontal(out, center, tile, nwTile, swTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveVerticalCross:
	case AutomapTile::Types::CaveVerticalWoodCross:
		DrawHorizontal(out, center, tile, nwTile, neTile, seTile, colorBright, colorDim);
		DrawCaveVertical(out, center, tile, neTile, seTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveHorizontal:
	case AutomapTile::Types::CaveHorizontalWood:
		DrawCaveHorizontal(out, center, tile, nwTile, swTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveVertical:
	case AutomapTile::Types::CaveVerticalWood:
		DrawCaveVertical(out, center, tile, neTile, seTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveCross:
		// Add the missing dirt pixel
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), colorDim);
		[[fallthrough]];
	case AutomapTile::Types::CaveWoodCross:
	case AutomapTile::Types::CaveRightWoodCross:
	case AutomapTile::Types::CaveLeftWoodCross:
		DrawCaveHorizontal(out, center, tile, nwTile, swTile, colorBright, colorDim);
		DrawCaveVertical(out, center, tile, neTile, seTile, colorBright, colorDim);
		break;
	case AutomapTile::Types::CaveLeftCorner:
		DrawCaveLeftCorner(out, center, colorDim);
		break;
	case AutomapTile::Types::CaveRightCorner:
		DrawCaveRightCorner(out, center, colorDim);
		break;
	case AutomapTile::Types::Corner:
		break;
	case AutomapTile::Types::CaveBottomCorner:
		// Add the missing dirt pixel
		// BUGFIX: A tile in poisoned water supply isn't drawing this pixel
		SetMapPixel(out, center + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown), colorDim);
		break;
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
	case AutomapTile::Types::HorizontalLavaThin:
		DrawLavaRiver<Direction::NorthWest, Direction::SouthEast>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::VerticalLavaThin:
		DrawLavaRiver<Direction::NorthEast, Direction::SouthWest>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::BendSouthLavaThin:
		DrawLavaRiver<Direction::SouthWest, Direction::SouthEast>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::BendWestLavaThin:
		DrawLavaRiver<Direction::NorthWest, Direction::SouthWest>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::BendEastLavaThin:
		DrawLavaRiver<Direction::NorthEast, Direction::SouthEast>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::BendNorthLavaThin:
		DrawLavaRiver<Direction::NorthWest, Direction::NorthEast>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::VerticalWallLava:
		DrawVertical(out, center, tile, nwTile, neTile, swTile, colorBright, colorDim);
		DrawLavaRiver<Direction::SouthEast, Direction::NoDirection>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::HorizontalWallLava:
		DrawHorizontal(out, center, tile, nwTile, neTile, swTile, colorBright, colorDim);
		DrawLavaRiver<Direction::SouthWest, Direction::NoDirection>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::SELava:
		DrawLava<Direction::SouthEast>(out, center, lavaColor);
		break;
	case AutomapTile::Types::SWLava:
		DrawLava<Direction::SouthWest>(out, center, lavaColor);
		break;
	case AutomapTile::Types::NELava:
		DrawLava<Direction::NorthEast>(out, center, lavaColor);
		break;
	case AutomapTile::Types::NWLava:
		DrawLava<Direction::NorthWest>(out, center, lavaColor);
		break;
	case AutomapTile::Types::SLava:
		DrawLava<Direction::South>(out, center, lavaColor);
		break;
	case AutomapTile::Types::WLava:
		DrawLava<Direction::West>(out, center, lavaColor);
		break;
	case AutomapTile::Types::ELava:
		DrawLava<Direction::East>(out, center, lavaColor);
		break;
	case AutomapTile::Types::NLava:
		DrawLava<Direction::North>(out, center, lavaColor);
		break;
	case AutomapTile::Types::Lava:
		DrawLava<Direction::NoDirection>(out, center, lavaColor);
		break;
	case AutomapTile::Types::CaveHorizontalWallLava:
		DrawCaveHorizontal(out, center, tile, nwTile, swTile, colorBright, colorDim);
		DrawLavaRiver<Direction::NorthEast, Direction::NoDirection>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::CaveVerticalWallLava:
		DrawCaveVertical(out, center, tile, neTile, seTile, colorBright, colorDim);
		DrawLavaRiver<Direction::NorthWest, Direction::NoDirection>(out, center, lavaColor, false);
		break;
	case AutomapTile::Types::HorizontalBridgeLava:
		DrawLavaRiver<Direction::NorthWest, Direction::SouthEast>(out, center, lavaColor, true);
		break;
	case AutomapTile::Types::VerticalBridgeLava:
		DrawLavaRiver<Direction::NorthEast, Direction::SouthWest>(out, center, lavaColor, true);
		break;
	case AutomapTile::Types::PentagramClosed:
		// Functions are called twice to integrate shadow. Shadows are not drawn inside these functions to avoid shadows being drawn on top of normal pixels.
		DrawMapEllipse(out, center + Displacement { 0, 1 }, AmLine(AmLineLength::OctupleTile), 0); // shadow
		DrawMapStar(out, center + Displacement { 0, 1 }, AmLine(AmLineLength::OctupleTile), 0);    // shadow
		DrawMapEllipse(out, center, AmLine(AmLineLength::OctupleTile), colorDim);
		DrawMapStar(out, center, AmLine(AmLineLength::OctupleTile), colorDim);
		break;
	case AutomapTile::Types::PentagramOpen:
		// Functions are called twice to integrate shadow. Shadows are not drawn inside these functions to avoid shadows being drawn on top of normal pixels.
		DrawMapEllipse(out, center + Displacement { 0, 1 }, AmLine(AmLineLength::OctupleTile), 0); // shadow
		DrawMapStar(out, center + Displacement { 0, 1 }, AmLine(AmLineLength::OctupleTile), 0);    // shadow
		DrawMapEllipse(out, center, AmLine(AmLineLength::OctupleTile), MapColorsPentagramOpen);
		DrawMapStar(out, center, AmLine(AmLineLength::OctupleTile), MapColorsPentagramOpen);
		break;
	}
}

Displacement GetAutomapScreen()
{
	Displacement screen = {};

	if (GetAutomapType() == AutomapType::Minimap) {
		screen = {
			MinimapRect.position.x + MinimapRect.size.width / 2,
			MinimapRect.position.y + MinimapRect.size.height / 2
		};
	} else {
		screen = {
			gnScreenWidth / 2,
			(gnScreenHeight - GetMainPanel().size.height) / 2
		};
	}
	screen += AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown);

	return screen;
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

	const int startX = std::clamp(tile.x - searchRadius, 0, MAXDUNX);
	const int startY = std::clamp(tile.y - searchRadius, 0, MAXDUNY);
	const int endX = std::clamp(tile.x + searchRadius, 0, MAXDUNX);
	const int endY = std::clamp(tile.y + searchRadius, 0, MAXDUNY);

	const AutomapType mapType = GetAutomapType();
	const int scale = (mapType == AutomapType::Minimap) ? MinimapScale : AutoMapScale;

	for (int i = startX; i < endX; i++) {
		for (int j = startY; j < endY; j++) {
			if (!highlightTile({ i, j }))
				continue;

			const int px = i - 2 * AutomapOffset.deltaX - ViewPosition.x;
			const int py = j - 2 * AutomapOffset.deltaY - ViewPosition.y;

			Point screen = {
				(myPlayerOffset.deltaX * scale / 100 / 2) + (px - py) * AmLine(AmLineLength::DoubleTile),
				(myPlayerOffset.deltaY * scale / 100 / 2) + (px + py) * AmLine(AmLineLength::FullTile),
			};

			screen += GetAutomapScreen();

			if (mapType != AutomapType::Minimap && CanPanelsCoverView()) {
				if (IsRightPanelOpen())
					screen.x -= gnScreenWidth / 4;
				if (IsLeftPanelOpen())
					screen.x += gnScreenWidth / 4;
			}

			screen.y -= AmLine(AmLineLength::FullTile);
			DrawDiamond(out, screen, MapColorsItem);
		}
	}
}

/**
 * @brief Renders an arrow on the automap, centered on and facing the direction of the player.
 */
void DrawAutomapPlr(const Surface &out, const Displacement &myPlayerOffset, const Player &player)
{
	const uint8_t playerColor = MapColorsPlayer + (8 * player.getId()) % 128;

	Point tile = player.position.tile;

	int px = tile.x - 2 * AutomapOffset.deltaX - ViewPosition.x;
	int py = tile.y - 2 * AutomapOffset.deltaY - ViewPosition.y;

	Displacement playerOffset = {};
	if (player.isWalking())
		playerOffset = GetOffsetForWalking(player.AnimInfo, player._pdir);

	int scale = (GetAutomapType() == AutomapType::Minimap) ? MinimapScale : AutoMapScale;

	Point base = {
		((playerOffset.deltaX + myPlayerOffset.deltaX) * scale / 100 / 2) + (px - py) * AmLine(AmLineLength::DoubleTile),
		((playerOffset.deltaY + myPlayerOffset.deltaY) * scale / 100 / 2) + (px + py) * AmLine(AmLineLength::FullTile) + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown).deltaY
	};

	base += GetAutomapScreen();

	if (CanPanelsCoverView()) {
		if (IsRightPanelOpen())
			base.x -= gnScreenWidth / 4;
		if (IsLeftPanelOpen())
			base.x += gnScreenWidth / 4;
	}
	base.y -= AmLine(AmLineLength::DoubleTile);

	switch (player._pdir) {
	case Direction::North: {
		const Point point = base + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileUp);
		DrawMapLineNS(out, point, AmLine(AmLineLength::DoubleTile), playerColor);
		DrawMapLineSteepNE(out, point + AmOffset(AmWidthOffset::EighthTileLeft, AmHeightOffset::HalfTileDown), AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineSteepNW(out, point + AmOffset(AmWidthOffset::EighthTileRight, AmHeightOffset::HalfTileDown), AmLine(AmLineLength::HalfTile), playerColor);
	} break;
	case Direction::NorthEast: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileUp);
		DrawMapLineWE(out, point + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::None), AmLine(AmLineLength::FullTile), playerColor);
		DrawMapLineNE(out, point + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown), AmLine(AmLineLength::FullTile), playerColor);
		DrawMapLineSteepSW(out, point, AmLine(AmLineLength::HalfTile), playerColor);
	} break;
	case Direction::East: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None);
		DrawMapLineNW(out, point, AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineWE(out, point + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None), AmLine(AmLineLength::DoubleTile), playerColor);
		DrawMapLineSW(out, point, AmLine(AmLineLength::HalfTile), playerColor);
	} break;
	case Direction::SouthEast: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown);
		DrawMapLineSteepNW(out, point, AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineSE(out, point + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::FullTile), playerColor);
		DrawMapLineWE(out, point + AmOffset(AmWidthOffset::QuarterTileLeft, AmHeightOffset::None) + Displacement { -1, 0 }, AmLine(AmLineLength::FullTile) + 1, playerColor);
	} break;
	case Direction::South: {
		const Point point = base + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown);
		DrawMapLineNS(out, point + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileUp), AmLine(AmLineLength::DoubleTile), playerColor);
		DrawMapLineSteepSW(out, point + AmOffset(AmWidthOffset::EighthTileRight, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineSteepSE(out, point + AmOffset(AmWidthOffset::EighthTileLeft, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::HalfTile), playerColor);
	} break;
	case Direction::SouthWest: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileDown);
		DrawMapLineSteepNE(out, point, AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineSW(out, point + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileUp), AmLine(AmLineLength::FullTile), playerColor);
		DrawMapLineWE(out, point, AmLine(AmLineLength::FullTile) + 1, playerColor);
	} break;
	case Direction::West: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::None);
		DrawMapLineNE(out, point, AmLine(AmLineLength::HalfTile), playerColor);
		DrawMapLineWE(out, point, AmLine(AmLineLength::DoubleTile) + 1, playerColor);
		DrawMapLineSE(out, point, AmLine(AmLineLength::HalfTile), playerColor);
	} break;
	case Direction::NorthWest: {
		const Point point = base + AmOffset(AmWidthOffset::HalfTileLeft, AmHeightOffset::HalfTileUp);
		DrawMapLineNW(out, point + AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::HalfTileDown), AmLine(AmLineLength::FullTile), playerColor);
		DrawMapLineWE(out, point, AmLine(AmLineLength::FullTile) + 1, playerColor);
		DrawMapLineSteepSE(out, point, AmLine(AmLineLength::HalfTile), playerColor);
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
	std::string_view difficulty;
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

#ifdef _DEBUG
	const TextRenderOptions debugTextOptions {
		.flags = UiFlags::ColorOrange,
	};
	linePosition.y += 45;
	if (DebugGodMode) {
		linePosition.y += 15;
		DrawString(out, "God Mode", linePosition, debugTextOptions);
	}
	if (DebugInvisible) {
		linePosition.y += 15;
		DrawString(out, "Invisible", linePosition, debugTextOptions);
	}
	if (DisableLighting) {
		linePosition.y += 15;
		DrawString(out, "Fullbright", linePosition, debugTextOptions);
	}
	if (DebugVision) {
		linePosition.y += 15;
		DrawString(out, "Draw Vision", linePosition, debugTextOptions);
	}
	if (DebugPath) {
		linePosition.y += 15;
		DrawString(out, "Draw Path", linePosition, debugTextOptions);
	}
	if (DebugGrid) {
		linePosition.y += 15;
		DrawString(out, "Draw Grid", linePosition, debugTextOptions);
	}
	if (DebugScrollViewEnabled) {
		linePosition.y += 15;
		DrawString(out, "Scroll View", linePosition, debugTextOptions);
	}
#endif
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
AutomapType CurrentAutomapType = AutomapType::Opaque;
uint8_t AutomapView[DMAXX][DMAXY];
int AutoMapScale;
int MinimapScale;
Displacement AutomapOffset;
Rectangle MinimapRect {};

void InitAutomapOnce()
{
	AutomapActive = false;
	AutoMapScale = 50;

	// Set the dimensions and screen position of the minimap relative to the screen dimensions
	int minimapWidth = gnScreenWidth / 4;
	Size minimapSize { minimapWidth, minimapWidth / 2 };
	int minimapPadding = gnScreenWidth / 128;
	MinimapRect = Rectangle { { gnScreenWidth - minimapPadding - minimapSize.width, minimapPadding }, minimapSize };

	// Set minimap scale
	int height = 480;
	int scale = 25;
	int factor = gnScreenHeight / height;

	if (factor >= 8) {
		MinimapScale = scale * 8;
	} else {
		MinimapScale = scale * factor;
	}
}

void InitAutomap()
{
	size_t tileCount = 0;
	std::unique_ptr<AutomapTile[]> tileTypes = LoadAutomapData(tileCount);

	switch (leveltype) {
	case DTYPE_CATACOMBS:
		tileTypes[41] = { AutomapTile::Types::FenceHorizontal };
		break;
	case DTYPE_TOWN: // Town automap uses a dun file that contains caves tileset
	case DTYPE_CAVES:
	case DTYPE_NEST:
		tileTypes[4] = { AutomapTile::Types::CaveBottomCorner };
		tileTypes[12] = { AutomapTile::Types::CaveRightCorner };
		tileTypes[13] = { AutomapTile::Types::CaveLeftCorner };
		if (IsAnyOf(leveltype, DTYPE_CAVES)) {
			tileTypes[129] = { AutomapTile::Types::CaveHorizontalWoodCross };
			tileTypes[131] = { AutomapTile::Types::CaveHorizontalWoodCross };
			tileTypes[133] = { AutomapTile::Types::CaveHorizontalWood };
			tileTypes[135] = { AutomapTile::Types::CaveHorizontalWood };
			tileTypes[150] = { AutomapTile::Types::CaveHorizontalWood };
			tileTypes[145] = { AutomapTile::Types::CaveHorizontalWood, AutomapTile::Flags::VerticalDoor };
			tileTypes[147] = { AutomapTile::Types::CaveHorizontalWood, AutomapTile::Flags::VerticalDoor };
			tileTypes[130] = { AutomapTile::Types::CaveVerticalWoodCross };
			tileTypes[132] = { AutomapTile::Types::CaveVerticalWoodCross };
			tileTypes[134] = { AutomapTile::Types::CaveVerticalWood };
			tileTypes[136] = { AutomapTile::Types::CaveVerticalWood };
			tileTypes[151] = { AutomapTile::Types::CaveVerticalWood };
			tileTypes[146] = { AutomapTile::Types::CaveVerticalWood, AutomapTile::Flags::HorizontalDoor };
			tileTypes[148] = { AutomapTile::Types::CaveVerticalWood, AutomapTile::Flags::HorizontalDoor };
			tileTypes[137] = { AutomapTile::Types::CaveWoodCross };
			tileTypes[140] = { AutomapTile::Types::CaveWoodCross };
			tileTypes[141] = { AutomapTile::Types::CaveWoodCross };
			tileTypes[142] = { AutomapTile::Types::CaveWoodCross };
			tileTypes[138] = { AutomapTile::Types::CaveRightWoodCross };
			tileTypes[139] = { AutomapTile::Types::CaveLeftWoodCross };
			tileTypes[14] = { AutomapTile::Types::HorizontalLavaThin };
			tileTypes[15] = { AutomapTile::Types::HorizontalLavaThin };
			tileTypes[16] = { AutomapTile::Types::VerticalLavaThin };
			tileTypes[17] = { AutomapTile::Types::VerticalLavaThin };
			tileTypes[18] = { AutomapTile::Types::BendSouthLavaThin };
			tileTypes[19] = { AutomapTile::Types::BendWestLavaThin };
			tileTypes[20] = { AutomapTile::Types::BendEastLavaThin };
			tileTypes[21] = { AutomapTile::Types::BendNorthLavaThin };
			tileTypes[22] = { AutomapTile::Types::VerticalWallLava };
			tileTypes[23] = { AutomapTile::Types::HorizontalWallLava };
			tileTypes[24] = { AutomapTile::Types::SELava };
			tileTypes[25] = { AutomapTile::Types::SWLava };
			tileTypes[26] = { AutomapTile::Types::NELava };
			tileTypes[27] = { AutomapTile::Types::NWLava };
			tileTypes[28] = { AutomapTile::Types::SLava };
			tileTypes[29] = { AutomapTile::Types::WLava };
			tileTypes[30] = { AutomapTile::Types::ELava };
			tileTypes[31] = { AutomapTile::Types::NLava };
			tileTypes[32] = { AutomapTile::Types::Lava };
			tileTypes[33] = { AutomapTile::Types::Lava };
			tileTypes[34] = { AutomapTile::Types::Lava };
			tileTypes[35] = { AutomapTile::Types::Lava };
			tileTypes[36] = { AutomapTile::Types::Lava };
			tileTypes[37] = { AutomapTile::Types::Lava };
			tileTypes[38] = { AutomapTile::Types::Lava };
			tileTypes[39] = { AutomapTile::Types::Lava };
			tileTypes[40] = { AutomapTile::Types::Lava };
			tileTypes[41] = { AutomapTile::Types::CaveHorizontalWallLava };
			tileTypes[42] = { AutomapTile::Types::CaveVerticalWallLava };
			tileTypes[43] = { AutomapTile::Types::HorizontalBridgeLava };
			tileTypes[44] = { AutomapTile::Types::VerticalBridgeLava };
		} else if (IsAnyOf(leveltype, DTYPE_NEST)) {
			tileTypes[102] = { AutomapTile::Types::HorizontalLavaThin };
			tileTypes[103] = { AutomapTile::Types::HorizontalLavaThin };
			tileTypes[108] = { AutomapTile::Types::HorizontalLavaThin };
			tileTypes[104] = { AutomapTile::Types::VerticalLavaThin };
			tileTypes[105] = { AutomapTile::Types::VerticalLavaThin };
			tileTypes[107] = { AutomapTile::Types::VerticalLavaThin };
			tileTypes[112] = { AutomapTile::Types::BendSouthLavaThin };
			tileTypes[113] = { AutomapTile::Types::BendWestLavaThin };
			tileTypes[110] = { AutomapTile::Types::BendEastLavaThin };
			tileTypes[111] = { AutomapTile::Types::BendNorthLavaThin };
			tileTypes[134] = { AutomapTile::Types::VerticalWallLava };
			tileTypes[135] = { AutomapTile::Types::HorizontalWallLava };
			tileTypes[118] = { AutomapTile::Types::SELava };
			tileTypes[119] = { AutomapTile::Types::SWLava };
			tileTypes[120] = { AutomapTile::Types::NELava };
			tileTypes[121] = { AutomapTile::Types::NWLava };
			tileTypes[106] = { AutomapTile::Types::SLava };
			tileTypes[114] = { AutomapTile::Types::WLava };
			tileTypes[130] = { AutomapTile::Types::ELava };
			tileTypes[122] = { AutomapTile::Types::NLava };
			tileTypes[117] = { AutomapTile::Types::Lava };
			tileTypes[124] = { AutomapTile::Types::Lava };
			tileTypes[126] = { AutomapTile::Types::Lava };
			tileTypes[127] = { AutomapTile::Types::Lava };
			tileTypes[128] = { AutomapTile::Types::Lava };
			tileTypes[129] = { AutomapTile::Types::Lava };
			tileTypes[131] = { AutomapTile::Types::Lava };
			tileTypes[132] = { AutomapTile::Types::Lava };
			tileTypes[133] = { AutomapTile::Types::Lava };
			tileTypes[136] = { AutomapTile::Types::CaveHorizontalWallLava };
			tileTypes[137] = { AutomapTile::Types::CaveVerticalWallLava };
			tileTypes[115] = { AutomapTile::Types::HorizontalBridgeLava };
			tileTypes[116] = { AutomapTile::Types::VerticalBridgeLava };
		}
		break;
	case DTYPE_HELL:
		tileTypes[51] = { AutomapTile::Types::VerticalDiamond };
		tileTypes[55] = { AutomapTile::Types::HorizontalDiamond };
		tileTypes[102] = { AutomapTile::Types::PentagramClosed };
		tileTypes[111] = { AutomapTile::Types::PentagramOpen };
		break;
	default:
		break;
	}
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
	int &scale = (GetAutomapType() == AutomapType::Minimap) ? MinimapScale : AutoMapScale;

	if (scale >= 200)
		return;

	scale += 25;
}

void AutomapZoomOut()
{
	int &scale = (GetAutomapType() == AutomapType::Minimap) ? MinimapScale : AutoMapScale;

	if (scale <= 25)
		return;

	scale -= 25;
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

	int scale = (GetAutomapType() == AutomapType::Minimap) ? MinimapScale : AutoMapScale;
	int d = (scale * 64) / 100;
	int cells = 2 * (gnScreenWidth / 2 / d) + 1;
	if (((gnScreenWidth / 2) % d) != 0)
		cells++;
	if (((gnScreenWidth / 2) % d) >= (scale * 32) / 100)
		cells++;
	if ((myPlayerOffset.deltaX + myPlayerOffset.deltaY) != 0)
		cells++;

	if (GetAutomapType() == AutomapType::Minimap) {
		// Background fill
		DrawHalfTransparentRectTo(out, MinimapRect.position.x, MinimapRect.position.y, MinimapRect.size.width, MinimapRect.size.height);

		uint8_t frameShadowColor = PAL16_YELLOW + 12;

		// Shadow
		DrawHorizontalLine(out, MinimapRect.position + Displacement { -1, -1 }, MinimapRect.size.width + 1, frameShadowColor);
		DrawHorizontalLine(out, MinimapRect.position + Displacement { -2, MinimapRect.size.height + 1 }, MinimapRect.size.width + 4, frameShadowColor);
		DrawVerticalLine(out, MinimapRect.position + Displacement { -1, 0 }, MinimapRect.size.height, frameShadowColor);
		DrawVerticalLine(out, MinimapRect.position + Displacement { MinimapRect.size.width + 1, -2 }, MinimapRect.size.height + 3, frameShadowColor);

		// Frame
		DrawHorizontalLine(out, MinimapRect.position + Displacement { -2, -2 }, MinimapRect.size.width + 3, MapColorsDim);
		DrawHorizontalLine(out, MinimapRect.position + Displacement { -2, MinimapRect.size.height }, MinimapRect.size.width + 3, MapColorsDim);
		DrawVerticalLine(out, MinimapRect.position + Displacement { -2, -1 }, MinimapRect.size.height + 1, MapColorsDim);
		DrawVerticalLine(out, MinimapRect.position + Displacement { MinimapRect.size.width, -1 }, MinimapRect.size.height + 1, MapColorsDim);

		if (AutoMapShowItems)
			SearchAutomapItem(out, myPlayerOffset, 8, [](Point position) {
				return dItem[position.x][position.y] != 0;
			});
	}

	Point screen = {};

	screen += GetAutomapScreen();

	if ((cells & 1) != 0) {
		screen.x -= AmOffset(AmWidthOffset::DoubleTileRight, AmHeightOffset::None).deltaX * ((cells - 1) / 2);
		screen.y -= AmOffset(AmWidthOffset::None, AmHeightOffset::DoubleTileDown).deltaY * ((cells + 1) / 2);

	} else {
		screen.x -= AmOffset(AmWidthOffset::DoubleTileRight, AmHeightOffset::None).deltaX * (cells / 2) + AmOffset(AmWidthOffset::FullTileLeft, AmHeightOffset::None).deltaX;
		screen.y -= AmOffset(AmWidthOffset::None, AmHeightOffset::DoubleTileDown).deltaY * (cells / 2) + AmOffset(AmWidthOffset::None, AmHeightOffset::FullTileDown).deltaY;
	}
	if ((ViewPosition.x & 1) != 0) {
		screen.x -= AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None).deltaX;
		screen.y -= AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown).deltaY;
	}
	if ((ViewPosition.y & 1) != 0) {
		screen.x += AmOffset(AmWidthOffset::HalfTileRight, AmHeightOffset::None).deltaX;
		screen.y -= AmOffset(AmWidthOffset::None, AmHeightOffset::HalfTileDown).deltaY;
	}

	screen.x += scale * myPlayerOffset.deltaX / 100 / 2;
	screen.y += scale * myPlayerOffset.deltaY / 100 / 2;

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
			tile1.x += AmOffset(AmWidthOffset::DoubleTileRight, AmHeightOffset::None).deltaX;
		}
		map.y++;

		Point tile2 = screen + AmOffset(AmWidthOffset::FullTileLeft, AmHeightOffset::FullTileDown);
		for (int j = 0; j <= cells; j++) {
			DrawAutomapTile(out, tile2, { map.x + j, map.y - j });
			tile2.x += AmOffset(AmWidthOffset::DoubleTileRight, AmHeightOffset::None).deltaX;
		}
		map.x++;
		screen.y += AmOffset(AmWidthOffset::None, AmHeightOffset::DoubleTileDown).deltaY;
	}

	for (const Player &player : Players) {
		if (player.isOnActiveLevel() && player.plractive && !player._pLvlChanging && (&player == MyPlayer || player.friendlyMode)) {
			DrawAutomapPlr(out, myPlayerOffset, player);
		}
	}

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

	AutomapTile tile = GetAutomapTileType(map);
	bool solid = tile.hasFlag(AutomapTile::Flags::Dirt);

	switch (tile.type) {
	case AutomapTile::Types::Vertical:
		if (solid) {
			auto tileSW = GetAutomapTileType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.hasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
		} else if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
		}
		break;
	case AutomapTile::Types::Horizontal:
		if (solid) {
			auto tileSE = GetAutomapTileType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.hasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x + 1, map.y }, explorer);
		} else if (HasAutomapFlag({ map.x, map.y - 1 }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x, map.y - 1 }, explorer);
		}
		break;
	case AutomapTile::Types::Cross:
		if (solid) {
			auto tileSW = GetAutomapTileType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.hasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
			auto tileSE = GetAutomapTileType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.hasFlag(AutomapTile::Flags::Dirt))
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
			auto tileSW = GetAutomapTileType({ map.x, map.y + 1 });
			if (tileSW.type == AutomapTile::Types::Corner && tileSW.hasFlag(AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x, map.y + 1 }, explorer);
		} else if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt)) {
			UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
		}
		break;
	case AutomapTile::Types::FenceHorizontal:
		if (solid) {
			if (HasAutomapFlag({ map.x - 1, map.y }, AutomapTile::Flags::Dirt))
				UpdateAutomapExplorer({ map.x - 1, map.y }, explorer);
			auto tileSE = GetAutomapTileType({ map.x + 1, map.y });
			if (tileSE.type == AutomapTile::Types::Corner && tileSE.hasFlag(AutomapTile::Flags::Dirt))
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
