#include "towners.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>

#include "controls/local_coop.hpp"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "game_mode.hpp"
#include "inv.h"
#include "minitext.h"
#include "stores.h"
#include "textdat.h"
#include "townerdat.hpp"
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

/** Storage for animation order data loaded from TSV (needs stable addresses for span). */
std::vector<std::vector<uint8_t>> TownerAnimOrderStorage;

/**
 * @brief Defines the behavior (init and talk functions) for each towner type.
 *
 * The actual data (position, animation, gossip) comes from TSV files.
 * This struct only holds the code that can't be data-driven.
 */
struct TownerData {
	_talker_id type;
	/** Custom initialization function, or nullptr to use the default InitTownerFromData. */
	void (*init)(Towner &towner, const TownerDataEntry &entry);
	/** Function called when the player talks to this towner. */
	void (*talk)(Player &player, Towner &towner);
};

/**
 * @brief Lookup table from towner type to its behavior data.
 *
 * Populated during InitTowners() from the TownersData array.
 */
std::unordered_map<_talker_id, const TownerData *> TownerBehaviors;

/**
 * @brief Default towner initialization using TSV data.
 *
 * Sets up animation, gossip texts, and other properties from the TSV entry.
 * Used for most towners; special cases (cows, cow farmer) have custom init functions.
 */
void InitTownerFromData(Towner &towner, const TownerDataEntry &entry);

#ifdef _DEBUG
/**
 * @brief Finds the towner data entry from TSV for a given type.
 */
const TownerDataEntry *FindTownerDataEntry(_talker_id type, Point position = {})
{
	for (const auto &entry : TownersDataEntries) {
		if (entry.type == type) {
			// For types with multiple instances (like cows), match by position
			if (position != Point {} && entry.position != position)
				continue;
			return &entry;
		}
	}
	return nullptr;
}
#endif

void NewTownerAnim(Towner &towner, ClxSpriteList sprites, uint8_t numFrames, int delay)
{
	towner.anim.emplace(sprites);
	towner._tAnimLen = numFrames;
	towner._tAnimFrame = 0;
	towner._tAnimCnt = 0;
	towner._tAnimDelay = delay;
}

void InitTownerInfo(Towner &towner, const TownerData &townerData, const TownerDataEntry &entry)
{
	towner._ttype = townerData.type;
	auto nameIt = TownerLongNames.find(townerData.type);
	towner.name = nameIt != TownerLongNames.end() ? _(nameIt->second.c_str()) : std::string_view(entry.name);
	towner.position = entry.position;
	towner.talk = townerData.talk;

	if (townerData.init != nullptr) {
		townerData.init(towner, entry);
	} else {
		InitTownerFromData(towner, entry);
	}
}

void LoadTownerAnimations(Towner &towner, const char *path, int frames, int delay)
{
	towner.ownedAnim = std::nullopt;
	towner.ownedAnim = LoadCel(path, towner._tAnimWidth);
	NewTownerAnim(towner, *towner.ownedAnim, frames, delay);
}

/**
 * @brief Default towner initialization using TSV data.
 */
void InitTownerFromData(Towner &towner, const TownerDataEntry &entry)
{
	towner._tAnimWidth = entry.animWidth;

	// Store animation order and set the span
	if (!entry.animOrder.empty()) {
		TownerAnimOrderStorage.push_back(entry.animOrder);
		towner.animOrder = { TownerAnimOrderStorage.back() };
	} else {
		towner.animOrder = {};
	}

	if (!entry.animPath.empty()) {
		LoadTownerAnimations(towner, entry.animPath.c_str(), entry.animFrames, entry.animDelay);
	}

	// Set gossip from TSV data
	if (!entry.gossipTexts.empty()) {
		const auto index = std::max<int32_t>(GenerateRnd(static_cast<int32_t>(entry.gossipTexts.size())), 0);
		towner.gossip = entry.gossipTexts[index];
	}
}

/**
 * @brief Special initialization for cows.
 *
 * Cows differ from other towners:
 * - They share a sprite sheet (CowSprites) instead of loading individual animations
 * - They occupy multiple tiles (4 tiles for collision purposes)
 * - Animation frame is randomized on spawn
 */
