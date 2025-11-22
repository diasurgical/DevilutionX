#include "levels/drlg_l1.h"

#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "engine/rectangle.hpp"
#include "levels/crypt.h"
#include "levels/gendung.h"
#include "player.h"
#include "quests.h"
#include "utils/bitset2d.hpp"
#include "utils/is_of.hpp"

namespace devilution {

namespace {

/** Marks where walls may not be added to the level */
Bitset2d<DMAXX, DMAXY> Chamber;
/** Specifies whether to generate a horizontal or vertical layout. */
bool VerticalLayout;
/** Specifies whether to generate a room at position 1 in the Cathedral. */
bool HasChamber1;
/** Specifies whether to generate a room at position 2 in the Cathedral. */
bool HasChamber2;
/** Specifies whether to generate a room at position 3 in the Cathedral. */
bool HasChamber3;

/** Miniset: stairs up on a corner wall. */
const Miniset STAIRSUP {
	{ 4, 4 },
	{
	    { 13, 13, 13, 13 },
	    { 2, 2, 2, 2 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 0, 66, 6, 0 },
	    { 63, 64, 65, 0 },
	    { 0, 67, 68, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: stairs down. */
const Miniset STAIRSDOWN {
	{ 4, 3 },
	{
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 62, 57, 58, 0 },
	    { 61, 59, 60, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: candlestick. */
const Miniset LAMPS {
	{ 2, 2 },
	{
	    { 13, 0 },
	    { 13, 13 },
	},
	{
	    { 129, 0 },
	    { 130, 128 },
	}
};
/** Miniset: Poisoned Water Supply entrance. */
const Miniset PWATERIN {
	{ 6, 6 },
	{
	    { 13, 13, 13, 13, 13, 13 },
	    { 13, 13, 13, 13, 13, 13 },
	    { 13, 13, 13, 13, 13, 13 },
	    { 13, 13, 13, 13, 13, 13 },
	    { 13, 13, 13, 13, 13, 13 },
	    { 13, 13, 13, 13, 13, 13 },
	},
	{
	    { 0, 0, 0, 0, 0, 0 },
	    { 0, 202, 200, 200, 84, 0 },
	    { 0, 199, 203, 203, 83, 0 },
	    { 0, 85, 206, 80, 81, 0 },
	    { 0, 0, 134, 135, 0, 0 },
	    { 0, 0, 0, 0, 0, 0 },
	}
};

enum Tile : uint8_t {
	// clang-format off
	VWall          =   1,
	HWall          =   2,
	Corner         =   3,
	DWall          =   4,
	DArch          =   5,
	VWallEnd       =   6,
	HWallEnd       =   7,
	HArchEnd       =   8,
	VArchEnd       =   9,
	HArchVWall     =  10,
	VArch          =  11,
	HArch          =  12,
	Floor          =  13,
	HWallVArch     =  14,
	Pillar         =  15,
	VCorner        =  16,
	HCorner        =  17,
	DirtHwall      =  18,
	DirtVwall      =  19,
	VDirtCorner    =  20,
	HDirtCorner    =  21,
	Dirt           =  22,
	DirtHwallEnd   =  23,
	DirtVwallEnd   =  24,
	VDoor          =  25,
	HDoor          =  26,
	HFenceVWall    =  27,
	HDoorVDoor     =  28,
	DFence         =  29,
	VDoorEnd       =  30,
	HDoorEnd       =  31,
	VFenceEnd      =  32,
	VArchEnd2      =  33,
	HArchVWall2    =  34,
	VFence         =  35,
	HFence         =  36,
	HWallVFence    =  37,
	HArchVFence    =  38,
	HArchVDoor     =  39,
	HArchVWall3    =  40,
	DWall2         =  41,
	HWallVArch2    =  42,
	DWall3         =  43,
	EntranceStairs =  64,
	VWall2         =  79,
	HWall2         =  80,
	DWall4         =  82,
	VWallEnd2      =  84,
	VWall4         =  89,
	VWall5         =  90,
	HWall4         =  91,
	HWall5         =  92,
	VWall8         = 100,
	Floor12        = 139,
	Floor13        = 140,
	Floor14        = 141,
	Floor15        = 142,
	Floor16        = 143,
	Floor17        = 144,
	Floor18        = 145,
	VWall17        = 146,
	VArch5         = 147,
	HWallShadow    = 148,
	HArchShadow    = 149,
	Floor19        = 150,
	Floor20        = 151,
	Floor21        = 152,
	HArchShadow2   = 153,
	HWallShadow2   = 154,
	Floor22        = 162,
	Floor23        = 163,
	DirtHWall2     = 199,
	DirtVWall2     = 200,
	DirtCorner2    = 202,
	DirtHWallEnd2  = 204,
	DirtVWallEnd2  = 205,
	// clang-format on
};

/** Contains shadows for 2x2 blocks of base tile IDs in the Cathedral. */
const ShadowStruct ShadowPatterns[37] = {
	// clang-format off
	// strig,     s1,    s2,    s3,    nv1,         nv2,     nv3
	{ HWallEnd,   Floor, 0,     Floor, Floor17,     0,       Floor15     },
	{ VCorner,    Floor, 0,     Floor, Floor17,     0,       Floor15     },
	{ Pillar,     Floor, 0,     Floor, Floor18,     0,       Floor15     },
	{ DArch,      Floor, Floor, Floor, Floor21,     Floor13, Floor12     },
	{ DArch,      Floor, VWall, Floor, Floor16,     VWall17, Floor12     },
	{ DArch,      Floor, Floor, HWall, Floor16,     Floor13, HWallShadow },
	{ DArch,      0,     VWall, HWall, 0,           VWall17, HWallShadow },
	{ DArch,      Floor, VArch, Floor, Floor16,     VArch5,  Floor12     },
	{ DArch,      Floor, Floor, HArch, Floor16,     Floor13, HArchShadow },
	{ DArch,      Floor, VArch, HArch, Floor19,     VArch5,  HArchShadow },
	{ DArch,      Floor, VWall, HArch, Floor16,     VWall17, HArchShadow },
	{ DArch,      Floor, VArch, HWall, Floor16,     VArch5,  HWallShadow },
	{ VArchEnd,   Floor, Floor, Floor, Floor17,     Floor13, Floor15     },
	{ VArchEnd,   Floor, VWall, Floor, Floor17,     VWall17, Floor15     },
	{ VArchEnd,   Floor, VArch, Floor, Floor20,     VArch5,  Floor15     },
	{ HArchEnd,   Floor, 0,     Floor, Floor17,     0,       Floor12     },
	{ HArchEnd,   Floor, 0,     HArch, Floor16,     0,       HArchShadow },
	{ HArchEnd,   0,     0,     HWall, 0,           0,       HWallShadow },
	{ VArch,      0,     0,     Floor, 0,           0,       Floor12     },
	{ VArch,      Floor, 0,     Floor, Floor12,     0,       Floor12     },
	{ VArch,      HWall, 0,     Floor, HWallShadow, 0,       Floor12     },
	{ VArch,      HArch, 0,     Floor, HArchShadow, 0,       Floor12     },
	{ VArch,      Floor, VArch, HArch, Floor12,     0,       HArchShadow },
	{ HWallVArch, 0,     0,     Floor, 0,           0,       Floor12     },
	{ HWallVArch, Floor, 0,     Floor, Floor12,     0,       Floor12     },
	{ HWallVArch, HWall, 0,     Floor, HWallShadow, 0,       Floor12     },
	{ HWallVArch, HArch, 0,     Floor, HArchShadow, 0,       Floor12     },
	{ HWallVArch, Floor, VArch, HArch, Floor12,     0,       HArchShadow },
	{ HArchVWall, 0,     Floor, 0,     0,           Floor13, 0           },
	{ HArchVWall, Floor, Floor, 0,     Floor13,     Floor13, 0           },
	{ HArchVWall, 0,     VWall, 0,     0,           VWall17, 0           },
	{ HArchVWall, Floor, VArch, 0,     Floor13,     VArch5,  0           },
	{ HArch,      0,     Floor, 0,     0,           Floor13, 0           },
	{ HArch,      Floor, Floor, 0,     Floor13,     Floor13, 0           },
	{ HArch,      0,     VWall, 0,     0,           VWall17, 0           },
	{ HArch,      Floor, VArch, 0,     Floor13,     VArch5,  0           },
	{ Corner,     Floor, VArch, HArch, Floor19,     0,       0           }
	// clang-format on
};

/** Maps tile IDs to their corresponding base tile ID. */
const uint8_t BaseTypes[207] = {
	0,
	VWall, HWall, Corner, DWall, DArch, VWallEnd, HWallEnd, HArchEnd, VArchEnd,
	HArchVWall, VArch, HArch, Floor, HWallVArch, Pillar, VCorner, HCorner,
	0, 0, 0, 0, 0, 0, 0,
	VWall, HWall, HArchVWall, DWall, DArch, VWallEnd, HWallEnd, HArchEnd,
	VArchEnd, HArchVWall, VArch, HArch, HWallVArch, DArch, HWallVArch,
	HArchVWall, DWall, HWallVArch, DWall, DArch,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	VWall, HWall, Corner, DWall, VWall, VWallEnd, HWallEnd, VCorner, HCorner,
	HWall, VWall, VWall, HWall, HWall, VWall, VWall, HWall, HWall, HWall, HWall,
	HWall, VWall, VWall, VArch, VWall, Floor, Floor, Floor, VWall, HWall, VWall,
	HWall, VWall, HWall, VWall, HWall, HWall, HWall, HWall, HArch,
	0, 0,
	VArch, VWall, VArch, VWall, Floor,
	0, 0, 0, 0, 0, 0, 0,
	Floor, Floor, Floor, Floor, Floor, Floor, Floor, Floor, Floor, Floor, Floor,
	Floor, Floor, VWall, VArch, HWall, HArch, Floor, Floor, Floor, HArch, HWall,
	VWall, HWall, HWall, DWall, HWallVArch, DWall, HArchVWall, Floor, Floor,
	DWall, DWall, VWall, VWall, DWall, HWall, HWall, Floor, Floor, Floor, Floor,
	VDoor, HDoor, HDoorVDoor, VDoorEnd, HDoorEnd, DWall2, DWall3, HArchVWall3,
	DWall2, HWallVArch2, DWall3, VDoor, DWall2, DWall3, HDoorVDoor, HDoorVDoor,
	VWall, HWall, VDoor, HDoor, Dirt, Dirt, VDoor, HDoor,
	0, 0, 0, 0, 0, 0, 0, 0
};

/** Maps tile IDs to their corresponding undecorated tile ID. */
const uint8_t TileDecorations[207] = {
	0,
	VWall, HWall, Corner, DWall, DArch, VWallEnd, HWallEnd, HArchEnd, VArchEnd,
	HArchVWall, VArch, HArch, Floor, HWallVArch, Pillar, VCorner, HCorner,
	0, 0, 0, 0, 0, 0, 0,
	VDoor, HDoor,
	0,
	HDoorVDoor,
	0,
	VDoorEnd, HDoorEnd,
	0, 0, 0, 0, 0, 0, 0, 0,
	HArchVWall3, DWall2, HWallVArch2, DWall3,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	VWall2, HWall2,
	0,
	DWall4,
	0, 0, 0, 0, 0, 0,
	VWall2,
	0,
	HWall2,
	0, 0,
	VWall2, HWall2,
	0,
	HWall, HWall, HWall, VWall, VWall, VArch, VDoor, Floor, Floor, Floor, VWall,
	HWall, VWall, HWall, VWall, HWall, VWall, HWall, HWall, HWall, HWall, HArch,
	0, 0,
	VArch, VWall, VArch, VWall, Floor,
	0, 0, 0, 0, 0, 0, 0,
	Floor, Floor, Floor, Floor, Floor, Floor,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void ApplyShadowsPatterns()
{
	uint8_t slice[2][2];

	for (int y = 1; y < DMAXY; y++) {
		for (int x = 1; x < DMAXX; x++) {
			slice[0][0] = BaseTypes[dungeon[x][y]];
			slice[1][0] = BaseTypes[dungeon[x - 1][y]];
			slice[0][1] = BaseTypes[dungeon[x][y - 1]];
			slice[1][1] = BaseTypes[dungeon[x - 1][y - 1]];

			for (const auto &shadow : ShadowPatterns) {
				if (shadow.strig != slice[0][0])
					continue;
				if (shadow.s1 != 0 && shadow.s1 != slice[1][1])
					continue;
				if (shadow.s2 != 0 && shadow.s2 != slice[0][1])
					continue;
				if (shadow.s3 != 0 && shadow.s3 != slice[1][0])
					continue;

				if (shadow.nv1 != 0 && !Protected.test(x - 1, y - 1)) {
					dungeon[x - 1][y - 1] = shadow.nv1;
				}
				if (shadow.nv2 != 0 && !Protected.test(x, y - 1)) {
					dungeon[x][y - 1] = shadow.nv2;
				}
				if (shadow.nv3 != 0 && !Protected.test(x - 1, y)) {
					dungeon[x - 1][y] = shadow.nv3;
				}
			}
		}
	}

	for (int y = 1; y < DMAXY; y++) {
		for (int x = 1; x < DMAXX; x++) {
			if (Protected.test(x - 1, y))
				continue;

			if (dungeon[x - 1][y] == Floor12) {
				Tile tnv3 = Floor12;
				if (IsAnyOf(dungeon[x][y], DFence, VFenceEnd, VFence, HWallVFence, HArchVFence, HArchVDoor)) {
					tnv3 = Floor14;
				}
				dungeon[x - 1][y] = tnv3;
			}
			if (dungeon[x - 1][y] == HArchShadow) {
				Tile tnv3 = HArchShadow;
				if (IsAnyOf(dungeon[x][y], DFence, VFenceEnd, VFence, HWallVFence, HArchVFence, HArchVDoor)) {
					tnv3 = HArchShadow2;
				}
				dungeon[x - 1][y] = tnv3;
			}
			if (dungeon[x - 1][y] == HWallShadow) {
				Tile tnv3 = HWallShadow;
				if (IsAnyOf(dungeon[x][y], DFence, VFenceEnd, VFence, HWallVFence, HArchVFence, HArchVDoor)) {
					tnv3 = HWallShadow2;
				}
				dungeon[x - 1][y] = tnv3;
			}
		}
	}
}

bool CanReplaceTile(uint8_t replace, Point tile)
{
	if (replace < VWallEnd2 || replace > VWall8) {
		return true;
	}

	// BUGFIX: p2 is a workaround for a bug, only p1 should have been used (fixing this breaks compatibility)
	constexpr auto ComparisonWithBoundsCheck = [](Point p1, Point p2) {
		return (p1.x >= 0 && p1.x < DMAXX && p1.y >= 0 && p1.y < DMAXY)
		    && (p2.x >= 0 && p2.x < DMAXX && p2.y >= 0 && p2.y < DMAXY)
		    && (dungeon[p1.x][p1.y] >= VWallEnd2 && dungeon[p2.x][p2.y] <= VWall8);
	};
	if (ComparisonWithBoundsCheck(tile + Direction::NorthWest, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::SouthEast, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::SouthWest, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::NorthEast, tile + Direction::NorthWest)) {
		return false;
	}

	return true;
}

void FillFloor()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] != Floor || Protected.test(i, j))
				continue;

			const int rv = RandomIntLessThan(3);
			if (rv == 1)
				dungeon[i][j] = Floor22;
			else if (rv == 2)
				dungeon[i][j] = Floor23;
		}
	}
}

void InitSetPiece()
{
	std::unique_ptr<uint16_t[]> setPieceData;
	if (Quests[Q_BUTCHER].IsAvailable()) {
		setPieceData = LoadFileInMem<uint16_t>("levels\\l1data\\rnd6.dun");
	} else if (Quests[Q_SKELKING].IsAvailable() && !UseMultiplayerQuests()) {
		setPieceData = LoadFileInMem<uint16_t>("levels\\l1data\\skngdo.dun");
	} else if (Quests[Q_LTBANNER].IsAvailable()) {
		setPieceData = LoadFileInMem<uint16_t>("levels\\l1data\\banner2.dun");
	} else {
		return; // no setpiece needed for this level
	}

	const WorldTilePosition setPiecePosition = SelectChamber();
	PlaceDunTiles(setPieceData.get(), setPiecePosition, Floor);
	SetPiece = { setPiecePosition, GetDunSize(setPieceData.get()) };
}

void InitDungeonPieces()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			int8_t pc;
			if (IsAnyOf(dPiece[i][j], 11, 70, 320, 210, 340, 417)) {
				pc = 1;
			} else if (IsAnyOf(dPiece[i][j], 10, 248, 324, 343, 330, 420)) {
				pc = 2;
			} else if (dPiece[i][j] == 252) {
				pc = 3;
			} else if (dPiece[i][j] == 254) {
				pc = 4;
			} else if (dPiece[i][j] == 258) {
				pc = 5;
			} else if (dPiece[i][j] == 266) {
				pc = 6;
			} else {
				continue;
			}
			dSpecial[i][j] = pc;
		}
	}
}

