#include "towners.h"

#include <cstdint>

#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "game_mode.hpp"
#include "inv.h"
#include "minitext.h"
#include "stores.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/str_case.hpp"

namespace devilution {
namespace {

OptionalOwnedClxSpriteSheet CowSprites;
int CowMsg;
int CowClicks;

/** Specifies the active sound effect ID for interacting with cows. */
SfxID CowPlaying = SfxID::None;

struct TownerData {
	_talker_id type;
	Point position;
	Direction dir;
	void (*init)(Towner &towner, const TownerData &townerData);
	void (*talk)(Player &player, Towner &towner);
};

void NewTownerAnim(Towner &towner, ClxSpriteList sprites, uint8_t numFrames, int delay)
{
	towner.anim.emplace(sprites);
	towner._tAnimLen = numFrames;
	towner._tAnimFrame = 0;
	towner._tAnimCnt = 0;
	towner._tAnimDelay = delay;
}

void InitTownerInfo(Towner &towner, const TownerData &townerData)
{
	towner._ttype = townerData.type;
	towner.name = _(TownerLongNames[townerData.type]);
	towner.position = townerData.position;
	towner.talk = townerData.talk;

	townerData.init(towner, townerData);
}

void InitTownerInfo(int16_t i, const TownerData &townerData)
{
	// It's necessary to assign this before invoking townerData.init()
	// specifically for the cows that need to read this value to fill adjacent tiles
	dMonster[townerData.position.x][townerData.position.y] = i + 1;
	InitTownerInfo(Towners[i], townerData);
}

void LoadTownerAnimations(Towner &towner, const char *path, int frames, int delay)
{
	towner.ownedAnim = std::nullopt;
	towner.ownedAnim = LoadCel(path, towner._tAnimWidth);
	NewTownerAnim(towner, *towner.ownedAnim, frames, delay);
}

/**
 * @brief Load Griswold into the game
 */
void InitSmith(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4,
		4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 1, 2, 3
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\smith\\smithn", 16, 3);
	towner.gossip = PickRandomlyAmong({ TEXT_GRISWOLD2, TEXT_GRISWOLD3, TEXT_GRISWOLD4, TEXT_GRISWOLD5, TEXT_GRISWOLD6, TEXT_GRISWOLD7, TEXT_GRISWOLD8, TEXT_GRISWOLD9, TEXT_GRISWOLD10, TEXT_GRISWOLD12, TEXT_GRISWOLD13 });
}

void InitBarOwner(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 1, 0, 15, 14, 13, 13, 14, 15,
		0, 1, 2, 3, 4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\twnf\\twnfn", 16, 3);
	towner.gossip = PickRandomlyAmong({ TEXT_OGDEN2, TEXT_OGDEN3, TEXT_OGDEN4, TEXT_OGDEN5, TEXT_OGDEN6, TEXT_OGDEN8, TEXT_OGDEN9, TEXT_OGDEN10 });
}

void InitTownDead(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, "towners\\butch\\deadguy", 8, 6);
}

void InitWitch(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		 3,  3,  3,  4,  5,  5,  5,  4,  3, 14, 13, 12, 12, 12, 13, 14, 3, 4, 5, 5, 5, 4,
		 3,  3,  3,  4,  5,  5,  5,  4,  3, 14, 13, 12, 12, 12, 13, 14, 3, 4, 5, 5, 5, 4,
		 3,  3,  3,  4,  5,  5,  5,  4,  3, 14, 13, 12, 12, 12, 13, 14, 3, 4, 5, 5, 5, 4,
		 3,  2,  1,  0, 18, 17, 18,  0,  1,  0, 18, 17, 18,  0,  1,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		14, 14, 13, 12, 12, 12, 12, 13, 14,
		14, 14, 13, 12, 11, 11, 11, 10,  9,  9,  9,  8,
		 7,  8,  9,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
		 0,  1,  0, 18, 17, 18,  0,  1,  0,  1,  2
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\townwmn1\\witch", 19, 6);
	towner.gossip = PickRandomlyAmong({ TEXT_ADRIA2, TEXT_ADRIA3, TEXT_ADRIA4, TEXT_ADRIA5, TEXT_ADRIA6, TEXT_ADRIA7, TEXT_ADRIA8, TEXT_ADRIA9, TEXT_ADRIA10, TEXT_ADRIA12, TEXT_ADRIA13 });
}

void InitBarmaid(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, "towners\\townwmn1\\wmnn", 18, 6);
	towner.gossip = PickRandomlyAmong({ TEXT_GILLIAN2, TEXT_GILLIAN3, TEXT_GILLIAN4, TEXT_GILLIAN5, TEXT_GILLIAN6, TEXT_GILLIAN7, TEXT_GILLIAN9, TEXT_GILLIAN10 });
}

void InitBoy(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, "towners\\townboy\\pegkid1", 20, 6);
	towner.gossip = PickRandomlyAmong({ TEXT_WIRT2, TEXT_WIRT3, TEXT_WIRT4, TEXT_WIRT5, TEXT_WIRT6, TEXT_WIRT7, TEXT_WIRT8, TEXT_WIRT9, TEXT_WIRT11, TEXT_WIRT12 });
}

