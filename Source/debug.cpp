/**
 * @file debug.cpp
 *
 * Implementation of debug functions.
 */

#ifdef _DEBUG

#include <cstdint>
#include <cstdio>

#include "debug.h"

#include "automap.h"
#include "control.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/point.hpp"
#include "error.h"
#include "inv.h"
#include "levels/setmaps.h"
#include "lighting.h"
#include "monstdat.h"
#include "monster.h"
#include "pack.h"
#include "plrmsg.h"
#include "quests.h"
#include "spells.h"
#include "towners.h"
#include "utils/endian_stream.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

std::string TestMapPath;
OptionalOwnedClxSpriteList pSquareCel;
bool DebugToggle = false;
bool DebugGodMode = false;
bool DebugVision = false;
bool DebugPath = false;
bool DebugGrid = false;
std::unordered_map<int, Point> DebugCoordsMap;
bool DebugScrollViewEnabled = false;
std::string debugTRN;

// Used for debugging level generation
uint32_t glMid1Seed[NUMLEVELS];
uint32_t glMid2Seed[NUMLEVELS];
uint32_t glMid3Seed[NUMLEVELS];
uint32_t glEndSeed[NUMLEVELS];

namespace {

enum class DebugGridTextItem : uint16_t {
	None,
	dPiece,
	dTransVal,
	dLight,
	dPreLight,
	dFlags,
	dPlayer,
	dMonster,
	dCorpse,
	dObject,
	dItem,
	dSpecial,

	coords,
	cursorcoords,
	objectindex,

	// take dPiece as index
	Solid,
	Transparent,
	Trap,

	// megatiles
	AutomapView,
	dungeon,
	pdungeon,
	Protected,
};

DebugGridTextItem SelectedDebugGridTextItem;

int DebugMonsterId;

std::vector<std::string> SearchMonsters;
std::vector<std::string> SearchItems;
std::vector<std::string> SearchObjects;

void PrintDebugMonster(const Monster &monster)
{
	EventPlrMsg(StrCat(
	                "Monster ", static_cast<int>(monster.getId()), " = ", monster.name(),
	                "\nX = ", monster.position.tile.x, ", Y = ", monster.position.tile.y,
	                "\nEnemy = ", monster.enemy, ", HP = ", monster.hitPoints,
	                "\nMode = ", static_cast<int>(monster.mode), ", Var1 = ", monster.var1),
	    UiFlags::ColorWhite);

	bool bActive = false;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (&Monsters[ActiveMonsters[i]] == &monster) {
			bActive = true;
			break;
		}
	}

	EventPlrMsg(StrCat("Active List = ", bActive ? 1 : 0, ", Squelch = ", monster.activeForTicks), UiFlags::ColorWhite);
}

struct DebugCmdItem {
	const string_view text;
	const string_view description;
	const string_view requiredParameter;
	std::string (*actionProc)(const string_view);
};

extern std::vector<DebugCmdItem> DebugCmdList;

std::string DebugCmdHelp(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "Available Debug Commands: ";
		bool first = true;
		for (const auto &dbgCmd : DebugCmdList) {
			if (first)
				first = false;
			else
				ret.append(" - ");
			ret.append(std::string(dbgCmd.text));
		}
		return ret;
	}
	auto debugCmdIterator = std::find_if(DebugCmdList.begin(), DebugCmdList.end(), [&](const DebugCmdItem &elem) { return elem.text == parameter; });
	if (debugCmdIterator == DebugCmdList.end())
		return StrCat("Debug command ", parameter, " wasn't found");
	auto &dbgCmdItem = *debugCmdIterator;
	if (dbgCmdItem.requiredParameter.empty())
		return StrCat("Description: ", dbgCmdItem.description, "\nParameters: No additional parameter needed.");
	return StrCat("Description: ", dbgCmdItem.description, "\nParameters: ", dbgCmdItem.requiredParameter);
}

std::string DebugCmdGiveGoldCheat(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	for (int8_t &itemIndex : myPlayer.InvGrid) {
		if (itemIndex != 0)
			continue;

		Item &goldItem = myPlayer.InvList[myPlayer._pNumInv];
		MakeGoldStack(goldItem, GOLD_MAX_LIMIT);
		myPlayer._pNumInv++;
		itemIndex = myPlayer._pNumInv;

		myPlayer._pGold += goldItem._ivalue;
	}
	CalcPlrInv(myPlayer, true);

	return "You are now rich! If only this was as easy in real life...";
}

std::string DebugCmdTakeGoldCheat(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	for (auto itemIndex : myPlayer.InvGrid) {
		itemIndex -= 1;

		if (itemIndex < 0)
			continue;
		if (myPlayer.InvList[itemIndex]._itype != ItemType::Gold)
			continue;

		myPlayer.RemoveInvItem(itemIndex);
	}

	myPlayer._pGold = 0;

	return "You are poor...";
}