void InitDungeonFlags()
{
	memset(dungeon, Dirt, sizeof(dungeon));
	Protected.reset();
	Chamber.reset();
}

void MapRoom(Rectangle room)
{
	for (int y = 0; y < room.size.height; y++) {
		for (int x = 0; x < room.size.width; x++) {
			DungeonMask.set(room.position.x + x, room.position.y + y);
		}
	}
}

bool CheckRoom(Rectangle room)
{
	for (int j = 0; j < room.size.height; j++) {
		for (int i = 0; i < room.size.width; i++) {
			if (i + room.position.x < 0 || i + room.position.x >= DMAXX || j + room.position.y < 0 || j + room.position.y >= DMAXY) {
				return false;
			}
			if (DungeonMask.test(i + room.position.x, j + room.position.y)) {
				return false;
			}
		}
	}

	return true;
}

void GenerateRoom(Rectangle area, bool verticalLayout)
{
	const bool rotate = FlipCoin(4);
	verticalLayout = (!verticalLayout && rotate) || (verticalLayout && !rotate);

	bool placeRoom1;
	Rectangle room1;

	for (int num = 0; num < 20; num++) {
		const int32_t randomWidth = (GenerateRnd(5) + 2) & ~1;
		const int32_t randomHeight = (GenerateRnd(5) + 2) & ~1;
		room1.size = { randomWidth, randomHeight };
		room1.position = area.position;
		if (verticalLayout) {
			room1.position += Displacement { -room1.size.width, area.size.height / 2 - room1.size.height / 2 };
			placeRoom1 = CheckRoom({ room1.position + Displacement { -1, -1 }, { room1.size.height + 2, room1.size.width + 1 } }); /// BUGFIX: swap height and width ({ room1.size.width + 1, room1.size.height + 2 }) (workaround applied below)
		} else {
			room1.position += Displacement { area.size.width / 2 - room1.size.width / 2, -room1.size.height };
			placeRoom1 = CheckRoom({ room1.position + Displacement { -1, -1 }, { room1.size.width + 2, room1.size.height + 1 } });
		}
		if (placeRoom1)
			break;
	}

	if (placeRoom1)
		MapRoom({ room1.position, { std::min(DMAXX - room1.position.x, room1.size.width), std::min(DMAXX - room1.position.y, room1.size.height) } });

	bool placeRoom2;
	Rectangle room2 = room1;
	if (verticalLayout) {
		room2.position.x = area.position.x + area.size.width;
		placeRoom2 = CheckRoom({ room2.position + Displacement { 0, -1 }, { room2.size.width + 1, room2.size.height + 2 } });
	} else {
		room2.position.y = area.position.y + area.size.height;
		placeRoom2 = CheckRoom({ room2.position + Displacement { -1, 0 }, { room2.size.width + 2, room2.size.height + 1 } });
	}

	if (placeRoom2)
		MapRoom(room2);
	if (placeRoom1)
		GenerateRoom(room1, !verticalLayout);
	if (placeRoom2)
		GenerateRoom(room2, !verticalLayout);
}

