#include "levels/setmaps.h"

#include <cstdint>

#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/load_file.hpp"
#include "engine/palette.h"
#include "levels/drlg_l1.h"
#include "levels/drlg_l2.h"
#include "levels/drlg_l3.h"
#include "levels/drlg_l4.h"
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "msg.h"
#include "objdat.h"
#include "objects.h"
#include "quests.h"
#include "utils/language.h"

namespace devilution {

/** Maps from quest level to quest level names. */
const char *const QuestLevelNames[] = {
	"",
	N_("Skeleton King's Lair"),
	N_("Chamber of Bone"),
	N_("Maze"),
	N_("Poisoned Water Supply"),
	N_("Archbishop Lazarus' Lair"),
	N_("Church Arena"),
	N_("Hell Arena"),
	N_("Circle of Life Arena"),
};

namespace {

void AddSKingObjs()
{
	constexpr WorldTileRectangle SmallSecretRoom { { 20, 7 }, { 3, 3 } };
	ObjectAtPosition({ 64, 34 }).InitializeLoadedObject(SmallSecretRoom, 1);

	constexpr WorldTileRectangle Gate { { 20, 14 }, { 1, 2 } };
	ObjectAtPosition({ 64, 59 }).InitializeLoadedObject(Gate, 2);

	constexpr WorldTileRectangle LargeSecretRoom { { 8, 1 }, { 7, 10 } };
	ObjectAtPosition({ 27, 37 }).InitializeLoadedObject(LargeSecretRoom, 3);
	ObjectAtPosition({ 46, 35 }).InitializeLoadedObject(LargeSecretRoom, 3);
	ObjectAtPosition({ 49, 53 }).InitializeLoadedObject(LargeSecretRoom, 3);
	ObjectAtPosition({ 27, 53 }).InitializeLoadedObject(LargeSecretRoom, 3);
}

void AddSChamObjs()
{
	ObjectAtPosition({ 37, 30 }).InitializeLoadedObject({ { 17, 0 }, { 4, 5 } }, 1);
	ObjectAtPosition({ 37, 46 }).InitializeLoadedObject({ { 13, 0 }, { 3, 5 } }, 2);
}

void AddVileObjs()
{
	ObjectAtPosition({ 26, 45 }).InitializeLoadedObject({ { 1, 1 }, { 8, 9 } }, 1);
	ObjectAtPosition({ 45, 46 }).InitializeLoadedObject({ { 11, 1 }, { 9, 9 } }, 2);
	ObjectAtPosition({ 35, 36 }).InitializeLoadedObject({ { 7, 11 }, { 6, 7 } }, 3);
}

void SetMapTransparency(const char *path)
{
	auto dunData = LoadFileInMem<uint16_t>(path);
	LoadTransparency(dunData.get());
}

void LoadCustomMap(const char *path, Point viewPosition)
{
	switch (setlvltype) {
	case DTYPE_CATHEDRAL:
	case DTYPE_CRYPT:
		LoadL1Dungeon(path, viewPosition);
		break;
	case DTYPE_CATACOMBS:
		LoadL2Dungeon(path, viewPosition);
		break;
	case DTYPE_CAVES:
	case DTYPE_NEST:
		LoadL3Dungeon(path, viewPosition);
		break;
	case DTYPE_HELL:
		LoadL4Dungeon(path, viewPosition);
		break;
	case DTYPE_TOWN:
	case DTYPE_NONE:
		break;
	}
	LoadRndLvlPal(setlvltype);
}

void LoadArenaMap(const char *path, Point viewPosition, Point exitTrigger)
{
	LoadCustomMap(path, viewPosition);
	trigflag = false;
	numtrigs = 1;
	trigs[0].position = exitTrigger;
	trigs[0]._tmsg = WM_DIABRTNLVL;
}

} // namespace

void LoadSetMap()
{
	switch (setlvlnum) {
	case SL_SKELKING:
		if (Quests[Q_SKELKING]._qactive == QUEST_INIT) {
			Quests[Q_SKELKING]._qactive = QUEST_ACTIVE;
			Quests[Q_SKELKING]._qvar1 = 1;
			NetSendCmdQuest(true, Quests[Q_SKELKING]);
		}
		LoadPreL1Dungeon("levels\\l1data\\sklkng1.dun");
		LoadL1Dungeon("levels\\l1data\\sklkng2.dun", { 83, 44 });
		SetMapTransparency("levels\\l1data\\sklkngt.dun");
		LoadPaletteAndInitBlending("levels\\l1data\\l1_2.pal");
		AddSKingObjs();
		InitSKingTriggers();
		break;
	case SL_BONECHAMB:
		LoadPreL2Dungeon("levels\\l2data\\bonecha2.dun");
		LoadL2Dungeon("levels\\l2data\\bonecha1.dun", { 70, 40 });
		SetMapTransparency("levels\\l2data\\bonechat.dun");
		LoadPaletteAndInitBlending("levels\\l2data\\l2_2.pal");
		AddSChamObjs();
		InitSChambTriggers();
		break;
	case SL_MAZE:
		break;
	case SL_POISONWATER:
		if (Quests[Q_PWATER]._qactive == QUEST_INIT)
			Quests[Q_PWATER]._qactive = QUEST_ACTIVE;
		LoadL3Dungeon("levels\\l3data\\foulwatr.dun", { 31, 83 });
		LoadPaletteAndInitBlending("levels\\l3data\\l3pfoul.pal");
		InitPWaterTriggers();
		break;
	case SL_VILEBETRAYER:
		if (Quests[Q_BETRAYER]._qactive == QUEST_DONE) {
			Quests[Q_BETRAYER]._qvar2 = 4;
		} else if (Quests[Q_BETRAYER]._qactive == QUEST_ACTIVE) {
			Quests[Q_BETRAYER]._qvar2 = 3;
		}
		LoadPreL1Dungeon("levels\\l1data\\vile1.dun");
		LoadL1Dungeon("levels\\l1data\\vile2.dun", { 35, 36 });
		SetMapTransparency("levels\\l1data\\vile1.dun");
		LoadPaletteAndInitBlending("levels\\l1data\\l1_2.pal");
		AddVileObjs();
		InitNoTriggers();
		break;
	case SL_ARENA_CHURCH:
		LoadArenaMap("arena\\church.dun", { 29, 22 }, { 28, 20 });
		break;
	case SL_ARENA_HELL:
		LoadArenaMap("arena\\hell.dun", { 34, 26 }, { 33, 26 });
		break;
	case SL_ARENA_CIRCLE_OF_LIFE:
		LoadArenaMap("arena\\circle_of_death.dun", { 30, 26 }, { 29, 26 });
		break;
	case SL_NONE:
#ifdef _DEBUG
		LoadCustomMap(TestMapPath.c_str(), ViewPosition);
		InitNoTriggers();
#endif
		break;
	}
}

} // namespace devilution
