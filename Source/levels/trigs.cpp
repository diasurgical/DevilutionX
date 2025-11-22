/**
 * @file trigs.cpp
 *
 * Implementation of functionality for triggering events when the player enters an area.
 */
#include "levels/trigs.h"

#include <cmath>
#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo_msg.hpp"
#include "game_mode.hpp"
#include "multi.h"
#include "utils/algorithm/container.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

bool trigflag;
int numtrigs;
TriggerStruct trigs[MAXTRIGGERS];
int TWarpFrom;

namespace {
/** Specifies the dungeon piece IDs which constitute stairways leading down to the cathedral from town. */
const uint16_t TownDownList[] = { 715, 714, 718, 719, 720, 722, 723, 724, 725, 726 };
/** Specifies the dungeon piece IDs which constitute stairways leading down to the catacombs from town. */
const uint16_t TownWarp1List[] = { 1170, 1171, 1172, 1173, 1174, 1175, 1176, 1177, 1178, 1180, 1182, 1184 };
const uint16_t TownCryptList[] = { 1330, 1331, 1332, 1333, 1334, 1335, 1336, 1337 };
const uint16_t TownHiveList[] = { 1306, 1307, 1308, 1309 };
/** Specifies the dungeon piece IDs which constitute stairways leading up from the cathedral. */
const uint16_t L1UpList[] = { 126, 128, 129, 130, 131, 132, 134, 136, 137, 138, 139 };
/** Specifies the dungeon piece IDs which constitute stairways leading down from the cathedral. */
const uint16_t L1DownList[] = { 105, 106, 107, 108, 109, 111, 113, 114, 117 };
/** Specifies the dungeon piece IDs which constitute stairways leading up from the catacombs. */
const uint16_t L2UpList[] = { 265, 266 };
/** Specifies the dungeon piece IDs which constitute stairways leading down from the catacombs. */
const uint16_t L2DownList[] = { 268, 269, 270, 271 };
/** Specifies the dungeon piece IDs which constitute stairways leading up to town from the catacombs. */
const uint16_t L2TWarpUpList[] = { 557, 558 };
/** Specifies the dungeon piece IDs which constitute stairways leading up from the caves. */
const uint16_t L3UpList[] = { 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182 };
/** Specifies the dungeon piece IDs which constitute stairways leading down from the caves. */
const uint16_t L3DownList[] = { 161, 162, 163, 164, 165, 166, 167, 168 };
/** Specifies the dungeon piece IDs which constitute stairways leading up to town from the caves. */
const uint16_t L3TWarpUpList[] = { 181, 547, 548, 549, 550, 551, 552, 553, 554, 555, 556, 557, 558, 559 };
/** Specifies the dungeon piece IDs which constitute stairways leading up from hell. */
const uint16_t L4UpList[] = { 81, 82, 89 };
/** Specifies the dungeon piece IDs which constitute stairways leading down from hell. */
const uint16_t L4DownList[] = { 119, 129, 130, 131, 132 };
/** Specifies the dungeon piece IDs which constitute stairways leading up to town from hell. */
const uint16_t L4TWarpUpList[] = { 420, 421, 428 };
/** Specifies the dungeon piece IDs which constitute stairways leading down to Diablo from hell. */
const uint16_t L4PentaList[] = { 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383 };
const uint16_t L5TWarpUpList[] = { 171, 172, 173, 174, 175, 176, 177, 178, 183 };
const uint16_t L5UpList[] = { 148, 149, 150, 151, 152, 153, 154, 156, 157, 158 };
const uint16_t L5DownList[] = { 124, 125, 128, 130, 131, 134, 135, 139, 141 };
const uint16_t L6TWarpUpList[] = { 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91 };
const uint16_t L6UpList[] = { 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77 };
const uint16_t L6DownList[] = { 56, 57, 58, 59, 60, 61, 62, 63 };
} // namespace

void InitNoTriggers()
{
	numtrigs = 0;
	trigflag = false;
}