/**
 * @brief Generate a boolean dungoen room layout
 */
void FirstRoom()
{
	DungeonMask.reset();

	VerticalLayout = FlipCoin();
	HasChamber1 = !FlipCoin();
	HasChamber2 = !FlipCoin();
	HasChamber3 = !FlipCoin();

	if (!HasChamber1 || !HasChamber3)
		HasChamber2 = true;

	Rectangle chamber1 { { 1, 15 }, { 10, 10 } };
	const Rectangle chamber2 { { 15, 15 }, { 10, 10 } };
	Rectangle chamber3 { { 29, 15 }, { 10, 10 } };
	Rectangle hallway { { 1, 17 }, { 38, 6 } };
	if (!HasChamber1) {
		hallway.position.x += 17;
		hallway.size.width -= 17;
	}
	if (!HasChamber3)
		hallway.size.width -= 16;
	if (VerticalLayout) {
		std::swap(chamber1.position.x, chamber1.position.y);
		std::swap(chamber3.position.x, chamber3.position.y);
		std::swap(hallway.position.x, hallway.position.y);
		std::swap(hallway.size.width, hallway.size.height);
	}

	if (HasChamber1)
		MapRoom(chamber1);
	if (HasChamber2)
		MapRoom(chamber2);
	if (HasChamber3)
		MapRoom(chamber3);

	MapRoom(hallway);

	if (HasChamber1)
		GenerateRoom(chamber1, VerticalLayout);
	if (HasChamber2)
		GenerateRoom(chamber2, VerticalLayout);
	if (HasChamber3)
		GenerateRoom(chamber3, VerticalLayout);
}

