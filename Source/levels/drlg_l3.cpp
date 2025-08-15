#include "levels/drlg_l3.h"

#include <algorithm>
#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "lighting.h"
#include "monster.h"
#include "objdat.h"
#include "objects.h"
#include "player.h"
#include "quests.h"
#include "utils/is_of.hpp"

namespace devilution {

namespace {

int lockoutcnt;

/**
 * A lookup table for the 16 possible patterns of a 2x2 area,
 * where each cell either contains a SW wall or it doesn't.
 */
const uint8_t L3ConvTbl[16] = { 8, 11, 3, 10, 1, 9, 12, 12, 6, 13, 4, 13, 2, 14, 5, 7 };
/** Miniset: Stairs up. */
const Miniset L3UP {
	{ 3, 3 },
	{
	    { 8, 8, 0 },
	    { 10, 10, 0 },
	    { 7, 7, 0 },
	},
	{
	    { 51, 50, 0 },
	    { 48, 49, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset L6UP {
	{ 3, 3 },
	{
	    { 8, 8, 0 },
	    { 10, 10, 0 },
	    { 7, 7, 0 },
	},
	{
	    { 20, 19, 0 },
	    { 17, 18, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stairs down. */
const Miniset L3DOWN {
	{ 3, 3 },
	{
	    { 8, 9, 7 },
	    { 8, 9, 7 },
	    { 0, 0, 0 },
	},
	{
	    { 0, 47, 0 },
	    { 0, 46, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset L6DOWN {
	{ 3, 3 },
	{
	    { 8, 9, 7 },
	    { 8, 9, 7 },
	    { 0, 0, 0 },
	},
	{
	    { 0, 16, 0 },
	    { 0, 15, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stairs up to town. */
const Miniset L3HOLDWARP {
	{ 3, 3 },
	{
	    { 8, 8, 0 },
	    { 10, 10, 0 },
	    { 7, 7, 0 },
	},
	{
	    { 125, 125, 0 },
	    { 125, 125, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset L6HOLDWARP {
	{ 3, 3 },
	{
	    { 8, 8, 0 },
	    { 10, 10, 0 },
	    { 7, 7, 0 },
	},
	{
	    { 24, 23, 0 },
	    { 21, 22, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stalagmite white stalactite 1. */
const Miniset L3TITE1 {
	{ 4, 4 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 0, 57, 58, 0 },
	    { 0, 56, 55, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stalagmite white stalactite 2. */
const Miniset L3TITE2 {
	{ 4, 4 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 0, 61, 62, 0 },
	    { 0, 60, 59, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stalagmite white stalactite 3. */
const Miniset L3TITE3 {
	{ 4, 4 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 0, 65, 66, 0 },
	    { 0, 64, 63, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stalagmite white stalactite horizontal. */
const Miniset L3TITE6 {
	{ 5, 4 },
	{
	    { 7, 7, 7, 7, 7 },
	    { 7, 7, 7, 0, 7 },
	    { 7, 7, 7, 0, 7 },
	    { 7, 7, 7, 7, 7 },
	},
	{
	    { 0, 0, 0, 0, 0 },
	    { 0, 77, 78, 0, 0 },
	    { 0, 76, 74, 75, 0 },
	    { 0, 0, 0, 0, 0 },
	}
};
/** Miniset: Stalagmite white stalactite vertical. */
const Miniset L3TITE7 {
	{ 4, 5 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 0, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 0, 0, 0, 0 },
	    { 0, 83, 0, 0 },
	    { 0, 82, 80, 0 },
	    { 0, 81, 79, 0 },
	    { 0, 0, 0, 0 },
	}
};
/** Miniset: Stalagmite 1. */
const Miniset L3TITE8 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 52, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stalagmite 2. */
const Miniset L3TITE9 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 53, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stalagmite 3. */
const Miniset L3TITE10 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 54, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stalagmite 4. */
const Miniset L3TITE11 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 67, 0 },
	    { 0, 0, 0 },
	}
};
/** Miniset: Stalagmite on vertical wall. */
const Miniset L3TITE12 {
	{ 2, 1 },
	{ { 9, 7 } },
	{ { 68, 0 } }
};
/** Miniset: Stalagmite on horizontal wall. */
const Miniset L3TITE13 {
	{ 1, 2 },
	{
	    { 10 },
	    { 7 },
	},
	{
	    { 69 },
	    { 0 },
	}
};
/** Miniset: Cracked vertical wall 1. */
const Miniset L3CREV1 {
	{ 2, 1 },
	{ { 8, 7 } },
	{ { 84, 85 } }
};
/** Miniset: Cracked vertical wall - north corner. */
const Miniset L3CREV2 {
	{ 2, 1 },
	{ { 8, 11 } },
	{ { 86, 87 } }
};
/** Miniset: Cracked horizontal wall 1. */
const Miniset L3CREV3 {
	{ 1, 2 },
	{
	    { 8 },
	    { 10 },
	},
	{
	    { 89 },
	    { 88 },
	}
};
/** Miniset: Cracked vertical wall 2. */
const Miniset L3CREV4 {
	{ 2, 1 },
	{ { 8, 7 } },
	{ { 90, 91 } }
};
/** Miniset: Cracked horizontal wall - north corner. */
const Miniset L3CREV5 {
	{ 1, 2 },
	{
	    { 8 },
	    { 11 },
	},
	{
	    { 92 },
	    { 93 },
	}
};
/** Miniset: Cracked horizontal wall 2. */
const Miniset L3CREV6 {
	{ 1, 2 },
	{
	    { 8 },
	    { 10 },
	},
	{
	    { 95 },
	    { 94 },
	}
};
/** Miniset: Cracked vertical wall - west corner. */
const Miniset L3CREV7 {
	{ 2, 1 },
	{ { 8, 7 } },
	{ { 96, 101 } }
};
/** Miniset: Cracked horizontal wall - north. */
const Miniset L3CREV8 {
	{ 1, 2 },
	{
	    { 2 },
	    { 8 },
	},
	{
	    { 102 },
	    { 97 },
	}
};
/** Miniset: Cracked vertical wall - east corner. */
const Miniset L3CREV9 {
	{ 2, 1 },
	{ { 3, 8 } },
	{ { 103, 98 } }
};
/** Miniset: Cracked vertical wall - west. */
const Miniset L3CREV10 {
	{ 2, 1 },
	{ { 4, 8 } },
	{ { 104, 99 } }
};
/** Miniset: Cracked horizontal wall - south corner. */
const Miniset L3CREV11 {
	{ 1, 2 },
	{
	    { 6 },
	    { 8 },
	},
	{
	    { 105 },
	    { 100 },
	}
};
/** Miniset: Replace broken wall with floor 1. */
const Miniset L3ISLE1 {
	{ 2, 3 },
	{
	    { 5, 14 },
	    { 4, 9 },
	    { 13, 12 },
	},
	{
	    { 7, 7 },
	    { 7, 7 },
	    { 7, 7 },
	}
};
/** Miniset: Replace small wall with floor 2. */
const Miniset L3ISLE2 {
	{ 3, 2 },
	{
	    { 5, 2, 14 },
	    { 13, 10, 12 },
	},
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	}
};
/** Miniset: Replace small wall with lava 1. */
const Miniset L3ISLE3 {
	{ 2, 3 },
	{
	    { 5, 14 },
	    { 4, 9 },
	    { 13, 12 },
	},
	{
	    { 29, 30 },
	    { 25, 28 },
	    { 31, 32 },
	}
};
/** Miniset: Replace small wall with lava 2. */
const Miniset L3ISLE4 {
	{ 3, 2 },
	{
	    { 5, 2, 14 },
	    { 13, 10, 12 },
	},
	{
	    { 29, 26, 30 },
	    { 31, 27, 32 },
	}
};
/** Miniset: Replace small wall with floor 3. */
const Miniset L3ISLE5 {
	{ 2, 2 },
	{
	    { 5, 14 },
	    { 13, 12 },
	},
	{
	    { 7, 7 },
	    { 7, 7 },
	}
};
const Miniset HivePattern9 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 126, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern10 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 124, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern29 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 67, 0, 0 },
	    { 66, 51, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern30 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 69, 0, 0 },
	    { 68, 52, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern31 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 70, 0, 0 },
	    { 71, 53, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern32 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 73, 0, 0 },
	    { 72, 54, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern33 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 75, 0, 0 },
	    { 74, 55, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern34 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 77, 0, 0 },
	    { 76, 56, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern35 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 79, 0, 0 },
	    { 78, 57, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern36 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 81, 0, 0 },
	    { 80, 58, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern37 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 83, 0, 0 },
	    { 82, 59, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset HivePattern38 {
	{ 3, 3 },
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	},
	{
	    { 84, 0, 0 },
	    { 85, 60, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset L6ISLE1 {
	{ 2, 3 },
	{
	    { 5, 14 },
	    { 4, 9 },
	    { 13, 12 },
	},
	{
	    { 7, 7 },
	    { 7, 7 },
	    { 7, 7 },
	}
};
const Miniset L6ISLE2 {
	{ 3, 2 },
	{
	    { 5, 2, 14 },
	    { 13, 10, 12 },
	},
	{
	    { 7, 7, 7 },
	    { 7, 7, 7 },
	}
};
const Miniset L6ISLE3 {
	{ 2, 3 },
	{
	    { 5, 14 },
	    { 4, 9 },
	    { 13, 12 },
	},
	{
	    { 107, 115 },
	    { 119, 122 },
	    { 131, 123 },
	}
};
const Miniset L6ISLE4 {
	{ 3, 2 },
	{
	    { 5, 2, 14 },
	    { 13, 10, 12 },
	},
	{
	    { 107, 120, 115 },
	    { 131, 121, 123 },
	}
};
const Miniset L6ISLE5 {
	{ 2, 2 },
	{
	    { 5, 14 },
	    { 13, 12 },
	},
	{
	    { 7, 7 },
	    { 7, 7 },
	}
};
const Miniset HivePattern39 {
	{ 4, 4 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 7, 7, 7, 7 },
	    { 7, 107, 115, 7 },
	    { 7, 131, 123, 7 },
	    { 7, 7, 7, 7 },
	}
};
const Miniset HivePattern40 {
	{ 4, 4 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 108, 7 },
	    { 7, 109, 112, 7 },
	    { 7, 7, 7, 7 },
	}
};
const Miniset HivePattern41 {
	{ 4, 5 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 7, 7, 7, 7 },
	    { 7, 107, 115, 7 },
	    { 7, 119, 122, 7 },
	    { 7, 131, 123, 7 },
	    { 7, 7, 7, 7 },
	}
};
const Miniset HivePattern42 {
	{ 4, 5 },
	{
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	    { 7, 7, 7, 7 },
	},
	{
	    { 7, 7, 7, 7 },
	    { 7, 126, 108, 7 },
	    { 7, 7, 117, 7 },
	    { 7, 109, 112, 7 },
	    { 7, 7, 7, 7 },
	}
};

void InitDungeonFlags()
{
	memset(dungeon, 0, sizeof(dungeon));
	Protected.reset();
}

bool FillRoom(int x1, int y1, int x2, int y2)
{
	if (x1 <= 1 || x2 >= 34 || y1 <= 1 || y2 >= 38) {
		return false;
	}

	int v = 0;
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			v += dungeon[i][j];
		}
	}

	if (v != 0) {
		return false;
	}

	for (int j = y1 + 1; j < y2; j++) {
		for (int i = x1 + 1; i < x2; i++) {
			dungeon[i][j] = 1;
		}
	}
	for (int j = y1; j <= y2; j++) {
		if (!FlipCoin()) {
			dungeon[x1][j] = 1;
		}
		if (!FlipCoin()) {
			dungeon[x2][j] = 1;
		}
	}
	for (int i = x1; i <= x2; i++) {
		if (!FlipCoin()) {
			dungeon[i][y1] = 1;
		}
		if (!FlipCoin()) {
			dungeon[i][y2] = 1;
		}
	}

	return true;
}

void CreateBlock(int x, int y, int obs, int dir)
{
	int x1;
	int y1;
	int x2;
	int y2;

	const int blksizex = RandomIntBetween(3, 4);
	const int blksizey = RandomIntBetween(3, 4);

	if (dir == 0) {
		y2 = y - 1;
		y1 = y2 - blksizey;
		if (blksizex < obs) {
			x1 = GenerateRnd(blksizex) + x;
		}
		if (blksizex == obs) {
			x1 = x;
		}
		if (blksizex > obs) {
			x1 = x - GenerateRnd(blksizex);
		}
		x2 = blksizex + x1;
	}
	if (dir == 1) {
		x1 = x + 1;
		x2 = x1 + blksizex;
		if (blksizey < obs) {
			y1 = GenerateRnd(blksizey) + y;
		}
		if (blksizey == obs) {
			y1 = y;
		}
		if (blksizey > obs) {
			y1 = y - GenerateRnd(blksizey);
		}
		y2 = y1 + blksizey;
	}
	if (dir == 2) {
		y1 = y + 1;
		y2 = y1 + blksizey;
		if (blksizex < obs) {
			x1 = GenerateRnd(blksizex) + x;
		}
		if (blksizex == obs) {
			x1 = x;
		}
		if (blksizex > obs) {
			x1 = x - GenerateRnd(blksizex);
		}
		x2 = blksizex + x1;
	}
	if (dir == 3) {
		x2 = x - 1;
		x1 = x2 - blksizex;
		if (blksizey < obs) {
			y1 = GenerateRnd(blksizey) + y;
		}
		if (blksizey == obs) {
			y1 = y;
		}
		if (blksizey > obs) {
			y1 = y - GenerateRnd(blksizey);
		}
		y2 = y1 + blksizey;
	}

	if (FillRoom(x1, y1, x2, y2)) {
		if (FlipCoin(4))
			return;

		if (dir != 2) {
			CreateBlock(x1, y1, blksizey, 0);
		}
		if (dir != 3) {
			CreateBlock(x2, y1, blksizex, 1);
		}
		if (dir != 0) {
			CreateBlock(x1, y2, blksizey, 2);
		}
		if (dir != 1) {
			CreateBlock(x1, y1, blksizex, 3);
		}
	}
}

void FloorArea(int x1, int y1, int x2, int y2)
{
	for (int j = y1; j <= y2; j++) {
		for (int i = x1; i <= x2; i++) {
			dungeon[i][j] = 1;
		}
	}
}

void FillDiagonals()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			const int v = dungeon[i + 1][j + 1] + 2 * dungeon[i][j + 1] + 4 * dungeon[i + 1][j] + 8 * dungeon[i][j];
			if (v == 6) {
				if (FlipCoin()) {
					dungeon[i][j] = 1;
				} else {
					dungeon[i + 1][j + 1] = 1;
				}
			}
			if (v == 9) {
				if (FlipCoin()) {
					dungeon[i + 1][j] = 1;
				} else {
					dungeon[i][j + 1] = 1;
				}
			}
		}
	}
}

void FillSingles()
{
	for (int j = 1; j < DMAXY - 1; j++) {
		for (int i = 1; i < DMAXX - 1; i++) {
			if (dungeon[i][j] == 0
			    && dungeon[i][j - 1] + dungeon[i - 1][j - 1] + dungeon[i + 1][j - 1] == 3
			    && dungeon[i + 1][j] + dungeon[i - 1][j] == 2
			    && dungeon[i][j + 1] + dungeon[i - 1][j + 1] + dungeon[i + 1][j + 1] == 3) {
				dungeon[i][j] = 1;
			}
		}
	}
}

void FillStraights()
{
	int xc;
	int yc;

	for (int j = 0; j < DMAXY - 1; j++) {
		int xs = 0;
		for (int i = 0; i < 37; i++) {
			if (dungeon[i][j] == 0 && dungeon[i][j + 1] == 1) {
				if (xs == 0) {
					xc = i;
				}
				xs++;
			} else {
				if (xs > 3 && !FlipCoin()) {
					for (int k = xc; k < i; k++) {
						const int rv = GenerateRnd(2);
						dungeon[k][j] = rv;
					}
				}
				xs = 0;
			}
		}
	}
	for (int j = 0; j < DMAXY - 1; j++) {
		int xs = 0;
		for (int i = 0; i < 37; i++) {
			if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 0) {
				if (xs == 0) {
					xc = i;
				}
				xs++;
			} else {
				if (xs > 3 && !FlipCoin()) {
					for (int k = xc; k < i; k++) {
						const int rv = GenerateRnd(2);
						dungeon[k][j + 1] = rv;
					}
				}
				xs = 0;
			}
		}
	}
	for (int i = 0; i < DMAXX - 1; i++) {
		int ys = 0;
		for (int j = 0; j < 37; j++) {
			if (dungeon[i][j] == 0 && dungeon[i + 1][j] == 1) {
				if (ys == 0) {
					yc = j;
				}
				ys++;
			} else {
				if (ys > 3 && !FlipCoin()) {
					for (int k = yc; k < j; k++) {
						const int rv = GenerateRnd(2);
						dungeon[i][k] = rv;
					}
				}
				ys = 0;
			}
		}
	}
	for (int i = 0; i < DMAXX - 1; i++) {
		int ys = 0;
		for (int j = 0; j < 37; j++) {
			if (dungeon[i][j] == 1 && dungeon[i + 1][j] == 0) {
				if (ys == 0) {
					yc = j;
				}
				ys++;
			} else {
				if (ys > 3 && !FlipCoin()) {
					for (int k = yc; k < j; k++) {
						const int rv = GenerateRnd(2);
						dungeon[i + 1][k] = rv;
					}
				}
				ys = 0;
			}
		}
	}
}

void Edges()
{
	for (int j = 0; j < DMAXY; j++) {
		dungeon[DMAXX - 1][j] = 0;
	}
	for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
		dungeon[i][DMAXY - 1] = 0;
	}
}

int GetFloorArea()
{
	int gfa = 0;

	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
			gfa += dungeon[i][j];
		}
	}

	return gfa;
}

void MakeMegas()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			int v = dungeon[i + 1][j + 1] + 2 * dungeon[i][j + 1] + 4 * dungeon[i + 1][j] + 8 * dungeon[i][j];
			if (v == 6) {
				v = PickRandomlyAmong({ 12, 5 });
			}
			if (v == 9) {
				v = PickRandomlyAmong({ 13, 14 });
			}
			dungeon[i][j] = L3ConvTbl[v];
		}
		dungeon[DMAXX - 1][j] = 8;
	}
	for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
		dungeon[i][DMAXY - 1] = 8;
	}
}

void River()
{
	int dir;
	int nodir;
	int river[3][100];
	int riveramt;

	int rivercnt = 0;
	int tries = 0;
	/// BUGFIX: pdir is uninitialized, add code `pdir = -1;`(fixed)
	int pdir = -1;

	while (tries < 200 && rivercnt < 4) {
		bool bail = false;
		while (!bail && tries < 200) {
			tries++;
			int rx = 0;
			int ry = 0;
			int i = 0;
			// BUGFIX: Replace with `(ry >= DMAXY || dungeon[rx][ry] < 25 || dungeon[rx][ry] > 28) && i < 100` (fixed)
			while ((ry >= DMAXY || dungeon[rx][ry] < 25 || dungeon[rx][ry] > 28) && i < 100) {
				rx = GenerateRnd(DMAXX);
				ry = GenerateRnd(DMAXY);
				i++;
				// BUGFIX: Move `ry < DMAXY` check before dungeon checks (fixed)
				while (ry < DMAXY && (dungeon[rx][ry] < 25 || dungeon[rx][ry] > 28)) {
					rx++;
					if (rx >= DMAXX) {
						rx = 0;
						ry++;
					}
				}
			}
			// BUGFIX: Continue if `ry >= DMAXY` (fixed)
			if (ry >= DMAXY)
				continue;
			if (i >= 100) {
				return;
			}
			switch (dungeon[rx][ry]) {
			case 25:
				dir = 3;
				nodir = 2;
				river[2][0] = 40;
				break;
			case 26:
				dir = 0;
				nodir = 1;
				river[2][0] = 38;
				break;
			case 27:
				dir = 1;
				nodir = 0;
				river[2][0] = 41;
				break;
			case 28:
				dir = 2;
				nodir = 3;
				river[2][0] = 39;
				break;
			}
			river[0][0] = rx;
			river[1][0] = ry;
			riveramt = 1;
			int nodir2 = 4;
			int dircheck = 0;
			while (dircheck < 4 && riveramt < 100) {
				const int px = rx;
				const int py = ry;
				if (dircheck == 0) {
					dir = GenerateRnd(4);
				} else {
					dir = (dir + 1) & 3;
				}
				dircheck++;
				while (dir == nodir || dir == nodir2) {
					dir = (dir + 1) & 3;
					dircheck++;
				}
				if (dir == 0 && ry > 0) {
					ry--;
				}
				if (dir == 1 && ry < DMAXY) {
					ry++;
				}
				if (dir == 2 && rx < DMAXX) {
					rx++;
				}
				if (dir == 3 && rx > 0) {
					rx--;
				}
				if (dungeon[rx][ry] == 7) {
					dircheck = 0;
					if (dir < 2) {
						river[2][riveramt] = PickRandomlyAmong({ 17, 18 });
					}
					if (dir > 1) {
						river[2][riveramt] = PickRandomlyAmong({ 15, 16 });
					}
					river[0][riveramt] = rx;
					river[1][riveramt] = ry;
					riveramt++;
					if ((dir == 0 && pdir == 2) || (dir == 3 && pdir == 1)) {
						if (riveramt > 2) {
							river[2][riveramt - 2] = 22;
						}
						if (dir == 0) {
							nodir2 = 1;
						} else {
							nodir2 = 2;
						}
					}
					if ((dir == 0 && pdir == 3) || (dir == 2 && pdir == 1)) {
						if (riveramt > 2) {
							river[2][riveramt - 2] = 21;
						}
						if (dir == 0) {
							nodir2 = 1;
						} else {
							nodir2 = 3;
						}
					}
					if ((dir == 1 && pdir == 2) || (dir == 3 && pdir == 0)) {
						if (riveramt > 2) {
							river[2][riveramt - 2] = 20;
						}
						if (dir == 1) {
							nodir2 = 0;
						} else {
							nodir2 = 2;
						}
					}
					if ((dir == 1 && pdir == 3) || (dir == 2 && pdir == 0)) {
						if (riveramt > 2) {
							river[2][riveramt - 2] = 19;
						}
						if (dir == 1) {
							nodir2 = 0;
						} else {
							nodir2 = 3;
						}
					}
					pdir = dir;
				} else {
					rx = px;
					ry = py;
				}
			}
			// BUGFIX: Check `ry >= 2` (fixed)
			if (dir == 0 && ry >= 2 && dungeon[rx][ry - 1] == 10 && dungeon[rx][ry - 2] == 8) {
				river[0][riveramt] = rx;
				river[1][riveramt] = ry - 1;
				river[2][riveramt] = 24;
				if (pdir == 2) {
					river[2][riveramt - 1] = 22;
				}
				if (pdir == 3) {
					river[2][riveramt - 1] = 21;
				}
				bail = true;
			}
			// BUGFIX: Check `ry + 2 < DMAXY` (fixed)
			if (dir == 1 && ry + 2 < DMAXY && dungeon[rx][ry + 1] == 2 && dungeon[rx][ry + 2] == 8) {
				river[0][riveramt] = rx;
				river[1][riveramt] = ry + 1;
				river[2][riveramt] = 42;
				if (pdir == 2) {
					river[2][riveramt - 1] = 20;
				}
				if (pdir == 3) {
					river[2][riveramt - 1] = 19;
				}
				bail = true;
			}
			// BUGFIX: Check `rx + 2 < DMAXX` (fixed)
			if (dir == 2 && rx + 2 < DMAXX && dungeon[rx + 1][ry] == 4 && dungeon[rx + 2][ry] == 8) {
				river[0][riveramt] = rx + 1;
				river[1][riveramt] = ry;
				river[2][riveramt] = 43;
				if (pdir == 0) {
					river[2][riveramt - 1] = 19;
				}
				if (pdir == 1) {
					river[2][riveramt - 1] = 21;
				}
				bail = true;
			}
			// BUGFIX: Check `rx >= 2` (fixed)
			if (dir == 3 && rx >= 2 && dungeon[rx - 1][ry] == 9 && dungeon[rx - 2][ry] == 8) {
				river[0][riveramt] = rx - 1;
				river[1][riveramt] = ry;
				river[2][riveramt] = 23;
				if (pdir == 0) {
					river[2][riveramt - 1] = 20;
				}
				if (pdir == 1) {
					river[2][riveramt - 1] = 22;
				}
				bail = true;
			}
		}
		if (bail && riveramt < 7) {
			bail = false;
		}
		if (bail) {
			int found = 0;
			int lpcnt = 0;
			int bridge;
			while (found == 0 && lpcnt < 30) {
				lpcnt++;
				bridge = GenerateRnd(riveramt);
				if ((river[2][bridge] == 15 || river[2][bridge] == 16)
				    && dungeon[river[0][bridge]][river[1][bridge] - 1] == 7
				    && dungeon[river[0][bridge]][river[1][bridge] + 1] == 7) {
					found = 1;
				}
				if ((river[2][bridge] == 17 || river[2][bridge] == 18)
				    && dungeon[river[0][bridge] - 1][river[1][bridge]] == 7
				    && dungeon[river[0][bridge] + 1][river[1][bridge]] == 7) {
					found = 2;
				}
				for (int i = 0; i < riveramt && found != 0; i++) {
					if (found == 1
					    && (river[1][bridge] - 1 == river[1][i] || river[1][bridge] + 1 == river[1][i])
					    && river[0][bridge] == river[0][i]) {
						found = 0;
					}
					if (found == 2
					    && (river[0][bridge] - 1 == river[0][i] || river[0][bridge] + 1 == river[0][i])
					    && river[1][bridge] == river[1][i]) {
						found = 0;
					}
				}
			}
			if (found != 0) {
				if (found == 1) {
					river[2][bridge] = 44;
				} else {
					river[2][bridge] = 45;
				}
				rivercnt++;
				for (bridge = 0; bridge <= riveramt; bridge++) {
					dungeon[river[0][bridge]][river[1][bridge]] = river[2][bridge];
				}
			} else {
				bail = false;
			}
		}
	}
}

bool Spawn(int x, int y, int *totarea);

bool SpawnEdge(int x, int y, int *totarea)
{
	constexpr uint8_t spawntable[15] = { 0x00, 0x0A, 0x43, 0x05, 0x2c, 0x06, 0x09, 0x00, 0x00, 0x1c, 0x83, 0x06, 0x09, 0x0A, 0x05 };

	if (*totarea > 40) {
		return true;
	}
	if (x < 0 || y < 0 || x >= DMAXX || y >= DMAXY) {
		return true;
	}
	if ((dungeon[x][y] & 0x80) != 0) {
		return false;
	}
	if (dungeon[x][y] > 15) {
		return true;
	}

	const uint8_t i = dungeon[x][y];
	dungeon[x][y] |= 0x80;
	*totarea += 1;

	if ((spawntable[i] & 8) != 0 && SpawnEdge(x, y - 1, totarea)) {
		return true;
	}
	if ((spawntable[i] & 4) != 0 && SpawnEdge(x, y + 1, totarea)) {
		return true;
	}
	if ((spawntable[i] & 2) != 0 && SpawnEdge(x + 1, y, totarea)) {
		return true;
	}
	if ((spawntable[i] & 1) != 0 && SpawnEdge(x - 1, y, totarea)) {
		return true;
	}
	if ((spawntable[i] & 0x80) != 0 && Spawn(x, y - 1, totarea)) {
		return true;
	}
	if ((spawntable[i] & 0x40) != 0 && Spawn(x, y + 1, totarea)) {
		return true;
	}
	if ((spawntable[i] & 0x20) != 0 && Spawn(x + 1, y, totarea)) {
		return true;
	}
	if ((spawntable[i] & 0x10) != 0 && Spawn(x - 1, y, totarea)) {
		return true;
	}

	return false;
}

bool Spawn(int x, int y, int *totarea)
{
	constexpr uint8_t spawntable[15] = { 0x00, 0x0A, 0x03, 0x05, 0x0C, 0x06, 0x09, 0x00, 0x00, 0x0C, 0x03, 0x06, 0x09, 0x0A, 0x05 };

	if (*totarea > 40) {
		return true;
	}
	if (x < 0 || y < 0 || x >= DMAXX || y >= DMAXY) {
		return true;
	}
	if ((dungeon[x][y] & 0x80) != 0) {
		return false;
	}
	if (dungeon[x][y] > 15) {
		return true;
	}

	const uint8_t i = dungeon[x][y];
	dungeon[x][y] |= 0x80;
	*totarea += 1;

	if (i != 8) {
		if ((spawntable[i] & 8) != 0 && SpawnEdge(x, y - 1, totarea)) {
			return true;
		}
		if ((spawntable[i] & 4) != 0 && SpawnEdge(x, y + 1, totarea)) {
			return true;
		}
		if ((spawntable[i] & 2) != 0 && SpawnEdge(x + 1, y, totarea)) {
			return true;
		}
		if ((spawntable[i] & 1) != 0 && SpawnEdge(x - 1, y, totarea)) {
			return true;
		}
	} else {
		if (Spawn(x + 1, y, totarea)) {
			return true;
		}
		if (Spawn(x - 1, y, totarea)) {
			return true;
		}
		if (Spawn(x, y + 1, totarea)) {
			return true;
		}
		if (Spawn(x, y - 1, totarea)) {
			return true;
		}
	}

	return false;
}

bool CanReplaceTile(uint8_t replace, Point tile)
{
	if (replace < 84 || replace > 100) {
		return true;
	}

	// BUGFIX: p2 is a workaround for a bug, only p1 should have been used (fixing this breaks compatibility)
	constexpr auto ComparisonWithBoundsCheck = [](Point p1, Point p2) {
		return (p1.x >= 0 && p1.x < DMAXX && p1.y >= 0 && p1.y < DMAXY)
		    && (p2.x >= 0 && p2.x < DMAXX && p2.y >= 0 && p2.y < DMAXY)
		    && (dungeon[p1.x][p1.y] >= 84 && dungeon[p2.x][p2.y] <= 100);
	};
	if (ComparisonWithBoundsCheck(tile + Direction::NorthWest, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::SouthEast, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::SouthWest, tile + Direction::NorthWest)
	    || ComparisonWithBoundsCheck(tile + Direction::NorthEast, tile + Direction::NorthWest)) {
		return false;
	}

	return true;
}

/**
 * @brief Randomly places the given miniset throughout the dungeon wherever it would fit
 * @return true if at least one instance was placed
 */
bool PlaceMiniSetRandom(const Miniset &miniset, int rndper)
{
	const WorldTileCoord sw = miniset.size.width;
	const WorldTileCoord sh = miniset.size.height;

	bool placed = false;
	for (WorldTileCoord sy = 0; sy < DMAXY - sh; sy++) {
		for (WorldTileCoord sx = 0; sx < DMAXX - sw; sx++) {
			if (!miniset.matches({ sx, sy }))
				continue;
			// BUGFIX: This should not be applied to Nest levels
			if (!CanReplaceTile(miniset.replace[0][0], { sx, sy }))
				continue;
			if (GenerateRnd(100) >= rndper)
				continue;
			miniset.place({ sx, sy });
			placed = true;
		}
	}

	return placed;
}

void PlaceMiniSetRandom1x1(uint8_t search, uint8_t replace, int rndper)
{
	PlaceMiniSetRandom({ { 1, 1 }, { { search } }, { { replace } } }, rndper);
}

bool PlaceSlimePool()
{
	int lavapool = 0;

	if (PlaceMiniSetRandom(HivePattern41, 30))
		lavapool++;
	if (PlaceMiniSetRandom(HivePattern42, 40))
		lavapool++;
	if (PlaceMiniSetRandom(HivePattern39, 50))
		lavapool++;
	if (PlaceMiniSetRandom(HivePattern40, 60))
		lavapool++;

	return lavapool >= 3;
}

/**
 * Flood fills dirt and wall tiles looking for
 * an area of at most 40 tiles and disconnected from the map edge.
 * If it finds one, converts it to lava tiles and return true.
 */
bool PlaceLavaPool()
{
	constexpr uint8_t Poolsub[15] = { 0, 35, 26, 36, 25, 29, 34, 7, 33, 28, 27, 37, 32, 31, 30 };

	bool lavePoolPlaced = false;

	for (int duny = 0; duny < DMAXY; duny++) {
		for (int dunx = 0; dunx < DMAXY; dunx++) {
			if (dungeon[dunx][duny] != 8) {
				continue;
			}
			dungeon[dunx][duny] |= 0x80;
			int totarea = 1;
			bool found = true;
			if (dunx + 1 < DMAXX) {
				found = Spawn(dunx + 1, duny, &totarea);
			}
			if (dunx - 1 > 0 && !found) {
				found = Spawn(dunx - 1, duny, &totarea);
			} else {
				found = true;
			}
			if (duny + 1 < DMAXY && !found) {
				found = Spawn(dunx, duny + 1, &totarea);
			} else {
				found = true;
			}
			if (duny - 1 > 0 && !found) {
				found = Spawn(dunx, duny - 1, &totarea);
			} else {
				found = true;
			}
			const bool placePool = GenerateRnd(100) < 25;
			for (int j = std::max(duny - totarea, 0); j < std::min(duny + totarea, DMAXY); j++) {
				for (int i = std::max(dunx - totarea, 0); i < std::min(dunx + totarea, DMAXX); i++) {
					// BUGFIX: In the following swap the order to first do the
					// index checks and only then access dungeon[i][j] (fixed)
					if ((dungeon[i][j] & 0x80) != 0) {
						dungeon[i][j] &= ~0x80;
						if (totarea > 4 && placePool && !found) {
							const uint8_t k = Poolsub[dungeon[i][j]];
							if (k != 0 && k <= 37) {
								dungeon[i][j] = k;
							}
							lavePoolPlaced = true;
						}
					}
				}
			}
		}
	}

	return lavePoolPlaced;
}

bool PlacePool()
{
	if (leveltype == DTYPE_NEST) {
		return PlaceSlimePool();
	}

	return PlaceLavaPool();
}

/**
 * @brief Fill lava pools correctly, because River() only generates the edges.
 */
void PoolFix()
{
	for (const Point tile : PointsInRectangle(Rectangle { { 1, 1 }, { DMAXX - 2, DMAXY - 2 } })) {
		// Check if the tile is the default dirt ceiling tile
		if (dungeon[tile.x][tile.y] != 8)
			continue;

		for (const Point adjacentTiles : PointsInRectangle(Rectangle { tile - Displacement(1, 1), { 3, 3 } })) {
			const int tileId = dungeon[adjacentTiles.x][adjacentTiles.y];
			// Check if the adjacent tile is a ground lava tile
			if (tileId >= 25 && tileId <= 41) {
				// A ground lava tile can never be directly connected to our ceiling tile.
				// There must always be a kind of transition tile between (from ground to ceiling).
				// That means our tile is part of a lava pool (and was missed in River()), so we should change our tile to a ground lava tile.
				dungeon[tile.x][tile.y] = 33;
				break;
			}
		}
	}
}

bool FenceVerticalUp(int i, int y)
{
	if ((dungeon[i + 1][y] > 152 || dungeon[i + 1][y] < 130)
	    && (dungeon[i - 1][y] > 152 || dungeon[i - 1][y] < 130)) {
		if (IsAnyOf(dungeon[i][y], 7, 10, 126, 129, 134, 136)) {
			return true;
		}
	}

	return false;
}

bool FenceVerticalDown(int i, int y)
{
	if ((dungeon[i + 1][y] > 152 || dungeon[i + 1][y] < 130)
	    && (dungeon[i - 1][y] > 152 || dungeon[i - 1][y] < 130)) {
		if (IsAnyOf(dungeon[i][y], 2, 7, 134, 136)) {
			return true;
		}
	}

	return false;
}

bool FenceHorizontalLeft(int x, int j)
{
	if ((dungeon[x][j + 1] > 152 || dungeon[x][j + 1] < 130)
	    && (dungeon[x][j - 1] > 152 || dungeon[x][j - 1] < 130)) {
		if (IsAnyOf(dungeon[x][j], 7, 9, 121, 124, 135, 137)) {
			return true;
		}
	}

	return false;
}

bool FenceHorizontalRight(int x, int j)
{
	if ((dungeon[x][j + 1] > 152 || dungeon[x][j + 1] < 130)
	    && (dungeon[x][j - 1] > 152 || dungeon[x][j - 1] < 130)) {
		if (IsAnyOf(dungeon[x][j], 4, 7, 135, 137)) {
			return true;
		}
	}

	return false;
}

void AddFenceDoors()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 7) {
				if (dungeon[i - 1][j] <= 152 && dungeon[i - 1][j] >= 130
				    && dungeon[i + 1][j] <= 152 && dungeon[i + 1][j] >= 130) {
					dungeon[i][j] = 146;
					continue;
				}
			}
			if (dungeon[i][j] == 7) {
				if (dungeon[i][j - 1] <= 152 && dungeon[i][j - 1] >= 130
				    && dungeon[i][j + 1] <= 152 && dungeon[i][j + 1] >= 130) {
					dungeon[i][j] = 147;
					continue;
				}
			}
		}
	}
}

void FenceDoorFix()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 146) {
				if (dungeon[i + 1][j] > 152 || dungeon[i + 1][j] < 130
				    || dungeon[i - 1][j] > 152 || dungeon[i - 1][j] < 130) {
					dungeon[i][j] = 7;
					continue;
				}
			}
			if (dungeon[i][j] == 146) {
				if (IsNoneOf(dungeon[i + 1][j], 130, 132, 133, 134, 136, 138, 140) && IsNoneOf(dungeon[i - 1][j], 130, 132, 133, 134, 136, 138, 140)) {
					dungeon[i][j] = 7;
					continue;
				}
			}
			if (dungeon[i][j] == 147) {
				if (dungeon[i][j + 1] > 152 || dungeon[i][j + 1] < 130
				    || dungeon[i][j - 1] > 152 || dungeon[i][j - 1] < 130) {
					dungeon[i][j] = 7;
					continue;
				}
			}
			if (dungeon[i][j] == 147) {
				if (IsNoneOf(dungeon[i][j + 1], 131, 132, 133, 135, 137, 138, 139) && IsNoneOf(dungeon[i][j - 1], 131, 132, 133, 135, 137, 138, 139)) {
					dungeon[i][j] = 7;
					continue;
				}
			}
		}
	}
}