bool IsWarpOpen(dungeon_type type)
{
	if (gbIsSpawn)
		return false;

	if (gbIsMultiplayer && type != DTYPE_NEST) // Opening the nest is part of in town quest
		return true;

	const Player &myPlayer = *MyPlayer;

	if (type == DTYPE_CATACOMBS && (myPlayer.pTownWarps & 1) != 0)
		return true;
	if (type == DTYPE_CAVES && (myPlayer.pTownWarps & 2) != 0)
		return true;
	if (type == DTYPE_HELL && (myPlayer.pTownWarps & 4) != 0)
		return true;

	if (gbIsHellfire) {
		if (type == DTYPE_CATACOMBS && myPlayer.getCharacterLevel() >= 10)
			return true;
		if (type == DTYPE_CAVES && myPlayer.getCharacterLevel() >= 15)
			return true;
		if (type == DTYPE_HELL && myPlayer.getCharacterLevel() >= 20)
			return true;
		if (type == DTYPE_NEST && IsAnyOf(Quests[Q_FARMER]._qactive, QUEST_DONE, QUEST_HIVE_DONE))
			return true;
		if (type == DTYPE_CRYPT && Quests[Q_GRAVE]._qactive == QUEST_DONE)
			return true;
	}

	return false;
}

void InitTownTriggers()
{
	numtrigs = 0;

	// Cathedral
	trigs[numtrigs].position = { 25, 29 };
	trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
	numtrigs++;

	if (IsWarpOpen(DTYPE_CATACOMBS)) {
		trigs[numtrigs].position = { 49, 21 };
		trigs[numtrigs]._tmsg = WM_DIABTOWNWARP;
		trigs[numtrigs]._tlvl = 5;
		numtrigs++;
	}
	if (IsWarpOpen(DTYPE_CAVES)) {
		trigs[numtrigs].position = { 17, 69 };
		trigs[numtrigs]._tmsg = WM_DIABTOWNWARP;
		trigs[numtrigs]._tlvl = 9;
		numtrigs++;
	}
	if (IsWarpOpen(DTYPE_HELL)) {
		trigs[numtrigs].position = { 41, 80 };
		trigs[numtrigs]._tmsg = WM_DIABTOWNWARP;
		trigs[numtrigs]._tlvl = 13;
		numtrigs++;
	}
	if (IsWarpOpen(DTYPE_NEST)) {
		trigs[numtrigs].position = { 80, 62 };
		trigs[numtrigs]._tmsg = WM_DIABTOWNWARP;
		trigs[numtrigs]._tlvl = 17;
		numtrigs++;
	}
	if (IsWarpOpen(DTYPE_CRYPT)) {
		trigs[numtrigs].position = { 36, 24 };
		trigs[numtrigs]._tmsg = WM_DIABTOWNWARP;
		trigs[numtrigs]._tlvl = 21;
		numtrigs++;
	}

	trigflag = false;
}

void InitL1Triggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 128) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}
			if (dPiece[i][j] == 114) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitL2Triggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 266 && (!Quests[Q_SCHAMB].IsAvailable() || i != Quests[Q_SCHAMB].position.x || j != Quests[Q_SCHAMB].position.y)) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 558) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABTWARPUP;
				trigs[numtrigs]._tlvl = 0;
				numtrigs++;
			}

			if (dPiece[i][j] == 270) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitL3Triggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 170) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 167) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 548) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABTWARPUP;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitL4Triggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 82) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 421) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABTWARPUP;
				trigs[numtrigs]._tlvl = 0;
				numtrigs++;
			}

			if (dPiece[i][j] == 119) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}
		}
	}

	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 369 && Quests[Q_BETRAYER]._qactive == QUEST_DONE) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitHiveTriggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 65) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 62) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}

			if (dPiece[i][j] == 79) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABTWARPUP;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitCryptTriggers()
{
	numtrigs = 0;
	for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
		for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
			if (dPiece[i][j] == 183) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABTWARPUP;
				trigs[numtrigs]._tlvl = 0;
				numtrigs++;
			}
			if (dPiece[i][j] == 157) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABPREVLVL;
				numtrigs++;
			}
			if (dPiece[i][j] == 125) {
				trigs[numtrigs].position = { i, j };
				trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
				numtrigs++;
			}
		}
	}
	trigflag = false;
}

void InitSKingTriggers()
{
	trigflag = false;
	numtrigs = 1;
	trigs[0].position = { 82, 42 };
	trigs[0]._tmsg = WM_DIABRTNLVL;
}

void InitSChambTriggers()
{
	trigflag = false;
	numtrigs = 1;
	trigs[0].position = { 70, 39 };
	trigs[0]._tmsg = WM_DIABRTNLVL;
}

void InitPWaterTriggers()
{
	trigflag = false;
	numtrigs = 1;
	trigs[0].position = { 30, 83 };
	trigs[0]._tmsg = WM_DIABRTNLVL;
}

void InitVPTriggers()
{
	trigflag = false;
	numtrigs = 1;
	trigs[0].position = { 35, 32 };
	trigs[0]._tmsg = WM_DIABRTNLVL;
}