void InitHealer(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		 0,  1,  2,  2,  1,  0, 19, 18, 18, 19,
		 0,  1,  2,  2,  1,  0, 19, 18, 18, 19,
		 0,  1,  2,  2,  1,  0, 19, 18, 18, 19,
		 0,  1,  2,  2,  1,  0, 19, 18, 18, 19,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,
		 4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,
		 4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\healer\\healer", 20, 6);
	towner.gossip = PickRandomlyAmong({ TEXT_PEPIN2, TEXT_PEPIN3, TEXT_PEPIN4, TEXT_PEPIN5, TEXT_PEPIN6, TEXT_PEPIN7, TEXT_PEPIN9, TEXT_PEPIN10, TEXT_PEPIN11 });
}

void InitTeller(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		 0,  0, 24, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 24, 24,  0,  0,  0, 24,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\strytell\\strytell", 25, 3);
	towner.gossip = PickRandomlyAmong({ TEXT_STORY2, TEXT_STORY3, TEXT_STORY4, TEXT_STORY5, TEXT_STORY6, TEXT_STORY7, TEXT_STORY9, TEXT_STORY10, TEXT_STORY11 });
}

void InitDrunk(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	static const uint8_t AnimOrder[] = {
		// clang-format off
		 0, 0, 0,  1,  2,  3,  4,  5,  6,  7,  8, 9, 10, 10, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
		 0, 0, 0, 17, 16, 15, 14, 13, 12, 11, 10, 9, 10, 11, 12, 13, 14, 15, 16, 17,
		 0, 1, 2,  3,  4,  4,  4,  3,  2,  1
		// clang-format on
	};
	towner.animOrder = { AnimOrder };
	LoadTownerAnimations(towner, "towners\\drunk\\twndrunk", 18, 3);
	towner.gossip = PickRandomlyAmong({ TEXT_FARNHAM2, TEXT_FARNHAM3, TEXT_FARNHAM4, TEXT_FARNHAM5, TEXT_FARNHAM6, TEXT_FARNHAM8, TEXT_FARNHAM9, TEXT_FARNHAM10, TEXT_FARNHAM11, TEXT_FARNHAM12, TEXT_FARNHAM13 });
}

void InitCows(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 128;
	towner.animOrder = {};

	NewTownerAnim(towner, (*CowSprites)[static_cast<size_t>(townerData.dir)], 12, 3);
	towner._tAnimFrame = GenerateRnd(11);

	const Point position = townerData.position;
	int16_t cowId = dMonster[position.x][position.y];

	// Cows are large sprites so take up multiple tiles. Vanilla Diablo/Hellfire allowed the player to stand adjacent
	//  to a cow facing an ordinal direction (the two top-right cows) which leads to visual clipping. It's easier to
	//  treat all cows as 4 tile sprites since this works for all facings.
	// The active tile is always the south tile as this is closest to the camera, we mark the other 3 tiles as occupied
	//  using -id to match the convention used for moving/large monsters and players.
	Point offset = position + Direction::NorthWest;
	dMonster[offset.x][offset.y] = -cowId;
	offset = position + Direction::NorthEast;
	dMonster[offset.x][offset.y] = -cowId;
	offset = position + Direction::North;
	dMonster[offset.x][offset.y] = -cowId;
}

void InitFarmer(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, "towners\\farmer\\farmrn2", 15, 3);
}

void InitCowFarmer(Towner &towner, const TownerData &townerData)
{
	const char *celPath = "towners\\farmer\\cfrmrn2";
	if (Quests[Q_JERSEY]._qactive == QUEST_DONE) {
		celPath = "towners\\farmer\\mfrmrn2";
	}
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, celPath, 15, 3);
}

void InitGirl(Towner &towner, const TownerData &townerData)
{
	towner._tAnimWidth = 96;
	towner.animOrder = {};
	LoadTownerAnimations(towner, "towners\\girl\\girlw1", 20, 6);
}

void TownDead(Towner &towner)
{
	if (qtextflag) {
		if (Quests[Q_BUTCHER]._qvar1 == 1)
			towner._tAnimCnt = 0; // Freeze while speaking
		return;
	}

	if ((Quests[Q_BUTCHER]._qactive == QUEST_DONE || Quests[Q_BUTCHER]._qvar1 == 1) && towner._tAnimLen != 1) {
		towner._tAnimLen = 1;
		towner.name = _("Slain Townsman");
	}
}

void TownerTalk(_speech_id message)
{
	CowClicks = 0;
	CowMsg = 0;
	InitQTextMsg(message);
}