/**
 * @brief Find the number of mega tiles used by layout
 */
inline size_t FindArea()
{
	return DungeonMask.count();
}

void MakeDmt()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			if (DungeonMask.test(i, j))
				dungeon[i][j] = Floor;
			else if (!DungeonMask.test(i + 1, j + 1) && DungeonMask.test(i, j + 1) && DungeonMask.test(i + 1, j))
				dungeon[i][j] = Floor; // Remove diagonal corners
			else if (DungeonMask.test(i + 1, j + 1) && DungeonMask.test(i, j + 1) && DungeonMask.test(i + 1, j))
				dungeon[i][j] = VCorner;
			else if (DungeonMask.test(i, j + 1))
				dungeon[i][j] = HWall;
			else if (DungeonMask.test(i + 1, j))
				dungeon[i][j] = VWall;
			else if (DungeonMask.test(i + 1, j + 1))
				dungeon[i][j] = DWall;
			else
				dungeon[i][j] = Dirt;
		}
	}
}

int HorizontalWallOk(Point position)
{
	int length;
	for (length = 1; dungeon[position.x + length][position.y] == Floor; length++) {
		if (dungeon[position.x + length][position.y - 1] != Floor || dungeon[position.x + length][position.y + 1] != Floor || Protected.test(position.x + length, position.y) || Chamber.test(position.x + length, position.y))
			break;
	}

	if (length == 1)
		return -1;

	auto tileId = static_cast<Tile>(dungeon[position.x + length][position.y]);

	if (!IsAnyOf(tileId, Corner, DWall, DArch, VWallEnd, HWallEnd, VCorner, HCorner, DirtHwall, DirtVwall, VDirtCorner, HDirtCorner, DirtHwallEnd, DirtVwallEnd))
		return -1;

	return length;
}

int VerticalWallOk(Point position)
{
	int length;
	for (length = 1; dungeon[position.x][position.y + length] == Floor; length++) {
		if (dungeon[position.x - 1][position.y + length] != Floor || dungeon[position.x + 1][position.y + length] != Floor || Protected.test(position.x, position.y + length) || Chamber.test(position.x, position.y + length))
			break;
	}

	if (length == 1)
		return -1;

	auto tileId = static_cast<Tile>(dungeon[position.x][position.y + length]);

	if (!IsAnyOf(tileId, Corner, DWall, DArch, VWallEnd, HWallEnd, VCorner, HCorner, DirtHwall, DirtVwall, VDirtCorner, HDirtCorner, DirtHwallEnd, DirtVwallEnd))
		return -1;

	return length;
}