void Fence()
{
	for (int j = 1; j < DMAXY - 1; j++) {     // BUGFIX: Change '0' to '1' (fixed)
		for (int i = 1; i < DMAXX - 1; i++) { // BUGFIX: Change '0' to '1' (fixed)
			if (dungeon[i][j] == 10 && !FlipCoin()) {
				int x = i;
				while (dungeon[x][j] == 10) {
					x++;
				}
				x--;
				if (x - i > 0) {
					dungeon[i][j] = 127;
					for (int xx = i + 1; xx < x; xx++) {
						dungeon[xx][j] = PickRandomlyAmong({ 129, 126 });
					}
					dungeon[x][j] = 128;
				}
			}
			if (dungeon[i][j] == 9 && !FlipCoin()) {
				int y = j;
				while (dungeon[i][y] == 9) {
					y++;
				}
				y--;
				if (y - j > 0) {
					dungeon[i][j] = 123;
					for (int yy = j + 1; yy < y; yy++) {
						dungeon[i][yy] = PickRandomlyAmong({ 124, 121 });
					}
					dungeon[i][y] = 122;
				}
			}
			if (dungeon[i][j] == 11 && dungeon[i + 1][j] == 10 && dungeon[i][j + 1] == 9 && !FlipCoin()) {
				dungeon[i][j] = 125;
				int x = i + 1;
				while (dungeon[x][j] == 10) {
					x++;
				}
				x--;
				for (int xx = i + 1; xx < x; xx++) {
					dungeon[xx][j] = PickRandomlyAmong({ 129, 126 });
				}
				dungeon[x][j] = 128;
				int y = j + 1;
				while (dungeon[i][y] == 9) {
					y++;
				}
				y--;
				for (int yy = j + 1; yy < y; yy++) {
					dungeon[i][yy] = PickRandomlyAmong({ 124, 121 });
				}
				dungeon[i][y] = 122;
			}
		}
	}

	for (WorldTileCoord j = 1; j < DMAXY; j++) {     // BUGFIX: Change '0' to '1' (fixed)
		for (WorldTileCoord i = 1; i < DMAXX; i++) { // BUGFIX: Change '0' to '1' (fixed)
			// note the comma operator is used here to advance the RNG state
			if (dungeon[i][j] == 7 && (DiscardRandomValues(1), !IsNearThemeRoom({ i, j }))) {
				if (FlipCoin()) {
					int y1 = j;
					// BUGFIX: Check `y1 >= 0` first (fixed)
					while (y1 >= 0 && FenceVerticalUp(i, y1)) {
						y1--;
					}
					y1++;
					int y2 = j;
					// BUGFIX: Check `y2 < DMAXY` first (fixed)
					while (y2 < DMAXY && FenceVerticalDown(i, y2)) {
						y2++;
					}
					y2--;
					bool skip = true;
					if (dungeon[i][y1] == 7) {
						skip = false;
					}
					if (dungeon[i][y2] == 7) {
						skip = false;
					}
					if (y2 - y1 > 1 && skip) {
						const int rp = GenerateRnd(y2 - y1 - 1) + y1 + 1;
						for (int y = y1; y <= y2; y++) {
							if (y == rp) {
								continue;
							}
							if (dungeon[i][y] == 7) {
								dungeon[i][y] = PickRandomlyAmong({ 137, 135 });
							}
							if (dungeon[i][y] == 10) {
								dungeon[i][y] = 131;
							}
							if (dungeon[i][y] == 126) {
								dungeon[i][y] = 133;
							}
							if (dungeon[i][y] == 129) {
								dungeon[i][y] = 133;
							}
							if (dungeon[i][y] == 2) {
								dungeon[i][y] = 139;
							}
							if (dungeon[i][y] == 134) {
								dungeon[i][y] = 138;
							}
							if (dungeon[i][y] == 136) {
								dungeon[i][y] = 138;
							}
						}
					}
				} else {
					int x1 = i;
					// BUGFIX: Check `x1 >= 0` first (fixed)
					while (x1 >= 0 && FenceHorizontalLeft(x1, j)) {
						x1--;
					}
					x1++;
					int x2 = i;
					// BUGFIX: Check `x2 < DMAXX` first (fixed)
					while (x2 < DMAXX && FenceHorizontalRight(x2, j)) {
						x2++;
					}
					x2--;
					bool skip = true;
					if (dungeon[x1][j] == 7) {
						skip = false;
					}
					if (dungeon[x2][j] == 7) {
						skip = false;
					}
					if (x2 - x1 > 1 && skip) {
						const int rp = GenerateRnd(x2 - x1 - 1) + x1 + 1;
						for (int x = x1; x <= x2; x++) {
							if (x == rp) {
								continue;
							}
							if (dungeon[x][j] == 7) {
								dungeon[x][j] = PickRandomlyAmong({ 136, 134 });
							}
							if (dungeon[x][j] == 9) {
								dungeon[x][j] = 130;
							}
							if (dungeon[x][j] == 121) {
								dungeon[x][j] = 132;
							}
							if (dungeon[x][j] == 124) {
								dungeon[x][j] = 132;
							}
							if (dungeon[x][j] == 4) {
								dungeon[x][j] = 140;
							}
							if (dungeon[x][j] == 135) {
								dungeon[x][j] = 138;
							}
							if (dungeon[x][j] == 137) {
								dungeon[x][j] = 138;
							}
						}
					}
				}
			}
		}
	}

	AddFenceDoors();
	FenceDoorFix();
}