void TalkToBarOwner(Player &player, Towner &barOwner)
{
	if (!player._pLvlVisited[0]) {
		InitQTextMsg(TEXT_INTRO);
		return;
	}

	auto &kingQuest = Quests[Q_SKELKING];
	if (kingQuest._qactive != QUEST_NOTAVAIL) {
		if (player._pLvlVisited[2] || player._pLvlVisited[4]) {
			if (kingQuest._qvar2 == 0) {
				kingQuest._qvar2 = 1;
				kingQuest._qlog = true;
				if (kingQuest._qactive == QUEST_INIT) {
					kingQuest._qactive = QUEST_ACTIVE;
					kingQuest._qvar1 = 1;
				}
				InitQTextMsg(TEXT_KING2);
				NetSendCmdQuest(true, kingQuest);
				return;
			}
			if (kingQuest._qactive == QUEST_DONE && kingQuest._qvar2 == 1) {
				kingQuest._qvar2 = 2;
				kingQuest._qvar1 = 2;
				InitQTextMsg(TEXT_KING4);
				NetSendCmdQuest(true, kingQuest);
				return;
			}
		}
	}

	auto &bannerQuest = Quests[Q_LTBANNER];
	if (bannerQuest._qactive != QUEST_NOTAVAIL) {
		if ((player._pLvlVisited[3] || player._pLvlVisited[4]) && bannerQuest._qactive != QUEST_DONE) {
			if (bannerQuest._qvar2 == 0) {
				bannerQuest._qvar2 = 1;
				if (bannerQuest._qactive == QUEST_INIT) {
					bannerQuest._qvar1 = 1;
					bannerQuest._qactive = QUEST_ACTIVE;
				}
				bannerQuest._qlog = true;
				NetSendCmdQuest(true, bannerQuest);
				InitQTextMsg(TEXT_BANNER2);
				return;
			}

			if (bannerQuest._qvar2 == 1 && RemoveInventoryItemById(player, IDI_BANNER)) {
				bannerQuest._qactive = QUEST_DONE;
				bannerQuest._qvar1 = 3;
				NetSendCmdQuest(true, bannerQuest);
				SpawnUnique(UITEM_HARCREST, barOwner.position + Direction::SouthWest, bannerQuest._qlevel);
				InitQTextMsg(TEXT_BANNER3);
				return;
			}
		}
	}

	TownerTalk(TEXT_OGDEN1);
	StartStore(TalkID::Tavern);
}

void TalkToDeadguy(Player &player, Towner & /*deadguy*/)
{
	auto &quest = Quests[Q_BUTCHER];
	if (quest._qactive == QUEST_DONE)
		return;

	if (quest._qvar1 == 1) {
		player.SaySpecific(HeroSpeech::YourDeathWillBeAvenged);
		return;
	}

	quest._qactive = QUEST_ACTIVE;
	quest._qlog = true;
	quest._qmsg = TEXT_BUTCH9;
	quest._qvar1 = 1;
	InitQTextMsg(TEXT_BUTCH9);
	NetSendCmdQuest(true, quest);
}

void TalkToBlackSmith(Player &player, Towner &blackSmith)
{
	if (Quests[Q_ROCK]._qactive != QUEST_NOTAVAIL) {
		if ((player._pLvlVisited[4] || player._pLvlVisited[5]) && Quests[Q_ROCK]._qactive != QUEST_DONE) {
			if (Quests[Q_ROCK]._qvar2 == 0) {
				Quests[Q_ROCK]._qvar2 = 1;
				Quests[Q_ROCK]._qlog = true;
				if (Quests[Q_ROCK]._qactive == QUEST_INIT) {
					Quests[Q_ROCK]._qactive = QUEST_ACTIVE;
				}
				NetSendCmdQuest(true, Quests[Q_ROCK]);
				InitQTextMsg(TEXT_INFRA5);
				return;
			}

			if (Quests[Q_ROCK]._qvar2 == 1 && RemoveInventoryItemById(player, IDI_ROCK)) {
				Quests[Q_ROCK]._qactive = QUEST_DONE;
				NetSendCmdQuest(true, Quests[Q_ROCK]);
				SpawnUnique(UITEM_INFRARING, blackSmith.position + Direction::SouthWest, Quests[Q_ROCK]._qlevel);
				InitQTextMsg(TEXT_INFRA7);
				return;
			}
		}
	}
	if (IsNoneOf(Quests[Q_ANVIL]._qactive, QUEST_NOTAVAIL, QUEST_DONE)) {
		if ((player._pLvlVisited[9] || player._pLvlVisited[10]) && Quests[Q_ANVIL]._qvar2 == 0) {
			Quests[Q_ANVIL]._qvar2 = 1;
			Quests[Q_ANVIL]._qlog = true;
			if (Quests[Q_ANVIL]._qactive == QUEST_INIT) {
				Quests[Q_ANVIL]._qactive = QUEST_ACTIVE;
			}
			NetSendCmdQuest(true, Quests[Q_ANVIL]);
			InitQTextMsg(TEXT_ANVIL5);
			return;
		}

		if (Quests[Q_ANVIL]._qvar2 == 1 && RemoveInventoryItemById(player, IDI_ANVIL)) {
			Quests[Q_ANVIL]._qactive = QUEST_DONE;
			NetSendCmdQuest(true, Quests[Q_ANVIL]);
			SpawnUnique(UITEM_GRISWOLD, blackSmith.position + Direction::SouthWest, Quests[Q_ANVIL]._qlevel);
			InitQTextMsg(TEXT_ANVIL7);
			return;
		}
	}

	TownerTalk(TEXT_GRISWOLD1);
	StartStore(TalkID::Smith);
}