std::string DebugCmdWarpToLevel(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	auto level = atoi(parameter.data());
	if (level < 0 || level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " is not known. Do you want to write a mod?");
	if (!setlevel && myPlayer.isOnLevel(level))
		return StrCat("I did nothing but fulfilled your wish. You are already at level ", level, ".");

	StartNewLvl(myPlayer, (level != 21) ? interface_mode::WM_DIABNEXTLVL : interface_mode::WM_DIABTOWNWARP, level);
	return StrCat("Welcome to level ", level, ".");
}

std::string DebugCmdLoadQuestMap(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What mapid do you want to visit?";
		for (auto &quest : Quests) {
			if (quest._qslvl <= 0)
				continue;
			StrAppend(ret, " ", quest._qslvl, " (", QuestLevelNames[quest._qslvl], ")");
		}
		return ret;
	}

	auto level = atoi(parameter.data());
	if (level < 1)
		return "Map id must be 1 or higher";
	if (setlevel && setlvlnum == level)
		return StrCat("I did nothing but fulfilled your wish. You are already at mapid .", level);

	for (auto &quest : Quests) {
		if (level != quest._qslvl)
			continue;

		setlvltype = quest._qlvltype;
		StartNewLvl(*MyPlayer, WM_DIABSETLVL, level);

		return StrCat("Welcome to ", QuestLevelNames[level], ".");
	}

	return StrCat("Mapid ", level, " is not known. Do you want to write a mod?");
}

std::string DebugCmdLoadMap(const string_view parameter)
{
	TestMapPath.clear();
	int mapType = 0;
	Point spawn = {};

	int count = 0;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		switch (count) {
		case 0:
			TestMapPath = StrCat(arg, ".dun");
			break;
		case 1:
			mapType = atoi(std::string(arg).c_str());
			break;
		case 2:
			spawn.x = atoi(std::string(arg).c_str());
			break;
		case 3:
			spawn.y = atoi(std::string(arg).c_str());
			break;
		}
		count++;
	}

	if (TestMapPath.empty() || mapType < DTYPE_CATHEDRAL || mapType > DTYPE_LAST || !InDungeonBounds(spawn))
		return "Directions not understood";

	setlvltype = static_cast<dungeon_type>(mapType);
	ViewPosition = spawn;

	StartNewLvl(*MyPlayer, WM_DIABSETLVL, SL_NONE);

	return "Welcome to this unique place.";
}

std::string ExportDun(const string_view parameter)
{
	std::string levelName = StrCat(currlevel, "-", glSeedTbl[currlevel], ".dun");

	FILE *dunFile = OpenFile(levelName.c_str(), "ab");

	WriteLE16(dunFile, DMAXX);
	WriteLE16(dunFile, DMAXY);

	/** Tiles. */
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			WriteLE16(dunFile, dungeon[x][y]);
		}
	}

	/** Padding */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, 0);
		}
	}

	/** Monsters */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t monsterId = 0;
			if (dMonster[x][y] > 0) {
				for (int i = 0; i < 157; i++) {
					if (MonstConvTbl[i] == Monsters[abs(dMonster[x][y]) - 1].type().type) {
						monsterId = i + 1;
						break;
					}
				}
			}
			WriteLE16(dunFile, monsterId);
		}
	}

	/** Objects */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t objectId = 0;
			Object *object = FindObjectAtPosition({ x, y }, false);
			if (object != nullptr) {
				for (int i = 0; i < 147; i++) {
					if (ObjTypeConv[i] == object->_otype) {
						objectId = i;
						break;
					}
				}
			}
			WriteLE16(dunFile, objectId);
		}
	}

	/** Transparency */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, dTransVal[x][y]);
		}
	}
	std::fclose(dunFile);

	return StrCat(levelName, " saved. Happy mapping!");
}

std::unordered_map<string_view, _talker_id> TownerShortNameToTownerId = {
	{ "griswold", _talker_id::TOWN_SMITH },
	{ "pepin", _talker_id::TOWN_HEALER },
	{ "ogden", _talker_id::TOWN_TAVERN },
	{ "cain", _talker_id::TOWN_STORY },
	{ "farnham", _talker_id::TOWN_DRUNK },
	{ "adria", _talker_id::TOWN_WITCH },
	{ "gillian", _talker_id::TOWN_BMAID },
	{ "wirt", _talker_id ::TOWN_PEGBOY },
	{ "lester", _talker_id ::TOWN_FARMER },
	{ "girl", _talker_id ::TOWN_GIRL },
	{ "nut", _talker_id::TOWN_COWFARM },
};