void InitCows(Towner &towner, const TownerDataEntry &entry)
{
	// Cows use a shared sprite sheet and need special handling
	towner._tAnimWidth = entry.animWidth;
	towner.animOrder = {};

	NewTownerAnim(towner, (*CowSprites)[static_cast<size_t>(entry.direction)], 12, 3);
	towner._tAnimFrame = GenerateRnd(11);

	const Point position = entry.position;
	const int16_t cowId = dMonster[position.x][position.y];

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

/**
 * @brief Special initialization for the cow farmer (Complete Nut).
 *
 * Uses different sprites depending on whether the Jersey quest is complete.
 */
void InitCowFarmer(Towner &towner, const TownerDataEntry &entry)
{
	towner._tAnimWidth = entry.animWidth;
	towner.animOrder = {};

	// CowFarmer has special logic for quest state
	const char *celPath = "towners\\farmer\\cfrmrn2";
	if (Quests[Q_JERSEY]._qactive == QUEST_DONE) {
		celPath = "towners\\farmer\\mfrmrn2";
	}
	LoadTownerAnimations(towner, celPath, 15, 3);
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
					SetTownerQuestDialog(TOWN_HEALER, Q_MUSHROOM, TEXT_MUSH3);
					SetTownerQuestDialog(TOWN_WITCH, Q_MUSHROOM, TEXT_NONE);
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
			SetTownerQuestDialog(TOWN_HEALER, Q_MUSHROOM, TEXT_NONE);
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
	// type          init (nullptr = default)  talk
	{ TOWN_SMITH,   nullptr,       TalkToBlackSmith  },
	{ TOWN_HEALER,  nullptr,       TalkToHealer      },
	{ TOWN_DEADGUY, nullptr,       TalkToDeadguy     },
	{ TOWN_TAVERN,  nullptr,       TalkToBarOwner    },
	{ TOWN_STORY,   nullptr,       TalkToStoryteller },
	{ TOWN_DRUNK,   nullptr,       TalkToDrunk       },
	{ TOWN_WITCH,   nullptr,       TalkToWitch       },
	{ TOWN_BMAID,   nullptr,       TalkToBarmaid     },
	{ TOWN_PEGBOY,  nullptr,       TalkToBoy         },
	{ TOWN_COW,     InitCows,      TalkToCow         },
	{ TOWN_COWFARM, InitCowFarmer, TalkToCowFarmer   },
	{ TOWN_FARMER,  nullptr,       TalkToFarmer      },
	{ TOWN_GIRL,    nullptr,       TalkToGirl        },
	// clang-format on
};

} // namespace

std::vector<Towner> Towners;

std::unordered_map<_talker_id, std::string> TownerLongNames;

size_t GetNumTownerTypes()
{
	return TownerLongNames.size();
}

size_t GetNumTowners()
{
	return Towners.size();
}

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

	// Load towner data from TSV files
	LoadTownerData();
	TownerAnimOrderStorage.clear();

	// Build lookup table for towner behaviors
	TownerBehaviors.clear();
	for (const auto &behavior : TownersData) {
		TownerBehaviors[behavior.type] = &behavior;
	}

	// Build TownerLongNames from TSV data (first occurrence of each type wins)
	TownerLongNames.clear();
	for (const auto &entry : TownersDataEntries) {
		TownerLongNames.try_emplace(entry.type, entry.name);
	}

	CowSprites.emplace(LoadCelSheet("towners\\animals\\cow", 128));

	Towners.clear();
	Towners.reserve(TownersDataEntries.size());
	int16_t i = 0;
	for (const auto &entry : TownersDataEntries) {
		if (!IsTownerPresent(entry.type))
			continue;

		auto behaviorIt = TownerBehaviors.find(entry.type);
		if (behaviorIt == TownerBehaviors.end() || behaviorIt->second == nullptr)
			continue;

		// It's necessary to assign this before invoking townerData.init()
		// specifically for the cows that need to read this value to fill adjacent tiles
		dMonster[entry.position.x][entry.position.y] = i + 1;

		Towners.emplace_back();
		InitTownerInfo(Towners.back(), *behaviorIt->second, entry);
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

	// Handle store ownership for local co-op
	// Player 0 is the main player (MyPlayer), players 1+ are local co-op players
	if (IsLocalCoopEnabled()) {
		int localIndex = PlayerIdToLocalCoopIndex(player.getId());
		if (localIndex >= 0) {
			// This is a coop player - set them as store owner
			SetLocalCoopStoreOwner(localIndex);
		} else if (IsLocalCoopStoreActive()) {
			// Player 1 trying to talk while a coop player owns the store - don't allow
			return;
		}
		// Player 1 talking when no coop player owns store - that's fine, no action needed
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

	const TownerData *behavior = TownerBehaviors[type];
	if (behavior == nullptr)
		return false;

	const TownerDataEntry *entry = FindTownerDataEntry(type);
	if (entry == nullptr)
		return false;

	SetupTownStores();
	Player &myPlayer = *MyPlayer;
	Towner fakeTowner;
	InitTownerInfo(fakeTowner, *behavior, *entry);
	fakeTowner.position = myPlayer.position.tile;
	behavior->talk(myPlayer, fakeTowner);
	return true;
}
#endif

} // namespace devilution
