/**
 * @file levels/drlg_l4.cpp
 *
 * Implementation of the hell level generation algorithms.
 */
#include "levels/drlg_l4.h"

#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "multi.h"
#include "objdat.h"
#include "player.h"
#include "utils/is_of.hpp"

namespace devilution {

WorldTilePosition DiabloQuad1;
WorldTilePosition DiabloQuad2;
WorldTilePosition DiabloQuad3;
WorldTilePosition DiabloQuad4;

namespace {

bool hallok[20];
WorldTilePosition L4Hold;

/**
 * A lookup table for the 16 possible patterns of a 2x2 area,
 * where each cell either contains a SW wall or it doesn't.
 */
const uint8_t L4ConvTbl[16] = { 30, 6, 1, 6, 2, 6, 6, 6, 9, 6, 1, 6, 2, 6, 3, 6 };

/** Miniset: Stairs up. */
const Miniset L4USTAIRS {
	{ 4, 5 },
	{
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 36, 38, 35, 0 },
	    { 37, 34, 33, 32 },
	    { 0, 0, 31, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stairs up to town. */
const Miniset L4TWARP {
	{ 4, 5 },
	{
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	    { 6, 6, 6, 6 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 134, 136, 133, 0 },
	    { 135, 132, 131, 130 },
	    { 0, 0, 129, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stairs down. */
const Miniset L4DSTAIRS {
	{ 5, 5 },
	{
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	},
	{
	    { 0, 0, 0, 0, 0 },
	    { 0, 0, 45, 41, 0 },
	    { 0, 44, 43, 40, 0 },
	    { 0, 46, 42, 39, 0 },
	    { 0, 0, 0, 0, 0 },
	}
};
/** Miniset: Pentagram. */
const Miniset L4PENTA {
	{ 5, 5 },
	{
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	},
	{
	    { 0, 0, 0, 0, 0 },
	    { 0, 98, 100, 103, 0 },
	    { 0, 99, 102, 105, 0 },
	    { 0, 101, 104, 106, 0 },
	    { 0, 0, 0, 0, 0 },
	}
};
/** Miniset: Pentagram portal. */
const Miniset L4PENTA2 {
	{ 5, 5 },
	{
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	    { 6, 6, 6, 6, 6 },
	},
	{
	    { 0, 0, 0, 0, 0 },
	    { 0, 107, 109, 112, 0 },
	    { 0, 108, 111, 114, 0 },
	    { 0, 110, 113, 115, 0 },
	    { 0, 0, 0, 0, 0 },
	}
};

/** Maps tile IDs to their corresponding undecorated tile ID. */
const uint8_t L4BTYPES[140] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
	10, 11, 12, 13, 14, 15, 16, 17, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
	6, 6, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 1, 2, 1, 2, 1, 1, 2,
	2, 0, 0, 0, 0, 0, 0, 15, 16, 9,
	12, 4, 5, 7, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void ApplyShadowsPatterns()
{
	for (int y = 1; y < DMAXY; y++) {
		for (int x = 1; x < DMAXY; x++) {
			if (IsNoneOf(dungeon[x][y], 3, 4, 8, 15)) {
				continue;
			}
			if (dungeon[x - 1][y] == 6) {
				dungeon[x - 1][y] = 47;
			}
			if (dungeon[x - 1][y - 1] == 6) {
				dungeon[x - 1][y - 1] = 48;
			}
		}
	}
}

void InitSetPiece()
{
	std::unique_ptr<uint16_t[]> setPieceData;

	if (Quests[Q_WARLORD].IsAvailable()) {
		setPieceData = LoadFileInMem<uint16_t>("levels\\l4data\\warlord.dun");
	} else if (currlevel == 15 && UseMultiplayerQuests()) {
		setPieceData = LoadFileInMem<uint16_t>("levels\\l4data\\vile1.dun");
	} else {
		return; // no setpiece needed for this level
	}

	WorldTilePosition setPiecePosition = SetPieceRoom.position;
	PlaceDunTiles(setPieceData.get(), setPiecePosition, 6);
	SetPiece = { setPiecePosition, GetDunSize(setPieceData.get()) };
}

void InitDungeonFlags()
{
	DungeonMask.reset();
	Protected.reset();
	memset(dungeon, 30, sizeof(dungeon));
}

void MapRoom(WorldTileRectangle room)
{
	for (WorldTileCoord y = 0; y < room.size.height && y + room.position.y < DMAXY / 2; y++) {
		for (WorldTileCoord x = 0; x < room.size.width && x + room.position.x < DMAXX / 2; x++) {
			DungeonMask.set(room.position.x + x, room.position.y + y);
		}
	}
}

bool CheckRoom(WorldTileRectangle room)
{
	if (room.position.x <= 0 || room.position.y <= 0) {
		return false;
	}

	for (int y = 0; y < room.size.height; y++) {
		for (int x = 0; x < room.size.width; x++) {
			if (x + room.position.x < 0 || x + room.position.x >= DMAXX / 2 || y + room.position.y < 0 || y + room.position.y >= DMAXY / 2) {
				return false;
			}
			if (DungeonMask.test(room.position.x + x, room.position.y + y)) {
				return false;
			}
		}
	}

	return true;
}

void GenerateRoom(WorldTileRectangle area, bool verticalLayout)
{
	bool rotate = !FlipCoin(4);
	verticalLayout = (!verticalLayout && rotate) || (verticalLayout && !rotate);

	bool placeRoom1;
	WorldTileRectangle room1;

	for (int num = 0; num < 20; num++) {
		const int32_t randomWidth = (GenerateRnd(5) + 2) & ~1;
		const int32_t randomHeight = (GenerateRnd(5) + 2) & ~1;
		room1.size = WorldTileSize(randomWidth, randomHeight);
		room1.position = area.position;
		if (verticalLayout) {
			room1.position += WorldTileDisplacement(-room1.size.width, area.size.height / 2 - room1.size.height / 2);
			placeRoom1 = CheckRoom({ room1.position + WorldTileDisplacement { -1, -1 }, WorldTileSize(room1.size.height + 2, room1.size.width + 1) }); /// BUGFIX: swap height and width ({ room1.size.width + 1, room1.size.height + 2 }) (workaround applied below)
		} else {
			room1.position += WorldTileDisplacement(area.size.width / 2 - room1.size.width / 2, -room1.size.height);
			placeRoom1 = CheckRoom({ room1.position + WorldTileDisplacement { -1, -1 }, WorldTileSize(room1.size.width + 2, room1.size.height + 1) });
		}
		if (placeRoom1)
			break;
	}

	if (placeRoom1)
		MapRoom({ room1.position, WorldTileSize(std::min<int>(DMAXX - room1.position.x, room1.size.width), std::min<int>(DMAXX - room1.position.y, room1.size.height)) });

	bool placeRoom2;
	WorldTileRectangle room2 = room1;
	if (verticalLayout) {
		room2.position.x = area.position.x + area.size.width;
		placeRoom2 = CheckRoom({ room2.position + WorldTileDisplacement { 0, -1 }, WorldTileSize(room2.size.width + 1, room2.size.height + 2) });
	} else {
		room2.position.y = area.position.y + area.size.height;
		placeRoom2 = CheckRoom({ room2.position + WorldTileDisplacement { -1, 0 }, WorldTileSize(room2.size.width + 2, room2.size.height + 1) });
	}

	if (placeRoom2)
		MapRoom(room2);
	if (placeRoom1)
		GenerateRoom(room1, verticalLayout);
	if (placeRoom2)
		GenerateRoom(room2, verticalLayout);
}

void FirstRoom()
{
	WorldTileRectangle room { { 0, 0 }, { 14, 14 } };
	if (currlevel != 16) {
		if (currlevel == Quests[Q_WARLORD]._qlevel && Quests[Q_WARLORD]._qactive != QUEST_NOTAVAIL) {
			room.size = { 11, 11 };
		} else if (currlevel == Quests[Q_BETRAYER]._qlevel && UseMultiplayerQuests()) {
			room.size = { 11, 11 };
		} else {
			const int32_t randomWidth = GenerateRnd(5) + 2;
			const int32_t randomHeight = GenerateRnd(5) + 2;
			room.size = WorldTileSize(randomWidth, randomHeight);
		}
	}

	int xmin = (DMAXX / 2 - room.size.width) / 2;
	int xmax = DMAXX / 2 - 1 - room.size.width;
	int ymin = (DMAXY / 2 - room.size.height) / 2;
	int ymax = DMAXY / 2 - 1 - room.size.height;
	const int32_t randomX = GenerateRnd(xmax - xmin + 1) + xmin;
	const int32_t randomY = GenerateRnd(ymax - ymin + 1) + ymin;
	room.position = WorldTilePosition(randomX, randomY);

	if (currlevel == 16) {
		L4Hold = room.position;
	}
	if (Quests[Q_WARLORD].IsAvailable() || (currlevel == Quests[Q_BETRAYER]._qlevel && UseMultiplayerQuests())) {
		SetPieceRoom = { room.position + WorldTileDisplacement { 1, 1 }, WorldTileSize(room.size.width + 1, room.size.height + 1) };
	} else {
		SetPieceRoom = {};
	}

	MapRoom(room);
	GenerateRoom(room, !FlipCoin());
}

/**
 * @brief Mirrors the first quadrant to the rest of the map
 */
void MirrorDungeonLayout()
{
	for (int y = 0; y < DMAXY / 2; y++) {
		for (int x = 0; x < DMAXX / 2; x++) {
			if (DungeonMask.test(x, y)) {
				DungeonMask.set(x, DMAXY - 1 - y);
				DungeonMask.set(DMAXX - 1 - x, y);
				DungeonMask.set(DMAXX - 1 - x, DMAXY - 1 - y);
			}
		}
	}
}

void MakeDmt()
{
	for (int y = 0; y < DMAXY - 1; y++) {
		for (int x = 0; x < DMAXX - 1; x++) {
			int val = (DungeonMask.test(x + 1, y + 1) << 3) | (DungeonMask.test(x, y + 1) << 2) | (DungeonMask.test(x + 1, y) << 1) | (DungeonMask.test(x, y) << 0);
			dungeon[x][y] = L4ConvTbl[val];
		}
	}
}

int HorizontalWallOk(int i, int j)
{
	int x;
	for (x = 1; dungeon[i + x][j] == 6; x++) {
		if (Protected.test(i + x, j)) {
			break;
		}
		if (dungeon[i + x][j - 1] != 6) {
			break;
		}
		if (dungeon[i + x][j + 1] != 6) {
			break;
		}
	}

	if (IsAnyOf(dungeon[i + x][j], 10, 12, 13, 15, 16, 21, 22) && x > 3)
		return x;

	return -1;
}

int VerticalWallOk(int i, int j)
{
	int y;
	for (y = 1; dungeon[i][j + y] == 6; y++) {
		if (Protected.test(i, j + y)) {
			break;
		}
		if (dungeon[i - 1][j + y] != 6) {
			break;
		}
		if (dungeon[i + 1][j + y] != 6) {
			break;
		}
	}

	if (IsAnyOf(dungeon[i][j + y], 8, 9, 11, 14, 15, 16, 21, 23) && y > 3)
		return y;

	return -1;
}

void HorizontalWall(int i, int j, int dx)
{
	if (dungeon[i][j] == 13) {
		dungeon[i][j] = 17;
	}
	if (dungeon[i][j] == 16) {
		dungeon[i][j] = 11;
	}
	if (dungeon[i][j] == 12) {
		dungeon[i][j] = 14;
	}

	for (int xx = 1; xx < dx; xx++) {
		dungeon[i + xx][j] = 2;
	}

	if (dungeon[i + dx][j] == 15) {
		dungeon[i + dx][j] = 14;
	}
	if (dungeon[i + dx][j] == 10) {
		dungeon[i + dx][j] = 17;
	}
	if (dungeon[i + dx][j] == 21) {
		dungeon[i + dx][j] = 23;
	}
	if (dungeon[i + dx][j] == 22) {
		dungeon[i + dx][j] = 29;
	}

	int xx = GenerateRnd(dx - 3) + 1;
	dungeon[i + xx][j] = 57;
	dungeon[i + xx + 2][j] = 56;
	dungeon[i + xx + 1][j] = 60;

	if (dungeon[i + xx][j - 1] == 6) {
		dungeon[i + xx][j - 1] = 58;
	}
	if (dungeon[i + xx + 1][j - 1] == 6) {
		dungeon[i + xx + 1][j - 1] = 59;
	}
}

void VerticalWall(int i, int j, int dy)
{
	if (dungeon[i][j] == 14) {
		dungeon[i][j] = 17;
	}
	if (dungeon[i][j] == 8) {
		dungeon[i][j] = 9;
	}
	if (dungeon[i][j] == 15) {
		dungeon[i][j] = 10;
	}

	for (int yy = 1; yy < dy; yy++) {
		dungeon[i][j + yy] = 1;
	}

	if (dungeon[i][j + dy] == 11) {
		dungeon[i][j + dy] = 17;
	}
	if (dungeon[i][j + dy] == 9) {
		dungeon[i][j + dy] = 10;
	}
	if (dungeon[i][j + dy] == 16) {
		dungeon[i][j + dy] = 13;
	}
	if (dungeon[i][j + dy] == 21) {
		dungeon[i][j + dy] = 22;
	}
	if (dungeon[i][j + dy] == 23) {
		dungeon[i][j + dy] = 29;
	}

	int yy = GenerateRnd(dy - 3) + 1;
	dungeon[i][j + yy] = 53;
	dungeon[i][j + yy + 2] = 52;
	dungeon[i][j + yy + 1] = 6;

	if (dungeon[i - 1][j + yy] == 6) {
		dungeon[i - 1][j + yy] = 54;
	}
	if (dungeon[i - 1][j + yy - 1] == 6) {
		dungeon[i - 1][j + yy - 1] = 55;
	}
}

void AddWall()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (Protected.test(i, j)) {
				continue;
			}
			for (auto d : { 10, 12, 13, 15, 16, 21, 22 }) {
				if (d == dungeon[i][j]) {
					DiscardRandomValues(1);
					int x = HorizontalWallOk(i, j);
					if (x != -1) {
						HorizontalWall(i, j, x);
					}
				}
			}
			for (auto d : { 8, 9, 11, 14, 15, 16, 21, 23 }) {
				if (d == dungeon[i][j]) {
					DiscardRandomValues(1);
					int y = VerticalWallOk(i, j);
					if (y != -1) {
						VerticalWall(i, j, y);
					}
				}
			}
		}
	}
}

void FixTilesPatterns()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 6)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 13;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 14;
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 6)
				dungeon[i + 1][j] = 2;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 9 && dungeon[i + 1][j] == 6)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 13;
			if (dungeon[i][j] == 6 && dungeon[i + 1][j] == 14)
				dungeon[i + 1][j] = 15;
			if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 13)
				dungeon[i][j + 1] = 16;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 6 && dungeon[i][j - 1] == 1)
				dungeon[i][j - 1] = 1;
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 27;
			if (dungeon[i][j] == 27 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 27;
			if (dungeon[i][j] == 27 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 27)
				dungeon[i + 1][j] = 26;
			if (dungeon[i][j] == 27 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 15)
				dungeon[i + 1][j] = 14;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 15)
				dungeon[i + 1][j] = 14;
			if (dungeon[i][j] == 22 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 27 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 6 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] != 0) /* check */
				dungeon[i + 1][j] = 22;
			if (dungeon[i][j] == 22 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 1 && dungeon[i + 1][j - 1] == 1)
				dungeon[i + 1][j] = 13;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 30 && dungeon[i][j + 1] == 6)
				dungeon[i + 1][j] = 28;
			if (dungeon[i][j] == 16 && dungeon[i + 1][j] == 6 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 27;
			if (dungeon[i][j] == 16 && dungeon[i][j + 1] == 30 && dungeon[i + 1][j + 1] == 30)
				dungeon[i][j + 1] = 27;
			if (dungeon[i][j] == 6 && dungeon[i + 1][j] == 30 && dungeon[i + 1][j - 1] == 6)
				dungeon[i + 1][j] = 21;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] == 9)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 9 && dungeon[i + 1][j] == 15)
				dungeon[i + 1][j] = 14;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] == 2)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 18)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 9 && dungeon[i + 1][j] == 15)
				dungeon[i + 1][j] = 14;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 19 && dungeon[i + 1][j - 1] == 30)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 24 && dungeon[i][j - 1] == 30 && dungeon[i][j - 2] == 6)
				dungeon[i][j - 1] = 21;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 28;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 28;
			if (dungeon[i][j] == 28 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 28 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 19 && dungeon[i + 2][j] == 2 && dungeon[i + 1][j - 1] == 18 && dungeon[i + 1][j + 1] == 1)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 19 && dungeon[i + 2][j] == 2 && dungeon[i + 1][j - 1] == 22 && dungeon[i + 1][j + 1] == 1)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 19 && dungeon[i + 2][j] == 2 && dungeon[i + 1][j - 1] == 18 && dungeon[i + 1][j + 1] == 13)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 21 && dungeon[i + 2][j] == 2 && dungeon[i + 1][j - 1] == 18 && dungeon[i + 1][j + 1] == 1)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j + 1] == 1 && dungeon[i + 1][j - 1] == 22 && dungeon[i + 2][j] == 3)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 28 && dungeon[i + 2][j] == 30 && dungeon[i + 1][j - 1] == 6)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 28 && dungeon[i + 2][j] == 1)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] == 30)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 28 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j - 1] == 21)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] == 30)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 18)
				dungeon[i + 1][j] = 25;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 9 && dungeon[i + 2][j] == 2)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 10)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 15 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 22 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 24 && dungeon[i - 1][j] == 30)
				dungeon[i - 1][j] = 19;
			if (dungeon[i][j] == 21 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 21 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 22 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 21 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 16 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 22 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 18 && dungeon[i + 2][j] == 30)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 9 && dungeon[i + 1][j + 1] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 27 && dungeon[i + 1][j + 1] == 2)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 23 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 23 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 25 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 22 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 23 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 11 && dungeon[i + 1][j] == 15)
				dungeon[i + 1][j] = 14;
			if (dungeon[i][j] == 23 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 27)
				dungeon[i + 1][j] = 26;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 18)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 26 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 29 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 29 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 1 && dungeon[i][j - 1] == 15)
				dungeon[i][j - 1] = 10;
			if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 23 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 30 && dungeon[i + 1][j + 1] == 30)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 28 && dungeon[i + 1][j - 1] == 6)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 23 && dungeon[i + 1][j] == 18 && dungeon[i][j - 1] == 6)
				dungeon[i + 1][j] = 24;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 23 && dungeon[i + 2][j] == 30)
				dungeon[i + 1][j] = 28;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 28 && dungeon[i + 2][j] == 30 && dungeon[i + 1][j - 1] == 6)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 23 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 29 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 29 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 26 && dungeon[i + 1][j] == 30)
				dungeon[i + 1][j] = 19;
			if (dungeon[i][j] == 16 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 25 && dungeon[i][j + 1] == 30)
				dungeon[i][j + 1] = 18;
			if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 2)
				dungeon[i][j + 1] = 15;
			if (dungeon[i][j] == 11 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 13;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 13 && dungeon[i + 1][j - 1] == 6)
				dungeon[i + 1][j] = 16;
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 21 && dungeon[i][j + 1] == 24 && dungeon[i][j + 2] == 1)
				dungeon[i][j + 1] = 17;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j + 1] == 9 && dungeon[i + 1][j - 1] == 1 && dungeon[i + 2][j] == 16)
				dungeon[i + 1][j] = 29;
			if (dungeon[i][j] == 2 && dungeon[i - 1][j] == 6)
				dungeon[i - 1][j] = 8;
			if (dungeon[i][j] == 1 && dungeon[i][j - 1] == 6)
				dungeon[i][j - 1] = 7;
			if (dungeon[i][j] == 6 && dungeon[i + 1][j] == 15 && dungeon[i + 1][j + 1] == 4)
				dungeon[i + 1][j] = 10;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 6)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 9 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 10 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 16)
				dungeon[i][j + 1] = 13;
			if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 13)
				dungeon[i][j + 1] = 16;
			if (dungeon[i][j] == 25 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 28 && dungeon[i][j - 1] == 6 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 10)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 11 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 10 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 27 && dungeon[i + 1][j] == 9)
				dungeon[i + 1][j] = 11;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 1)
				dungeon[i + 1][j] = 16;
			if (dungeon[i][j] == 11 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 9 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 14 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 3)
				dungeon[i + 1][j] = 5;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 5 && dungeon[i + 1][j - 1] == 16)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 9 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 1 && dungeon[i][j - 1] == 8)
				dungeon[i][j - 1] = 9;
			if (dungeon[i][j] == 28 && dungeon[i + 1][j] == 23 && dungeon[i + 1][j + 1] == 3)
				dungeon[i + 1][j] = 16;
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 10)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 17 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 10 && dungeon[i + 1][j] == 4)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 17 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 29 && dungeon[i][j + 1] == 9)
				dungeon[i][j + 1] = 10;
			if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 9 && dungeon[i][j + 1] == 16)
				dungeon[i][j + 1] = 13;
			if (dungeon[i][j] == 10 && dungeon[i][j + 1] == 16)
				dungeon[i][j + 1] = 13;
			if (dungeon[i][j] == 16 && dungeon[i][j + 1] == 3)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 11 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 10 && dungeon[i + 1][j] == 3 && dungeon[i + 1][j - 1] == 16)
				dungeon[i + 1][j] = 12;
			if (dungeon[i][j] == 16 && dungeon[i][j + 1] == 5)
				dungeon[i][j + 1] = 12;
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 6)
				dungeon[i][j + 1] = 4;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 13 && dungeon[i][j + 1] == 10)
				dungeon[i + 1][j + 1] = 12;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 10)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 22 && dungeon[i][j + 1] == 11)
				dungeon[i][j + 1] = 17;
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 28 && dungeon[i + 2][j] == 16)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 28 && dungeon[i + 1][j] == 23 && dungeon[i + 1][j + 1] == 1 && dungeon[i + 2][j] == 6)
				dungeon[i + 1][j] = 16;
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 15 && dungeon[i + 1][j] == 28 && dungeon[i + 2][j] == 16)
				dungeon[i + 1][j] = 23;
			if (dungeon[i][j] == 21 && dungeon[i + 1][j - 1] == 21 && dungeon[i + 1][j + 1] == 13 && dungeon[i + 2][j] == 2)
				dungeon[i + 1][j] = 17;
			if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 15 && dungeon[i + 1][j + 1] == 12)
				dungeon[i + 1][j] = 17;
		}
	}
}