std::string DebugCmdVisitTowner(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	if (setlevel || !myPlayer.isOnLevel(0))
		return "What kind of friends do you have in dungeons?";

	if (parameter.empty()) {
		std::string ret;
		ret = "Who? ";
		for (auto &entry : TownerShortNameToTownerId) {
			ret.append(" ");
			ret.append(std::string(entry.first));
		}
		return ret;
	}

	auto it = TownerShortNameToTownerId.find(parameter);
	if (it == TownerShortNameToTownerId.end())
		return StrCat(parameter, " is unknown. Perhaps he is a ninja?");

	for (auto &towner : Towners) {
		if (towner._ttype != it->second)
			continue;

		CastSpell(
		    MyPlayerId,
		    SpellID::Teleport,
		    myPlayer.position.tile.x,
		    myPlayer.position.tile.y,
		    towner.position.x,
		    towner.position.y,
		    1);

		return StrCat("Say hello to ", parameter, " from me.");
	}

	return StrCat("Couldn't find ", parameter, ".");
}

std::string DebugCmdResetLevel(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	auto args = SplitByChar(parameter, ' ');
	auto it = args.begin();
	if (it == args.end())
		return "What level do you want to visit?";
	auto level = atoi(std::string(*it).c_str());
	if (level < 0 || level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " is not known. Do you want to write an extension mod?");
	myPlayer._pLvlVisited[level] = false;
	DeltaClearLevel(level);

	if (++it != args.end()) {
		const auto seed = static_cast<uint32_t>(std::stoul(std::string(*it)));
		glSeedTbl[level] = seed;
	}

	if (myPlayer.isOnLevel(level))
		return StrCat("Level ", level, " can't be cleaned, cause you still occupy it!");
	return StrCat("Level ", level, " was restored and looks fabulous.");
}

std::string DebugCmdGodMode(const string_view parameter)
{
	DebugGodMode = !DebugGodMode;
	if (DebugGodMode)
		return "A god descended.";
	return "You are mortal, beware of the darkness.";
}

std::string DebugCmdLighting(const string_view parameter)
{
	ToggleLighting();

	return "All raindrops are the same.";
}

std::string DebugCmdMapReveal(const string_view parameter)
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);

	return "The way is made clear when viewed from above";
}

std::string DebugCmdMapHide(const string_view parameter)
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			AutomapView[x][y] = MAP_EXP_NONE;

	return "The way is made unclear when viewed from below";
}

std::string DebugCmdVision(const string_view parameter)
{
	DebugVision = !DebugVision;
	if (DebugVision)
		return "You see as I do.";

	return "My path is set.";
}

std::string DebugCmdPath(const string_view parameter)
{
	DebugPath = !DebugPath;
	if (DebugPath)
		return "The mushroom trail.";

	return "The path is hidden.";
}

std::string DebugCmdQuest(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "You must provide an id. This could be: all";
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;
			StrAppend(ret, ", ", quest._qidx, " (", QuestsData[quest._qidx]._qlstr, ")");
		}
		return ret;
	}

	if (parameter.compare("all") == 0) {
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;

			quest._qactive = QUEST_ACTIVE;
			quest._qlog = true;
		}

		return "Happy questing";
	}

	int questId = atoi(parameter.data());

	if (questId >= MAXQUESTS)
		return StrCat("Quest ", questId, " is not known. Do you want to write a mod?");
	auto &quest = Quests[questId];

	if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
		return StrCat(QuestsData[questId]._qlstr, " was already given.");

	quest._qactive = QUEST_ACTIVE;
	quest._qlog = true;

	return StrCat(QuestsData[questId]._qlstr, " enabled.");
}

std::string DebugCmdLevelUp(const string_view parameter)
{
	int levels = std::max(1, atoi(parameter.data()));
	for (int i = 0; i < levels; i++)
		NetSendCmd(true, CMD_CHEAT_EXPERIENCE);
	return "New experience leads to new insights.";
}

std::string DebugCmdMaxStats(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	ModifyPlrStr(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength) - myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic) - myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity) - myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality) - myPlayer._pBaseVit);
	return "Who needs elixirs anyway?";
}

std::string DebugCmdMinStats(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	ModifyPlrStr(myPlayer, -myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, -myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, -myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, -myPlayer._pBaseVit);
	return "From hero to zero.";
}

std::string DebugCmdSetSpellsLevel(const string_view parameter)
{
	uint8_t level = static_cast<uint8_t>(std::max(0, atoi(parameter.data())));
	for (uint8_t i = static_cast<uint8_t>(SpellID::Firebolt); i < MAX_SPELLS; i++) {
		if (GetSpellBookLevel(static_cast<SpellID>(i)) != -1) {
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, i, level);
		}
	}
	if (level == 0)
		MyPlayer->_pMemSpells = 0;

	return "Knowledge is power.";
}

std::string DebugCmdRefillHealthMana(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	myPlayer.RestoreFullLife();
	myPlayer.RestoreFullMana();
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);

	return "Ready for more.";
}