void TalkToWitch(Player &player, Towner & /*witch*/)
{
	if (Quests[Q_MUSHROOM]._qactive != QUEST_NOTAVAIL) {
		if (Quests[Q_MUSHROOM]._qactive == QUEST_INIT && RemoveInventoryItemById(player, IDI_FUNGALTM)) {
			Quests[Q_MUSHROOM]._qactive = QUEST_ACTIVE;
			Quests[Q_MUSHROOM]._qlog = true;
			Quests[Q_MUSHROOM]._qvar1 = QS_TOMEGIVEN;
			NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
			InitQTextMsg(TEXT_MUSH8);
			return;
		}
		if (Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE) {
			if (Quests[Q_MUSHROOM]._qvar1 >= QS_TOMEGIVEN && Quests[Q_MUSHROOM]._qvar1 < QS_MUSHGIVEN) {
				if (RemoveInventoryItemById(player, IDI_MUSHROOM)) {
					Quests[Q_MUSHROOM]._qvar1 = QS_MUSHGIVEN;
					QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_MUSH3;
					QuestDialogTable[TOWN_WITCH][Q_MUSHROOM] = TEXT_NONE;
					Quests[Q_MUSHROOM]._qmsg = TEXT_MUSH10;
					NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
					InitQTextMsg(TEXT_MUSH10);
					return;
				}
				if (Quests[Q_MUSHROOM]._qmsg != TEXT_MUSH9) {
					Quests[Q_MUSHROOM]._qmsg = TEXT_MUSH9;
					NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
					InitQTextMsg(TEXT_MUSH9);
					return;
				}
			}
			if (Quests[Q_MUSHROOM]._qvar1 >= QS_MUSHGIVEN) {
				if (HasInventoryItemWithId(player, IDI_BRAIN)) {
					Quests[Q_MUSHROOM]._qmsg = TEXT_MUSH11;
					NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
					InitQTextMsg(TEXT_MUSH11);
					return;
				}
				if (HasInventoryOrBeltItemWithId(player, IDI_SPECELIX)) {
					Quests[Q_MUSHROOM]._qactive = QUEST_DONE;
					NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
					InitQTextMsg(TEXT_MUSH12);
					return;
				}
			}
		}
	}

	TownerTalk(TEXT_ADRIA1);
	StartStore(TalkID::Witch);
}

void TalkToBarmaid(Player &player, Towner & /*barmaid*/)
{
	if (!player._pLvlVisited[21] && HasInventoryItemWithId(player, IDI_MAPOFDOOM) && Quests[Q_GRAVE]._qmsg != TEXT_GRAVE8) {
		Quests[Q_GRAVE]._qactive = QUEST_ACTIVE;
		Quests[Q_GRAVE]._qlog = true;
		Quests[Q_GRAVE]._qmsg = TEXT_GRAVE8;
		NetSendCmdQuest(true, Quests[Q_GRAVE]);
		InitQTextMsg(TEXT_GRAVE8);
		return;
	}

	TownerTalk(TEXT_GILLIAN1);
	StartStore(TalkID::Barmaid);
}

void TalkToDrunk(Player & /*player*/, Towner & /*drunk*/)
{
	TownerTalk(TEXT_FARNHAM1);
	StartStore(TalkID::Drunk);
}

void TalkToHealer(Player &player, Towner &healer)
{
	Quest &poisonWater = Quests[Q_PWATER];
	if (poisonWater._qactive != QUEST_NOTAVAIL) {
		if ((poisonWater._qactive == QUEST_INIT && (player._pLvlVisited[1] || player._pLvlVisited[5])) || (poisonWater._qactive == QUEST_ACTIVE && !poisonWater._qlog)) {
			// Play the dialog and make the quest visible in the log if the player has not started the quest but has
			// visited the dungeon at least once, or if they've found the poison water cave before speaking to Pepin
			poisonWater._qactive = QUEST_ACTIVE;
			poisonWater._qlog = true;
			poisonWater._qmsg = TEXT_POISON3;
			InitQTextMsg(TEXT_POISON3);
			NetSendCmdQuest(true, poisonWater);
			return;
		}
		if (poisonWater._qactive == QUEST_DONE && poisonWater._qvar1 != 2) {
			poisonWater._qvar1 = 2;
			InitQTextMsg(TEXT_POISON5);
			SpawnUnique(UITEM_TRING, healer.position + Direction::SouthWest, poisonWater._qlevel);
			NetSendCmdQuest(true, poisonWater);
			return;
		}
	}
	Quest &blackMushroom = Quests[Q_MUSHROOM];
	if (blackMushroom._qactive == QUEST_ACTIVE) {
		if (blackMushroom._qvar1 >= QS_MUSHGIVEN && blackMushroom._qvar1 < QS_BRAINGIVEN && RemoveInventoryItemById(player, IDI_BRAIN)) {
			SpawnQuestItem(IDI_SPECELIX, healer.position + Displacement { 0, 1 }, 0, SelectionRegion::None, true);
			InitQTextMsg(TEXT_MUSH4);
			blackMushroom._qvar1 = QS_BRAINGIVEN;
			QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_NONE;
			NetSendCmdQuest(true, blackMushroom);
			return;
		}
	}

	TownerTalk(TEXT_PEPIN1);
	StartStore(TalkID::Healer);
}

void TalkToBoy(Player & /*player*/, Towner & /*boy*/)
{
	TownerTalk(TEXT_WIRT1);
	StartStore(TalkID::Boy);
}