void Substitution()
{
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			if (FlipCoin(3)) {
				uint8_t c = L4BTYPES[dungeon[x][y]];
				if (c != 0 && !Protected.test(x, y)) {
					int rv = GenerateRnd(16);
					int i = -1;
					while (rv >= 0) {
						i++;
						if (i == sizeof(L4BTYPES)) {
							i = 0;
						}
						if (c == L4BTYPES[i]) {
							rv--;
						}
					}

					dungeon[x][y] = i;
				}
			}
		}
	}
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			if (FlipCoin(10)) {
				uint8_t c = dungeon[x][y];
				if (L4BTYPES[c] == 6 && !Protected.test(x, y)) {
					dungeon[x][y] = PickRandomlyAmong({ 95, 96, 97 });
				}
			}
		}
	}
}

/**
 * @brief Sets up the inside borders of the first quadrant so there are valid paths after mirroring the layout
 */
void PrepareInnerBorders()
{
	for (int y = DMAXY / 2 - 1; y >= 0; y--) {
		for (int x = DMAXX / 2 - 1; x >= 0; x--) {
			if (!DungeonMask.test(x, y)) {
				hallok[y] = false;
			} else {
				bool hasSouthWestRoom = y + 1 < DMAXY / 2 && DungeonMask.test(x, y + 1);
				bool hasSouthRoom = x + 1 < DMAXX / 2 && y + 1 < DMAXY / 2 && DungeonMask.test(x + 1, y + 1);
				hallok[y] = hasSouthWestRoom && !hasSouthRoom;
				x = 0;
			}
		}
	}

	int ry = GenerateRnd(DMAXY / 2 - 1) + 1;
	do {
		if (hallok[ry]) {
			for (int x = DMAXX / 2 - 1; x >= 0; x--) {
				if (DungeonMask.test(x, ry)) {
					x = -1;
					ry = 0;
				} else {
					DungeonMask.set(x, ry);
					DungeonMask.set(x, ry + 1);
				}
			}
		} else {
			ry++;
			if (ry == DMAXY / 2) {
				ry = 1;
			}
		}
	} while (ry != 0);

	for (int x = DMAXX / 2 - 1; x >= 0; x--) {
		for (int y = DMAXY / 2 - 1; y >= 0; y--) {
			if (!DungeonMask.test(x, y)) {
				hallok[x] = false;
			} else {
				bool hasSouthEastRoom = x + 1 < DMAXX / 2 && DungeonMask.test(x + 1, y);
				bool hasSouthRoom = x + 1 < DMAXX / 2 && y + 1 < DMAXY / 2 && DungeonMask.test(x + 1, y + 1);
				hallok[x] = hasSouthEastRoom && !hasSouthRoom;
				y = 0;
			}
		}
	}

	int rx = GenerateRnd(DMAXX / 2 - 1) + 1;
	do {
		if (hallok[rx]) {
			for (int y = DMAXY / 2 - 1; y >= 0; y--) {
				if (DungeonMask.test(rx, y)) {
					y = -1;
					rx = 0;
				} else {
					DungeonMask.set(rx, y);
					DungeonMask.set(rx + 1, y);
				}
			}
		} else {
			rx++;
			if (rx == DMAXX / 2) {
				rx = 1;
			}
		}
	} while (rx != 0);
}