void HorizontalWall(Point position, Tile start, int maxX)
{
	Tile wallTile = HWall;
	Tile doorTile = HDoor;

	switch (GenerateRnd(4)) {
	case 2: // Add arch
		wallTile = HArch;
		doorTile = HArch;
		if (start == HWall)
			start = HArch;
		else if (start == DWall)
			start = HArchVWall;
		break;
	case 3: // Add Fence
		wallTile = HFence;
		if (start == HWall)
			start = HFence;
		else if (start == DWall)
			start = HFenceVWall;
		break;
	default:
		break;
	}

	if (GenerateRnd(6) == 5)
		doorTile = HArch;

	dungeon[position.x][position.y] = start;

	for (int x = 1; x < maxX; x++) {
		dungeon[position.x + x][position.y] = wallTile;
	}

	const int x = GenerateRnd(maxX - 1) + 1;

	dungeon[position.x + x][position.y] = doorTile;
	if (doorTile == HDoor) {
		Protected.set(position.x + x, position.y);
	}
}

void VerticalWall(Point position, Tile start, int maxY)
{
	Tile wallTile = VWall;
	Tile doorTile = VDoor;

	switch (GenerateRnd(4)) {
	case 2: // Add arch
		wallTile = VArch;
		doorTile = VArch;
		if (start == VWall)
			start = VArch;
		else if (start == DWall)
			start = HWallVArch;
		break;
	case 3: // Add Fence
		wallTile = VFence;
		if (start == VWall)
			start = VFence;
		else if (start == DWall)
			start = HWallVFence;
		break;
	default:
		break;
	}

	if (GenerateRnd(6) == 5)
		doorTile = VArch;

	dungeon[position.x][position.y] = start;

	for (int y = 1; y < maxY; y++) {
		dungeon[position.x][position.y + y] = wallTile;
	}

	const int y = GenerateRnd(maxY - 1) + 1;

	dungeon[position.x][position.y + y] = doorTile;
	if (doorTile == VDoor) {
		Protected.set(position.x, position.y + y);
	}
}

void AddWall()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (Protected.test(i, j) || Chamber.test(i, j))
				continue;

			if (dungeon[i][j] == Corner) {
				DiscardRandomValues(1);
				const int maxX = HorizontalWallOk({ i, j });
				if (maxX != -1) {
					HorizontalWall({ i, j }, HWall, maxX);
				}
			}
			if (dungeon[i][j] == Corner) {
				DiscardRandomValues(1);
				const int maxY = VerticalWallOk({ i, j });
				if (maxY != -1) {
					VerticalWall({ i, j }, VWall, maxY);
				}
			}
			if (dungeon[i][j] == VWallEnd) {
				DiscardRandomValues(1);
				const int maxX = HorizontalWallOk({ i, j });
				if (maxX != -1) {
					HorizontalWall({ i, j }, DWall, maxX);
				}
			}
			if (dungeon[i][j] == HWallEnd) {
				DiscardRandomValues(1);
				const int maxY = VerticalWallOk({ i, j });
				if (maxY != -1) {
					VerticalWall({ i, j }, DWall, maxY);
				}
			}
			if (dungeon[i][j] == HWall) {
				DiscardRandomValues(1);
				const int maxX = HorizontalWallOk({ i, j });
				if (maxX != -1) {
					HorizontalWall({ i, j }, HWall, maxX);
				}
			}
			if (dungeon[i][j] == VWall) {
				DiscardRandomValues(1);
				const int maxY = VerticalWallOk({ i, j });
				if (maxY != -1) {
					VerticalWall({ i, j }, VWall, maxY);
				}
			}
		}
	}
}

void GenerateChamber(Point position, bool connectPrevious, bool connectNext, bool verticalLayout)
{
	if (connectPrevious) {
		if (verticalLayout) {
			dungeon[position.x + 2][position.y] = HArch;
			dungeon[position.x + 3][position.y] = HArch;
			dungeon[position.x + 4][position.y] = Corner;
			dungeon[position.x + 7][position.y] = VArchEnd;
			dungeon[position.x + 8][position.y] = HArch;
			dungeon[position.x + 9][position.y] = HWall;
		} else {
			dungeon[position.x][position.y + 2] = VArch;
			dungeon[position.x][position.y + 3] = VArch;
			dungeon[position.x][position.y + 4] = Corner;
			dungeon[position.x][position.y + 7] = HArchEnd;
			dungeon[position.x][position.y + 8] = VArch;
			dungeon[position.x][position.y + 9] = VWall;
		}
	}
	if (connectNext) {
		if (verticalLayout) {
			position.y += 11;
			dungeon[position.x + 2][position.y] = HArchVWall;
			dungeon[position.x + 3][position.y] = HArch;
			dungeon[position.x + 4][position.y] = HArchEnd;
			dungeon[position.x + 7][position.y] = DArch;
			dungeon[position.x + 8][position.y] = HArch;
			if (dungeon[position.x + 9][position.y] != DWall)
				dungeon[position.x + 9][position.y] = HDirtCorner;
			position.y -= 11;
		} else {
			position.x += 11;
			dungeon[position.x][position.y + 2] = HWallVArch;
			dungeon[position.x][position.y + 3] = VArch;
			dungeon[position.x][position.y + 4] = VArchEnd;
			dungeon[position.x][position.y + 7] = DArch;
			dungeon[position.x][position.y + 8] = VArch;
			if (dungeon[position.x][position.y + 9] != DWall)
				dungeon[position.x][position.y + 9] = HDirtCorner;
			position.x -= 11;
		}
	}

	for (int y = 1; y < 11; y++) {
		for (int x = 1; x < 11; x++) {
			dungeon[position.x + x][position.y + y] = Floor;
			Chamber.set(position.x + x, position.y + y);
		}
	}

	dungeon[position.x + 4][position.y + 4] = Pillar;
	dungeon[position.x + 7][position.y + 4] = Pillar;
	dungeon[position.x + 4][position.y + 7] = Pillar;
	dungeon[position.x + 7][position.y + 7] = Pillar;
}