bool PlaceAnvil()
{
	const std::unique_ptr<uint16_t[]> setPieceData = LoadFileInMem<uint16_t>("levels\\l3data\\anvil.dun");
	// growing the size by 2 to allow a 1 tile border on all sides
	const WorldTileSize areaSize = GetDunSize(setPieceData.get()) + 2;
	WorldTileCoord sx = GenerateRnd(DMAXX - areaSize.width);
	WorldTileCoord sy = GenerateRnd(DMAXY - areaSize.height);

	for (int tries = 0;; tries++, sx++) {
		if (tries > 198)
			return false;

		if (sx == DMAXX - areaSize.width) {
			sx = 0;
			sy++;
			if (sy == DMAXY - areaSize.height) {
				sy = 0;
			}
		}

		bool found = true;
		for (const WorldTilePosition tile : PointsInRectangle(WorldTileRectangle { { sx, sy }, areaSize })) {
			if (Protected.test(tile.x, tile.y) || dungeon[tile.x][tile.y] != 7) {
				found = false;
				break;
			}
		}
		if (found)
			break;
	}

	PlaceDunTiles(setPieceData.get(), { sx + 1, sy + 1 }, 7);
	SetPiece = { { sx, sy }, areaSize };

	for (const WorldTilePosition tile : PointsInRectangle(SetPiece)) {
		Protected.set(tile.x, tile.y);
	}

	// Hack to avoid rivers entering the island, reversed later
	dungeon[SetPiece.position.x + 7][SetPiece.position.y + 5] = 2;
	dungeon[SetPiece.position.x + 8][SetPiece.position.y + 5] = 2;
	dungeon[SetPiece.position.x + 9][SetPiece.position.y + 5] = 2;

	return true;
}

