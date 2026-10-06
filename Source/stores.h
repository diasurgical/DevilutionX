/**
 * @file stores.h
 *
 * Interface of functionality for stores and towner dialogs.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "control/control.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"
#include "game_mode.hpp"
#include "items.h"
#include "utils/attributes.h"
#include "utils/static_vector.hpp"

namespace devilution {

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

constexpr int NumStoreLines = 104;

enum class TalkID : uint8_t {
	None,
	Smith,
	SmithBuy,
	SmithSell,
	SmithRepair,
	Witch,
	WitchBuy,
	WitchSell,
	WitchRecharge,
	NoMoney,
	NoRoom,
	Confirm,
	Boy,
	BoyBuy,
	Healer,
	Storyteller,
	HealerBuy,
	StorytellerIdentify,
	SmithPremiumBuy,
	Gossip,
	StorytellerIdentifyShow,
	Tavern,
	Drunk,
	Barmaid,
};

/** Currently active store */
extern DVL_API_FOR_TEST TalkID ActiveStore;

/** Current index into PlayerItemIndexes/PlayerItems */
extern DVL_API_FOR_TEST int CurrentItemIndex;
/** Map of inventory items being presented in the store */
extern int8_t PlayerItemIndexes[48];
/** Copies of the players items as presented in the store */
extern DVL_API_FOR_TEST Item PlayerItems[48];

/** Items sold by Griswold */
extern DVL_API_FOR_TEST StaticVector<Item, NumSmithBasicItemsHf> SmithItems;
/** Number of premium items for sale by Griswold */
extern DVL_API_FOR_TEST int PremiumItemCount;
/** Base level of current premium items sold by Griswold */
extern DVL_API_FOR_TEST int PremiumItemLevel;
/** Premium items sold by Griswold */
extern DVL_API_FOR_TEST StaticVector<Item, NumSmithItemsHf> PremiumItems;

/** Items sold by Pepin */
extern DVL_API_FOR_TEST StaticVector<Item, NumHealerItemsHf> HealerItems;

/** Items sold by Adria */
extern DVL_API_FOR_TEST StaticVector<Item, NumWitchItemsHf> WitchItems;

/** Current level of the item sold by Wirt */
extern DVL_API_FOR_TEST int BoyItemLevel;
/** Current item sold by Wirt */
extern DVL_API_FOR_TEST Item BoyItem;

/** Currently selected text line from TextLine */
extern DVL_API_FOR_TEST int CurrentTextLine;
/** Remember currently selected text line from TextLine while displaying a dialog */
extern DVL_API_FOR_TEST int OldTextLine;
/** Scroll position */
extern DVL_API_FOR_TEST int ScrollPos;
/** Remember last scroll position */
extern DVL_API_FOR_TEST int OldScrollPos;
/** Remember current store while displaying a dialog */
extern DVL_API_FOR_TEST TalkID OldActiveStore;
/** Temporary item used to hold the item being traded */
extern DVL_API_FOR_TEST Item TempItem;

struct TownerDialogOption {
	std::function<std::string()> getLabel;
	std::function<void()> onSelect;
};

/** Extra dialog options injected by mods, keyed by towner short name. */
extern DVL_API_FOR_TEST std::vector<std::pair<std::string, std::vector<TownerDialogOption>>> ExtraTownerOptions;

/**
 * @brief Returns the towner short name for a top-level TalkID, or nullptr if not a towner store.
 *
 * Only maps top-level store entries (Smith, Witch, Boy, Healer, Storyteller, Tavern, Drunk, Barmaid).
 * Sub-pages (SmithBuy, WitchSell, etc.) return nullptr.
 */
DVL_API_FOR_TEST const char *TownerNameForTalkID(TalkID s);

/**
 * @brief Registers a dynamic dialog option for a towner's talk menu.
 *
 * Options are inserted into empty even-numbered lines before the towner's "leave" option.
 * If no empty lines are available the option is silently skipped for that dialog session
 * and a warning is logged.
 *
 * When the player selects a mod option, onSelect() is called. By default the dialog is
 * closed afterwards. If onSelect() sets ActiveStore to a value other than TalkID::None,
 * that value is preserved (e.g. to open a sub-dialog).
 *
 * @param townerName   Short name of the towner (e.g. "farnham"). Must match one of the
 *                     names in TownerShortNames; a warning is logged for unknown names.
 * @param getLabel     Called when the dialog is built; return a non-empty string to show the
 *                     option, or an empty string to hide it.
 * @param onSelect     Called when the player chooses this option.
 */
void RegisterTownerDialogOption(std::string_view townerName,
    std::function<std::string()> getLabel,
    std::function<void()> onSelect);

/**
 * @brief Clears all mod-registered towner dialog options.
 *
 * Must be called before the Lua state is destroyed, since registered callbacks
 * capture sol::function handles that reference the Lua state.
 */
void ClearTownerDialogOptions();

void AddStoreHoldRepair(Item *itm, int8_t i);

/** Clears premium items sold by Griswold and Wirt. */
void InitStores();

/** Spawns items sold by vendors, including premium items sold by Griswold and Wirt. */
void SetupTownStores();

void FreeStoreMem();

void PrintSString(const Surface &out, int margin, int line, std::string_view text, UiFlags flags, int price = 0, int cursId = -1, bool cursIndent = false);
void DrawSLine(const Surface &out, int sy);
void DrawSTextHelp();
void ClearSText(int s, int e);
void StartStore(TalkID s);
void DrawSText(const Surface &out);
void StoreESC();
void StoreUp();
void StoreDown();
void StorePrior();
void StoreNext();
void TakePlrsMoney(int cost);
void StoreEnter();
void CheckStoreBtn();
void ReleaseStoreBtn();
bool IsPlayerInStore();

/**
 * @brief Places an item in the player's inventory, belt, or equipment.
 * @param item The item to place.
 * @param persistItem If true, actually place the item. If false, just check if it can be placed.
 * @return true if the item can be/was placed.
 */
bool StoreAutoPlace(Item &item, bool persistItem);
bool PlayerCanAfford(int price);

/**
 * @brief Check if Griswold will buy this item.
 * @param item The item to check.
 * @return true if the item can be sold to Griswold.
 */
bool SmithWillBuy(const Item &item);

/**
 * @brief Check if Adria will buy this item.
 * @param item The item to check.
 * @return true if the item can be sold to Adria.
 */
bool WitchWillBuy(const Item &item);

} // namespace devilution
