#include "levels/town.h"

#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/world_tile.hpp"
#include "game_mode.hpp"
#include "levels/drlg_l1.h"
#include "levels/trigs.h"
#include "multi.h"
#include "player.h"
#include "quests.h"
#include "utils/endian_swap.hpp"

namespace devilution {

namespace {

/**
 * @brief Load level data into dPiece
 * @param path Path of dun file
 * @param xi upper left destination
 * @param yy upper left destination
 */
void FillSector(const char *path, int xi, int yy)
{
	auto dunData = LoadFileInMem<uint16_t>(path);

	const WorldTileSize size = GetDunSize(dunData.get());
	const uint16_t *tileLayer = &dunData[2];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		int xx = xi;
		for (WorldTileCoord i = 0; i < size.width; i++) {
			int v1 = 218;
			int v2 = 218;
			int v3 = 218;
			int v4 = 218;

			const int tileId = Swap16LE(tileLayer[j * size.width + i]) - 1;
			if (tileId >= 0) {
				const MegaTile mega = pMegaTiles[tileId];
				v1 = Swap16LE(mega.micro1);
				v2 = Swap16LE(mega.micro2);
				v3 = Swap16LE(mega.micro3);
				v4 = Swap16LE(mega.micro4);
			}

			dPiece[xx + 0][yy + 0] = v1;
			dPiece[xx + 1][yy + 0] = v2;
			dPiece[xx + 0][yy + 1] = v3;
			dPiece[xx + 1][yy + 1] = v4;
			xx += 2;
		}
		yy += 2;
	}
}

/**
 * @brief Load a tile in to dPiece
 * @param xx upper left destination
 * @param yy upper left destination
 * @param t tile id
 */
void FillTile(int xx, int yy, int t)
{
	const MegaTile mega = pMegaTiles[t - 1];

	dPiece[xx + 0][yy + 0] = Swap16LE(mega.micro1);
	dPiece[xx + 1][yy + 0] = Swap16LE(mega.micro2);
	dPiece[xx + 0][yy + 1] = Swap16LE(mega.micro3);
	dPiece[xx + 1][yy + 1] = Swap16LE(mega.micro4);
}

/**
 * @brief Update the map to show the closed hive
 */
void TownCloseHive()
{
	dungeon[35][27] = 18;
	dungeon[36][27] = 63;

	dPiece[78][60] = 0x489;
	dPiece[79][60] = 0x4ea;
	dPiece[78][61] = 0x4eb;
	dPiece[79][61] = 0x4ec;
	dPiece[78][62] = 0x4ed;
	dPiece[79][62] = 0x4ee;
	dPiece[78][63] = 0x4ef;
	dPiece[79][63] = 0x4f0;
	dPiece[78][64] = 0x4f1;
	dPiece[79][64] = 0x4f2;
	dPiece[78][65] = 0x4f3;
	dPiece[80][60] = 0x4f4;
	dPiece[81][60] = 0x4f5;
	dPiece[80][61] = 0x4f6;
	dPiece[81][61] = 0x4f7;
	dPiece[82][60] = 0x4f8;
	dPiece[83][60] = 0x4f9;
	dPiece[82][61] = 0x4fa;
	dPiece[83][61] = 0x4fb;
	dPiece[80][62] = 0x4fc;
	dPiece[81][62] = 0x4fd;
	dPiece[80][63] = 0x4fe;
	dPiece[81][63] = 0x4ff;
	dPiece[80][64] = 0x500;
	dPiece[81][64] = 0x501;
	dPiece[80][65] = 0x502;
	dPiece[81][65] = 0x503;
	dPiece[82][64] = 0x508;
	dPiece[83][64] = 0x509;
	dPiece[82][65] = 0x50a;
	dPiece[83][65] = 0x50b;
	dPiece[82][62] = 0x504;
	dPiece[83][62] = 0x505;
	dPiece[82][63] = 0x506;
	dPiece[83][63] = 0x507;
	dPiece[84][61] = 279;
	dPiece[84][62] = 280;
	dPiece[84][63] = 279;
	dPiece[84][64] = 10;
	dPiece[85][60] = 11;
	dPiece[85][61] = 12;
	dPiece[85][62] = 13;
	dPiece[85][63] = 14;
	dPiece[85][64] = 15;
	dPiece[86][60] = 16;
	dPiece[86][61] = 17;
}

/**
 * @brief Update the map to show the closed grave
 */
void TownCloseGrave()
{
	dPiece[36][21] = 0x52a;
	dPiece[37][21] = 0x52b;
	dPiece[36][22] = 0x52c;
	dPiece[37][22] = 0x52d;
	dPiece[36][23] = 0x52e;
	dPiece[37][23] = 0x52f;
	dPiece[36][24] = 0x530;
	dPiece[37][24] = 0x531;
	dPiece[35][21] = 0x53a;
	dPiece[34][21] = 0x53b;
}

void InitTownPieces()
{
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			if (dPiece[x][y] == 359) {
				dSpecial[x][y] = 1;
			} else if (dPiece[x][y] == 357) {
				dSpecial[x][y] = 2;
			} else if (dPiece[x][y] == 128) {
				dSpecial[x][y] = 6;
			} else if (dPiece[x][y] == 129) {
				dSpecial[x][y] = 7;
			} else if (dPiece[x][y] == 127) {
				dSpecial[x][y] = 8;
			} else if (dPiece[x][y] == 116) {
				dSpecial[x][y] = 9;
			} else if (dPiece[x][y] == 156) {
				dSpecial[x][y] = 10;
			} else if (dPiece[x][y] == 157) {
				dSpecial[x][y] = 11;
			} else if (dPiece[x][y] == 155) {
				dSpecial[x][y] = 12;
			} else if (dPiece[x][y] == 161) {
				dSpecial[x][y] = 13;
			} else if (dPiece[x][y] == 159) {
				dSpecial[x][y] = 14;
			} else if (dPiece[x][y] == 213) {
				dSpecial[x][y] = 15;
			} else if (dPiece[x][y] == 211) {
				dSpecial[x][y] = 16;
			} else if (dPiece[x][y] == 216) {
				dSpecial[x][y] = 17;
			} else if (dPiece[x][y] == 215) {
				dSpecial[x][y] = 18;
			}
		}
	}
}