std::string DebugCmdChangeHealth(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	int change = -1;

	if (!parameter.empty())
		change = atoi(parameter.data());

	if (change == 0)
		return "Health hasn't changed.";

	int newHealth = myPlayer._pHitPoints + (change * 64);
	SetPlayerHitPoints(myPlayer, newHealth);
	if (newHealth <= 0)
		SyncPlrKill(myPlayer, DeathReason::MonsterOrTrap);

	return "Health has changed.";
}

std::string DebugCmdChangeMana(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	int change = -1;

	if (!parameter.empty())
		change = atoi(parameter.data());

	if (change == 0)
		return "Mana hasn't changed.";

	int newMana = myPlayer._pMana + (change * 64);
	myPlayer._pMana = newMana;
	myPlayer._pManaBase = myPlayer._pMana + myPlayer._pMaxManaBase - myPlayer._pMaxMana;
	RedrawComponent(PanelDrawComponent::Mana);

	return "Mana has changed.";
}

std::string DebugCmdGenerateUniqueItem(const string_view parameter)
{
	return DebugSpawnUniqueItem(parameter.data());
}

std::string DebugCmdGenerateItem(const string_view parameter)
{
	return DebugSpawnItem(parameter.data());
}

std::string DebugCmdExit(const string_view parameter)
{
	gbRunGame = false;
	gbRunGameResult = false;
	return "See you again my Lord.";
}

std::string DebugCmdArrow(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	myPlayer._pIFlags &= ~ItemSpecialEffect::FireArrows;
	myPlayer._pIFlags &= ~ItemSpecialEffect::LightningArrows;

	if (parameter == "normal") {
		// we removed the parameter at the top
	} else if (parameter == "fire") {
		myPlayer._pIFlags |= ItemSpecialEffect::FireArrows;
	} else if (parameter == "lightning") {
		myPlayer._pIFlags |= ItemSpecialEffect::LightningArrows;
	} else if (parameter == "explosion") {
		myPlayer._pIFlags |= (ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows);
	} else {
		return "Unknown is sometimes similar to nothing (unkown effect).";
	}

	return "I can shoot any arrow.";
}

std::string DebugCmdTalkToTowner(const string_view parameter)
{
	if (DebugTalkToTowner(parameter.data())) {
		return "Hello from the other side.";
	}
	return "NPC not found.";
}

std::string DebugCmdShowGrid(const string_view parameter)
{
	DebugGrid = !DebugGrid;
	if (DebugGrid)
		return "A basket full of rectangles and mushrooms.";

	return "Back to boring.";
}

std::string DebugCmdSpawnUniqueMonster(const string_view parameter)
{
	if (leveltype == DTYPE_TOWN)
		return "Do you want to kill the towners?!?";

	std::string name;
	int count = 1;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		const int num = atoi(std::string(arg).c_str());
		if (num > 0) {
			count = num;
			break;
		}
		AppendStrView(name, arg);
		name += ' ';
	}
	if (name.empty())
		return "Monster name cannot be empty. Duh.";

	name.pop_back(); // remove last space
	AsciiStrToLower(name);

	int mtype = -1;
	UniqueMonsterType uniqueIndex = UniqueMonsterType::None;
	for (size_t i = 0; UniqueMonstersData[i].mtype != MT_INVALID; i++) {
		auto mondata = UniqueMonstersData[i];
		const std::string monsterName = AsciiStrToLower(mondata.mName);
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = mondata.mtype;
		uniqueIndex = static_cast<UniqueMonsterType>(i);
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1)
		return "Monster not found!";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		CMonster &monsterType = LevelMonsterTypes[id];
		monsterType.type = static_cast<_monster_id>(mtype);
		InitMonsterGFX(monsterType);
		InitMonsterSND(monsterType);
		monsterType.placeFlags |= PLACE_SCATTER;
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	int spawnedMonster = 0;

	auto ret = Crawl(0, MaxCrawlRadius, [&](Displacement displacement) -> std::optional<std::string> {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return {};
		if (!IsTileWalkable(pos))
			return {};

		Monster *monster = AddMonster(pos, myPlayer._pdir, id, true);
		if (monster == nullptr)
			return StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
		PrepareUniqueMonst(*monster, uniqueIndex, 0, 0, UniqueMonstersData[static_cast<size_t>(uniqueIndex)]);
		monster->corpseId = 1;
		spawnedMonster += 1;

		if (spawnedMonster >= count)
			return "Let the fighting begin!";

		return {};
	});

	if (!ret)
		ret = StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
	return *ret;
}