void Warp()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 125 && dungeon[i + 1][j] == 125 && dungeon[i][j + 1] == 125 && dungeon[i + 1][j + 1] == 125) {
				dungeon[i][j] = 156;
				dungeon[i + 1][j] = 155;
				dungeon[i][j + 1] = 153;
				dungeon[i + 1][j + 1] = 154;
				return;
			}
			if (dungeon[i][j] == 5 && dungeon[i + 1][j + 1] == 7) {
				dungeon[i][j] = 7;
			}
		}
	}
}

void HallOfHeroes()
{
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 5 && dungeon[i + 1][j + 1] == 7) {
				dungeon[i][j] = 7;
			}
		}
	}
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == 5 && dungeon[i + 1][j + 1] == 12 && dungeon[i + 1][j] == 7) {
				dungeon[i][j] = 7;
				dungeon[i][j + 1] = 7;
				dungeon[i + 1][j + 1] = 7;
			}
			if (dungeon[i][j] == 5 && dungeon[i + 1][j + 1] == 12 && dungeon[i][j + 1] == 7) {
				dungeon[i][j] = 7;
				dungeon[i + 1][j] = 7;
				dungeon[i + 1][j + 1] = 7;
			}
		}
	}
}

void LockRectangle(int x, int y)
{
	if (!DungeonMask.test(x, y)) {
		return;
	}

	DungeonMask.reset(x, y);
	lockoutcnt++;
	LockRectangle(x, y - 1);
	LockRectangle(x, y + 1);
	LockRectangle(x - 1, y);
	LockRectangle(x + 1, y);
}