/**
 * @brief Initialize all of the levels data
 */
void DrlgTPass3()
{
	for (int yy = 0; yy < MAXDUNY; yy += 2) {
		for (int xx = 0; xx < MAXDUNX; xx += 2) {
			dPiece[xx][yy] = 426;
			dPiece[xx + 1][yy] = 426;
			dPiece[xx][yy + 1] = 426;
			dPiece[xx + 1][yy + 1] = 426;
		}
	}

	FillSector("levels\\towndata\\sector1s.dun", 46, 46);
	FillSector("levels\\towndata\\sector2s.dun", 46, 0);
	FillSector("levels\\towndata\\sector3s.dun", 0, 46);
	FillSector("levels\\towndata\\sector4s.dun", 0, 0);

	auto dunData = LoadFileInMem<uint16_t>("levels\\towndata\\automap.dun");
	PlaceDunTiles(dunData.get(), { 0, 0 });

	if (!IsWarpOpen(DTYPE_CATACOMBS)) {
		dungeon[20][7] = 10;
		dungeon[20][6] = 8;
		FillTile(48, 20, 320);
	}
	if (!IsWarpOpen(DTYPE_CAVES)) {
		dungeon[4][30] = 8;
		FillTile(16, 68, 332);
		FillTile(16, 70, 331);
	}
	if (!IsWarpOpen(DTYPE_HELL)) {
		dungeon[15][35] = 7;
		dungeon[16][35] = 7;
		dungeon[17][35] = 7;
		for (int x = 36; x < 46; x++) {
			FillTile(x, 78, PickRandomlyAmong({ 1, 2, 3, 4 }));
		}
	}
	if (gbIsHellfire) {
		if (IsWarpOpen(DTYPE_NEST)) {
			TownOpenHive();
		} else {
			TownCloseHive();
		}
		if (IsWarpOpen(DTYPE_CRYPT))
			TownOpenGrave();
		else
			TownCloseGrave();
	}

	if (Quests[Q_PWATER]._qactive != QUEST_DONE && Quests[Q_PWATER]._qactive != QUEST_NOTAVAIL) {
		FillTile(60, 70, 342);
	} else {
		FillTile(60, 70, 71);
	}

	InitTownPieces();
}

} // namespace

bool OpensHive(Point position)
{
	const int yp = position.y;
	const int xp = position.x;
	return xp >= 79 && xp <= 82 && yp >= 61 && yp <= 64;
}