std::string DebugCmdSpawnMonster(const string_view parameter)
{
	if (leveltype == DTYPE_TOWN)
		return "Do you want to kill the towners?!?";

	std::string name;
	int count = 1;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		const int num = atoi(std::string(arg).c_str());
		if (num > 0) {
			count = num;
			break;
		}
		AppendStrView(name, arg);
		name += ' ';
	}
	if (name.empty())
		return "Monster name cannot be empty. Duh.";

	name.pop_back(); // remove last space
	AsciiStrToLower(name);

	int mtype = -1;

	for (int i = 0; i < NUM_MTYPES; i++) {
		auto mondata = MonstersData[i];
		const std::string monsterName = AsciiStrToLower(mondata.name);
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = i;
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1)
		return "Monster not found!";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		CMonster &monsterType = LevelMonsterTypes[id];
		monsterType.type = static_cast<_monster_id>(mtype);
		InitMonsterGFX(monsterType);
		InitMonsterSND(monsterType);
		monsterType.placeFlags |= PLACE_SCATTER;
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	int spawnedMonster = 0;

	auto ret = Crawl(0, MaxCrawlRadius, [&](Displacement displacement) -> std::optional<std::string> {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return {};
		if (!IsTileWalkable(pos))
			return {};

		if (AddMonster(pos, myPlayer._pdir, id, true) == nullptr)
			return StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
		spawnedMonster += 1;

		if (spawnedMonster >= count)
			return "Let the fighting begin!";

		return {};
	});

	if (!ret)
		ret = StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
	return *ret;
}

std::string DebugCmdShowTileData(const string_view parameter)
{
	std::string paramList[] = {
		"dPiece",
		"dTransVal",
		"dLight",
		"dPreLight",
		"dFlags",
		"dPlayer",
		"dMonster",
		"dCorpse",
		"dObject",
		"dItem",
		"dSpecial",
		"coords",
		"cursorcoords",
		"objectindex",
		"solid",
		"transparent",
		"trap",
		"AutomapView",
		"dungeon",
		"pdungeon",
		"Protected",
	};

	if (parameter == "clear") {
		SelectedDebugGridTextItem = DebugGridTextItem::None;
		return "Tile data cleared!";
	}
	if (parameter == "") {
		std::string list = "clear";
		for (const auto &param : paramList) {
			list += " / " + param;
		}
		return list;
	}
	bool found = false;
	int index = 0;
	for (const auto &param : paramList) {
		index++;
		if (parameter != param)
			continue;
		found = true;
		auto newGridText = static_cast<DebugGridTextItem>(index);
		if (newGridText == SelectedDebugGridTextItem) {
			SelectedDebugGridTextItem = DebugGridTextItem::None;
			return "Tile data toggled... now you see nothing.";
		}
		SelectedDebugGridTextItem = newGridText;
		break;
	}
	if (!found)
		return "Invalid name. Check names using tiledata command.";

	return "Special powers activated.";
}

std::string DebugCmdScrollView(const string_view parameter)
{
	DebugScrollViewEnabled = !DebugScrollViewEnabled;
	if (DebugScrollViewEnabled)
		return "You can see as far as an eagle.";
	InitMultiView();
	return "If you want to see the world, you need to explore it yourself.";
}

std::string DebugCmdItemInfo(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	Item *pItem = nullptr;
	if (!myPlayer.HoldItem.isEmpty()) {
		pItem = &myPlayer.HoldItem;
	} else if (pcursinvitem != -1) {
		if (pcursinvitem <= INVITEM_INV_LAST)
			pItem = &myPlayer.InvList[pcursinvitem - INVITEM_INV_FIRST];
		else
			pItem = &myPlayer.SpdList[pcursinvitem - INVITEM_BELT_FIRST];
	} else if (pcursitem != -1) {
		pItem = &Items[pcursitem];
	}
	if (pItem != nullptr) {
		string_view netPackValidation { "N/A" };
		if (gbIsMultiplayer) {
			ItemNetPack itemPack;
			Item unpacked;
			PackNetItem(*pItem, itemPack);
			netPackValidation = UnPackNetItem(myPlayer, itemPack, unpacked) ? "Success" : "Failure";
		}
		return StrCat("Name: ", pItem->_iIName,
		    "\nIDidx: ", pItem->IDidx, " (", AllItemsList[pItem->IDidx].iName, ")",
		    "\nSeed: ", pItem->_iSeed,
		    "\nCreateInfo: ", pItem->_iCreateInfo,
		    "\nLevel: ", pItem->_iCreateInfo & CF_LEVEL,
		    "\nOnly Good: ", ((pItem->_iCreateInfo & CF_ONLYGOOD) == 0) ? "False" : "True",
		    "\nUnique Monster: ", ((pItem->_iCreateInfo & CF_UPER15) == 0) ? "False" : "True",
		    "\nDungeon Item: ", ((pItem->_iCreateInfo & CF_UPER1) == 0) ? "False" : "True",
		    "\nUnique Item: ", ((pItem->_iCreateInfo & CF_UNIQUE) == 0) ? "False" : "True",
		    "\nSmith: ", ((pItem->_iCreateInfo & CF_SMITH) == 0) ? "False" : "True",
		    "\nSmith Premium: ", ((pItem->_iCreateInfo & CF_SMITHPREMIUM) == 0) ? "False" : "True",
		    "\nBoy: ", ((pItem->_iCreateInfo & CF_BOY) == 0) ? "False" : "True",
		    "\nWitch: ", ((pItem->_iCreateInfo & CF_WITCH) == 0) ? "False" : "True",
		    "\nHealer: ", ((pItem->_iCreateInfo & CF_HEALER) == 0) ? "False" : "True",
		    "\nPregen: ", ((pItem->_iCreateInfo & CF_PREGEN) == 0) ? "False" : "True",
		    "\nNet Validation: ", netPackValidation);
	}
	return StrCat("Numitems: ", ActiveItemCount);
}