bool Lockout()
{
	DungeonMask.reset();

	int fx;
	int fy;

	int t = 0;
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] != 0) {
				DungeonMask.set(i, j);
				fx = i;
				fy = j;
				t++;
			}
		}
	}

	lockoutcnt = 0;
	LockRectangle(fx, fy);

	return t == lockoutcnt;
}

bool PlaceCaveStairs(lvl_entry entry)
{
	std::optional<Point> position;

	// Place stairs up
	position = PlaceMiniSet(L3UP);
	if (!position)
		return false;
	if (entry == ENTRY_MAIN)
		ViewPosition = position->megaToWorld() + Displacement { 1, 3 };

	// Place stairs down
	position = PlaceMiniSet(L3DOWN);
	if (!position)
		return false;
	if (entry == ENTRY_PREV)
		ViewPosition = position->megaToWorld() + Displacement { 3, 1 };

	// Place town warp stairs
	if (currlevel == 9) {
		position = PlaceMiniSet(L3HOLDWARP);
		if (!position)
			return false;
		if (entry == ENTRY_TWARPDN)
			ViewPosition = position->megaToWorld() + Displacement { 1, 3 };
	}

	return true;
}

bool PlaceNestStairs(lvl_entry entry)
{
	std::optional<Point> position;

	// Place stairs up
	position = PlaceMiniSet(currlevel != 17 ? L6UP : L6HOLDWARP);
	if (!position)
		return false;
	if (entry == ENTRY_MAIN || entry == ENTRY_TWARPDN)
		ViewPosition = position->megaToWorld() + Displacement { 1, 3 };

	// Place stairs down
	if (currlevel != 20) {
		position = PlaceMiniSet(L6DOWN);
		if (!position)
			return false;
		if (entry == ENTRY_PREV)
			ViewPosition = position->megaToWorld() + Displacement { 3, 1 };
	}

	return true;
}