void GenerateHall(Point start, int length, bool verticalLayout)
{
	if (verticalLayout) {
		for (int i = start.y; i < start.y + length; i++) {
			dungeon[start.x][i] = VArch;
			dungeon[start.x + 3][i] = VArch;
		}
	} else {
		for (int i = start.x; i < start.x + length; i++) {
			dungeon[i][start.y] = HArch;
			dungeon[i][start.y + 3] = HArch;
		}
	}
}

void FixTilesPatterns()
{
	// BUGFIX: Bounds checks are required in all loop bodies.
	// See https://github.com/diasurgical/devilutionX/pull/401

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (i + 1 < DMAXX) {
				if (dungeon[i][j] == HWall && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = DirtHwallEnd;
				if (dungeon[i][j] == Floor && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = DirtHwall;
				if (dungeon[i][j] == Floor && dungeon[i + 1][j] == HWall)
					dungeon[i + 1][j] = HWallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = DirtVwallEnd;
			}
			if (j + 1 < DMAXY) {
				if (dungeon[i][j] == VWall && dungeon[i][j + 1] == Dirt)
					dungeon[i][j + 1] = DirtVwallEnd;
				if (dungeon[i][j] == Floor && dungeon[i][j + 1] == VWall)
					dungeon[i][j + 1] = VWallEnd;
				if (dungeon[i][j] == Floor && dungeon[i][j + 1] == Dirt)
					dungeon[i][j + 1] = DirtVwall;
			}
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (i + 1 < DMAXX) {
				if (dungeon[i][j] == Floor && dungeon[i + 1][j] == DirtVwall)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == Floor && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = VDirtCorner;
				if (dungeon[i][j] == HWallEnd && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = DirtHwallEnd;
				if (dungeon[i][j] == Floor && dungeon[i + 1][j] == DirtVwallEnd)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == DirtVwall && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = VDirtCorner;
				if (dungeon[i][j] == HWall && dungeon[i + 1][j] == DirtVwall)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == DirtVwall && dungeon[i + 1][j] == VWall)
					dungeon[i + 1][j] = VWallEnd;
				if (dungeon[i][j] == HWallEnd && dungeon[i + 1][j] == DirtVwall)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == HWall && dungeon[i + 1][j] == VWall)
					dungeon[i + 1][j] = VWallEnd;
				if (dungeon[i][j] == Corner && dungeon[i + 1][j] == Dirt)
					dungeon[i + 1][j] = DirtVwallEnd;
				if (dungeon[i][j] == HDirtCorner && dungeon[i + 1][j] == VWall)
					dungeon[i + 1][j] = VWallEnd;
				if (dungeon[i][j] == HWallEnd && dungeon[i + 1][j] == VWall)
					dungeon[i + 1][j] = VWallEnd;
				if (dungeon[i][j] == HWallEnd && dungeon[i + 1][j] == DirtVwallEnd)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == DWall && dungeon[i + 1][j] == VCorner)
					dungeon[i + 1][j] = HCorner;
				if (dungeon[i][j] == HWallEnd && dungeon[i + 1][j] == Floor)
					dungeon[i + 1][j] = HCorner;
				if (dungeon[i][j] == HWall && dungeon[i + 1][j] == DirtVwallEnd)
					dungeon[i + 1][j] = HDirtCorner;
				if (dungeon[i][j] == HWall && dungeon[i + 1][j] == Floor)
					dungeon[i + 1][j] = HCorner;
			}
			if (i > 0) {
				if (dungeon[i][j] == DirtHwallEnd && dungeon[i - 1][j] == Dirt)
					dungeon[i - 1][j] = DirtVwall;
				if (dungeon[i][j] == DirtVwall && dungeon[i - 1][j] == DirtHwallEnd)
					dungeon[i - 1][j] = HDirtCorner;
				if (dungeon[i][j] == VWallEnd && dungeon[i - 1][j] == Dirt)
					dungeon[i - 1][j] = DirtVwallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i - 1][j] == DirtHwallEnd)
					dungeon[i - 1][j] = HDirtCorner;
			}
			if (j + 1 < DMAXY) {
				if (dungeon[i][j] == VWall && dungeon[i][j + 1] == HWall)
					dungeon[i][j + 1] = HWallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i][j + 1] == DirtHwall)
					dungeon[i][j + 1] = HDirtCorner;
				if (dungeon[i][j] == DirtHwall && dungeon[i][j + 1] == HWall)
					dungeon[i][j + 1] = HWallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i][j + 1] == HWall)
					dungeon[i][j + 1] = HWallEnd;
				if (dungeon[i][j] == HDirtCorner && dungeon[i][j + 1] == HWall)
					dungeon[i][j + 1] = HWallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i][j + 1] == Dirt)
					dungeon[i][j + 1] = DirtVwallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i][j + 1] == Floor)
					dungeon[i][j + 1] = VCorner;
				if (dungeon[i][j] == VWall && dungeon[i][j + 1] == Floor)
					dungeon[i][j + 1] = VCorner;
				if (dungeon[i][j] == Floor && dungeon[i][j + 1] == VCorner)
					dungeon[i][j + 1] = HCorner;
			}
			if (j > 0) {
				if (dungeon[i][j] == VWallEnd && dungeon[i][j - 1] == Dirt)
					dungeon[i][j - 1] = HWallEnd;
				if (dungeon[i][j] == VWallEnd && dungeon[i][j - 1] == Dirt)
					dungeon[i][j - 1] = DirtVwallEnd;
				if (dungeon[i][j] == HWallEnd && dungeon[i][j - 1] == DirtVwallEnd)
					dungeon[i][j - 1] = HDirtCorner;
				if (dungeon[i][j] == DirtHwall && dungeon[i][j - 1] == DirtVwallEnd)
					dungeon[i][j - 1] = HDirtCorner;
			}
		}
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (j + 1 < DMAXY && dungeon[i][j] == DWall && dungeon[i][j + 1] == HWall)
				dungeon[i][j + 1] = HWallEnd;
			if (i + 1 < DMAXX && dungeon[i][j] == HWall && dungeon[i + 1][j] == DirtVwall)
				dungeon[i + 1][j] = HDirtCorner;
			if (j + 1 < DMAXY && dungeon[i][j] == DirtHwall && dungeon[i][j + 1] == Dirt)
				dungeon[i][j + 1] = VDirtCorner;
		}
	}
}