std::string DebugCmdQuestInfo(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "You must provide an id. This could be:";
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;
			StrAppend(ret, ", ", quest._qidx, " (", QuestsData[quest._qidx]._qlstr, ")");
		}
		return ret;
	}

	int questId = atoi(parameter.data());

	if (questId >= MAXQUESTS)
		return StrCat("Quest ", questId, " is not known. Do you want to write a mod?");
	auto &quest = Quests[questId];
	return StrCat("\nQuest: ", QuestsData[quest._qidx]._qlstr, "\nActive: ", quest._qactive, " Var1: ", quest._qvar1, " Var2: ", quest._qvar2);
}

std::string DebugCmdPlayerInfo(const string_view parameter)
{
	int playerId = atoi(parameter.data());
	if (static_cast<size_t>(playerId) >= Players.size())
		return "My friend, we need a valid playerId.";
	Player &player = Players[playerId];
	if (!player.plractive)
		return "Player is not active";

	const Point target = player.GetTargetPosition();
	return StrCat("Plr ", playerId, " is ", player._pName,
	    "\nLvl: ", player.plrlevel, " Changing: ", player._pLvlChanging,
	    "\nTile.x: ", player.position.tile.x, " Tile.y: ", player.position.tile.y, " Target.x: ", target.x, " Target.y: ", target.y,
	    "\nMode: ", player._pmode, " destAction: ", player.destAction, " walkpath[0]: ", player.walkpath[0],
	    "\nInvincible:", player._pInvincible ? 1 : 0, " HitPoints:", player._pHitPoints);
}

std::string DebugCmdToggleFPS(const string_view parameter)
{
	frameflag = !frameflag;
	return "";
}

std::string DebugCmdChangeTRN(const string_view parameter)
{
	std::string out;
	const auto parts = SplitByChar(parameter, ' ');
	auto it = parts.begin();
	if (it != parts.end()) {
		const string_view first = *it;
		if (++it != parts.end()) {
			const string_view second = *it;
			string_view prefix;
			if (first == "mon") {
				prefix = "monsters\\monsters\\";
			} else if (first == "plr") {
				prefix = "plrgfx\\";
			}
			debugTRN = StrCat(prefix, second, ".trn");
		} else {
			debugTRN = StrCat(first, ".trn");
		}
		out = fmt::format("I am a pretty butterfly. \n(Loading TRN: {:s})", debugTRN);
	} else {
		debugTRN = "";
		out = "I am a big brown potato.";
	}
	auto &player = *MyPlayer;
	InitPlayerGFX(player);
	StartStand(player, player._pdir);
	return out;
}

std::string DebugCmdSearchMonster(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a monster name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchMonsters.push_back(name);

	return "We will find this bastard!";
}

std::string DebugCmdSearchItem(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a item name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchItems.push_back(name);

	return "Are you greedy? Anyway I will help you.";
}

std::string DebugCmdSearchObject(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a object name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchObjects.push_back(name);

	return "I will look for the pyramids. Oh sorry, I'm looking for what you want, of course.";
}

std::string DebugCmdClearSearch(const string_view parameter)
{
	SearchMonsters.clear();
	SearchItems.clear();
	SearchObjects.clear();

	return "Now you have to find it yourself.";
}