/**
 * @brief Find the number of mega tiles used by layout
 */
inline size_t FindArea()
{
	// Hell layouts are mirrored based on a single quadrant, this function is called after the quadrant has been
	// generated but before mirroring the layout. We need to multiply by 4 to get the expected number of tiles
	return DungeonMask.count() * 4;
}

void ProtectQuads()
{
	for (int y = 0; y < 14; y++) {
		for (int x = 0; x < 14; x++) {
			Protected.set(L4Hold.x + x, L4Hold.y + y);
			Protected.set(DMAXX - 1 - x - L4Hold.x, L4Hold.y + y);
			Protected.set(L4Hold.x + x, DMAXY - 1 - y - L4Hold.y);
			Protected.set(DMAXX - 1 - x - L4Hold.x, DMAXY - 1 - y - L4Hold.y);
		}
	}
}

void LoadDiabQuads(bool preflag)
{
	{
		auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\diab1.dun");
		DiabloQuad1 = L4Hold + WorldTileDisplacement { 4, 4 };
		PlaceDunTiles(dunData.get(), DiabloQuad1, 6);
	}
	{
		auto dunData = LoadFileInMem<uint16_t>(preflag ? "levels\\l4data\\diab2b.dun" : "levels\\l4data\\diab2a.dun");
		DiabloQuad2 = WorldTilePosition(27 - L4Hold.x, 1 + L4Hold.y);
		PlaceDunTiles(dunData.get(), DiabloQuad2, 6);
	}
	{
		auto dunData = LoadFileInMem<uint16_t>(preflag ? "levels\\l4data\\diab3b.dun" : "levels\\l4data\\diab3a.dun");
		DiabloQuad3 = WorldTilePosition(1 + L4Hold.x, 27 - L4Hold.y);
		PlaceDunTiles(dunData.get(), DiabloQuad3, 6);
	}
	{
		auto dunData = LoadFileInMem<uint16_t>(preflag ? "levels\\l4data\\diab4b.dun" : "levels\\l4data\\diab4a.dun");
		DiabloQuad4 = WorldTilePosition(28 - L4Hold.x, 28 - L4Hold.y);
		PlaceDunTiles(dunData.get(), DiabloQuad4, 6);
	}
}