void Substitution()
{
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			if (FlipCoin(4)) {
				const uint8_t c = TileDecorations[dungeon[x][y]];
				if (c != 0 && !Protected.test(x, y)) {
					int rv = GenerateRnd(16);
					int i = -1;
					while (rv >= 0) {
						i++;
						if (i == sizeof(TileDecorations)) {
							i = 0;
						}
						if (c == TileDecorations[i]) {
							rv--;
						}
					}

					// BUGFIX: Add `&& y > 0` to the if statement. (fixed)
					if (i == VWall4 && y > 0) {
						if (TileDecorations[dungeon[x][y - 1]] != VWall2 || Protected.test(x, y - 1))
							i = VWall2;
						else
							dungeon[x][y - 1] = VWall5;
					}
					// BUGFIX: Add `&& x + 1 < DMAXX` to the if statement. (fixed)
					if (i == HWall4 && x + 1 < DMAXX) {
						if (TileDecorations[dungeon[x + 1][y]] != HWall2 || Protected.test(x + 1, y))
							i = HWall2;
						else
							dungeon[x + 1][y] = HWall5;
					}
					dungeon[x][y] = i;
				}
			}
		}
	}
}

void FillChambers()
{
	Point chamber1 { 0, 14 };
	Point chamber3 { 28, 14 };
	Point hall1 { 12, 18 };
	Point hall2 { 26, 18 };
	if (VerticalLayout) {
		std::swap(chamber1.x, chamber1.y);
		std::swap(chamber3.x, chamber3.y);
		std::swap(hall1.x, hall1.y);
		std::swap(hall2.x, hall2.y);
	}

	if (HasChamber1)
		GenerateChamber(chamber1, false, true, VerticalLayout);
	if (HasChamber2)
		GenerateChamber({ 14, 14 }, HasChamber1, HasChamber3, VerticalLayout);
	if (HasChamber3)
		GenerateChamber(chamber3, true, false, VerticalLayout);

	if (HasChamber2) {
		if (HasChamber1)
			GenerateHall(hall1, 2, VerticalLayout);
		if (HasChamber3)
			GenerateHall(hall2, 2, VerticalLayout);
	} else {
		GenerateHall(hall1, 16, VerticalLayout);
	}

	if (leveltype == DTYPE_CRYPT) {
		if (currlevel == 24) {
			SetCryptRoom();
		} else if (CornerStone.isAvailable()) {
			SetCornerRoom();
		}
	} else {
		InitSetPiece();
	}
}

void FixTransparency()
{
	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) {
			// BUGFIX: Should check for `j > 0` first. (fixed)
			if (dungeon[i][j] == DirtHwallEnd && j > 0 && dungeon[i][j - 1] == DirtHwall) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			// BUGFIX: Should check for `i + 1 < DMAXY` first. (fixed)
			if (dungeon[i][j] == DirtVwallEnd && i + 1 < DMAXY && dungeon[i + 1][j] == DirtVwall) {
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == DirtHwall) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == DirtVwall) {
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			if (dungeon[i][j] == VDirtCorner) {
				dTransVal[xx + 1][yy] = dTransVal[xx][yy];
				dTransVal[xx][yy + 1] = dTransVal[xx][yy];
				dTransVal[xx + 1][yy + 1] = dTransVal[xx][yy];
			}
			xx += 2;
		}
		yy += 2;
	}
}

void FixDirtTiles()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			if (dungeon[i][j] == HDirtCorner && dungeon[i + 1][j] != DirtVwall) {
				dungeon[i][j] = DirtCorner2;
			}
			if (dungeon[i][j] == DirtVwall && dungeon[i + 1][j] != DirtVwall) {
				dungeon[i][j] = DirtVWall2;
			}
			if (dungeon[i][j] == DirtVwallEnd && dungeon[i + 1][j] != DirtVwall) {
				dungeon[i][j] = DirtVWallEnd2;
			}
			if (dungeon[i][j] == DirtHwall && dungeon[i][j + 1] != DirtHwall) {
				dungeon[i][j] = DirtHWall2;
			}
			if (dungeon[i][j] == HDirtCorner && dungeon[i][j + 1] != DirtHwall) {
				dungeon[i][j] = DirtCorner2;
			}
			if (dungeon[i][j] == DirtHwallEnd && dungeon[i][j + 1] != DirtHwall) {
				dungeon[i][j] = DirtHWallEnd2;
			}
		}
	}
}

void FixCornerTiles()
{
	for (int j = 1; j < DMAXY - 1; j++) {
		for (int i = 1; i < DMAXX - 1; i++) {
			if (!Protected.test(i, j) && dungeon[i][j] == HCorner && dungeon[i - 1][j] == Floor && dungeon[i][j - 1] == VWall) {
				dungeon[i][j] = VCorner;
				// BUGFIX: Set tile as Protected
			}
			if (dungeon[i][j] == DirtCorner2 && dungeon[i + 1][j] == Floor && dungeon[i][j + 1] == VWall) {
				dungeon[i][j] = HArchEnd;
			}
			if (dungeon[i][j] == DirtCorner2 && dungeon[i][j + 1] == Floor && dungeon[i + 1][j] == HWall) {
				dungeon[i][j] = VArchEnd;
			}
		}
	}
}

bool PlaceCathedralStairs(lvl_entry entry)
{
	bool success = true;
	std::optional<Point> position;

	// Place poison water entrance
	if (Quests[Q_PWATER].IsAvailable()) {
		position = PlaceMiniSet(PWATERIN, DMAXX * DMAXY, true);
		if (!position) {
			success = false;
		} else {
			const int8_t t = TransVal;
			TransVal = 0;
			const Point miniPosition = *position;
			DRLG_MRectTrans({ miniPosition + Displacement { 0, 2 }, { 5, 2 } });
			TransVal = t;
			Quests[Q_PWATER].position = miniPosition.megaToWorld() + Displacement { 5, 6 };
			if (entry == ENTRY_RTNLVL)
				ViewPosition = Quests[Q_PWATER].position;
		}
	}

	// Place stairs up
	position = PlaceMiniSet(MyPlayer->pOriginalCathedral && !Quests[Q_LTBANNER].IsAvailable() ? L5STAIRSUP : STAIRSUP, DMAXX * DMAXY, true);
	if (!position) {
		if (MyPlayer->pOriginalCathedral)
			return false;
		success = false;
	} else if (entry == ENTRY_MAIN) {
		ViewPosition = position->megaToWorld() + Displacement { 3, 4 };
	}

	// Place stairs down
	if (Quests[Q_LTBANNER].IsAvailable()) {
		if (entry == ENTRY_PREV)
			ViewPosition = SetPiece.position.megaToWorld() + Displacement { 3, 11 };
	} else {
		position = PlaceMiniSet(STAIRSDOWN, DMAXX * DMAXY, true);
		if (!position) {
			success = false;
		} else if (entry == ENTRY_PREV) {
			ViewPosition = position->megaToWorld() + Displacement { 3, 3 };
		}
	}

	return success;
}