bool ForceTownTrig()
{
	for (const uint16_t tileId : TownDownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = _("Down to dungeon");
			cursPosition = { 25, 29 };
			return true;
		}
	}

	if (IsWarpOpen(DTYPE_CATACOMBS)) {
		for (const uint16_t tileId : TownWarp1List) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				InfoString = _("Down to catacombs");
				cursPosition = { 49, 21 };
				return true;
			}
		}
	}

	if (IsWarpOpen(DTYPE_CAVES)) {
		for (uint16_t i = 1198; i <= 1219; ++i) {
			if (dPiece[cursPosition.x][cursPosition.y] == i) {
				InfoString = _("Down to caves");
				cursPosition = { 17, 69 };
				return true;
			}
		}
	}

	if (IsWarpOpen(DTYPE_HELL)) {
		for (uint16_t i = 1239; i <= 1254; ++i) {
			if (dPiece[cursPosition.x][cursPosition.y] == i) {
				InfoString = _("Down to hell");
				cursPosition = { 41, 80 };
				return true;
			}
		}
	}

	if (IsWarpOpen(DTYPE_NEST)) {
		for (const uint16_t tileId : TownHiveList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				InfoString = _("Down to Hive");
				cursPosition = { 80, 62 };
				return true;
			}
		}
	}

	if (IsWarpOpen(DTYPE_CRYPT)) {
		for (const uint16_t tileId : TownCryptList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				InfoString = _("Down to Crypt");
				cursPosition = { 36, 24 };
				return true;
			}
		}
	}

	return false;
}