std::vector<DebugCmdItem> DebugCmdList = {
	{ "help", "Prints help overview or help for a specific command.", "({command})", &DebugCmdHelp },
	{ "givegold", "Fills the inventory with gold.", "", &DebugCmdGiveGoldCheat },
	{ "givexp", "Levels the player up (min 1 level or {levels}).", "({levels})", &DebugCmdLevelUp },
	{ "maxstats", "Sets all stat values to maximum.", "", &DebugCmdMaxStats },
	{ "minstats", "Sets all stat values to minimum.", "", &DebugCmdMinStats },
	{ "setspells", "Set spell level to {level} for all spells.", "{level}", &DebugCmdSetSpellsLevel },
	{ "takegold", "Removes all gold from inventory.", "", &DebugCmdTakeGoldCheat },
	{ "givequest", "Enable a given quest.", "({id})", &DebugCmdQuest },
	{ "givemap", "Reveal the map.", "", &DebugCmdMapReveal },
	{ "takemap", "Hide the map.", "", &DebugCmdMapHide },
	{ "goto", "Moves to specifided {level} (use 0 for town).", "{level}", &DebugCmdWarpToLevel },
	{ "questmap", "Load a quest level {level}.", "{level}", &DebugCmdLoadQuestMap },
	{ "map", "Load custom level from a given {path}.dun.", "{path} {type} {x} {y}", &DebugCmdLoadMap },
	{ "exportdun", "Save the current level as a dun-file.", "", &ExportDun },
	{ "visit", "Visit a towner.", "{towner}", &DebugCmdVisitTowner },
	{ "restart", "Resets specified {level}.", "{level} ({seed})", &DebugCmdResetLevel },
	{ "god", "Toggles godmode.", "", &DebugCmdGodMode },
	{ "drawvision", "Toggles vision debug rendering.", "", &DebugCmdVision },
	{ "drawpath", "Toggles path debug rendering.", "", &DebugCmdPath },
	{ "fullbright", "Toggles whether light shading is in effect.", "", &DebugCmdLighting },
	{ "fill", "Refills health and mana.", "", &DebugCmdRefillHealthMana },
	{ "changehp", "Changes health by {value} (Use a negative value to remove health).", "{value}", &DebugCmdChangeHealth },
	{ "changemp", "Changes mana by {value} (Use a negative value to remove mana).", "{value}", &DebugCmdChangeMana },
	{ "dropu", "Attempts to generate unique item {name}.", "{name}", &DebugCmdGenerateUniqueItem },
	{ "drop", "Attempts to generate item {name}.", "{name}", &DebugCmdGenerateItem },
	{ "talkto", "Interacts with a NPC whose name contains {name}.", "{name}", &DebugCmdTalkToTowner },
	{ "exit", "Exits the game.", "", &DebugCmdExit },
	{ "arrow", "Changes arrow effect (normal, fire, lightning, explosion).", "{effect}", &DebugCmdArrow },
	{ "grid", "Toggles showing grid.", "", &DebugCmdShowGrid },
	{ "spawnu", "Spawns unique monster {name}.", "{name} ({count})", &DebugCmdSpawnUniqueMonster },
	{ "spawn", "Spawns monster {name}.", "{name} ({count})", &DebugCmdSpawnMonster },
	{ "tiledata", "Toggles showing tile data {name} (leave name empty to see a list).", "{name}", &DebugCmdShowTileData },
	{ "scrollview", "Toggles scroll view feature (with shift+mouse).", "", &DebugCmdScrollView },
	{ "iteminfo", "Shows info of currently selected item.", "", &DebugCmdItemInfo },
	{ "questinfo", "Shows info of quests.", "{id}", &DebugCmdQuestInfo },
	{ "playerinfo", "Shows info of player.", "{playerid}", &DebugCmdPlayerInfo },
	{ "fps", "Toggles displaying FPS", "", &DebugCmdToggleFPS },
	{ "trn", "Makes player use TRN {trn} - Write 'plr' before it to look in plrgfx\\ or 'mon' to look in monsters\\monsters\\ - example: trn plr infra is equal to 'plrgfx\\infra.trn'", "{trn}", &DebugCmdChangeTRN },
	{ "searchmonster", "Searches the automap for {monster}", "{monster}", &DebugCmdSearchMonster },
	{ "searchitem", "Searches the automap for {item}", "{item}", &DebugCmdSearchItem },
	{ "searchobject", "Searches the automap for {object}", "{object}", &DebugCmdSearchObject },
	{ "clearsearch", "Search in the auto map is cleared", "", &DebugCmdClearSearch },
};

} // namespace

void LoadDebugGFX()
{
	pSquareCel = LoadCel("data\\square", 64);
}

void FreeDebugGFX()
{
	pSquareCel = std::nullopt;
}

void GetDebugMonster()
{
	int monsterIndex = pcursmonst;
	if (monsterIndex == -1)
		monsterIndex = abs(dMonster[cursPosition.x][cursPosition.y]) - 1;

	if (monsterIndex == -1)
		monsterIndex = DebugMonsterId;

	PrintDebugMonster(Monsters[monsterIndex]);
}

void NextDebugMonster()
{
	DebugMonsterId++;
	if (DebugMonsterId == MaxMonsters)
		DebugMonsterId = 0;

	EventPlrMsg(StrCat("Current debug monster = ", DebugMonsterId), UiFlags::ColorWhite);
}