void TalkToStoryteller(Player &player, Towner & /*storyteller*/)
{
	auto &betrayerQuest = Quests[Q_BETRAYER];
	if (!UseMultiplayerQuests()) {
		if (betrayerQuest._qactive == QUEST_INIT && RemoveInventoryItemById(player, IDI_LAZSTAFF)) {
			InitQTextMsg(TEXT_VILE1);
			betrayerQuest._qlog = true;
			betrayerQuest._qactive = QUEST_ACTIVE;
			betrayerQuest._qvar1 = 2;
			NetSendCmdQuest(true, betrayerQuest);
			return;
		}
	} else {
		if (betrayerQuest._qactive == QUEST_ACTIVE && !betrayerQuest._qlog) {
			InitQTextMsg(TEXT_VILE1);
			betrayerQuest._qlog = true;
			NetSendCmdQuest(true, betrayerQuest);
			return;
		}
	}
	if (betrayerQuest._qactive == QUEST_DONE && betrayerQuest._qvar1 == 7) {
		betrayerQuest._qvar1 = 8;
		InitQTextMsg(TEXT_VILE3);
		auto &diabloQuest = Quests[Q_DIABLO];
		diabloQuest._qlog = true;
		if (gbIsMultiplayer) {
			NetSendCmdQuest(true, betrayerQuest);
			NetSendCmdQuest(true, diabloQuest);
		}
		return;
	}

	TownerTalk(TEXT_STORY1);
	StartStore(TalkID::Storyteller);
}

void TalkToCow(Player &player, Towner &cow)
{
	if (CowPlaying != SfxID::None && effect_is_playing(CowPlaying))
		return;

	CowClicks++;

	CowPlaying = SfxID::Cow1;
	if (CowClicks == 4) {
		if (gbIsSpawn)
			CowClicks = 0;

		CowPlaying = SfxID::Cow2;
	} else if (CowClicks >= 8 && !gbIsSpawn) {
		CowClicks = 4;

		static const HeroSpeech SnSfx[3] = {
			HeroSpeech::YepThatsACowAlright,
			HeroSpeech::ImNotThirsty,
			HeroSpeech::ImNoMilkmaid,
		};
		player.SaySpecific(SnSfx[CowMsg]);
		CowMsg++;
		if (CowMsg >= 3)
			CowMsg = 0;
	}

	PlaySfxLoc(CowPlaying, cow.position);
}

void TalkToFarmer(Player &player, Towner &farmer)
{
	auto &quest = Quests[Q_FARMER];
	switch (quest._qactive) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
		if (HasInventoryItemWithId(player, IDI_RUNEBOMB)) {
			InitQTextMsg(TEXT_FARMER2);
			quest._qactive = QUEST_ACTIVE;
			quest._qvar1 = 1;
			quest._qmsg = TEXT_FARMER1;
			quest._qlog = true;
			if (gbIsMultiplayer)
				NetSendCmdQuest(true, quest);
			break;
		}

		if (!player._pLvlVisited[9] && player.getCharacterLevel() < 15) {
			_speech_id qt = TEXT_FARMER8;
			if (player._pLvlVisited[2])
				qt = TEXT_FARMER5;
			if (player._pLvlVisited[5])
				qt = TEXT_FARMER7;
			if (player._pLvlVisited[7])
				qt = TEXT_FARMER9;
			InitQTextMsg(qt);
			break;
		}

		InitQTextMsg(TEXT_FARMER1);
		quest._qactive = QUEST_ACTIVE;
		quest._qvar1 = 1;
		quest._qlog = true;
		quest._qmsg = TEXT_FARMER1;
		SpawnRuneBomb(farmer.position + Displacement { 1, 0 }, true);
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	case QUEST_ACTIVE:
		InitQTextMsg(HasInventoryItemWithId(player, IDI_RUNEBOMB) ? TEXT_FARMER2 : TEXT_FARMER3);
		break;
	case QUEST_DONE:
		InitQTextMsg(TEXT_FARMER4);
		SpawnRewardItem(IDI_AURIC, farmer.position + Displacement { 1, 0 }, true);
		quest._qactive = QUEST_HIVE_DONE;
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	case QUEST_HIVE_DONE:
		break;
	default:
		InitQTextMsg(TEXT_FARMER4);
		break;
	}
}