bool PlaceStairs(lvl_entry entry)
{
	if (leveltype == DTYPE_NEST) {
		return PlaceNestStairs(entry);
	}

	return PlaceCaveStairs(entry);
}

void GenerateLevel(lvl_entry entry)
{
	if (LevelSeeds[currlevel])
		SetRndSeed(*LevelSeeds[currlevel]);

	while (true) {
		LevelSeeds[currlevel] = GetLCGEngineState();
		InitDungeonFlags();
		int x1 = GenerateRnd(20) + 10;
		int y1 = GenerateRnd(20) + 10;
		int x2 = x1 + 2;
		int y2 = y1 + 2;
		FillRoom(x1, y1, x2, y2);
		CreateBlock(x1, y1, 2, 0);
		CreateBlock(x2, y1, 2, 1);
		CreateBlock(x1, y2, 2, 2);
		CreateBlock(x1, y1, 2, 3);
		if (Quests[Q_ANVIL].IsAvailable()) {
			x1 = GenerateRnd(10) + 10;
			y1 = GenerateRnd(10) + 10;
			x2 = x1 + 12;
			y2 = y1 + 12;
			FloorArea(x1, y1, x2, y2);
		}
		FillDiagonals();
		FillSingles();
		FillStraights();
		FillDiagonals();
		Edges();
		if (GetFloorArea() < 600 || !Lockout())
			continue;
		MakeMegas();
		if (!PlaceStairs(entry))
			continue;
		if (Quests[Q_ANVIL].IsAvailable() && !PlaceAnvil())
			continue;
		if (PlacePool())
			break;
	}

	if (leveltype == DTYPE_NEST) {
		PlaceMiniSetRandom(L6ISLE1, 70);
		PlaceMiniSetRandom(L6ISLE2, 70);
		PlaceMiniSetRandom(L6ISLE3, 30);
		PlaceMiniSetRandom(L6ISLE4, 30);
		PlaceMiniSetRandom(L6ISLE1, 100);
		PlaceMiniSetRandom(L6ISLE2, 100);
		PlaceMiniSetRandom(L6ISLE5, 90);
		PlaceMiniSetRandom1x1(8, 25, 20);
		PlaceMiniSetRandom1x1(8, 26, 20);
		PlaceMiniSetRandom1x1(8, 27, 20);
		PlaceMiniSetRandom1x1(8, 28, 20);
		PlaceMiniSetRandom(HivePattern29, 10);
		PlaceMiniSetRandom(HivePattern30, 15);
		PlaceMiniSetRandom(HivePattern31, 20);
		PlaceMiniSetRandom(HivePattern32, 25);
		PlaceMiniSetRandom(HivePattern33, 30);
		PlaceMiniSetRandom(HivePattern34, 35);
		PlaceMiniSetRandom(HivePattern35, 40);
		PlaceMiniSetRandom(HivePattern36, 45);
		PlaceMiniSetRandom(HivePattern37, 50);
		PlaceMiniSetRandom(HivePattern38, 55);
		PlaceMiniSetRandom(HivePattern38, 10);
		PlaceMiniSetRandom(HivePattern37, 15);
		PlaceMiniSetRandom(HivePattern36, 20);
		PlaceMiniSetRandom(HivePattern35, 25);
		PlaceMiniSetRandom(HivePattern34, 30);
		PlaceMiniSetRandom(HivePattern33, 35);
		PlaceMiniSetRandom(HivePattern32, 40);
		PlaceMiniSetRandom(HivePattern31, 45);
		PlaceMiniSetRandom(HivePattern30, 50);
		PlaceMiniSetRandom(HivePattern29, 55);
		PlaceMiniSetRandom(HivePattern9, 40);
		PlaceMiniSetRandom(HivePattern10, 45);
		PlaceMiniSetRandom1x1(7, 29, 25);
		PlaceMiniSetRandom1x1(7, 30, 25);
		PlaceMiniSetRandom1x1(7, 31, 25);
		PlaceMiniSetRandom1x1(7, 32, 25);
		PlaceMiniSetRandom1x1(9, 33, 25);
		PlaceMiniSetRandom1x1(9, 34, 25);
		PlaceMiniSetRandom1x1(9, 35, 25);
		PlaceMiniSetRandom1x1(9, 36, 25);
		PlaceMiniSetRandom1x1(9, 37, 25);
		PlaceMiniSetRandom1x1(10, 39, 25);
		PlaceMiniSetRandom1x1(10, 40, 25);
		PlaceMiniSetRandom1x1(10, 41, 25);
		PlaceMiniSetRandom1x1(10, 42, 25);
		PlaceMiniSetRandom1x1(10, 43, 25);
		PlaceMiniSetRandom1x1(9, 45, 25);
		PlaceMiniSetRandom1x1(9, 46, 25);
		PlaceMiniSetRandom1x1(10, 47, 25);
		PlaceMiniSetRandom1x1(10, 48, 25);
		PlaceMiniSetRandom1x1(11, 38, 25);
		PlaceMiniSetRandom1x1(11, 44, 25);
		PlaceMiniSetRandom1x1(11, 49, 25);
		PlaceMiniSetRandom1x1(11, 50, 25);
	} else {
		PoolFix();
		Warp();

		PlaceMiniSetRandom(L3ISLE1, 70);
		PlaceMiniSetRandom(L3ISLE2, 70);
		PlaceMiniSetRandom(L3ISLE3, 30);
		PlaceMiniSetRandom(L3ISLE4, 30);
		PlaceMiniSetRandom(L3ISLE1, 100);
		PlaceMiniSetRandom(L3ISLE2, 100);
		PlaceMiniSetRandom(L3ISLE5, 90);

		HallOfHeroes();
		River();

		if (Quests[Q_ANVIL].IsAvailable()) {
			dungeon[SetPiece.position.x + 7][SetPiece.position.y + 5] = 7;
			dungeon[SetPiece.position.x + 8][SetPiece.position.y + 5] = 7;
			dungeon[SetPiece.position.x + 9][SetPiece.position.y + 5] = 7;
			if (dungeon[SetPiece.position.x + 10][SetPiece.position.y + 5] == 17 || dungeon[SetPiece.position.x + 10][SetPiece.position.y + 5] == 18) {
				dungeon[SetPiece.position.x + 10][SetPiece.position.y + 5] = 45;
			}
		}

		DRLG_PlaceThemeRooms(5, 10, 7, 0, false);
		Fence();
		PlaceMiniSetRandom(L3TITE1, 10);
		PlaceMiniSetRandom(L3TITE2, 10);
		PlaceMiniSetRandom(L3TITE3, 10);
		PlaceMiniSetRandom(L3TITE6, 20);
		PlaceMiniSetRandom(L3TITE7, 20);
		PlaceMiniSetRandom(L3TITE8, 20);
		PlaceMiniSetRandom(L3TITE9, 20);
		PlaceMiniSetRandom(L3TITE10, 20);
		PlaceMiniSetRandom(L3TITE11, 30);
		PlaceMiniSetRandom(L3TITE12, 20);
		PlaceMiniSetRandom(L3TITE13, 20);
		PlaceMiniSetRandom(L3CREV1, 30);
		PlaceMiniSetRandom(L3CREV2, 30);
		PlaceMiniSetRandom(L3CREV3, 30);
		PlaceMiniSetRandom(L3CREV4, 30);
		PlaceMiniSetRandom(L3CREV5, 30);
		PlaceMiniSetRandom(L3CREV6, 30);
		PlaceMiniSetRandom(L3CREV7, 30);
		PlaceMiniSetRandom(L3CREV8, 30);
		PlaceMiniSetRandom(L3CREV9, 30);
		PlaceMiniSetRandom(L3CREV10, 30);
		PlaceMiniSetRandom(L3CREV11, 30);
		PlaceMiniSetRandom1x1(7, 106, 25);
		PlaceMiniSetRandom1x1(7, 107, 25);
		PlaceMiniSetRandom1x1(7, 108, 25);
		PlaceMiniSetRandom1x1(9, 109, 25);
		PlaceMiniSetRandom1x1(10, 110, 25);
	}

	memcpy(pdungeon, dungeon, sizeof(pdungeon));
}

