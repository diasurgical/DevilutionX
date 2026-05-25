/**
 * @file dun_tile_data.hpp
 *
 * Declarations for dungeon tile data globals and loading functions.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/world_tile.hpp"
#include "levels/dun_tile.hpp"
#include "levels/gendung_defs.hpp"
#include "utils/attributes.h"
#include "utils/bitset2d.hpp"
#include "utils/enum_traits.h"

namespace devilution {

#define MAXTHEMES 50
#define MAXTILES 1379

extern DVL_API_FOR_TEST uint8_t currlevel;
extern bool setlevel;

enum class DungeonFlag : uint8_t {
	// clang-format off
	None                  = 0, // Only used by lighting/automap
	Missile               = 1 << 0,
	Visible               = 1 << 1,
	DeadPlayer            = 1 << 2,
	Populated             = 1 << 3,
	MissileFireWall       = 1 << 4,
	MissileLightningWall  = 1 << 5,
	Lit                   = 1 << 6,
	Explored              = 1 << 7,
	SavedFlags            = (Populated | Lit | Explored), // ~(Missile | Visible | DeadPlayer)
	LoadedFlags           = (Missile | Visible | DeadPlayer | Populated | Lit | Explored)
	// clang-format on
};
use_enum_as_flags(DungeonFlag);

struct THEME_LOC {
	RectangleOf<uint8_t> room;
	int8_t ttval;
};

struct MegaTile {
	uint16_t micro1;
	uint16_t micro2;
	uint16_t micro3;
	uint16_t micro4;
};

/** Reprecents what tiles are being utilized in the generated map. */
extern Bitset2d<DMAXX, DMAXY> DungeonMask;
/** Contains the tile IDs of the map. */
extern DVL_API_FOR_TEST uint8_t dungeon[DMAXX][DMAXY];
/** Contains a backup of the tile IDs of the map. */
extern uint8_t pdungeon[DMAXX][DMAXY];
/** Tile that may not be overwritten by the level generator */
extern Bitset2d<DMAXX, DMAXY> Protected;
extern WorldTileRectangle SetPieceRoom;
/** Specifies the active set quest piece in coordinate. */
extern WorldTileRectangle SetPiece;
extern OptionalOwnedClxSpriteList pSpecialCels;
/** Specifies the tile definitions of the active dungeon type; (e.g. levels/l1data/l1.til). */
extern DVL_API_FOR_TEST std::unique_ptr<MegaTile[]> pMegaTiles;
extern DVL_API_FOR_TEST std::unique_ptr<std::byte[]> pDungeonCels;
/**
 * List tile properties
 */
extern DVL_API_FOR_TEST TileProperties SOLData[MAXTILES];
/** Specifies the minimum X,Y-coordinates of the map. */
extern WorldTilePosition dminPosition;
/** Specifies the maximum X,Y-coordinates of the map. */
extern WorldTilePosition dmaxPosition;
/** Specifies the active dungeon type of the current game. */
extern DVL_API_FOR_TEST dungeon_type leveltype;
/** Specifies the active quest level of the current game. */
extern _setlevels setlvlnum;
/** Specifies the dungeon type of the active quest level of the current game. */
extern dungeon_type setlvltype;
/** Specifies the player viewpoint X,Y-coordinates of the map. */
extern DVL_API_FOR_TEST Point ViewPosition;
extern uint_fast8_t MicroTileLen;
extern int8_t TransVal;
/** Specifies the active transparency indices. */
extern std::array<bool, 256> TransList;
/** Contains the piece IDs of each tile on the map. */
extern DVL_API_FOR_TEST uint16_t dPiece[MAXDUNX][MAXDUNY];
/** Map of micros that comprises a full tile for any given dungeon piece. */
extern DVL_API_FOR_TEST MICROS DPieceMicros[MAXTILES];
/** Specifies the transparency at each coordinate of the map. */
extern DVL_API_FOR_TEST int8_t dTransVal[MAXDUNX][MAXDUNY];
/** Current realtime lighting. Per tile. */
extern DVL_API_FOR_TEST uint8_t dLight[MAXDUNX][MAXDUNY];
/** Precalculated static lights. dLight uses this as a base before applying lights. Per tile. */
extern uint8_t dPreLight[MAXDUNX][MAXDUNY];
/** Holds various information about dungeon tiles, @see DungeonFlag */
extern DungeonFlag dFlags[MAXDUNX][MAXDUNY];
/** Contains the player numbers (players array indices) of the map. negative id indicates player moving. */
extern int8_t dPlayer[MAXDUNX][MAXDUNY];
/**
 * Contains the NPC numbers of the map. The NPC number represents a
 * towner number (towners array index) in Tristram and a monster number
 * (monsters array index) in the dungeon.
 * Negative id indicates monsters moving.
 */