void TalkToCowFarmer(Player &player, Towner &cowFarmer)
{
	if (RemoveInventoryItemById(player, IDI_GREYSUIT)) {
		InitQTextMsg(TEXT_JERSEY7);
		return;
	}

	auto &quest = Quests[Q_JERSEY];

	if (RemoveInventoryItemById(player, IDI_BROWNSUIT)) {
		SpawnUnique(UITEM_BOVINE, cowFarmer.position + Direction::SouthEast, quest._qlevel);
		InitQTextMsg(TEXT_JERSEY8);
		quest._qactive = QUEST_DONE;
		UpdateCowFarmerAnimAfterQuestComplete();
		NetSendCmdQuest(true, quest);
		return;
	}

	if (HasInventoryItemWithId(player, IDI_RUNEBOMB)) {
		InitQTextMsg(TEXT_JERSEY5);
		quest._qactive = QUEST_ACTIVE;
		quest._qvar1 = 1;
		quest._qmsg = TEXT_JERSEY4;
		quest._qlog = true;
		NetSendCmdQuest(true, quest);
		return;
	}

	switch (quest._qactive) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
		InitQTextMsg(TEXT_JERSEY1);
		quest._qactive = QUEST_HIVE_TEASE1;
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	case QUEST_DONE:
		InitQTextMsg(TEXT_JERSEY1);
		break;
	case QUEST_HIVE_TEASE1:
		InitQTextMsg(TEXT_JERSEY2);
		quest._qactive = QUEST_HIVE_TEASE2;
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	case QUEST_HIVE_TEASE2:
		InitQTextMsg(TEXT_JERSEY3);
		quest._qactive = QUEST_HIVE_ACTIVE;
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	case QUEST_HIVE_ACTIVE:
		if (!player._pLvlVisited[9] && player.getCharacterLevel() < 15) {
			InitQTextMsg(PickRandomlyAmong({ TEXT_JERSEY9, TEXT_JERSEY10, TEXT_JERSEY11, TEXT_JERSEY12 }));
			break;
		}

		InitQTextMsg(TEXT_JERSEY4);
		quest._qactive = QUEST_ACTIVE;
		quest._qvar1 = 1;
		quest._qmsg = TEXT_JERSEY4;
		quest._qlog = true;
		SpawnRuneBomb(cowFarmer.position + Displacement { 1, 0 }, true);
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		break;
	default:
		InitQTextMsg(TEXT_JERSEY5);
		break;
	}
}

void TalkToGirl(Player &player, Towner &girl)
{
	auto &quest = Quests[Q_GIRL];

	if (quest._qactive != QUEST_DONE && RemoveInventoryItemById(player, IDI_THEODORE)) {
		InitQTextMsg(TEXT_GIRL4);
		CreateAmulet(girl.position, 13, false, false, true);
		quest._qactive = QUEST_DONE;
		UpdateGirlAnimAfterQuestComplete();
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		return;
	}

	switch (quest._qactive) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
		InitQTextMsg(TEXT_GIRL2);
		quest._qactive = QUEST_ACTIVE;
		quest._qvar1 = 1;
		quest._qlog = true;
		quest._qmsg = TEXT_GIRL2;
		if (gbIsMultiplayer)
			NetSendCmdQuest(true, quest);
		return;
	case QUEST_ACTIVE:
		InitQTextMsg(TEXT_GIRL3);
		return;
	default:
		return;
	}
}

const TownerData TownersData[] = {
	// clang-format off
	// type         position    dir                   init           talk
	{ TOWN_SMITH,   { 62, 63 }, Direction::SouthWest, InitSmith,     TalkToBlackSmith  },
	{ TOWN_HEALER,  { 55, 79 }, Direction::SouthEast, InitHealer,    TalkToHealer      },
	{ TOWN_DEADGUY, { 24, 32 }, Direction::North,     InitTownDead,  TalkToDeadguy     },
	{ TOWN_TAVERN,  { 55, 62 }, Direction::SouthWest, InitBarOwner,  TalkToBarOwner    },
	{ TOWN_STORY,   { 62, 71 }, Direction::South,     InitTeller,    TalkToStoryteller },
	{ TOWN_DRUNK,   { 71, 84 }, Direction::South,     InitDrunk,     TalkToDrunk       },
	{ TOWN_WITCH,   { 80, 20 }, Direction::South,     InitWitch,     TalkToWitch       },
	{ TOWN_BMAID,   { 43, 66 }, Direction::South,     InitBarmaid,   TalkToBarmaid     },
	{ TOWN_PEGBOY,  { 11, 53 }, Direction::South,     InitBoy,       TalkToBoy         },
	{ TOWN_COW,     { 58, 16 }, Direction::SouthWest, InitCows,      TalkToCow         },
	{ TOWN_COW,     { 56, 14 }, Direction::NorthWest, InitCows,      TalkToCow         },
	{ TOWN_COW,     { 59, 20 }, Direction::North,     InitCows,      TalkToCow         },
	{ TOWN_COWFARM, { 61, 22 }, Direction::SouthWest, InitCowFarmer, TalkToCowFarmer   },
	{ TOWN_FARMER,  { 62, 16 }, Direction::South,     InitFarmer,    TalkToFarmer      },
	{ TOWN_GIRL,    { 77, 43 }, Direction::South,     InitGirl,      TalkToGirl        },
	// clang-format on
};

} // namespace

Towner Towners[NUM_TOWNERS];

const char *const TownerLongNames[NUM_TOWNER_TYPES] {
	N_("Griswold the Blacksmith"),
	N_("Pepin the Healer"),
	N_("Wounded Townsman"),
	N_("Ogden the Tavern owner"),
	N_("Cain the Elder"),
	N_("Farnham the Drunk"),
	N_("Adria the Witch"),
	N_("Gillian the Barmaid"),
	N_("Wirt the Peg-legged boy"),
	N_("Cow"),
	N_("Lester the farmer"),
	N_("Celia"),
	N_("Complete Nut")
};