void Pass3()
{
	DRLG_LPass3(8 - 1);
}

void PlaceCaveLights()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] >= 55 && dPiece[i][j] <= 146) {
				DoLighting({ i, j }, 7, {});
			} else if (dPiece[i][j] >= 153 && dPiece[i][j] <= 160) {
				DoLighting({ i, j }, 7, {});
			} else if (IsAnyOf(dPiece[i][j], 149, 151)) {
				DoLighting({ i, j }, 7, {});
			}
		}
	}
}

void PlaceHiveLights()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] >= 381 && dPiece[i][j] <= 456) {
				DoLighting({ i, j }, 9, {});
			}
		}
	}
}

void PlaceLights()
{
	if (leveltype == DTYPE_NEST) {
		PlaceHiveLights();
		return;
	}

	PlaceCaveLights();
}

} // namespace

void CreateL3Dungeon(uint32_t rseed, lvl_entry entry)
{
	SetRndSeed(rseed);

	GenerateLevel(entry);

	Pass3();
	PlaceLights();
}

void LoadPreL3Dungeon(const char *path)
{
	memset(dungeon, 8, sizeof(dungeon));

	auto dunData = LoadFileInMem<uint16_t>(path);
	PlaceDunTiles(dunData.get(), { 0, 0 }, 7);

	memcpy(pdungeon, dungeon, sizeof(pdungeon));
}

void LoadL3Dungeon(const char *path, Point spawn)
{
	LoadDungeonBase(path, spawn, 7, 8);

	Pass3();
	PlaceLights();

	if (leveltype == DTYPE_CAVES)
		AddL3Objs(0, 0, MAXDUNX, MAXDUNY);
}

} // namespace devilution