bool ForceL1Trig()
{
	for (const uint16_t tileId : L1UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			if (currlevel > 1)
				InfoString = fmt::format(fmt::runtime(_("Up to level {:d}")), currlevel - 1);
			else
				InfoString = _("Up to town");
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}
	for (const uint16_t tileId : L1DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to level {:d}")), currlevel + 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	return false;
}

bool ForceL2Trig()
{
	for (const uint16_t tileId : L2UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					const int dx = std::abs(trigs[j].position.x - cursPosition.x);
					const int dy = std::abs(trigs[j].position.y - cursPosition.y);
					if (dx < 4 && dy < 4) {
						InfoString = fmt::format(fmt::runtime(_("Up to level {:d}")), currlevel - 1);
						cursPosition = trigs[j].position;
						return true;
					}
				}
			}
		}
	}

	for (const uint16_t tileId : L2DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to level {:d}")), currlevel + 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	if (currlevel == 5) {
		for (const uint16_t tileId : L2TWarpUpList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABTWARPUP) {
						const int dx = std::abs(trigs[j].position.x - cursPosition.x);
						const int dy = std::abs(trigs[j].position.y - cursPosition.y);
						if (dx < 4 && dy < 4) {
							InfoString = _("Up to town");
							cursPosition = trigs[j].position;
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool ForceL3Trig()
{
	for (const uint16_t tileId : L3UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Up to level {:d}")), currlevel - 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					const int dx = std::abs(trigs[j].position.x - cursPosition.x);
					const int dy = std::abs(trigs[j].position.y - cursPosition.y);
					if (dx < 4 && dy < 4) {
						cursPosition = trigs[j].position;
						return true;
					}
				}
			}
		}
	}
	for (const uint16_t tileId : L3DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId
		    || dPiece[cursPosition.x + 1][cursPosition.y] == tileId
		    || dPiece[cursPosition.x + 2][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to level {:d}")), currlevel + 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	if (currlevel == 9) {
		for (const uint16_t tileId : L3TWarpUpList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABTWARPUP) {
						const int dx = std::abs(trigs[j].position.x - cursPosition.x);
						const int dy = std::abs(trigs[j].position.y - cursPosition.y);
						if (dx < 4 && dy < 4) {
							InfoString = _("Up to town");
							cursPosition = trigs[j].position;
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool ForceL4Trig()
{
	for (const uint16_t tileId : L4UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Up to level {:d}")), currlevel - 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	for (const uint16_t tileId : L4DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to level {:d}")), currlevel + 1);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	if (currlevel == 13) {
		for (const uint16_t tileId : L4TWarpUpList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABTWARPUP) {
						const int dx = std::abs(trigs[j].position.x - cursPosition.x);
						const int dy = std::abs(trigs[j].position.y - cursPosition.y);
						if (dx < 4 && dy < 4) {
							InfoString = _("Up to town");
							cursPosition = trigs[j].position;
							return true;
						}
					}
				}
			}
		}
	}

	if (currlevel == 15) {
		for (const uint16_t tileId : L4PentaList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				InfoString = _("Down to Diablo");
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
						cursPosition = trigs[j].position;
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool ForceHiveTrig()
{
	for (const uint16_t tileId : L6UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Up to Nest level {:d}")), currlevel - 17);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}
	for (const uint16_t tileId : L6DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId
		    || dPiece[cursPosition.x + 1][cursPosition.y] == tileId
		    || dPiece[cursPosition.x + 2][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to level {:d}")), currlevel - 15);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}

	if (currlevel == 17) {
		for (const uint16_t tileId : L6TWarpUpList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABTWARPUP) {
						const int dx = std::abs(trigs[j].position.x - cursPosition.x);
						const int dy = std::abs(trigs[j].position.y - cursPosition.y);
						if (dx < 4 && dy < 4) {
							InfoString = _("Up to town");
							cursPosition = trigs[j].position;
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool ForceCryptTrig()
{
	for (const uint16_t tileId : L5UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Up to Crypt level {:d}")), currlevel - 21);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABPREVLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}
	if (dPiece[cursPosition.x][cursPosition.y] == 316) {
		InfoString = _("Cornerstone of the World");
		return true;
	}
	for (const uint16_t tileId : L5DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Down to Crypt level {:d}")), currlevel - 19);
			for (int j = 0; j < numtrigs; j++) {
				if (trigs[j]._tmsg == WM_DIABNEXTLVL) {
					cursPosition = trigs[j].position;
					return true;
				}
			}
		}
	}
	if (currlevel == 21) {
		for (const uint16_t tileId : L5TWarpUpList) {
			if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
				for (int j = 0; j < numtrigs; j++) {
					if (trigs[j]._tmsg == WM_DIABTWARPUP) {
						const int dx = std::abs(trigs[j].position.x - cursPosition.x);
						const int dy = std::abs(trigs[j].position.y - cursPosition.y);
						if (dx < 4 && dy < 4) {
							InfoString = _("Up to town");
							cursPosition = trigs[j].position;
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

void Freeupstairs()
{
	for (int i = 0; i < numtrigs; i++) {
		const int tx = trigs[i].position.x;
		const int ty = trigs[i].position.y;

		for (int yy = -2; yy <= 2; yy++) {
			for (int xx = -2; xx <= 2; xx++) {
				dFlags[tx + xx][ty + yy] |= DungeonFlag::Populated;
			}
		}
	}
}

bool ForceSKingTrig()
{
	for (const uint16_t tileId : L1UpList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Back to Level {:d}")), Quests[Q_SKELKING]._qlevel);
			cursPosition = trigs[0].position;

			return true;
		}
	}

	return false;
}

bool ForceSChambTrig()
{
	for (const uint16_t tileId : L2DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Back to Level {:d}")), Quests[Q_SCHAMB]._qlevel);
			cursPosition = trigs[0].position;

			return true;
		}
	}

	return false;
}

bool ForcePWaterTrig()
{
	for (const uint16_t tileId : L3DownList) {
		if (dPiece[cursPosition.x][cursPosition.y] == tileId) {
			InfoString = fmt::format(fmt::runtime(_("Back to Level {:d}")), Quests[Q_PWATER]._qlevel);
			cursPosition = trigs[0].position;

			return true;
		}
	}

	return false;
}

bool ForceArenaTrig()
{
	const uint16_t *checkList;
	size_t len;
	switch (setlvltype) {
	case DTYPE_TOWN:
		checkList = TownWarp1List;
		len = sizeof(TownWarp1List) / sizeof(TownWarp1List[0]);
		break;
	case DTYPE_CATHEDRAL:
		checkList = L1UpList;
		len = sizeof(L1UpList) / sizeof(L1UpList[0]);
		break;
	case DTYPE_CATACOMBS:
		checkList = L2TWarpUpList;
		len = sizeof(L2TWarpUpList) / sizeof(L2TWarpUpList[0]);
		break;
	case DTYPE_CAVES:
		checkList = L3TWarpUpList;
		len = sizeof(L3TWarpUpList) / sizeof(L3TWarpUpList[0]);
		break;
	case DTYPE_HELL:
		checkList = L4TWarpUpList;
		len = sizeof(L4TWarpUpList) / sizeof(L4TWarpUpList[0]);
		break;
	case DTYPE_NEST:
		checkList = L5TWarpUpList;
		len = sizeof(L5TWarpUpList) / sizeof(L5TWarpUpList[0]);
		break;
	case DTYPE_CRYPT:
		checkList = L6TWarpUpList;
		len = sizeof(L6TWarpUpList) / sizeof(L6TWarpUpList[0]);
		break;
	default:
		return false;
	}
	for (size_t i = 0; i < len; ++i) {
		if (dPiece[cursPosition.x][cursPosition.y] == checkList[i]) {
			InfoString = _("Up to town");
			cursPosition = trigs[0].position;
			return true;
		}
	}

	return false;
}

void CheckTrigForce()
{
	trigflag = false;

	if (ControlMode == ControlTypes::KeyboardAndMouse && GetMainPanel().contains(MousePosition)) {
		return;
	}

	if (!setlevel) {
		switch (leveltype) {
		case DTYPE_TOWN:
			trigflag = ForceTownTrig();
			break;
		case DTYPE_CATHEDRAL:
			trigflag = ForceL1Trig();
			break;
		case DTYPE_CATACOMBS:
			trigflag = ForceL2Trig();
			break;
		case DTYPE_CAVES:
			trigflag = ForceL3Trig();
			break;
		case DTYPE_HELL:
			trigflag = ForceL4Trig();
			break;
		case DTYPE_NEST:
			trigflag = ForceHiveTrig();
			break;
		case DTYPE_CRYPT:
			trigflag = ForceCryptTrig();
			break;
		default:
			break;
		}
		if (leveltype != DTYPE_TOWN && !trigflag) {
			trigflag = ForceQuests();
		}
	} else {
		switch (setlvlnum) {
		case SL_SKELKING:
			trigflag = ForceSKingTrig();
			break;
		case SL_BONECHAMB:
			trigflag = ForceSChambTrig();
			break;
		case SL_POISONWATER:
			trigflag = ForcePWaterTrig();
			break;
		default:
			if (IsArenaLevel(setlvlnum))
				trigflag = ForceArenaTrig();
			break;
		}
	}
}

void CheckTriggers()
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pmode != PM_STAND)
		return;

	for (int i = 0; i < numtrigs; i++) {
		if (myPlayer.position.tile != trigs[i].position) {
			continue;
		}

		switch (trigs[i]._tmsg) {
		case WM_DIABNEXTLVL:
			if (gbIsSpawn && currlevel >= 2) {
				NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, { myPlayer.position.tile.x, myPlayer.position.tile.y + 1 });
				myPlayer.Say(HeroSpeech::NotAChance);
				InitDiabloMsg(EMSG_NOT_IN_SHAREWARE);
			} else {
				StartNewLvl(myPlayer, trigs[i]._tmsg, currlevel + 1);
			}
			break;
		case WM_DIABPREVLVL:
			StartNewLvl(myPlayer, trigs[i]._tmsg, currlevel - 1);
			break;
		case WM_DIABRTNLVL:
			StartNewLvl(myPlayer, trigs[i]._tmsg, GetMapReturnLevel());
			break;
		case WM_DIABTOWNWARP:
			if (gbIsMultiplayer) {
				bool abort = false;
				diablo_message abortflag;

				auto position = myPlayer.position.tile;
				if (trigs[i]._tlvl == 5 && myPlayer.getCharacterLevel() < 8) {
					abort = true;
					position.y += 1;
					abortflag = EMSG_REQUIRES_LVL_8;
				}

				if (IsAnyOf(trigs[i]._tlvl, 9, 17) && myPlayer.getCharacterLevel() < 13) {
					abort = true;
					position.x += 1;
					abortflag = EMSG_REQUIRES_LVL_13;
				}

				if (IsAnyOf(trigs[i]._tlvl, 13, 21) && myPlayer.getCharacterLevel() < 17) {
					abort = true;
					position.y += 1;
					abortflag = EMSG_REQUIRES_LVL_17;
				}

				if (abort) {
					myPlayer.Say(HeroSpeech::ICantGetThereFromHere);

					InitDiabloMsg(abortflag);
					NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, position);
					return;
				}
			}

			StartNewLvl(myPlayer, trigs[i]._tmsg, trigs[i]._tlvl);
			break;
		case WM_DIABTWARPUP:
			TWarpFrom = currlevel;
			StartNewLvl(myPlayer, trigs[i]._tmsg, 0);
			break;
		default:
			app_fatal("Unknown trigger msg");
		}
	}
}

bool EntranceBoundaryContains(Point entrance, Point position)
{
	constexpr Displacement entranceOffsets[7] = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 }, { -2, -1 }, { -1, -2 }, { -2, -2 } };

	return c_any_of(
	    entranceOffsets,
	    [=](Displacement offset) { return entrance + offset == position; });
}

} // namespace devilution