void SetDebugLevelSeedInfos(uint32_t mid1Seed, uint32_t mid2Seed, uint32_t mid3Seed, uint32_t endSeed)
{
	glMid1Seed[currlevel] = mid1Seed;
	glMid2Seed[currlevel] = mid2Seed;
	glMid3Seed[currlevel] = mid3Seed;
	glEndSeed[currlevel] = endSeed;
}

bool CheckDebugTextCommand(const string_view text)
{
	auto debugCmdIterator = std::find_if(DebugCmdList.begin(), DebugCmdList.end(), [&](const DebugCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (debugCmdIterator == DebugCmdList.end())
		return false;

	auto &dbgCmd = *debugCmdIterator;
	string_view parameter = "";
	if (text.length() > (dbgCmd.text.length() + 1))
		parameter = text.substr(dbgCmd.text.length() + 1);
	const auto result = dbgCmd.actionProc(parameter);
	Log("DebugCmd: {} Result: {}", text, result);
	if (result != "")
		EventPlrMsg(result, UiFlags::ColorRed);
	return true;
}

bool IsDebugGridTextNeeded()
{
	return SelectedDebugGridTextItem != DebugGridTextItem::None;
}

bool IsDebugGridInMegatiles()
{
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::AutomapView:
	case DebugGridTextItem::dungeon:
	case DebugGridTextItem::pdungeon:
	case DebugGridTextItem::Protected:
		return true;
	default:
		return false;
	}
}

bool GetDebugGridText(Point dungeonCoords, char *debugGridTextBuffer)
{
	int info = 0;
	int blankValue = 0;
	Point megaCoords = dungeonCoords.worldToMega();
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::coords:
		*BufCopy(debugGridTextBuffer, dungeonCoords.x, ":", dungeonCoords.y) = '\0';
		return true;
	case DebugGridTextItem::cursorcoords:
		if (dungeonCoords != cursPosition)
			return false;
		*BufCopy(debugGridTextBuffer, dungeonCoords.x, ":", dungeonCoords.y) = '\0';
		return true;
	case DebugGridTextItem::objectindex: {
		info = 0;
		Object *object = FindObjectAtPosition(dungeonCoords);
		if (object != nullptr) {
			info = static_cast<int>(object->_otype);
		}
		break;
	}
	case DebugGridTextItem::dPiece:
		info = dPiece[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dTransVal:
		info = dTransVal[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dLight:
		info = dLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dPreLight:
		info = dPreLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dFlags:
		info = static_cast<int>(dFlags[dungeonCoords.x][dungeonCoords.y]);
		break;
	case DebugGridTextItem::dPlayer:
		info = dPlayer[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dMonster:
		info = dMonster[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dCorpse:
		info = dCorpse[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dItem:
		info = dItem[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dSpecial:
		info = dSpecial[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dObject:
		info = dObject[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::Solid:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Solid) << 0 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::BlockLight) << 1 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::BlockMissile) << 2;
		break;
	case DebugGridTextItem::Transparent:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Transparent) << 0 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::TransparentLeft) << 1 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::TransparentRight) << 2;
		break;
	case DebugGridTextItem::Trap:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Trap);
		break;
	case DebugGridTextItem::AutomapView:
		info = AutomapView[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::dungeon:
		info = dungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::pdungeon:
		info = pdungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::Protected:
		info = Protected.test(megaCoords.x, megaCoords.y);
		break;
	case DebugGridTextItem::None:
		return false;
	}
	if (info == blankValue)
		return false;
	*BufCopy(debugGridTextBuffer, info) = '\0';
	return true;
}

bool IsDebugAutomapHighlightNeeded()
{
	return SearchMonsters.size() > 0 || SearchItems.size() > 0 || SearchObjects.size() > 0;
}

bool ShouldHighlightDebugAutomapTile(Point position)
{
	auto matchesSearched = [](const string_view name, const std::vector<std::string> &searchedNames) {
		const std::string lowercaseName = AsciiStrToLower(name);
		for (const auto &searchedName : searchedNames) {
			if (lowercaseName.find(searchedName) != std::string::npos) {
				return true;
			}
		}
		return false;
	};

	if (SearchMonsters.size() > 0 && dMonster[position.x][position.y] != 0) {
		const int mi = abs(dMonster[position.x][position.y]) - 1;
		const Monster &monster = Monsters[mi];
		if (matchesSearched(monster.name(), SearchMonsters))
			return true;
	}

	if (SearchItems.size() > 0 && dItem[position.x][position.y] != 0) {
		const int itemId = abs(dItem[position.x][position.y]) - 1;
		const Item &item = Items[itemId];
		if (matchesSearched(item.getName(), SearchItems))
			return true;
	}

	if (SearchObjects.size() > 0 && IsObjectAtPosition(position)) {
		const Object &object = ObjectAtPosition(position);
		if (matchesSearched(object.name(), SearchObjects))
			return true;
	}

	return false;
}

} // namespace devilution

#endif