bool OpensGrave(Point position)
{
	const int yp = position.y;
	const int xp = position.x;
	return xp >= 35 && xp <= 38 && yp >= 20 && yp <= 24;
}

void OpenHive()
{
	NetSendCmd(false, CMD_OPENHIVE);
	auto &quest = Quests[Q_FARMER];
	quest._qactive = QUEST_DONE;
	if (gbIsMultiplayer)
		NetSendCmdQuest(true, quest);
}

void OpenGrave()
{
	NetSendCmd(false, CMD_OPENGRAVE);
	auto &quest = Quests[Q_GRAVE];
	quest._qactive = QUEST_DONE;
	if (gbIsMultiplayer)
		NetSendCmdQuest(true, quest);
}

void TownOpenHive()
{
	dungeon[36][27] = 47;

	dPiece[78][60] = 0x489;
	dPiece[79][60] = 0x48a;
	dPiece[78][61] = 0x48b;
	dPiece[79][61] = 0x50d;
	dPiece[78][62] = 0x4ed;
	dPiece[78][63] = 0x4ef;
	dPiece[79][62] = 0x50f;
	dPiece[79][63] = 0x510;
	dPiece[79][64] = 0x511;
	dPiece[78][64] = 0x119;
	dPiece[78][65] = 0x11b;
	dPiece[79][65] = 0x11c;
	dPiece[80][60] = 0x512;
	dPiece[80][61] = 0x514;
	dPiece[81][61] = 0x515;
	dPiece[82][60] = 0x516;
	dPiece[83][60] = 0x517;
	dPiece[82][61] = 0x518;
	dPiece[83][61] = 0x519;
	dPiece[80][62] = 0x51a;
	dPiece[81][62] = 0x51b;
	dPiece[80][63] = 0x51c;
	dPiece[81][63] = 0x51d;
	dPiece[80][64] = 0x51e;
	dPiece[81][64] = 0x51f;
	dPiece[80][65] = 0x520;
	dPiece[81][65] = 0x521;
	dPiece[82][64] = 0x526;
	dPiece[83][64] = 0x527;
	dPiece[82][65] = 0x528;
	dPiece[83][65] = 0x529;
	dPiece[82][62] = 0x522;
	dPiece[83][62] = 0x523;
	dPiece[82][63] = 0x524;
	dPiece[83][63] = 0x525;
	dPiece[84][61] = 279;
	dPiece[84][62] = 280;
	dPiece[84][63] = 279;
	dPiece[84][64] = 10;
	dPiece[85][60] = 11;
	dPiece[85][61] = 12;
	dPiece[85][62] = 13;
	dPiece[85][63] = 14;
	dPiece[85][64] = 15;
	dPiece[86][60] = 16;
	dPiece[86][61] = 17;
}

void TownOpenGrave()
{
	dungeon[14][8] = 47;
	dungeon[14][7] = 47;

	dPiece[36][21] = 0x532;
	dPiece[37][21] = 0x533;
	dPiece[36][22] = 0x534;
	dPiece[37][22] = 0x535;
	dPiece[36][23] = 0x536;
	dPiece[37][23] = 0x537;
	dPiece[36][24] = 0x538;
	dPiece[37][24] = 0x539;
	dPiece[35][21] = 0x53a;
	dPiece[34][21] = 0x53b;
}

void CleanTownFountain()
{
	if (!pMegaTiles)
		return;
	FillTile(60, 70, 71);
}

void CreateTown(lvl_entry entry)
{
	dminPosition = { 10, 10 };
	dmaxPosition = { 84, 84 };

	if (entry == ENTRY_MAIN) { // New game
		ViewPosition = { 75, 68 };
	} else if (entry == ENTRY_PREV) { // Cathedral
		ViewPosition = { 25, 31 };
	} else if (entry == ENTRY_TWARPUP) {
		if (TWarpFrom == 5) {
			ViewPosition = { 49, 22 };
		}
		if (TWarpFrom == 9) {
			ViewPosition = { 18, 69 };
		}
		if (TWarpFrom == 13) {
			ViewPosition = { 41, 81 };
		}
		if (TWarpFrom == 21) {
			ViewPosition = { 36, 25 };
		}
		if (TWarpFrom == 17) {
			ViewPosition = { 79, 62 };
		}
	}

	DrlgTPass3();
}

} // namespace devilution