/** Contains the data related to quest gossip for each towner ID. */
_speech_id QuestDialogTable[NUM_TOWNER_TYPES][MAXQUESTS] = {
	// clang-format off
	//                 Q_ROCK,       Q_MUSHROOM,  Q_GARBUD,  Q_ZHAR,    Q_VEIL,     Q_DIABLO,   Q_BUTCHER,   Q_LTBANNER,   Q_BLIND,     Q_BLOOD,     Q_ANVIL,      Q_WARLORD,    Q_SKELKING,  Q_PWATER,      Q_SCHAMB,   Q_BETRAYER,  Q_GRAVE,     Q_FARMER,  Q_GIRL,    Q_TRADER,  Q_DEFILER, Q_NAKRUL,  Q_CORNSTN, Q_JERSEY
	/*TOWN_SMITH*/   { TEXT_INFRA6,  TEXT_MUSH6,  TEXT_NONE, TEXT_NONE, TEXT_VEIL5, TEXT_NONE,  TEXT_BUTCH5, TEXT_BANNER6, TEXT_BLIND5, TEXT_BLOOD5, TEXT_ANVIL6,  TEXT_WARLRD5, TEXT_KING7,  TEXT_POISON7,  TEXT_BONE5, TEXT_VILE9,  TEXT_GRAVE2, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_HEALER*/  { TEXT_INFRA3,  TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_VEIL3, TEXT_NONE,  TEXT_BUTCH3, TEXT_BANNER4, TEXT_BLIND3, TEXT_BLOOD3, TEXT_ANVIL3,  TEXT_WARLRD3, TEXT_KING5,  TEXT_POISON4,  TEXT_BONE3, TEXT_VILE7,  TEXT_GRAVE3, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_DEADGUY*/ { TEXT_NONE,    TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE,  TEXT_NONE,  TEXT_NONE,   TEXT_NONE,    TEXT_NONE,   TEXT_NONE,   TEXT_NONE,    TEXT_NONE,    TEXT_NONE,   TEXT_NONE,     TEXT_NONE,  TEXT_NONE,   TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_TAVERN*/  { TEXT_INFRA2,  TEXT_MUSH2,  TEXT_NONE, TEXT_NONE, TEXT_VEIL2, TEXT_NONE,  TEXT_BUTCH2, TEXT_NONE,    TEXT_BLIND2, TEXT_BLOOD2, TEXT_ANVIL2,  TEXT_WARLRD2, TEXT_KING3,  TEXT_POISON2,  TEXT_BONE2, TEXT_VILE4,  TEXT_GRAVE5, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_STORY*/   { TEXT_INFRA1,  TEXT_MUSH1,  TEXT_NONE, TEXT_NONE, TEXT_VEIL1, TEXT_VILE3, TEXT_BUTCH1, TEXT_BANNER1, TEXT_BLIND1, TEXT_BLOOD1, TEXT_ANVIL1,  TEXT_WARLRD1, TEXT_KING1,  TEXT_POISON1,  TEXT_BONE1, TEXT_VILE2,  TEXT_GRAVE6, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_DRUNK*/   { TEXT_INFRA8,  TEXT_MUSH7,  TEXT_NONE, TEXT_NONE, TEXT_VEIL6, TEXT_NONE,  TEXT_BUTCH6, TEXT_BANNER7, TEXT_BLIND6, TEXT_BLOOD6, TEXT_ANVIL8,  TEXT_WARLRD6, TEXT_KING8,  TEXT_POISON8,  TEXT_BONE6, TEXT_VILE10, TEXT_GRAVE7, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_WITCH*/   { TEXT_INFRA9,  TEXT_MUSH9,  TEXT_NONE, TEXT_NONE, TEXT_VEIL7, TEXT_NONE,  TEXT_BUTCH7, TEXT_BANNER8, TEXT_BLIND7, TEXT_BLOOD7, TEXT_ANVIL9,  TEXT_WARLRD7, TEXT_KING9,  TEXT_POISON9,  TEXT_BONE7, TEXT_VILE11, TEXT_GRAVE1, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_BMAID*/   { TEXT_INFRA4,  TEXT_MUSH5,  TEXT_NONE, TEXT_NONE, TEXT_VEIL4, TEXT_NONE,  TEXT_BUTCH4, TEXT_BANNER5, TEXT_BLIND4, TEXT_BLOOD4, TEXT_ANVIL4,  TEXT_WARLRD4, TEXT_KING6,  TEXT_POISON6,  TEXT_BONE4, TEXT_VILE8,  TEXT_GRAVE8, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_PEGBOY*/  { TEXT_INFRA10, TEXT_MUSH13, TEXT_NONE, TEXT_NONE, TEXT_VEIL8, TEXT_NONE,  TEXT_BUTCH8, TEXT_BANNER9, TEXT_BLIND8, TEXT_BLOOD8, TEXT_ANVIL10, TEXT_WARLRD8, TEXT_KING10, TEXT_POISON10, TEXT_BONE8, TEXT_VILE12, TEXT_GRAVE9, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_COW*/     { TEXT_NONE,    TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE,  TEXT_NONE,  TEXT_NONE,   TEXT_NONE,    TEXT_NONE,   TEXT_NONE,   TEXT_NONE,    TEXT_NONE,    TEXT_NONE,   TEXT_NONE,     TEXT_NONE,  TEXT_NONE,   TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_FARMER*/  { TEXT_NONE,    TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE,  TEXT_NONE,  TEXT_NONE,   TEXT_NONE,    TEXT_NONE,   TEXT_NONE,   TEXT_NONE,    TEXT_NONE,    TEXT_NONE,   TEXT_NONE,     TEXT_NONE,  TEXT_NONE,   TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_GIRL*/    { TEXT_NONE,    TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE,  TEXT_NONE,  TEXT_NONE,   TEXT_NONE,    TEXT_NONE,   TEXT_NONE,   TEXT_NONE,    TEXT_NONE,    TEXT_NONE,   TEXT_NONE,     TEXT_NONE,  TEXT_NONE,   TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	/*TOWN_COWFARM*/ { TEXT_NONE,    TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE,  TEXT_NONE,  TEXT_NONE,   TEXT_NONE,    TEXT_NONE,   TEXT_NONE,   TEXT_NONE,    TEXT_NONE,    TEXT_NONE,   TEXT_NONE,     TEXT_NONE,  TEXT_NONE,   TEXT_NONE,   TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE, TEXT_NONE },
	// clang-format on
};

bool IsTownerPresent(_talker_id npc)
{
	switch (npc) {
	case TOWN_DEADGUY:
		return Quests[Q_BUTCHER]._qactive != QUEST_NOTAVAIL && Quests[Q_BUTCHER]._qactive != QUEST_DONE;
	case TOWN_FARMER:
		return gbIsHellfire && sgGameInitInfo.bCowQuest == 0 && Quests[Q_FARMER]._qactive != QUEST_HIVE_DONE;
	case TOWN_COWFARM:
		return gbIsHellfire && sgGameInitInfo.bCowQuest != 0;
	case TOWN_GIRL:
		return gbIsHellfire && sgGameInitInfo.bTheoQuest != 0 && MyPlayer->_pLvlVisited[17] && Quests[Q_GIRL]._qactive != QUEST_DONE;
	default:
		return true;
	}
}

Towner *GetTowner(_talker_id type)
{
	for (Towner &towner : Towners) {
		if (towner._ttype == type)
			return &towner;
	}
	return nullptr;
}

void InitTowners()
{
	assert(!CowSprites);

	CowSprites.emplace(LoadCelSheet("towners\\animals\\cow", 128));

	int16_t i = 0;
	for (const auto &townerData : TownersData) {
		if (!IsTownerPresent(townerData.type))
			continue;

		InitTownerInfo(i, townerData);
		i++;
	}
}

void FreeTownerGFX()
{
	for (Towner &towner : Towners) {
		towner.ownedAnim = std::nullopt;
	}
	CowSprites = std::nullopt;
}

void ProcessTowners()
{
	// BUGFIX: should be `i < numtowners`, was `i < NUM_TOWNERS`
	for (auto &towner : Towners) {
		if (towner._ttype == TOWN_DEADGUY) {
			TownDead(towner);
		}

		towner._tAnimCnt++;
		if (towner._tAnimCnt < towner._tAnimDelay) {
			continue;
		}

		towner._tAnimCnt = 0;

		if (!towner.animOrder.empty()) {
			towner._tAnimFrameCnt++;
			if (towner._tAnimFrameCnt > towner.animOrder.size() - 1)
				towner._tAnimFrameCnt = 0;

			towner._tAnimFrame = towner.animOrder[towner._tAnimFrameCnt];
			continue;
		}

		towner._tAnimFrame++;
		if (towner._tAnimFrame >= towner._tAnimLen)
			towner._tAnimFrame = 0;
	}
}

void TalkToTowner(Player &player, int t)
{
	auto &towner = Towners[t];

	if (player.position.tile.WalkingDistance(towner.position) >= 2)
		return;

	if (!player.HoldItem.isEmpty()) {
		return;
	}

	towner.talk(player, towner);
}

void UpdateGirlAnimAfterQuestComplete()
{
	Towner *girl = GetTowner(TOWN_GIRL);
	if (girl == nullptr || !girl->ownedAnim)
		return; // Girl is not spawned in town yet
	auto curFrame = girl->_tAnimFrame;
	LoadTownerAnimations(*girl, "towners\\girl\\girls1", 20, 6);
	girl->_tAnimFrame = std::min<uint8_t>(curFrame, girl->_tAnimLen - 1);
}

void UpdateCowFarmerAnimAfterQuestComplete()
{
	Towner *cowFarmer = GetTowner(TOWN_COWFARM);
	auto curFrame = cowFarmer->_tAnimFrame;
	LoadTownerAnimations(*cowFarmer, "towners\\farmer\\mfrmrn2", 15, 3);
	cowFarmer->_tAnimFrame = std::min<uint8_t>(curFrame, cowFarmer->_tAnimLen - 1);
}

#ifdef _DEBUG
bool DebugTalkToTowner(_talker_id type)
{
	if (!IsTownerPresent(type))
		return false;
	// cows have an init function that differs from the rest and isn't compatible with this code, skip them :(
	if (type == TOWN_COW)
		return false;
	SetupTownStores();
	Player &myPlayer = *MyPlayer;
	for (const TownerData &townerData : TownersData) {
		if (townerData.type != type) continue;
		Towner fakeTowner;
		InitTownerInfo(fakeTowner, townerData);
		fakeTowner.position = myPlayer.position.tile;
		townerData.talk(myPlayer, fakeTowner);
		return true;
	}
	return false;
}
#endif

} // namespace devilution