extern int16_t dMonster[MAXDUNX][MAXDUNY];
/**
 * Contains the dead numbers (deads array indices) and dead direction of
 * the map, encoded as specified by the pseudo-code below.
 * dDead[x][y] & 0x1F - index of dead
 * dDead[x][y] >> 0x5 - direction
 */
extern DVL_API_FOR_TEST int8_t dCorpse[MAXDUNX][MAXDUNY];
/**
 * Contains the object numbers (objects array indices) of the map.
 * Large objects have negative id for their extended area.
 */
extern DVL_API_FOR_TEST int8_t dObject[MAXDUNX][MAXDUNY];
/**
 * Contains the arch frame numbers of the map from the special tileset
 * (e.g. "levels/l1data/l1s"). Note, the special tileset of Tristram (i.e.
 * "levels/towndata/towns") contains trees rather than arches.
 */
extern int8_t dSpecial[MAXDUNX][MAXDUNY];
extern int themeCount;
extern THEME_LOC themeLoc[MAXTHEMES];

extern uint32_t DungeonSeeds[NUMLEVELS];
extern DVL_API_FOR_TEST std::optional<uint32_t> LevelSeeds[NUMLEVELS];

DVL_ALWAYS_INLINE constexpr bool InDungeonBounds(Point position)
{
	return position.x >= 0 && position.x < MAXDUNX && position.y >= 0 && position.y < MAXDUNY;
}

/**
 * @brief Checks if a given tile contains at least one missile
 * @param position Coordinates of the dungeon tile to check
 * @return true if a missile exists at this position
 */
constexpr bool TileContainsMissile(Point position)
{
	return InDungeonBounds(position) && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Missile);
}

/**
 * @brief Checks if a given tile contains a player corpse
 * @param position Coordinates of the dungeon tile to check
 * @return true if a dead player exists at this position
 */
constexpr bool TileContainsDeadPlayer(Point position)
{
	return InDungeonBounds(position) && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::DeadPlayer);
}

/**
 * @brief Check if a given tile contains a decorative object (or similar non-pathable set piece)
 *
 * This appears to include stairs so that monsters do not spawn or path onto them, but players can path to them to navigate between layers
 *
 * @param position Coordinates of the dungeon tile to check
 * @return true if a set piece was spawned at this position
 */
constexpr bool TileContainsSetPiece(Point position)
{
	return InDungeonBounds(position) && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Populated);
}

/**
 * @brief Checks if any player can currently see this tile
 *
 * Currently only used by monster AI routines so basic monsters out of sight can be ignored until they're likely to interact with the player
 *
 * @param position Coordinates of the dungeon tile to check
 * @return true if the tile is within at least one players vision
 */
constexpr bool IsTileVisible(Point position)
{
	return InDungeonBounds(position) && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Visible);
}

/**
 * @brief Checks if a light source is illuminating this tile
 * @param position Coordinates of the dungeon tile to check
 * @return true if the tile is within the radius of at least one light source
 */
constexpr bool IsTileLit(Point position)
{
	return InDungeonBounds(position) && HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Lit);
}

[[nodiscard]] DVL_ALWAYS_INLINE bool TileHasAny(Point coords, TileProperties property)
{
	return HasAnyOf(SOLData[dPiece[coords.x][coords.y]], property);
}

tl::expected<void, std::string> LoadLevelSOLData();
void SetDungeonMicros(std::unique_ptr<std::byte[]> &dungeonCels, uint_fast8_t &microTileLen);

} // namespace devilution