bool IsDURightWall(char d)
{
	if (d == 25) {
		return true;
	}
	if (d == 28) {
		return true;
	}
	if (d == 23) {
		return true;
	}

	return false;
}

bool IsDLLeftWall(char dd)
{
	if (dd == 27) {
		return true;
	}
	if (dd == 26) {
		return true;
	}
	if (dd == 22) {
		return true;
	}

	return false;
}

void FixTransparency()
{
	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) {
			// BUGFIX: Should check for `j > 0` first.
			if (IsDURightWall(dungeon[i][j]) && dungeon[i][j - 1] == 18) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			// BUGFIX: Should check for `i + 1 < DMAXY` first.
			if (IsDLLeftWall(dungeon[i][j]) && dungeon[i + 1][j] == 19) {
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == 18) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == 19) {
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == 24) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == 57) {
				dTransVal[xx - 1][yy] = dTransVal[xx][yy + 1];
				dTransVal[xx][yy] = dTransVal[xx][yy + 1];
			}
			if (dungeon[i][j] == 53) {
				dTransVal[xx][yy - 1] = dTransVal[xx + 1][yy];
				dTransVal[xx][yy] = dTransVal[xx + 1][yy];
			}
			xx += 2;
		}
		yy += 2;
	}
}

void FixCornerTiles()
{
	for (int j = 1; j < DMAXY - 1; j++) {
		for (int i = 1; i < DMAXX - 1; i++) {
			if (dungeon[i][j] >= 18 && dungeon[i][j] <= 30) {
				if (dungeon[i + 1][j] < 18 || dungeon[i][j + 1] < 18) {
					dungeon[i][j] += 98;
				}
			}
		}
	}
}