bool PlaceStairs(lvl_entry entry)
{
	if (leveltype == DTYPE_CRYPT) {
		return PlaceCryptStairs(entry);
	}

	return PlaceCathedralStairs(entry);
}

void GenerateLevel(lvl_entry entry)
{
	if (LevelSeeds[currlevel])
		SetRndSeed(*LevelSeeds[currlevel]);

	size_t minarea = 761;
	switch (currlevel) {
	case 1:
		minarea = 533;
		break;
	case 2:
		minarea = 693;
		break;
	default:
		break;
	}

	while (true) {
		DRLG_InitTrans();

		do {
			LevelSeeds[currlevel] = GetLCGEngineState();
			FirstRoom();
		} while (FindArea() < minarea);

		InitDungeonFlags();
		MakeDmt();
		FillChambers();
		FixTilesPatterns();
		AddWall();
		FloodTransparencyValues(13);
		if (PlaceStairs(entry))
			break;
	}

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == EntranceStairs) {
				const int xx = 2 * i + 16; /* todo: fix loop */
				const int yy = 2 * j + 16;
				DRLG_CopyTrans(xx, yy + 1, xx, yy);
				DRLG_CopyTrans(xx + 1, yy + 1, xx + 1, yy);
			}
		}
	}

	FixTransparency();
	if (leveltype == DTYPE_CRYPT) {
		FixCryptDirtTiles();
	} else {
		FixDirtTiles();
	}
	FixCornerTiles();

	if (leveltype == DTYPE_CRYPT) {
		CryptSubstitution();
	} else {
		Substitution();
		ApplyShadowsPatterns();

		const int numt = GenerateRnd(5) + 5;
		for (int i = 0; i < numt; i++) {
			PlaceMiniSet(LAMPS, DMAXX * DMAXY, true);
		}

		FillFloor();
	}

	memcpy(pdungeon, dungeon, sizeof(pdungeon));

	DRLG_CheckQuests(SetPiece.position);
}

void Pass3()
{
	DRLG_LPass3(Dirt - 1);

	if (leveltype == DTYPE_CRYPT)
		InitCryptPieces();
	else
		InitDungeonPieces();
}

} // namespace

void PlaceMiniSetRandom(const Miniset &miniset, int rndper)
{
	const WorldTileCoord sw = miniset.size.width;
	const WorldTileCoord sh = miniset.size.height;

	for (WorldTileCoord sy = 0; sy < DMAXY - sh; sy++) {
		for (WorldTileCoord sx = 0; sx < DMAXX - sw; sx++) {
			if (!miniset.matches({ sx, sy }, false))
				continue;
			// BUGFIX: This code is copied from Cave and should not be applied for crypt
			if (!CanReplaceTile(miniset.replace[0][0], { sx, sy }))
				continue;
			if (GenerateRnd(100) >= rndper)
				continue;
			miniset.place({ sx, sy });
		}
	}
}

WorldTilePosition SelectChamber()
{
	int chamber;
	if (HasChamber1 && HasChamber2 && HasChamber3) {
		chamber = GenerateRnd(3) + 1;
	} else if (HasChamber1 && HasChamber2) {
		chamber = PickRandomlyAmong({ 2, 1 }); // Reverse order to match vanilla
	} else if (HasChamber1 && HasChamber3) {
		chamber = PickRandomlyAmong({ 3, 1 }); // Reverse order to match vanilla
	} else if (HasChamber2 && HasChamber3) {
		chamber = PickRandomlyAmong({ 2, 3 });
	} else {
		// The dungeon generation logic ensures that chamber 2 is available if
		// either (or both of) 1 or 3 aren't, so if we ever end up with a single
		// chamber layout it's always chamber 2.
		chamber = 2;
	}

	switch (chamber) {
	case 1:
		return VerticalLayout ? WorldTilePosition { 16, 2 } : WorldTilePosition { 2, 16 };
	case 3:
		return VerticalLayout ? WorldTilePosition { 16, 30 } : WorldTilePosition { 30, 16 };
	default:
		return { 16, 16 };
	}
}

void CreateL5Dungeon(uint32_t rseed, lvl_entry entry)
{
	SetRndSeed(rseed);

	UberRow = 0;
	UberCol = 0;

	GenerateLevel(entry);

	Pass3();

	if (leveltype == DTYPE_CRYPT) {
		PlaceCryptLights();
		SetCryptSetPieceRoom();
	}
}

void LoadPreL1Dungeon(const char *path)
{
	InitDungeonFlags();

	auto dunData = LoadFileInMem<uint16_t>(path);
	PlaceDunTiles(dunData.get(), { 0, 0 }, Floor);

	if (setlvltype == DTYPE_CATHEDRAL)
		FillFloor();

	memcpy(pdungeon, dungeon, sizeof(pdungeon));
}

void LoadL1Dungeon(const char *path, Point spawn)
{
	LoadDungeonBase(path, spawn, Floor, Dirt);

	if (setlvltype == DTYPE_CATHEDRAL)
		FillFloor();

	Pass3();

	if (setlvltype == DTYPE_CRYPT) {
		AddCryptObjects(0, 0, MAXDUNX, MAXDUNY);
		PlaceCryptLights();
	} else {
		AddL1Objs(0, 0, MAXDUNX, MAXDUNY);
	}
}

} // namespace devilution
