/**
 * @file stores.h
 *
 * Interface of functionality for stores and towner dialogs.
 */
#pragma once

#include <cstdint>
#include <optional>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"
#include "game_mode.hpp"
#include "inv.h"
#include "towners.h"
#include "utils/attributes.h"

namespace devilution {

/** @brief Number of player items that display in stores (Inventory slots and belt slots) */
const int NumPlayerItems = (NUM_XY_SLOTS - (SLOTXY_EQUIPPED_LAST + 1));

constexpr int NumSmithBasicItems = 19;
constexpr int NumSmithBasicItemsHf = 24;

constexpr int NumSmithItems = 6;
constexpr int NumSmithItemsHf = 15;

constexpr int NumHealerItems = 17;
constexpr int NumHealerItemsHf = 19;
constexpr int NumHealerPinnedItems = 2;
constexpr int NumHealerPinnedItemsMp = 3;

constexpr int NumWitchItems = 17;
constexpr int NumWitchItemsHf = 24;
constexpr int NumWitchPinnedItems = 3;

constexpr int NumBoyItems = 1;

constexpr int NumStoreLines = 104;

extern _talker_id TownerId;

enum class TalkID : uint8_t {
	None,
	MainMenu,
	BasicBuy,
	Buy,
	Sell,
	Repair,
	Recharge,
	Identify,
	IdentifyShow,
	Stash,
	NoMoney,
	NoRoom,
	Confirm,
	Gossip,
	Invalid,
};

enum class ItemLocation {
	Inventory,
	Belt,
	Body
};

struct StoreMenuOption {
	TalkID action;
	std::string text;
};

struct StoreData {
	std::string_view name;
	std::string_view welcomeMessage;
	std::vector<StoreMenuOption> menuOptions;
};

struct IndexedItem {
	/** Pointer to the original item */
	Item *itemPtr;
	/** Location in the player's inventory (Inventory, Belt, or Body) */
	ItemLocation location;
	/** Index in the corresponding array */
	int index;
};

enum class ResourceType {
	Life,
	Mana,
	Invalid,
};

/** Currently active store */
extern TalkID ActiveStore;
/** Pointers to player items, coupled with necessary information */
extern DVL_API_FOR_TEST std::vector<IndexedItem> playerItems;

class TownerStore {
public:
	TownerStore(TalkID buyBasic, TalkID buy, TalkID sell, TalkID special, ResourceType resource)
	    : buyBasic(buyBasic)
	    , buy(buy)
	    , sell(sell)
	    , special(special)
	    , resourceType(resource)
	{
	}

	/** Used for the blacksmith store that only displays non-magical items */
	std::vector<Item> basicItems;
	std::vector<Item> items;
	uint8_t itemLevel;

	TalkID buyBasic;
	TalkID buy;
	TalkID sell;
	TalkID special;
	/** Resource type to restore for stores that restore player's resources */
	ResourceType resourceType;
};

extern TownerStore Blacksmith;
extern TownerStore Healer;
extern TownerStore Witch;
extern TownerStore Boy;
extern TownerStore Storyteller;
extern TownerStore Barmaid;

void FilterRepairableItems();
/** Clears premium items sold by Griswold and Wirt. */
void InitStores();
/** Spawns items sold by vendors, including premium items sold by Griswold and Wirt. */
void SetupTownStores();
void FreeStoreMem();
void ExitStore();
void PrintSString(const Surface &out, int margin, int line, std::string_view text, UiFlags flags, int price = 0, int cursId = -1, bool cursIndent = false);
void DrawSLine(const Surface &out, int sy);
void DrawSTextHelp();
void ClearTextLines(int start, int end);
void StartStore(TalkID s = TalkID::MainMenu);
void DrawStore(const Surface &out);
void StoreESC();
void StoreUp();
void StoreDown();
void StorePrior();
void StoreNext();
void TakePlrsMoney(int cost);
void StoreEnter();
void CheckStoreButton();
void ReleaseStoreButton();
bool IsPlayerInStore();

} // namespace devilution
