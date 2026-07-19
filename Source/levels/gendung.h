/**
 * @file gendung.h
 *
 * Interface of general dungeon generation code.
 */
#pragma once

#include <cstdint>
#include <optional>

#include "engine/point.hpp"
#include "engine/world_tile.hpp"
#include "levels/dun_tile_data.hpp"
#include "levels/gendung_defs.hpp"

namespace devilution {

struct ShadowStruct {
	uint8_t strig;
	uint8_t s1;
	uint8_t s2;
	uint8_t s3;
	uint8_t nv1;
	uint8_t nv2;
	uint8_t nv3;
};

#ifdef BUILD_TESTING
std::optional<WorldTileSize> GetSizeForThemeRoom();
#endif

dungeon_type GetLevelType(int level);
void CreateDungeon(uint32_t rseed, lvl_entry entry);

struct Miniset {
	WorldTileSize size;
	/* these are indexed as [y][x] */
	uint8_t search[6][6];
	uint8_t replace[6][6];

	/**
	 * @param position Coordinates of the dungeon tile to check
	 * @param respectProtected Match bug from Crypt levels if false
	 */
	bool matches(WorldTilePosition position, bool respectProtected = true) const
	{
		for (WorldTileCoord yy = 0; yy < size.height; yy++) {
			for (WorldTileCoord xx = 0; xx < size.width; xx++) {
				if (search[yy][xx] != 0 && dungeon[xx + position.x][yy + position.y] != search[yy][xx])
					return false;
				if (respectProtected && Protected.test(xx + position.x, yy + position.y))
					return false;
			}
		}
		return true;
	}

	void place(WorldTilePosition position, bool protect = false) const
	{
		for (WorldTileCoord y = 0; y < size.height; y++) {
			for (WorldTileCoord x = 0; x < size.width; x++) {
				if (replace[y][x] == 0)
					continue;
				dungeon[x + position.x][y + position.y] = replace[y][x];
				if (protect)
					Protected.set(x + position.x, y + position.y);
			}
		}
	}
};

void DRLG_InitTrans();
void DRLG_MRectTrans(WorldTilePosition origin, WorldTilePosition extent);
void DRLG_MRectTrans(WorldTileRectangle area);
void DRLG_RectTrans(WorldTileRectangle area);
void DRLG_CopyTrans(int sx, int sy, int dx, int dy);
void LoadTransparency(const uint16_t *dunData);
void LoadDungeonBase(const char *path, Point spawn, int floorId, int dirtId);
void Make_SetPC(WorldTileRectangle area);
/**
 * @param miniset The miniset to place
 * @param tries Tiles to try, 1600 will scan the full map
 * @param drlg1Quirk Match buggy behaviour of Diablo's Cathedral
 */
std::optional<Point> PlaceMiniSet(const Miniset &miniset, int tries = 199, bool drlg1Quirk = false);
void PlaceDunTiles(const uint16_t *dunData, Point position, int floorId = 0);
void DRLG_PlaceThemeRooms(int minSize, int maxSize, int floor, int freq, bool rndSize);
void DRLG_HoldThemeRooms();
/**
 * @brief Returns the size in tiles of the specified ".dun" Data
 */
WorldTileSize GetDunSize(const uint16_t *dunData);
void DRLG_LPass3(int lv);

/**
 * @brief Checks if a theme room is located near the target point
 * @param position Target location in dungeon coordinates
 * @return True if a theme room is near (within 2 tiles of) this point, false if it is free.
 */
bool IsNearThemeRoom(WorldTilePosition position);
void InitLevels();
void FloodTransparencyValues(uint8_t floorID);

} // namespace devilution
