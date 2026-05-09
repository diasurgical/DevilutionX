#include "levels/crypt.h"

#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "items.h"
#include "levels/drlg_l1.h"
#include "lighting.h"

namespace devilution {

int UberRow;
int UberCol;
bool IsUberRoomOpened;
bool IsUberLeverActivated;
int UberDiabloMonsterIndex;

/** Miniset: stairs up. */
const Miniset L5STAIRSUP {
	{ 4, 4 },
	{
	    { 22, 22, 22, 22 },
	    { 2, 2, 2, 2 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 0, 66, 23, 0 },
	    { 63, 64, 65, 0 },
	    { 0, 67, 68, 0 },
	    { 0, 0, 0, 0 },
	}
};

namespace {

const Miniset L5STAIRSUPHF {
	{ 4, 5 },
	{
	    { 22, 22, 22, 22 },
	    { 22, 22, 22, 22 },
	    { 2, 2, 2, 2 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 0, 54, 23, 0 },
	    { 0, 53, 18, 0 },
	    { 55, 56, 57, 0 },
	    { 58, 59, 60, 0 },
	    { 0, 0, 0, 0 },
	}
};
const Miniset L5STAIRSDOWN {
	{ 4, 5 },
	{
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 0, 0, 52, 0 },
	    { 0, 48, 51, 0 },
	    { 0, 47, 50, 0 },
	    { 45, 46, 49, 0 },
	    { 0, 0, 0, 0 },
	}
};
const Miniset L5STAIRSTOWN {
	{ 4, 5 },
	{
	    { 22, 22, 22, 22 },
	    { 22, 22, 22, 22 },
	    { 2, 2, 2, 2 },
	    { 13, 13, 13, 13 },
	    { 13, 13, 13, 13 },
	},
	{
	    { 0, 62, 23, 0 },
	    { 0, 61, 18, 0 },
	    { 63, 64, 65, 0 },
	    { 66, 67, 68, 0 },
	    { 0, 0, 0, 0 },
	}
};
const Miniset VWallSection {
	{ 1, 3 },
	{
	    { 1 },
	    { 1 },
	    { 1 },
	},
	{
	    { 91 },
	    { 90 },
	    { 89 },
	}
};
const Miniset HWallSection {
	{ 3, 1 },
	{ { 2, 2, 2 } },
	{ { 94, 93, 92 } }
};
const Miniset CryptFloorLave {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 101, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptPillar1 {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 167, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptPillar2 {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 168, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptPillar3 {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 169, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptPillar4 {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 170, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptPillar5 {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 171, 0 },
	    { 0, 0, 0 },
	}
};
const Miniset CryptStar {
	{ 3, 3 },
	{
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	    { 13, 13, 13 },
	},
	{
	    { 0, 0, 0 },
	    { 0, 172, 0 },
	    { 0, 0, 0 },
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
	Pillar1        =  16,
	Pillar2        =  17,
	DirtHwall      =  18,
	DirtVwall      =  19,
	DirtCorner     =  21,
	DirtHWallEnd   =  23,
	DirtVWallEnd   =  24,
	VDoor          =  25,
	HDoor          =  26,
	HFenceVWall    =  27,
	HDoorVDoor     =  28,
	DFence         =  29,
	VDoorEnd       =  30,
	HDoorEnd       =  31,
	VFenceEnd      =  32,
	VFence         =  35,
	HFence         =  36,
	HWallVFence    =  37,
	HArchVFence    =  38,
	HArchVDoor     =  39,
	EntranceStairs =  64,
	DirtHWall2     =  82,
	DirtVWall2     =  83,
	DirtCorner2    =  85,
	DirtHWallEnd2  =  87,
	DirtVWallEnd2  =  88,
	VWall5         =  89,
	VWall6         =  90,
	VWall7         =  91,
	HWall5         =  92,
	HWall6         =  93,
	HWall7         =  94,
	VArch5         =  95,
	HArch5         =  96,
	Floor6         =  97,
	Floor7         =  98,
	Floor8         =  99,
	Floor9         = 100,
	Floor10        = 101,
	VWall2         = 112,
	HWall2         = 113,
	Corner2        = 114,
	DWall2         = 115,
	DArch2         = 116,
	VWallEnd2      = 117,
	HWallEnd2      = 118,
	HArchEnd2      = 119,
	VArchEnd2      = 120,
	HArchVWall2    = 121,
	VArch2         = 122,
	HArch2         = 123,
	Floor2         = 124,
	HWallVArch2    = 125,
	Pillar3        = 126,
	Pillar4        = 127,
	Pillar5        = 128,
	VWall3         = 129,
	HWall3         = 130,
	Corner3        = 131,
	DWall3         = 132,
	DArch3         = 133,
	VWallEnd3      = 134,
	HWallEnd3      = 135,
	HArchEnd3      = 136,
	VArchEnd3      = 137,
	HArchVWall3    = 138,
	VArch3         = 139,
	HArch3         = 140,
	Floor3         = 141,
	HWallVArch3    = 142,
	Pillar6        = 143,
	Pillar7        = 144,
	Pillar8        = 145,
	VWall4         = 146,
	HWall4         = 147,
	Corner4        = 148,
	DWall4         = 149,
	DArch4         = 150,
	VWallEnd4      = 151,
	HWallEnd4      = 152,
	HArchEnd4      = 153,
	VArchEnd4      = 154,
	HArchVWall4    = 155,
	VArch4         = 156,
	HArch4         = 157,
	Floor4         = 158,
	HWallVArch4    = 159,
	Pillar9        = 160,
	Pillar10       = 161,
	Pillar11       = 162,
	Floor11        = 163,
	Floor12        = 164,
	Floor13        = 165,
	Floor14        = 166,
	PillarHalf     = 167,
	VWall8         = 173,
	VWall9         = 174,
	VWall10        = 175,
	VWall11        = 176,
	VWall12        = 177,
	VWall13        = 178,
	HWall8         = 179,
	HWall9         = 180,
	HWall10        = 181,
	HWall11        = 182,
	HWall12        = 183,
	HWall13        = 184,
	VArch6         = 185,
	VArch7         = 186,
	HArch6         = 187,
	HArch7         = 188,
	Floor15        = 189,
	Floor16        = 190,
	Floor17        = 191,
	Pillar12       = 192,
	Floor18        = 193,
	Floor19        = 194,
	Floor20        = 195,
	Floor21        = 196,
	Floor22        = 197,
	Floor23        = 198,
	VDemon         = 199,
	HDemon         = 200,
	VSuccubus      = 201,
	HSuccubus      = 202,
	Shadow1        = 203,
	Shadow2        = 204,
	Shadow3        = 205,
	Shadow4        = 206,
	Shadow5        = 207,
	Shadow6        = 208,
	Shadow7        = 209,
	Shadow8        = 210,
	Shadow9        = 211,
	Shadow10       = 212,
	Shadow11       = 213,
	Shadow12       = 214,
	Shadow13       = 215,
	Shadow14       = 216,
	Shadow15       = 217,
	// clang-format on
};

struct ReplaceTile {
	Tile search;
	Tile replace;
};

const ReplaceTile Statues[] {
	// clang-format off
	{ VWall, VDemon    },
	{ VWall, VSuccubus },
	{ HWall, HDemon    },
	{ HWall, HSuccubus },
	// clang-format on
};

const ReplaceTile CrackedTiles[] {
	// clang-format off
	{ VWall,      VWall2      },
	{ HWall,      HWall2      },
	{ Corner,     Corner2     },
	{ DWall,      DWall2      },
	{ DArch,      DArch2      },
	{ VWallEnd,   VWallEnd2   },
	{ HWallEnd,   HWallEnd2   },
	{ HArchEnd,   HArchEnd2   },
	{ VArchEnd,   VArchEnd2   },
	{ HArchVWall, HArchVWall2 },
	{ VArch,      VArch2      },
	{ HArch,      HArch2      },
	{ Floor,      Floor2      },
	{ HWallVArch, HWallVArch2 },
	{ Pillar,     Pillar3     },
	{ Pillar1,    Pillar4     },
	{ Pillar2,    Pillar5     },
	// clang-format on
};

const ReplaceTile BrokenTiles[] {
	// clang-format off
	{ VWall,      VWall3      },
	{ HWall,      HWall3      },
	{ Corner,     Corner3     },
	{ DWall,      DWall3      },
	{ DArch,      DArch3      },
	{ VWallEnd,   VWallEnd3   },
	{ HWallEnd,   HWallEnd3   },
	{ HArchEnd,   HArchEnd3   },
	{ VArchEnd,   VArchEnd3   },
	{ HArchVWall, HArchVWall3 },
	{ VArch,      VArch3      },
	{ HArch,      HArch3      },
	{ Floor,      Floor3      },
	{ HWallVArch, HWallVArch3 },
	{ Pillar,     Pillar6     },
	{ Pillar1,    Pillar7     },
	{ Pillar2,    Pillar8     },
	// clang-format on
};

const ReplaceTile LeakingTiles[] {
	// clang-format off
	{ VWall,      VWall4      },
	{ HWall,      HWall4      },
	{ Corner,     Corner4     },
	{ DWall,      DWall4      },
	{ DArch,      DArch4      },
	{ VWallEnd,   VWallEnd4   },
	{ HWallEnd,   HWallEnd4   },
	{ HArchEnd,   HArchEnd4   },
	{ VArchEnd,   VArchEnd4   },
	{ HArchVWall, HArchVWall4 },
	{ VArch,      VArch4      },
	{ HArch,      HArch4      },
	{ Floor,      Floor4      },
	{ HWallVArch, HWallVArch4 },
	{ Pillar,     Pillar9     },
	{ Pillar1,    Pillar10    },
	{ Pillar2,    Pillar11    },
	// clang-format on
};

const ReplaceTile Substitions1Tiles[] {
	// clang-format off
	{ VArch,   VArch6   },
	{ HArch,   HArch6   },
	{ VArch,   VArch7   },
	{ HArch,   HArch7   },
	{ VWall5,  VWall8   },
	{ VWall5,  VWall9   },
	{ VWall6,  VWall10  },
	{ VWall6,  VWall11  },
	{ VWall7,  VWall12  },
	{ VWall7,  VWall13  },
	{ HWall5,  HWall8   },
	{ HWall5,  HWall9   },
	{ HWall5,  HWall10  },
	{ HWall5,  HWall11  },
	{ HWall5,  HWall12  },
	{ HWall5,  HWall13  },
	{ Floor7,  Floor15  },
	{ Floor7,  Floor16  },
	{ Floor6,  Floor17  },
	{ Pillar,  Pillar12 },
	{ Floor8,  Floor18  },
	{ Floor8,  Floor19  },
	{ Floor9,  Floor20  },
	{ Floor10, Floor21  },
	{ Floor10, Floor22  },
	{ Floor10, Floor23  },
	// clang-format on
};

const ReplaceTile Substition1Floor[] {
	// clang-format off
	{ Floor, Floor11 },
	{ Floor, Floor12 },
	{ Floor, Floor13 },
	{ Floor, Floor14 },
	// clang-format on
};

const ReplaceTile Substition2Floor[] {
	// clang-format off
	{ Floor, Floor6 },
	{ Floor, Floor7 },
	{ Floor, Floor8 },
	{ Floor, Floor9 },
	// clang-format on
};

void ApplyCryptShadowsPatterns()
{
	for (int j = 1; j < DMAXY; j++) {
		for (int i = 1; i < DMAXX; i++) {
			switch (dungeon[i][j]) {
			case DArch:
			case DArch2:
			case DArch3:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow1;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow2;
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow3;
				break;
			case HWallEnd:
			case HWallEnd2:
			case HWallEnd3:
			case HWallEnd4:
			case Pillar:
			case Pillar2:
			case Pillar3:
			case Pillar5:
			case Pillar9:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow4;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow5;
				break;
			case HArchEnd:
			case HArchEnd2:
			case HArchEnd3:
			case HArchEnd4:
			case HWallVArch:
			case HWallVArch2:
			case HWallVArch3:
			case HWallVArch4:
			case VArch:
			case VArch4:
			case VArch5:
			case VArch6:
			case VArch7:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow1;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow2;
				break;
			case VArchEnd:
			case VArchEnd2:
			case VArchEnd4:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow4;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow5;
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow3;
				break;
			case HArch:
			case HArch2:
			case HArchVWall:
			case HArchVWall2:
			case HArchVWall3:
			case HArchVWall4:
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow3;
				break;
			case HArch5:
			case HArch6:
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow6;
				break;
			case VArch2:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow9;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow10;
				break;
			case VArchEnd3:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow11;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow12;
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow3;
				break;
			case VArch3:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow13;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow14;
				break;
			case HArch3:
			case HArch4:
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow15;
				break;
			case Pillar6:
			case Pillar8:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow11;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow12;
				break;
			case DArch4:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow1;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow2;
				if (dungeon[i][j - 1] == Floor)
					dungeon[i][j - 1] = Shadow15;
				break;
			case Pillar11:
			case Pillar12:
			case PillarHalf:
				if (dungeon[i - 1][j] == Floor)
					dungeon[i - 1][j] = Shadow7;
				if (dungeon[i - 1][j - 1] == Floor)
					dungeon[i - 1][j - 1] = Shadow8;
				break;
			}
		}
	}
}

void PlaceMiniSetRandom1x1(uint8_t search, uint8_t replace, int rndper)
{
	PlaceMiniSetRandom({ { 1, 1 }, { { search } }, { { replace } } }, rndper);
}

void CryptCracked(int rndper)
{
	for (const ReplaceTile pair : CrackedTiles) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

void CryptBroken(int rndper)
{
	for (const ReplaceTile pair : BrokenTiles) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

void CryptLeaking(int rndper)
{
	for (const ReplaceTile pair : LeakingTiles) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

void CryptSubstitions1(int rndper)
{
	for (const ReplaceTile pair : Substitions1Tiles) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

void CryptSubstitions2(int rndper)
{
	PlaceMiniSetRandom(CryptPillar1, rndper);
	PlaceMiniSetRandom(CryptPillar2, rndper);
	PlaceMiniSetRandom(CryptPillar3, rndper);
	PlaceMiniSetRandom(CryptPillar4, rndper);
	PlaceMiniSetRandom(CryptPillar5, rndper);
	PlaceMiniSetRandom(CryptStar, rndper);

	for (const ReplaceTile pair : Substition1Floor) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

void CryptFloor(int rndper)
{
	for (const ReplaceTile pair : Substition2Floor) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, rndper);
	}
}

} // namespace

void InitCryptPieces()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 76) {
				dSpecial[i][j] = 1;
			} else if (dPiece[i][j] == 79) {
				dSpecial[i][j] = 2;
			}
		}
	}
}

void SetCryptRoom()
{
	const Point position = SelectChamber();

	UberRow = 2 * position.x + 6;
	UberCol = 2 * position.y + 8;
	IsUberRoomOpened = false;
	IsUberLeverActivated = false;

	auto dunData = LoadFileInMem<uint16_t>("nlevels\\l5data\\uberroom.dun");

	SetPiece = { position, GetDunSize(dunData.get()) };

	PlaceDunTiles(dunData.get(), position, 0);
}

void SetCornerRoom()
{
	const Point position = SelectChamber();

	auto dunData = LoadFileInMem<uint16_t>("nlevels\\l5data\\cornerstone.dun");

	SetPiece = { position, GetDunSize(dunData.get()) };

	PlaceDunTiles(dunData.get(), position, 0);
}

void FixCryptDirtTiles()
{
	for (int j = 0; j < DMAXY - 1; j++) {
		for (int i = 0; i < DMAXX - 1; i++) {
			if (dungeon[i][j] == DirtVwall)
				dungeon[i][j] = DirtVWall2;
			if (dungeon[i][j] == DirtCorner)
				dungeon[i][j] = DirtCorner2;
			if (dungeon[i][j] == DirtHWallEnd)
				dungeon[i][j] = DirtHWallEnd2;
			if (dungeon[i][j] == DirtVWallEnd)
				dungeon[i][j] = DirtVWallEnd2;
			if (dungeon[i][j] == DirtHwall)
				dungeon[i][j] = DirtHWall2;
		}
	}
}

bool PlaceCryptStairs(lvl_entry entry)
{
	bool success = true;
	std::optional<Point> position;

	// Place stairs up
	position = PlaceMiniSet(currlevel != 21 ? L5STAIRSUPHF : L5STAIRSTOWN, DMAXX * DMAXY, true);
	if (!position) {
		success = false;
	} else if (entry == ENTRY_MAIN || entry == ENTRY_TWARPDN) {
		ViewPosition = position->megaToWorld() + Displacement { 3, 5 };
	}

	// Place stairs down
	if (currlevel != 24) {
		position = PlaceMiniSet(L5STAIRSDOWN, DMAXX * DMAXY, true);
		if (!position)
			success = false;
		else if (entry == ENTRY_PREV)
			ViewPosition = position->megaToWorld() + Displacement { 3, 7 };
	}

	return success;
}

void CryptSubstitution()
{
	for (const ReplaceTile pair : Statues) {
		PlaceMiniSetRandom1x1(pair.search, pair.replace, 10);
	}
	PlaceMiniSetRandom1x1(VArch, VArch5, 95);
	PlaceMiniSetRandom1x1(HArch, HArch5, 95);
	PlaceMiniSetRandom(VWallSection, 100);
	PlaceMiniSetRandom(HWallSection, 100);
	PlaceMiniSetRandom(CryptFloorLave, 60);
	ApplyCryptShadowsPatterns();
	switch (currlevel) {
	case 21:
		CryptCracked(30);
		CryptBroken(15);
		CryptLeaking(5);
		ApplyCryptShadowsPatterns();
		CryptFloor(10);
		CryptSubstitions1(5);
		CryptSubstitions2(20);
		break;
	case 22:
		CryptFloor(10);
		CryptSubstitions1(10);
		CryptSubstitions2(20);
		CryptCracked(30);
		CryptBroken(20);
		CryptLeaking(10);
		ApplyCryptShadowsPatterns();
		break;
	case 23:
		CryptFloor(10);
		CryptSubstitions1(15);
		CryptSubstitions2(30);
		CryptCracked(30);
		CryptBroken(20);
		CryptLeaking(15);
		ApplyCryptShadowsPatterns();
		break;
	default:
		CryptFloor(10);
		CryptSubstitions1(20);
		CryptSubstitions2(30);
		CryptCracked(30);
		CryptBroken(20);
		CryptLeaking(20);
		ApplyCryptShadowsPatterns();
		break;
	}
}

void SetCryptSetPieceRoom()
{
	for (int j = dminPosition.y; j < dmaxPosition.y; j++) {
		for (int i = dminPosition.x; i < dmaxPosition.x; i++) {
			if (dPiece[i][j] == 289) {
				UberRow = i;
				UberCol = j;
			}
			if (dPiece[i][j] == 316) {
				CornerStone.position = { i, j };
			}
		}
	}
}

void PlaceCryptLights()
{
	constexpr int lavaTiles[] = {
		124, 128, 130, 132, 133, 134, 135, 139, 141, 143, 145, 156, 164, 166,
		167, 168, 169, 170, 182, 190, 192, 195, 196, 199, 200, 254, 266, 273,
		276, 281, 282, 283, 284, 285, 286, 287, 288, 290, 302, 316, 434, 435,
		436, 437, 445, 446, 447, 453, 457, 460, 466, 470, 477, 479, 484, 485,
		486, 490, 507, 537, 557, 559, 561, 563, 564, 568, 569, 572, 578, 580,
		584, 585, 589, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601
	};

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			for (const int lavaTile : lavaTiles) {
				if (dPiece[i][j] == lavaTile) {
					DoLighting({ i, j }, 3, {});
					break;
				}
			}
		}
	}
}

} // namespace devilution