/**
 * @brief Marks the edge of the map as solid/not part of the dungeon layout
 */
void CloseOuterBorders()
{
	for (int x = 0; x < DMAXX / 2; x++) { // NOLINT(modernize-loop-convert)
		DungeonMask.reset(x, 0);
	}
	for (int y = 0; y < DMAXY / 2; y++) {
		DungeonMask.reset(0, y);
	}
}

void GeneralFix()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			if ((dungeon[i][j] == 24 || dungeon[i][j] == 122) && dungeon[i + 1][j] == 2 && dungeon[i][j + 1] == 5) {
				dungeon[i][j] = 17;
			}
		}
	}
}

bool PlaceStairs(lvl_entry entry)
{
	std::optional<Point> position;

	// Place stairs up
	position = PlaceMiniSet(L4USTAIRS);
	if (!position)
		return false;
	if (entry == ENTRY_MAIN)
		ViewPosition = position->megaToWorld() + Displacement { 6, 6 };

	if (currlevel != 15) {
		// Place stairs down
		if (currlevel != 16) {
			if (Quests[Q_WARLORD].IsAvailable()) {
				if (entry == ENTRY_PREV)
					ViewPosition = SetPiece.position.megaToWorld() + Displacement { 7, 7 };
			} else {
				position = PlaceMiniSet(L4DSTAIRS);
				if (!position)
					return false;
				if (entry == ENTRY_PREV)
					ViewPosition = position->megaToWorld() + Displacement { 7, 5 };
			}
		}

		// Place town warp stairs
		if (currlevel == 13) {
			position = PlaceMiniSet(L4TWARP);
			if (!position)
				return false;
			if (entry == ENTRY_TWARPDN)
				ViewPosition = position->megaToWorld() + Displacement { 6, 6 };
		}
	} else {
		// Place hell gate
		position = PlaceMiniSet(L4PENTA2);
		if (!position)
			return false;
		Quests[Q_DIABLO].position = *position;
		if (entry == ENTRY_PREV)
			ViewPosition = position->megaToWorld() + Displacement { 6, 5 };
	}

	return true;
}

void GenerateLevel(lvl_entry entry)
{
	if (LevelSeeds[currlevel])
		SetRndSeed(*LevelSeeds[currlevel]);

	while (true) {
		DRLG_InitTrans();

		constexpr size_t Minarea = 692;
		do {
			LevelSeeds[currlevel] = GetLCGEngineState();
			InitDungeonFlags();
			FirstRoom();
			CloseOuterBorders();
		} while (FindArea() < Minarea);

		PrepareInnerBorders();
		MirrorDungeonLayout();

		MakeDmt();
		FixTilesPatterns();
		if (currlevel == 16) {
			ProtectQuads();
		}
		if (Quests[Q_WARLORD].IsAvailable() || (currlevel == Quests[Q_BETRAYER]._qlevel && UseMultiplayerQuests())) {
			for (int spi = SetPieceRoom.position.x; spi < SetPieceRoom.position.x + SetPieceRoom.size.width - 1; spi++) {
				for (int spj = SetPieceRoom.position.y; spj < SetPieceRoom.position.y + SetPieceRoom.size.height - 1; spj++) {
					Protected.set(spi, spj);
				}
			}
		}
		AddWall();
		FloodTransparencyValues(6);
		FixTransparency();
		InitSetPiece();
		if (currlevel == 16) {
			LoadDiabQuads(true);
		}
		if (PlaceStairs(entry))
			break;
	}

	GeneralFix();

	if (currlevel != 16) {
		DRLG_PlaceThemeRooms(7, 10, 6, 8, true);
	}

	ApplyShadowsPatterns();
	FixCornerTiles();
	Substitution();

	memcpy(pdungeon, dungeon, sizeof(pdungeon));

	DRLG_CheckQuests(SetPieceRoom.position);

	if (currlevel == 15) {
		bool isGateOpen = UseMultiplayerQuests() || IsAnyOf(Quests[Q_DIABLO]._qactive, QUEST_ACTIVE, QUEST_DONE);
		if (!isGateOpen)
			L4PENTA.place(Quests[Q_DIABLO].position);

		for (WorldTileCoord j = 1; j < DMAXY; j++) {
			for (WorldTileCoord i = 1; i < DMAXX; i++) {
				if (IsAnyOf(dungeon[i][j], 98, 107)) {
					Make_SetPC({ WorldTilePosition(i - 1, j - 1), { 5, 5 } });
					// Set the portal position to the location of the northmost pentagram tile.
					Quests[Q_BETRAYER].position = Point(i, j).megaToWorld();
				}
			}
		}
	}
	if (currlevel == 16) {
		LoadDiabQuads(false);
	}
}

void Pass3()
{
	DRLG_LPass3(30 - 1);
}

} // namespace

void CreateL4Dungeon(uint32_t rseed, lvl_entry entry)
{
	SetRndSeed(rseed);

	GenerateLevel(entry);

	Pass3();
}

void LoadPreL4Dungeon(const char *path)
{
	memset(dungeon, 30, sizeof(dungeon));

	auto dunData = LoadFileInMem<uint16_t>(path);
	PlaceDunTiles(dunData.get(), { 0, 0 }, 6);

	memcpy(pdungeon, dungeon, sizeof(pdungeon));
}

void LoadL4Dungeon(const char *path, Point spawn)
{
	LoadDungeonBase(path, spawn, 6, 30);

	Pass3();
}

} // namespace devilution
