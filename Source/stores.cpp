/**
 * @file stores.cpp
 *
 * Implementation of functionality for stores and towner dialogs.
 */
#include "stores.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "game_mode.hpp"
#include "minitext.h"
#include "multi.h"
#include "options.h"
#include "panels/info_box.hpp"
#include "qol/stash.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

TownerStore Blacksmith(TalkID::BasicBuy, TalkID::Buy, TalkID::Sell, TalkID::Repair, ResourceType::Invalid);
TownerStore Healer(TalkID::Invalid, TalkID::Buy, TalkID::Invalid, TalkID::Invalid, ResourceType::Life);
TownerStore Witch(TalkID::Invalid, TalkID::Buy, TalkID::Sell, TalkID::Recharge, ResourceType::Mana);
TownerStore Boy(TalkID::Invalid, TalkID::Buy, TalkID::Invalid, TalkID::Invalid, ResourceType::Invalid);
TownerStore Storyteller(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Identify, ResourceType::Invalid);
TownerStore Barmaid(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Stash, ResourceType::Invalid);
TownerStore Tavern(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, ResourceType::Invalid);
TownerStore Drunk(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, ResourceType::Invalid);
TownerStore CowFarmer(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, ResourceType::Invalid);
TownerStore Farmer(TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, TalkID::Invalid, ResourceType::Invalid);

TalkID ActiveStore;  // The current store screen
_talker_id TownerId; // The current towner being interacted with

std::vector<IndexedItem> playerItems;

void FilterRepairableItems()
{
	// Filter playerItems in place to only include items that can be repaired
	playerItems.erase(std::remove_if(playerItems.begin(), playerItems.end(),
	                      [](const IndexedItem &indexedItem) {
		                      const Item &itemPtr = *indexedItem.itemPtr;
		                      return itemPtr._iDurability == itemPtr._iMaxDur || itemPtr._iMaxDur == DUR_INDESTRUCTIBLE;
	                      }),
	    playerItems.end());
}

namespace {

constexpr int PaddingTop = 32;

const int SingleLineSpace = 1;
const int DoubleLineSpace = 2;
const int TripleLineSpace = 3;

constexpr int MainMenuDividerLine = 5;
constexpr int BuySellMenuDividerLine = 3;
constexpr int ItemLineSpace = 4;
constexpr int ConfirmLine = 18;

constexpr int WirtDialogueDrawLine = 12;

bool IsTextFullSize; // Is the current dialog full size
int NumTextLines;    // Number of text lines in the current dialog
int OldTextLine;     // Remember currently selected text line from TextLine while displaying a dialog
int CurrentTextLine; // Currently selected text line from TextLine

struct STextStruct {
	enum Type : uint8_t {
		Label,
		Divider,
		Selectable,
	};

	std::string text;
	int _sval;
	int y;
	UiFlags flags;
	Type type;
	uint8_t _sx;
	uint8_t _syoff;
	int cursId;
	bool cursIndent;

	[[nodiscard]] bool isDivider() const
	{
		return type == Divider;
	}
	[[nodiscard]] bool isSelectable() const
	{
		return type == Selectable;
	}

	[[nodiscard]] bool hasText() const
	{
		return !text.empty();
	}
};

std::array<STextStruct, NumStoreLines> TextLine; // Text lines

bool RenderGold;            // Whether to render the player's gold amount in the top left
int OldScrollPos;           // Remember last scroll position
int ScrollPos;              // Scroll position
int NextScrollPos;          // Next scroll position
int PreviousScrollPos;      // Previous scroll position
int8_t CountdownScrollUp;   // Countdown for the push state of the scroll up button
int8_t CountdownScrollDown; // Countdown for the push state of the scroll down button

TalkID OldActiveStore; // Remember current store while displaying a dialog

Item TempItem; // Temporary item used to hold the item being traded

std::vector<std::pair<int, TalkID>> LineActionMappings;
int CurrentMenuDrawLine;

std::vector<StoreData> Stores = {
	{ N_("Blacksmith"), N_("Welcome to the\n\nBlacksmith's shop"), { { TalkID::Gossip, N_("Talk to Griswold") }, { TalkID::BasicBuy, N_("Buy basic items") }, { TalkID::Buy, N_("Buy premium items") }, { TalkID::Sell, N_("Sell items") }, { TalkID::Repair, N_("Repair items") }, { TalkID::None, N_("Leave the shop") } } },
	{ N_("Healer"), N_("Welcome to the\n\nHealer's home"), { { TalkID::Gossip, N_("Talk to Pepin") }, { TalkID::Buy, N_("Buy items") }, { TalkID::None, N_("Leave Healer's home") } } },
	{},
	{ N_("Tavern"), N_("Welcome to the\n\nRising Sun"), { { TalkID::Gossip, N_("Talk to the Ogden") }, { TalkID::None, N_("Leave the tavern") } } },
	{ N_("Storyteller"), N_("The Town Elder"), { { TalkID::Gossip, N_("Talk to Cain") }, { TalkID::Identify, N_("Identify an item") }, { TalkID::None, N_("Say goodbye") } } },
	{ N_("Drunk"), N_("Farnham the Drunk"), { { TalkID::Gossip, N_("Talk to Farnham") }, { TalkID::None, N_("Say goodbye") } } },
	{ N_("Witch"), N_("Welcome to the\n\nWitch's shack"), { { TalkID::Gossip, N_("Talk to Adria") }, { TalkID::Buy, N_("Buy items") }, { TalkID::Sell, N_("Sell items") }, { TalkID::Recharge, N_("Recharge staves") }, { TalkID::None, N_("Leave the shack") } } },
	{ N_("Barmaid"), N_("Gillian"), { { TalkID::Gossip, N_("Talk to Gillian") }, { TalkID::Stash, N_("Access Stash") }, { TalkID::None, N_("Say goodbye") } } },
	{ N_("Boy"), N_("Wirt the Peg-legged boy"), { { TalkID::Gossip, N_("Talk to Wirt") }, { TalkID::Buy, N_("What have you got?") }, { TalkID::None, N_("Say goodbye") } } },
	{},
	{},
	{},
	{},
};

// For most languages, line height is always 12.
// This includes blank lines and divider line.
constexpr int SmallLineHeight = 12;
constexpr int SmallTextHeight = 12;

// For larger small fonts (Chinese and Japanese), text lines are
// taller and overflow.
// We space out blank lines a bit more to give space to 3-line store items.
constexpr int LargeLineHeight = SmallLineHeight + 1;
constexpr int LargeTextHeight = 18;

std::unordered_map<_talker_id, TownerStore *> townerStores;

void InitializeTownerStores()
{
	townerStores[TOWN_SMITH] = &Blacksmith;
	townerStores[TOWN_HEALER] = &Healer;
	townerStores[TOWN_WITCH] = &Witch;
	townerStores[TOWN_PEGBOY] = &Boy;
	townerStores[TOWN_STORY] = &Storyteller;
	townerStores[TOWN_BMAID] = &Barmaid;
	townerStores[TOWN_TAVERN] = &Tavern;
	townerStores[TOWN_DRUNK] = &Drunk;

	if (gbIsHellfire) {
		townerStores[TOWN_COWFARM] = &CowFarmer;
		townerStores[TOWN_FARMER] = &Farmer;
	}
}

void SetActiveStore(TalkID talkId)
{
	OldActiveStore = ActiveStore;
	ActiveStore = talkId;
}

int GetItemCount(TalkID talkId)
{
	TownerStore *towner = townerStores[TownerId];

	switch (talkId) {
	case TalkID::BasicBuy:
		return towner->basicItems.size();
	case TalkID::Buy:
		return towner->items.size();
	default:
		return playerItems.size();
	}
}

bool HasScrollbar()
{
	if (!IsAnyOf(ActiveStore, TalkID::BasicBuy, TalkID::Buy, TalkID::Sell, TalkID::Repair, TalkID::Recharge, TalkID::Identify))
		return false;

	int itemCount = GetItemCount(ActiveStore);

	return itemCount > ItemLineSpace;
}

/**
 * The line index with the Back / Leave button.
 * This is a special button that is always the last line.
 *
 * For lists with a scrollbar, it is not selectable (mouse-only).
 */
int BackButtonLine()
{
	if (IsSmallFontTall()) {
		return HasScrollbar() ? 21 : 20;
	}
	return 22;
}

int LineHeight()
{
	return IsSmallFontTall() ? LargeLineHeight : SmallLineHeight;
}

int TextHeight()
{
	return IsSmallFontTall() ? LargeTextHeight : SmallTextHeight;
}

void CalculateLineHeights()
{
	TextLine[0].y = 0;
	if (IsSmallFontTall()) {
		for (int i = 1; i < NumStoreLines; ++i) {
			// Space out consecutive text lines, unless they are both selectable (never the case currently).
			if (TextLine[i].hasText() && TextLine[i - 1].hasText() && !(TextLine[i].isSelectable() && TextLine[i - 1].isSelectable())) {
				TextLine[i].y = TextLine[i - 1].y + LargeTextHeight;
			} else {
				TextLine[i].y = i * LargeLineHeight;
			}
		}
	} else {
		for (int i = 1; i < NumStoreLines; ++i) {
			TextLine[i].y = i * SmallLineHeight;
		}
	}
}

void DrawTextUI(const Surface &out)
{
	const Point uiPosition = GetUIRectangle().position;
	ClxDraw(out, { uiPosition.x + 320 + 24, 327 + uiPosition.y }, (*pSTextBoxCels)[0]);
	DrawHalfTransparentRectTo(out, uiPosition.x + 347, uiPosition.y + 28, 265, 297);
}

void DrawScrollbar(const Surface &out, int y1, int y2)
{
	const int itemCount = GetItemCount(ActiveStore);
	const Point uiPosition = GetUIRectangle().position;
	int yd1 = y1 * 12 + 44 + uiPosition.y;
	int yd2 = y2 * 12 + 44 + uiPosition.y;
	if (CountdownScrollUp != -1)
		ClxDraw(out, { uiPosition.x + 601, yd1 }, (*pSTextSlidCels)[11]);
	else
		ClxDraw(out, { uiPosition.x + 601, yd1 }, (*pSTextSlidCels)[9]);
	if (CountdownScrollDown != -1)
		ClxDraw(out, { uiPosition.x + 601, yd2 }, (*pSTextSlidCels)[10]);
	else
		ClxDraw(out, { uiPosition.x + 601, yd2 }, (*pSTextSlidCels)[8]);
	yd1 += 12;
	int yd3 = yd1;
	for (; yd3 < yd2; yd3 += 12) {
		ClxDraw(out, { uiPosition.x + 601, yd3 }, (*pSTextSlidCels)[13]);
	}
	if (CurrentTextLine == BackButtonLine())
		yd3 = OldTextLine;
	else
		yd3 = CurrentTextLine;

	yd3 = 1000 * (ScrollPos + ((yd3 - PreviousScrollPos) / 4)) / (itemCount - 1) * ((y2 * 12) - (y1 * 12) - 24) / 1000;
	ClxDraw(out, { uiPosition.x + 601, (y1 + 1) * 12 + 44 + uiPosition.y + yd3 }, (*pSTextSlidCels)[12]);
}

void SetLineAsDivider(size_t y)
{
	TextLine[y]._sx = 0;
	TextLine[y]._syoff = 0;
	TextLine[y].text.clear();
	TextLine[y].text.shrink_to_fit();
	TextLine[y].type = STextStruct::Divider;
	TextLine[y].cursId = -1;
	TextLine[y].cursIndent = false;
}

void SetLineValue(size_t y, int val)
{
	TextLine[y]._sval = val;
}

void SetLineText(uint8_t x, size_t y, std::string_view text, UiFlags flags, bool sel, int cursId = -1, bool cursIndent = false)
{
	TextLine[y]._sx = x;
	TextLine[y]._syoff = 0;
	TextLine[y].text.clear();
	TextLine[y].text.append(text);
	TextLine[y].flags = flags;
	TextLine[y].type = sel ? STextStruct::Selectable : STextStruct::Label;
	TextLine[y].cursId = cursId;
	TextLine[y].cursIndent = cursIndent;
}

void SetLineAsOptionsBackButton()
{
	const int line = BackButtonLine();
	SetLineText(0, line, _("Back"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	TextLine[line]._syoff = IsSmallFontTall() ? 0 : 6;
}

void AddItemListBackButton(TalkID talkId, bool selectable = false)
{
	const int line = BackButtonLine();
	std::string_view text = (TownerId == TOWN_PEGBOY && talkId == TalkID::Buy) ? _("Leave") : _("Back");
	if (!selectable && IsSmallFontTall()) {
		SetLineText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignRight, selectable);
	} else {
		SetLineAsDivider(line - 1);
		SetLineText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignCenter, selectable);
		TextLine[line]._syoff = 6;
	}
}

void PrintStoreItem(const Item &item, int l, UiFlags flags, bool cursIndent = false)
{
	std::string productLine;

	if (item._iIdentified) {
		if (item._iMagical != ITEM_QUALITY_UNIQUE) {
			if (item._iPrePower != -1) {
				productLine.append(PrintItemPower(item._iPrePower, item));
			}
		}
		if (item._iSufPower != -1) {
			if (!productLine.empty())
				productLine.append(_(",  "));
			productLine.append(PrintItemPower(item._iSufPower, item));
		}
	}
	if (item._iMiscId == IMISC_STAFF && item._iMaxCharges != 0) {
		if (!productLine.empty())
			productLine.append(_(",  "));
		productLine.append(fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges));
	}
	if (!productLine.empty()) {
		SetLineText(40, l, productLine, flags, false, -1, cursIndent);
		l++;
		productLine.clear();
	}

	if (item._itype != ItemType::Misc) {
		if (item._iClass == ICLASS_WEAPON)
			productLine = fmt::format(fmt::runtime(_("Damage: {:d}-{:d}  ")), item._iMinDam, item._iMaxDam);
		else if (item._iClass == ICLASS_ARMOR)
			productLine = fmt::format(fmt::runtime(_("Armor: {:d}  ")), item._iAC);
		if (item._iMaxDur != DUR_INDESTRUCTIBLE && item._iMaxDur != 0)
			productLine += fmt::format(fmt::runtime(_("Dur: {:d}/{:d}")), item._iDurability, item._iMaxDur);
		else
			productLine.append(_("Indestructible"));
	}

	int8_t str = item._iMinStr;
	uint8_t mag = item._iMinMag;
	int8_t dex = item._iMinDex;

	if (str != 0 || mag != 0 || dex != 0) {
		if (!productLine.empty())
			productLine.append(_(",  "));
		productLine.append(_("Required:"));
		if (str != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Str")), str));
		if (mag != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Mag")), mag));
		if (dex != 0)
			productLine.append(fmt::format(fmt::runtime(_(" {:d} Dex")), dex));
	}
	SetLineText(40, l++, productLine, flags, false, -1, cursIndent);
}

bool GiveItemToPlayer(Item &item, bool persistItem)
{

	if (AutoEquipEnabled(*MyPlayer, item) && AutoEquip(*MyPlayer, item, persistItem, true)) {
		return true;
	}

	if (AutoPlaceItemInBelt(*MyPlayer, item, persistItem, true)) {
		return true;
	}

	if (persistItem) {
		return AutoPlaceItemInInventory(*MyPlayer, item, true);
	}

	return CanFitItemInInventory(*MyPlayer, item);
}

void SetupScreenElements(TalkID talkId)
{
	IsTextFullSize = true;
	RenderGold = true;
	ScrollPos = 0;

	SetLineAsDivider(BuySellMenuDividerLine);
	AddItemListBackButton(talkId, /*selectable=*/true);

	const UiFlags flags = UiFlags::ColorWhitegold;
	const int itemCount = GetItemCount(talkId);

	switch (talkId) {
	case TalkID::BasicBuy:
	case TalkID::Buy: {
		if (itemCount == 0) {
			SetLineText(20, 1, _("I have nothing for sale."), UiFlags::ColorWhitegold, false);
			return;
		}

		ScrollPos = 0;
		NumTextLines = std::max(itemCount - ItemLineSpace, 0);

		if (itemCount == 1) {
			SetLineText(20, 1, _("I have this item for sale:"), flags, false);
		} else {
			SetLineText(20, 1, _("I have these items for sale:"), flags, false);
		}
	} break;
	case TalkID::Sell: {
		if (itemCount == 0) {
			SetLineText(20, 1, _("You have nothing I want."), UiFlags::ColorWhitegold, false);
			return;
		}

		ScrollPos = 0;
		NumTextLines = itemCount;

		SetLineText(20, 1, _("Which item is for sale?"), UiFlags::ColorWhitegold, false);
	} break;
	case TalkID::Repair: {
		if (itemCount == 0) {
			SetLineText(20, 1, _("You have nothing to repair."), UiFlags::ColorWhitegold, false);
			return;
		}

		ScrollPos = 0;
		NumTextLines = itemCount;
		SetLineText(20, 1, _("Repair which item?"), UiFlags::ColorWhitegold, false);
	} break;
	case TalkID::Recharge: {
		if (itemCount == 0) {
			SetLineText(20, 1, _("You have nothing to recharge."), UiFlags::ColorWhitegold, false);
			return;
		}

		ScrollPos = 0;
		NumTextLines = itemCount;
		SetLineText(20, 1, _("Recharge which item?"), UiFlags::ColorWhitegold, false);
	} break;
	case TalkID::Identify: {
		if (itemCount == 0) {
			SetLineText(20, 1, _("You have nothing to identify."), UiFlags::ColorWhitegold, false);
			return;
		}

		ScrollPos = 0;
		NumTextLines = itemCount;

		SetLineText(20, 1, _("Identify which item?"), UiFlags::ColorWhitegold, false);
	} break;
	}
}

void SetupErrorScreen(TalkID talkId)
{
	SetupScreenElements(OldActiveStore);
	ClearTextLines(5, 23);

	std::string_view text;

	switch (talkId) {
	case TalkID::NoMoney:
		IsTextFullSize = true;
		RenderGold = true;
		text = _("You do not have enough gold");
		break;
	case TalkID::NoRoom:
		text = _("You do not have enough room in inventory");
		break;
	}

	SetLineText(0, 14, text, UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

int GetItemBuyValue(const Item &item)
{
	int price = item._iIdentified ? item._iIvalue : item._ivalue;

	if (TownerId == TOWN_PEGBOY) {
		price = gbIsHellfire ? price - (price / 4) : price + (price / 2);
	}

	return price;
}

int GetItemSellValue(const Item &item)
{
	int price = item._iIdentified ? item._iIvalue : item._ivalue;

	return price / 4;
}

int GetItemRepairCost(const Item &item)
{
	int dur = item._iMaxDur - item._iDurability;
	int repairCost = 0;

	if (item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified) {
		repairCost = 30 * item._iIvalue * dur / (item._iMaxDur * 100 * 2);
	} else {
		repairCost = std::max(item._ivalue * dur / (item._iMaxDur * 2), 1);
	}

	return repairCost;
}

int GetItemRechargeCost(const Item &item)
{
	int rechargeCost = GetSpellData(item._iSpell).staffCost();
	rechargeCost = (rechargeCost * (item._iMaxCharges - item._iCharges)) / (item._iMaxCharges * 2);
	return rechargeCost;
}

int GetItemIdentifyCost()
{
	return 100;
}

void SetupConfirmScreen()
{
	SetupScreenElements(OldActiveStore);
	ClearTextLines(5, 23);

	int goldAmountDisplay;
	std::string_view prompt;

	switch (OldActiveStore) {
	case TalkID::BasicBuy:
	case TalkID::Buy: {
		goldAmountDisplay = GetItemBuyValue(TempItem);
		if (TownerId == TOWN_PEGBOY)
			prompt = _("Do we have a deal?");
		else
			prompt = _("Are you sure you want to buy this item?");
	} break;
	case TalkID::Sell:
		goldAmountDisplay = GetItemSellValue(TempItem);
		prompt = _("Are you sure you want to sell this item?");
		break;
	case TalkID::Repair:
		goldAmountDisplay = GetItemRepairCost(TempItem);
		prompt = _("Are you sure you want to repair this item?");
		break;
	case TalkID::Recharge:
		goldAmountDisplay = GetItemRechargeCost(TempItem);
		prompt = _("Are you sure you want to recharge this item?");
		break;
	case TalkID::Identify:
		goldAmountDisplay = GetItemIdentifyCost();
		prompt = _("Are you sure you want to identify this item?");
		break;
	default:
		app_fatal(StrCat("Unknown store dialog ", static_cast<int>(OldActiveStore)));
	}

	UiFlags itemColor = TempItem.getTextColorWithStatCheck();

	SetLineText(20, 8, TempItem.getName(), itemColor, false);
	SetLineValue(8, goldAmountDisplay);
	PrintStoreItem(TempItem, 9, itemColor);
	SetLineText(0, ConfirmLine - TripleLineSpace, prompt, UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	SetLineText(0, ConfirmLine, _("Yes"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	SetLineText(0, ConfirmLine + DoubleLineSpace, _("No"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void SetupGossipScreen()
{
	int la;
	const StoreData &store = Stores[TownerId];

	auto gossipOption = std::find_if(store.menuOptions.begin(), store.menuOptions.end(),
	    [](const StoreMenuOption &option) { return option.action == TalkID::Gossip; });

	const std::string &gossipText = gossipOption->text;

	IsTextFullSize = false;

	SetLineText(0, 2, fmt::format(fmt::runtime(_("{}")), gossipText),
	    UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);

	SetLineAsDivider(5);

	if (gbIsSpawn) {
		SetLineText(0, 10, fmt::format(fmt::runtime(_("{}")), gossipText),
		    UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		SetLineText(0, 12, _("is not available"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		SetLineText(0, 14, _("in the shareware"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		SetLineText(0, 16, _("version"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		SetLineAsOptionsBackButton();
		return;
	}

	int sn = 0;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[TownerId][quest._qidx] != TEXT_NONE && quest._qlog)
			sn++;
	}

	if (sn > 6) {
		sn = 14 - (sn / 2);
		la = 1;
	} else {
		sn = 15 - sn;
		la = 2;
	}

	int sn2 = sn - 2;

	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[TownerId][quest._qidx] != TEXT_NONE && quest._qlog) {
			SetLineText(0, sn, _(QuestsData[quest._qidx]._qlstr), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			sn += la;
		}
	}

	SetLineText(0, sn2, _("Gossip"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	SetLineAsOptionsBackButton();
}

void SetMenuHeader(const StoreData &store)
{
	// Translate and convert store.welcomeMessage properly
	std::string translatedHeader = fmt::format(fmt::runtime(_("{:s}")), store.welcomeMessage);

	// Check if the translated header contains "\n\n", which indicates a two-line header
	std::string::size_type pos = translatedHeader.find("\n\n");

	if (pos != std::string::npos) {
		// Split the header into two parts for a two-line header
		std::string header1 = translatedHeader.substr(0, pos);
		std::string header2 = translatedHeader.substr(pos + 2);

		// Set the headers on lines 1 and 3
		SetLineText(0, 1, header1, UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		SetLineText(0, 3, header2, UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	} else {
		// If there's no "\n\n", treat it as a single-line header
		SetLineText(0, 2, translatedHeader, UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	}
}

void SetMenuText(const StoreData &store)
{
	const UiFlags flags = UiFlags::ColorWhitegold | UiFlags::AlignCenter;
	int startLine = MainMenuDividerLine + SingleLineSpace;

	if (TownerId != TOWN_PEGBOY) {
		CurrentMenuDrawLine = store.menuOptions.size() > 5 ? startLine + SingleLineSpace : startLine + TripleLineSpace;
		SetLineText(0, CurrentMenuDrawLine, _("Would you like to:"), flags, false);
		CurrentMenuDrawLine += TripleLineSpace;
	} else if (!Boy.items.empty()) {
		CurrentMenuDrawLine = WirtDialogueDrawLine;
		SetLineText(0, CurrentMenuDrawLine, _("I have something for sale,"), flags, false);
		CurrentMenuDrawLine += DoubleLineSpace;
		SetLineText(0, CurrentMenuDrawLine, _("but it will cost 50 gold"), flags, false);
		CurrentMenuDrawLine += DoubleLineSpace;
		SetLineText(0, CurrentMenuDrawLine, _("just to take a look. "), flags, false);
		CurrentMenuDrawLine = WirtDialogueDrawLine - (DoubleLineSpace * 2);
	} else {
		CurrentMenuDrawLine = startLine + (TripleLineSpace * 2);
	}
}

void SetMenuOption(TalkID action, const std::string_view &text)
{
	UiFlags flags = (action == TalkID::Gossip) ? UiFlags::ColorBlue | UiFlags::AlignCenter : UiFlags::ColorWhite | UiFlags::AlignCenter;

	// Set leave option as the last menu option, trying for line 18 if there's room, otherwise line 20.
	if (action == TalkID::None) {
		CurrentMenuDrawLine = CurrentMenuDrawLine < 18 ? 18 : 20;
	}

	SetLineText(0, CurrentMenuDrawLine, text, flags, true);

	// Update the vector to map the current line to the action
	LineActionMappings.push_back({ CurrentMenuDrawLine, action });

	CurrentMenuDrawLine += DoubleLineSpace;

	if (TownerId == TOWN_PEGBOY && !Boy.items.empty() && CurrentMenuDrawLine == (WirtDialogueDrawLine - DoubleLineSpace)) {
		CurrentMenuDrawLine = WirtDialogueDrawLine + (TripleLineSpace * 2);
	}
}

void RestoreResource()
{
	int *resource = nullptr;
	int *maxResource = nullptr;
	int *baseResource = nullptr;
	int *baseMaxResource = nullptr;
	PanelDrawComponent component;
	TownerStore *towner = townerStores[TownerId];

	switch (towner->resourceType) {
	case ResourceType::Life:
		resource = &MyPlayer->_pHitPoints;
		maxResource = &MyPlayer->_pMaxHP;
		baseResource = &MyPlayer->_pHPBase;
		baseMaxResource = &MyPlayer->_pMaxHPBase;
		component = PanelDrawComponent::Health;
		break;
	case ResourceType::Mana:
		if (!*GetOptions().Gameplay.adriaRefillsMana)
			return;
		resource = &MyPlayer->_pMana;
		maxResource = &MyPlayer->_pMaxMana;
		baseResource = &MyPlayer->_pManaBase;
		baseMaxResource = &MyPlayer->_pMaxManaBase;
		component = PanelDrawComponent::Mana;
		break;
	default:
		return;
	}

	if (*resource == *maxResource)
		return;

	PlaySFX(SfxID::CastHealing);
	*resource = *maxResource;
	*baseResource = *baseMaxResource;
	RedrawComponent(component);
}

void SetupMainMenuScreen()
{
	RestoreResource();
	IsTextFullSize = false;

	const StoreData &store = Stores[TownerId]; // Fetch store data dynamically

	SetMenuHeader(store); // Translate on display
	SetLineAsDivider(MainMenuDividerLine);
	SetMenuText(store);

	LineActionMappings.clear();

	for (const StoreMenuOption &option : store.menuOptions) {
		// Special case for Wirt: If buying and Wirt has no items, skip option
		if (TownerId == TOWN_PEGBOY && option.action == TalkID::Buy && Boy.items.empty())
			continue;

		SetMenuOption(option.action, _(option.text)); // Translate menu text dynamically
	}
}

void BuildPlayerItemsVector()
{
	playerItems.clear();

	// Add body items
	for (int8_t i = 0; i < SLOTXY_EQUIPPED_LAST; i++) {
		if (MyPlayer->InvBody[i].isEmpty())
			continue;
		playerItems.push_back({ &MyPlayer->InvBody[i], ItemLocation::Body, i });
	}

	// Add inventory items
	for (int8_t i = 0; i < MyPlayer->_pNumInv; i++) {
		if (MyPlayer->InvList[i].isEmpty())
			continue;
		playerItems.push_back({ &MyPlayer->InvList[i], ItemLocation::Inventory, i });
	}

	// Add belt items
	for (int i = 0; i < MaxBeltItems; i++) {
		if (MyPlayer->SpdList[i].isEmpty())
			continue;
		playerItems.push_back({ &MyPlayer->SpdList[i], ItemLocation::Belt, i });
	}
}

void FilterSellableItems(TalkID talkId)
{
	playerItems.erase(std::remove_if(playerItems.begin(), playerItems.end(),
	                      [talkId](const IndexedItem &indexedItem) {
		                      Item *pI = indexedItem.itemPtr;

		                      // Cannot sell equipped items
		                      if (indexedItem.location == ItemLocation::Body)
			                      return true; // Remove this item

		                      // Common conditions for both Smith and Witch
		                      if (pI->_itype == ItemType::Gold || pI->_iClass == ICLASS_QUEST || pI->IDidx == IDI_LAZSTAFF)
			                      return true; // Remove this item

		                      switch (TownerId) {
		                      case TOWN_SMITH:
			                      if (pI->_iMiscId > IMISC_OILFIRST && pI->_iMiscId < IMISC_OILLAST)
				                      return false; // Keep this item
			                      if (pI->_itype == ItemType::Misc || (pI->_itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(pI->_iSpell))))
				                      return true; // Remove this item
			                      return false;    // Keep this item

		                      case TOWN_WITCH:
			                      if (pI->_itype == ItemType::Misc && (pI->_iMiscId > 29 && pI->_iMiscId < 41))
				                      return true; // Remove this item
			                      if (pI->_itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(pI->_iSpell)))
				                      return false;                    // Keep this item
			                      return pI->_itype != ItemType::Misc; // Keep if it's not Misc

		                      default:
			                      return true; // Remove this item for unsupported TalkID
		                      }
	                      }),
	    playerItems.end());
}

void FilterRechargeableItems()
{
	// Filter playerItems to include only items that can be recharged
	playerItems.erase(std::remove_if(playerItems.begin(), playerItems.end(),
	                      [](const IndexedItem &indexedItem) {
		                      const Item &itemPtr = *indexedItem.itemPtr;
		                      return itemPtr._iCharges == itemPtr._iMaxCharges || (itemPtr._itype != ItemType::Staff && itemPtr._iMiscId != IMISC_UNIQUE && itemPtr._iMiscId != IMISC_STAFF);
	                      }),
	    playerItems.end());
}

void FilterIdentifiableItems()
{
	// Filter playerItems to include only items that can be identified
	playerItems.erase(std::remove_if(playerItems.begin(), playerItems.end(),
	                      [](const IndexedItem &indexedItem) {
		                      const Item &itemPtr = *indexedItem.itemPtr;
		                      return itemPtr._iMagical == ITEM_QUALITY_NORMAL || itemPtr._iIdentified;
	                      }),
	    playerItems.end());
}

void FilterPlayerItemsForAction(TalkID talkId)
{
	BuildPlayerItemsVector();

	switch (talkId) {
	case TalkID::Sell:
		// Filter items for selling
		FilterSellableItems(talkId);
		break;
	case TalkID::Repair:
		// Filter items for repairing
		FilterRepairableItems();
		break;
	case TalkID::Recharge:
		// Filter items for recharging
		FilterRechargeableItems();
		break;
	case TalkID::Identify:
		// Filter items for identifying
		FilterIdentifiableItems();
		break;
	}
}

void SetupTownerItemList(TalkID talkId, std::vector<Item> &items, int idx, bool selling /*= true*/)
{
	ClearTextLines(5, 21);
	PreviousScrollPos = 5;

	int startLine = (TownerId == TOWN_PEGBOY) ? 10 : 5;
	for (int l = startLine; l < 20 && idx < items.size(); l += 4) {
		const Item &item = items[idx];
		int price = GetItemBuyValue(item);
		UiFlags itemColor = item.getTextColorWithStatCheck();

		SetLineText(20, l, item.getName(), itemColor, true, item._iCurs, true);
		SetLineValue(l, price);
		PrintStoreItem(item, l + 1, itemColor, true);
		NextScrollPos = l;
		idx++;
	}

	if (selling) {
		if (CurrentTextLine != -1 && !TextLine[CurrentTextLine].isSelectable() && CurrentTextLine != BackButtonLine())
			CurrentTextLine = NextScrollPos;
	} else {
		NumTextLines = std::max(static_cast<int>(items.size()) - ItemLineSpace, 0);
	}
}

void SetupPlayerItemList(TalkID talkId, std::vector<IndexedItem> &items, int idx, bool selling /*= true*/)
{
	ClearTextLines(5, 21);
	PreviousScrollPos = 5;

	int goldAmountDisplay;

	for (int l = 5; l < 20 && idx < items.size(); l += 4) {
		const Item &item = *items[idx].itemPtr;
		UiFlags itemColor = item.getTextColorWithStatCheck();
		SetLineText(20, l, item.getName(), itemColor, true, item._iCurs, true);
		switch (talkId) {
		case TalkID::Sell:
			goldAmountDisplay = GetItemSellValue(item);
			break;
		case TalkID::Repair:
			goldAmountDisplay = GetItemRepairCost(item);
			break;
		case TalkID::Recharge:
			goldAmountDisplay = GetItemRechargeCost(item);
			break;
		case TalkID::Identify:
			goldAmountDisplay = GetItemIdentifyCost();
			break;
		}
		SetLineValue(l, goldAmountDisplay);
		PrintStoreItem(item, l + 1, itemColor, true);
		NextScrollPos = l;
		idx++;
	}

	if (selling) {
		if (CurrentTextLine != -1 && !TextLine[CurrentTextLine].isSelectable() && CurrentTextLine != BackButtonLine())
			CurrentTextLine = NextScrollPos;
	} else {
		NumTextLines = std::max(static_cast<int>(items.size()) - ItemLineSpace, 0);
	}
}

void SetupItemList(TalkID talkId)
{
	TownerStore *towner = townerStores[TownerId];

	switch (talkId) {
	case TalkID::BasicBuy:
		SetupTownerItemList(talkId, towner->basicItems, ScrollPos, true);
		break;
	case TalkID::Buy:
		SetupTownerItemList(talkId, towner->items, ScrollPos, true);
		break;
	case TalkID::Sell:
	case TalkID::Repair:
	case TalkID::Recharge:
	case TalkID::Identify:
		SetupPlayerItemList(talkId, playerItems, ScrollPos, false);
		break;
	}
}

void UpdateBookMinMagic(Item &bookItem)
{
	if (bookItem._iMiscId != IMISC_BOOK)
		return;
	bookItem._iMinMag = GetSpellData(bookItem._iSpell).minInt;
	uint8_t spellLevel = MyPlayer->_pSplLvl[static_cast<int8_t>(bookItem._iSpell)];
	while (spellLevel > 0) {
		bookItem._iMinMag += 20 * bookItem._iMinMag / 100;
		spellLevel--;
		if (bookItem._iMinMag + 20 * bookItem._iMinMag / 100 > 255) {
			bookItem._iMinMag = 255;
			spellLevel = 0;
		}
	}
}

static void UpdateItemStatFlag(Item &item)
{
	item._iStatFlag = MyPlayer->CanUseItem(item);
}

void UpdateItemStatFlags(TalkID talkId)
{
	TownerStore *towner = townerStores[TownerId];

	switch (talkId) {
	case TalkID::BasicBuy:
		for (Item &item : towner->basicItems)
			UpdateItemStatFlag(item);
		break;
	case TalkID::Buy:
		for (Item &item : towner->items)
			UpdateItemStatFlag(item);
		break;
	}
}

uint32_t GetTotalPlayerGold()
{
	return MyPlayer->_pGold + Stash.gold;
}

bool CanPlayerAfford(uint32_t price)
{
	return GetTotalPlayerGold() >= price;
}

void SetupIdentifyResultScreen()
{
	SetupScreenElements(OldActiveStore);
	ClearTextLines(5, 23);

	UiFlags itemColor = TempItem.getTextColorWithStatCheck();

	SetLineText(0, 7, _("This item is:"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	SetLineText(20, 11, TempItem.getName(), itemColor, false);
	PrintStoreItem(TempItem, 12, itemColor);
	SetLineText(0, 18, _("Done"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

int GetLineForAction(TalkID action)
{
	auto it = std::find_if(LineActionMappings.begin(), LineActionMappings.end(),
	    [action](const std::pair<int, TalkID> &pair) {
		    return pair.second == action;
	    });
	return (it != LineActionMappings.end()) ? it->first : -1;
}

TalkID GetActionForLine(int line)
{
	auto it = std::find_if(LineActionMappings.begin(), LineActionMappings.end(),
	    [line](const std::pair<int, TalkID> &pair) {
		    return pair.first == line;
	    });
	return (it != LineActionMappings.end()) ? it->second : TalkID::Invalid;
}

void MainMenuEnter()
{
	TalkID selectedAction = GetActionForLine(CurrentTextLine);
	TownerStore *towner = townerStores[TownerId];

	switch (selectedAction) {
	case TalkID::None:
		ExitStore();
		return;
	case TalkID::Gossip:
		OldTextLine = CurrentTextLine;
		break;
	case TalkID::Buy:
		if (TownerId == TOWN_PEGBOY) {
			if (!CanPlayerAfford(50)) {
				// OldActiveStore is TalkID::Buy at this point, and we need to override and set "most recent" store to the main menu
				OldActiveStore = TalkID::MainMenu;
				selectedAction = TalkID::NoMoney;
			} else {
				TakePlrsMoney(50);
			}
		}
		break;
	case TalkID::Stash:
		ExitStore();
		IsStashOpen = true;
		Stash.RefreshItemStatFlags();
		invflag = true;
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			if (pcurs == CURSOR_DISARM)
				NewCursor(CURSOR_HAND);
			FocusOnInventory();
		}
		return;
	}

	StartStore(selectedAction);
}

int GetItemIndex()
{
	return OldScrollPos + ((OldTextLine - PreviousScrollPos) / ItemLineSpace);
}

bool ReturnToMainMenu()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::MainMenu);
		return true;
	}

	return false;
}

void BuyEnter()
{
	if (ReturnToMainMenu())
		return;

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = GetItemIndex();

	// Boy displays his item in the 2nd slot instead of the 1st, so we need to adjust the index
	if (TownerId == TOWN_PEGBOY)
		idx--;

	TownerStore *towner = townerStores[TownerId];
	Item &item = (ActiveStore == TalkID::BasicBuy) ? towner->basicItems[idx] : towner->items[idx];
	int cost = GetItemBuyValue(item);

	if (!CanPlayerAfford(cost)) {
		StartStore(TalkID::NoMoney);
	} else if (!GiveItemToPlayer(item, false)) {
		StartStore(TalkID::NoRoom);
	} else {
		TempItem = item;
		StartStore(TalkID::Confirm);
	}
}

bool StoreGoldFit(Item &item)
{
	int cost = item._iIvalue;

	Size itemSize = GetInventorySize(item);
	int itemRoomForGold = itemSize.width * itemSize.height * MaxGold;

	if (cost <= itemRoomForGold) {
		return true;
	}

	return cost <= itemRoomForGold + RoomForGold();
}

/**
 * @brief Sells an item from the player's inventory or belt.
 */
void SellItem()
{
	int idx = GetItemIndex();

	IndexedItem &itemToSell = playerItems[idx];

	// Remove the sold item from the player's inventory or belt
	if (itemToSell.location == ItemLocation::Inventory) {
		MyPlayer->RemoveInvItem(itemToSell.index);
	} else if (itemToSell.location == ItemLocation::Belt) {
		MyPlayer->RemoveSpdBarItem(itemToSell.index);
	}

	int price = GetItemSellValue(TempItem);

	// Remove the sold item from the playerItems vector
	playerItems.erase(playerItems.begin() + idx);

	// Add the gold to the player's inventory
	AddGoldToInventory(*MyPlayer, price);
	MyPlayer->_pGold += price;
}

void SellEnter()
{
	if (ReturnToMainMenu())
		return;

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = GetItemIndex();

	// Check if there's enough room for the gold that will be earned from selling the item
	if (!StoreGoldFit(*playerItems[idx].itemPtr)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	// Store the item to be sold temporarily
	TempItem = *playerItems[idx].itemPtr;

	// Proceed to the confirmation store screen
	StartStore(TalkID::Confirm);
}

/**
 * @brief Repairs an item in the player's inventory or body in the smith.
 */
void RepairItem()
{
	int idx = GetItemIndex();

	// Get a reference to the IndexedItem at the calculated index
	IndexedItem &indexedItem = playerItems[idx];

	// Repair the item by setting its durability to the maximum
	indexedItem.itemPtr->_iDurability = indexedItem.itemPtr->_iMaxDur;

	// Deduct the repair cost from the player's money
	TakePlrsMoney(GetItemRepairCost(*indexedItem.itemPtr));

	// Update the player's inventory
	CalcPlrInv(*MyPlayer, true);
}

void RepairEnter()
{
	if (ReturnToMainMenu())
		return;

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = GetItemIndex();

	// Check if the player can afford the repair cost
	if (!CanPlayerAfford(GetItemRepairCost(*playerItems[idx].itemPtr))) {
		StartStore(TalkID::NoMoney);
		return;
	}

	// Temporarily store the item being repaired
	TempItem = *playerItems[idx].itemPtr;

	// Proceed to the confirmation screen
	StartStore(TalkID::Confirm);
}

/**
 * @brief Purchases an item from the witch.
 */
void BuyItem(Item &item)
{
	// Get the index of the purchased item
	int idx = GetItemIndex();

	// Boy displays his item in the 2nd slot instead of the 1st, so we need to adjust the index
	if (TownerId == TOWN_PEGBOY)
		idx--;

	int numPinnedItems = 0;

	switch (TownerId) {
	case TOWN_HEALER:
		numPinnedItems = !gbIsMultiplayer ? NumHealerPinnedItems : NumHealerPinnedItemsMp;
		break;
	case TOWN_WITCH:
		numPinnedItems = NumWitchPinnedItems;
		break;
	}

	// If the item is one of the pinned items, generate a new seed for it
	if (idx < numPinnedItems) {
		item._iSeed = AdvanceRndSeed();
	}

	// Non-magical items are unidentified
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;

	// Deduct the player's gold and give the item to the player
	TakePlrsMoney(item._iIvalue);
	GiveItemToPlayer(item, true);

	TownerStore *towner = townerStores[TownerId];

	// If the purchased item is not a pinned item, remove it from the store
	if (idx >= numPinnedItems) {
		if (OldActiveStore == TalkID::BasicBuy) {
			towner->basicItems.erase(towner->basicItems.begin() + idx);
		} else {
			towner->items.erase(towner->items.begin() + idx);
		}
	}

	// Blacksmith replaces the item with a new one
	if (TownerId == TOWN_SMITH) {
		SpawnPremium(*MyPlayer);
	}

	// Boy returns to main menu instead of item list
	if (TownerId == TOWN_PEGBOY) {
		OldActiveStore = TalkID::MainMenu;
		OldTextLine = CurrentTextLine;
	}

	// Recalculate the player's inventory
	CalcPlrInv(*MyPlayer, true);
}

/**
 * @brief Recharges an item in the player's inventory or body in the witch.
 */
void RechargeItem()
{
	int idx = GetItemIndex();

	// Get a reference to the IndexedItem at the calculated index
	IndexedItem &indexedItem = playerItems[idx];

	// Recharge the item by setting its charges to the maximum
	indexedItem.itemPtr->_iCharges = indexedItem.itemPtr->_iMaxCharges;

	// Send network commands for synchronization
	if (indexedItem.location == ItemLocation::Body) {
		NetSendCmdChItem(true, indexedItem.index);
	} else {
		NetSyncInvItem(*MyPlayer, indexedItem.index);
	}

	// Deduct the recharge cost from the player's money
	TakePlrsMoney(GetItemRechargeCost(*indexedItem.itemPtr));

	// Recalculate and update the player's inventory
	CalcPlrInv(*MyPlayer, true);
}

void RechargeEnter()
{
	if (ReturnToMainMenu()) {
		return;
	}

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = GetItemIndex();

	// Check if the player can afford the recharge cost
	if (!CanPlayerAfford(GetItemRechargeCost(*playerItems[idx].itemPtr))) {
		StartStore(TalkID::NoMoney);
		return;
	}

	// Store the item temporarily for the confirmation screen
	TempItem = *playerItems[idx].itemPtr;
	StartStore(TalkID::Confirm);
}

/**
 * @brief Identifies an item in the player's inventory or body.
 */
void IdentifyItem()
{
	int idx = GetItemIndex();

	// Get a reference to the IndexedItem at the calculated index
	IndexedItem &indexedItem = playerItems[idx];

	// Mark the item as identified
	indexedItem.itemPtr->_iIdentified = true;

	// Deduct the identification cost from the player's money
	TakePlrsMoney(GetItemIdentifyCost());

	// Update the player's inventory
	CalcPlrInv(*MyPlayer, true);
}

void ConfirmEnter(Item &item)
{
	if (CurrentTextLine == ConfirmLine) {
		switch (OldActiveStore) {
		case TalkID::BasicBuy:
		case TalkID::Buy:
			BuyItem(item);
			break;
		case TalkID::Sell:
			SellItem();
			break;
		case TalkID::Repair:
			RepairItem();
			break;
		case TalkID::Recharge:
			RechargeItem();
			break;
		case TalkID::Identify:
			IdentifyItem();
			StartStore(TalkID::IdentifyShow);
			return;
		}
	}

	StartStore(OldActiveStore);

	if (CurrentTextLine == BackButtonLine())
		return;

	CurrentTextLine = OldTextLine;
	ScrollPos = std::min(OldScrollPos, NumTextLines);

	while (CurrentTextLine != -1 && !TextLine[CurrentTextLine].isSelectable()) {
		CurrentTextLine--;
	}
}

void IdentifyEnter()
{
	if (ReturnToMainMenu()) {
		return;
	}

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = GetItemIndex();

	// Check if the player can afford the identification cost
	if (!CanPlayerAfford(GetItemIdentifyCost())) {
		StartStore(TalkID::NoMoney);
		return;
	}

	// Store the item temporarily for the confirmation screen
	TempItem = *playerItems[idx].itemPtr;
	StartStore(TalkID::Confirm);
}

void TalkEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(OldActiveStore);
		CurrentTextLine = OldTextLine;
		return;
	}

	int sn = 0;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[TownerId][quest._qidx] != TEXT_NONE && quest._qlog)
			sn++;
	}
	int la = 2;
	if (sn > 6) {
		sn = 14 - (sn / 2);
		la = 1;
	} else {
		sn = 15 - sn;
	}

	if (CurrentTextLine == sn - 2) {
		Towner *target = GetTowner(TownerId);
		assert(target != nullptr);
		InitQTextMsg(target->gossip);
		return;
	}

	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_ACTIVE && QuestDialogTable[TownerId][quest._qidx] != TEXT_NONE && quest._qlog) {
			if (sn == CurrentTextLine) {
				InitQTextMsg(QuestDialogTable[TownerId][quest._qidx]);
			}
			sn += la;
		}
	}
}

int TakeGold(Player &player, int cost, bool skipMaxPiles)
{
	for (int i = 0; i < player._pNumInv; i++) {
		auto &item = player.InvList[i];
		if (item._itype != ItemType::Gold || (skipMaxPiles && item._ivalue == MaxGold))
			continue;

		if (cost < item._ivalue) {
			item._ivalue -= cost;
			SetPlrHandGoldCurs(player.InvList[i]);
			return 0;
		}

		cost -= item._ivalue;
		player.RemoveInvItem(i);
		i = -1;
	}

	return cost;
}

void DrawSelector(const Surface &out, const Rectangle &rect, std::string_view text, UiFlags flags)
{
	int lineWidth = GetLineWidth(text);

	int x1 = rect.position.x - 20;
	if (HasAnyOf(flags, UiFlags::AlignCenter))
		x1 += (rect.size.width - lineWidth) / 2;

	ClxDraw(out, { x1, rect.position.y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);

	int x2 = rect.position.x + rect.size.width + 5;
	if (HasAnyOf(flags, UiFlags::AlignCenter))
		x2 = rect.position.x + (rect.size.width - lineWidth) / 2 + lineWidth + 5;

	ClxDraw(out, { x2, rect.position.y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);
}

} // namespace

void InitStores()
{
	int numSmithItems = gbIsHellfire ? NumSmithItemsHf : NumSmithItems;
	ClearTextLines(0, NumStoreLines);
	ExitStore();
	IsTextFullSize = false;
	Blacksmith.itemLevel = 1;
	Boy.itemLevel = 0;

	InitializeTownerStores();
}

void SetupTownStores()
{
	int l = MyPlayer->getCharacterLevel() / 2;
	if (!gbIsMultiplayer) {
		l = 0;
		for (int i = 0; i < NUMLEVELS; i++) {
			if (MyPlayer->_pLvlVisited[i])
				l = i;
		}
	}

	l = std::clamp(l + 2, 6, 16);
	SpawnSmith(l);
	SpawnWitch(l);
	SpawnHealer(l);
	SpawnBoy(MyPlayer->getCharacterLevel());
	SpawnPremium(*MyPlayer);
}

void FreeStoreMem()
{
	if (*GetOptions().Gameplay.showItemGraphicsInStores) {
		FreeHalfSizeItemSprites();
	}
	ExitStore();
	for (STextStruct &entry : TextLine) {
		entry.text.clear();
		entry.text.shrink_to_fit();
	}
}

void ExitStore()
{
	SetActiveStore(TalkID::None);
}

void PrintSString(const Surface &out, int margin, int line, std::string_view text, UiFlags flags, int price, int cursId, bool cursIndent)
{
	const Point uiPosition = GetUIRectangle().position;
	int sx = uiPosition.x + 32 + margin;
	if (!IsTextFullSize) {
		sx += 320;
	}

	const int sy = uiPosition.y + PaddingTop + TextLine[line].y + TextLine[line]._syoff;

	int width = IsTextFullSize ? 575 : 255;
	if (HasScrollbar() && line >= 4 && line <= 20) {
		width -= 9; // Space for the selector
	}
	width -= margin * 2;

	const Rectangle rect { { sx, sy }, { width, 0 } };

	// Space reserved for item graphic is based on the size of 2x3 cursor sprites
	constexpr int CursWidth = INV_SLOT_SIZE_PX * 2;
	constexpr int HalfCursWidth = CursWidth / 2;

	if (*GetOptions().Gameplay.showItemGraphicsInStores && cursId >= 0) {
		const Size size = GetInvItemSize(static_cast<int>(CURSOR_FIRSTITEM) + cursId);
		const bool useHalfSize = size.width > INV_SLOT_SIZE_PX || size.height > INV_SLOT_SIZE_PX;
		const bool useRed = HasAnyOf(flags, UiFlags::ColorRed);
		const ClxSprite sprite = useHalfSize
		    ? (useRed ? GetHalfSizeItemSpriteRed(cursId) : GetHalfSizeItemSprite(cursId))
		    : GetInvItemSprite(static_cast<int>(CURSOR_FIRSTITEM) + cursId);
		const Point position {
			rect.position.x + (HalfCursWidth - sprite.width()) / 2,
			rect.position.y + (TextHeight() * 3 + sprite.height()) / 2
		};
		if (useHalfSize || !useRed) {
			ClxDraw(out, position, sprite);
		} else {
			ClxDrawTRN(out, position, sprite, GetInfravisionTRN());
		}
	}

	if (*GetOptions().Gameplay.showItemGraphicsInStores && cursIndent) {
		const Rectangle textRect { { rect.position.x + HalfCursWidth + 8, rect.position.y }, { rect.size.width - HalfCursWidth + 8, rect.size.height } };
		DrawString(out, text, textRect, { .flags = flags });
	} else {
		DrawString(out, text, rect, { .flags = flags });
	}

	if (price > 0)
		DrawString(out, FormatInteger(price), rect, { .flags = flags | UiFlags::AlignRight });

	if (CurrentTextLine == line) {
		DrawSelector(out, rect, text, flags);
	}
}

void DrawSLine(const Surface &out, int sy)
{
	const Point uiPosition = GetUIRectangle().position;
	int sx = 26;
	int width = 587;

	if (!IsTextFullSize) {
		sx += SidePanelSize.width;
		width -= SidePanelSize.width;
	}

	uint8_t *src = out.at(uiPosition.x + sx, uiPosition.y + 25);
	uint8_t *dst = out.at(uiPosition.x + sx, sy);

	for (int i = 0; i < 3; i++, src += out.pitch(), dst += out.pitch())
		memcpy(dst, src, width);
}

void DrawSTextHelp()
{
	CurrentTextLine = -1;
	IsTextFullSize = true;
}

void ClearTextLines(int start, int end)
{
	for (int i = start; i < end; i++) {
		TextLine[i]._sx = 0;
		TextLine[i]._syoff = 0;
		TextLine[i].text.clear();
		TextLine[i].text.shrink_to_fit();
		TextLine[i].flags = UiFlags::None;
		TextLine[i].type = STextStruct::Label;
		TextLine[i]._sval = 0;
	}

	// std::fill(storeLineMapping.begin(), storeLineMapping.end(), TalkID::None);
}

void StartStore(TalkID store /*= TalkID::MainMenu*/)
{
	SetActiveStore(store);
	if (*GetOptions().Gameplay.showItemGraphicsInStores) {
		CreateHalfSizeItemSprites();
	}
	SpellbookFlag = false;
	CloseInventory();
	CloseCharPanel();
	RenderGold = false;
	QuestLogIsOpen = false;
	CloseGoldDrop();
	ClearTextLines(0, NumStoreLines);
	ReleaseStoreButton();

	switch (store) {
	case TalkID::MainMenu:
		SetupMainMenuScreen();
		break;
	case TalkID::Gossip:
		SetupGossipScreen();
		break;
	case TalkID::BasicBuy:
	case TalkID::Buy:
		SetupScreenElements(store);
		SetupItemList(store);
		UpdateItemStatFlags(store);
		break;
	case TalkID::Sell:
	case TalkID::Repair:
	case TalkID::Recharge:
	case TalkID::Identify:
		FilterPlayerItemsForAction(store);
		SetupScreenElements(store);
		SetupItemList(store);
		break;
	case TalkID::IdentifyShow:
		SetupIdentifyResultScreen();
		break;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
		SetupErrorScreen(store);
		break;
	case TalkID::Confirm:
		SetupConfirmScreen();
		break;
	case TalkID::None:
		break;
	}

	CurrentTextLine = -1;

	if (store == TalkID::MainMenu && IsNoneOf(OldActiveStore, TalkID::None, TalkID::Invalid)) {
		CurrentTextLine = GetLineForAction(OldActiveStore);
	} else { // Set currently selected line to the first selectable line
		for (int i = 0; i < NumStoreLines; i++) {
			if (TextLine[i].isSelectable()) {
				CurrentTextLine = i;
				break;
			}
		}
	}
}

void DrawStore(const Surface &out)
{
	if (!IsTextFullSize)
		DrawTextUI(out);
	else
		DrawQTextBack(out);

	if (GetItemCount(ActiveStore) > 0) {
		SetupItemList(ActiveStore);
	}

	CalculateLineHeights();
	const Point uiPosition = GetUIRectangle().position;
	for (int i = 0; i < NumStoreLines; i++) {
		if (TextLine[i].isDivider())
			DrawSLine(out, uiPosition.y + PaddingTop + TextLine[i].y + TextHeight() / 2);
		else if (TextLine[i].hasText())
			PrintSString(out, TextLine[i]._sx, i, TextLine[i].text, TextLine[i].flags, TextLine[i]._sval, TextLine[i].cursId, TextLine[i].cursIndent);
	}

	if (RenderGold) {
		PrintSString(out, 28, 1, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(GetTotalPlayerGold())).c_str(), UiFlags::ColorWhitegold | UiFlags::AlignRight);
	}

	if (HasScrollbar())
		DrawScrollbar(out, 4, 20);
}

void StoreESC()
{
	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();
		return;
	}

	switch (ActiveStore) {
	case TalkID::MainMenu:
		ExitStore();
		return;
	case TalkID::Gossip:
	case TalkID::BasicBuy:
	case TalkID::Buy:
	case TalkID::Sell:
	case TalkID::Repair:
	case TalkID::Recharge:
	case TalkID::Identify:
		StartStore(TalkID::MainMenu);
		CurrentTextLine = OldTextLine;
		return;
	case TalkID::IdentifyShow:
		StartStore(TalkID::Identify);
		return;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
	case TalkID::Confirm:
		StartStore(OldActiveStore);
		CurrentTextLine = OldTextLine;
		ScrollPos = OldScrollPos;
		return;
	}
}

void StoreUp()
{
	PlaySFX(SfxID::MenuMove);
	if (CurrentTextLine == -1) {
		return;
	}

	if (HasScrollbar()) {
		if (CurrentTextLine == PreviousScrollPos) {
			if (ScrollPos != 0)
				ScrollPos--;
			return;
		}

		CurrentTextLine--;
		while (!TextLine[CurrentTextLine].isSelectable()) {
			if (CurrentTextLine == 0)
				CurrentTextLine = NumStoreLines - 1;
			else
				CurrentTextLine--;
		}
		return;
	}

	if (CurrentTextLine == 0)
		CurrentTextLine = NumStoreLines - 1;
	else
		CurrentTextLine--;

	while (!TextLine[CurrentTextLine].isSelectable()) {
		if (CurrentTextLine == 0)
			CurrentTextLine = NumStoreLines - 1;
		else
			CurrentTextLine--;
	}
}

void StoreDown()
{
	PlaySFX(SfxID::MenuMove);
	if (CurrentTextLine == -1) {
		return;
	}

	if (HasScrollbar()) {
		if (CurrentTextLine == NextScrollPos) {
			if (ScrollPos < NumTextLines)
				ScrollPos++;
			return;
		}

		CurrentTextLine++;
		while (!TextLine[CurrentTextLine].isSelectable()) {
			if (CurrentTextLine == NumStoreLines - 1)
				CurrentTextLine = 0;
			else
				CurrentTextLine++;
		}
		return;
	}

	if (CurrentTextLine == NumStoreLines - 1)
		CurrentTextLine = 0;
	else
		CurrentTextLine++;

	while (!TextLine[CurrentTextLine].isSelectable()) {
		if (CurrentTextLine == NumStoreLines - 1)
			CurrentTextLine = 0;
		else
			CurrentTextLine++;
	}
}

void StorePrior()
{
	PlaySFX(SfxID::MenuMove);
	if (CurrentTextLine != -1 && HasScrollbar()) {
		if (CurrentTextLine == PreviousScrollPos) {
			ScrollPos = std::max(ScrollPos - 4, 0);
		} else {
			CurrentTextLine = PreviousScrollPos;
		}
	}
}

void StoreNext()
{
	PlaySFX(SfxID::MenuMove);
	if (CurrentTextLine != -1 && HasScrollbar()) {
		if (CurrentTextLine == NextScrollPos) {
			if (ScrollPos < NumTextLines)
				ScrollPos += 4;
			if (ScrollPos > NumTextLines)
				ScrollPos = NumTextLines;
		} else {
			CurrentTextLine = NextScrollPos;
		}
	}
}

void TakePlrsMoney(int cost)
{
	MyPlayer->_pGold -= std::min(cost, MyPlayer->_pGold);

	cost = TakeGold(*MyPlayer, cost, true);
	if (cost != 0) {
		cost = TakeGold(*MyPlayer, cost, false);
	}

	Stash.gold -= cost;
	Stash.dirty = true;
}

void StoreEnter()
{
	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();

		return;
	}

	PlaySFX(SfxID::MenuSelect);

	switch (ActiveStore) {
	case TalkID::MainMenu:
		MainMenuEnter();
		break;
	case TalkID::BasicBuy:
	case TalkID::Buy:
		BuyEnter();
		break;
	case TalkID::Sell:
		SellEnter();
		break;
	case TalkID::Repair:
		RepairEnter();
		break;
	case TalkID::Recharge:
		RechargeEnter();
		break;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
		StartStore(OldActiveStore);
		CurrentTextLine = OldTextLine;
		ScrollPos = OldScrollPos;
		break;
	case TalkID::Confirm:
		ConfirmEnter(TempItem);
		break;
	case TalkID::Identify:
		IdentifyEnter();
		break;
	case TalkID::Gossip:
		TalkEnter();
		break;
	case TalkID::IdentifyShow:
		StartStore(TalkID::Identify);
		break;
	}
}

void CheckStoreButton()
{
	const Point uiPosition = GetUIRectangle().position;
	const Rectangle windowRect { { uiPosition.x + 344, uiPosition.y + PaddingTop - 7 }, { 271, 303 } };
	const Rectangle windowRectFull { { uiPosition.x + 24, uiPosition.y + PaddingTop - 7 }, { 591, 303 } };

	if (!IsTextFullSize) {
		if (!windowRect.contains(MousePosition)) {
			while (ActiveStore != TalkID::None)
				StoreESC();
		}
	} else {
		if (!windowRectFull.contains(MousePosition)) {
			while (ActiveStore != TalkID::None)
				StoreESC();
		}
	}

	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();
	} else if (CurrentTextLine != -1) {
		const int relativeY = MousePosition.y - (uiPosition.y + PaddingTop);

		if (HasScrollbar() && MousePosition.x > 600 + uiPosition.x) {
			// Scroll bar is always measured in terms of the small line height.
			int y = relativeY / SmallLineHeight;
			if (y == 4) {
				if (CountdownScrollUp <= 0) {
					StoreUp();
					CountdownScrollUp = 10;
				} else {
					CountdownScrollUp--;
				}
			}
			if (y == 20) {
				if (CountdownScrollDown <= 0) {
					StoreDown();
					CountdownScrollDown = 10;
				} else {
					CountdownScrollDown--;
				}
			}
			return;
		}

		int y = relativeY / LineHeight();

		// Large small fonts draw beyond LineHeight. Check if the click was on the overflow text.
		if (IsSmallFontTall() && y > 0 && y < NumStoreLines
		    && TextLine[y - 1].hasText() && !TextLine[y].hasText()
		    && relativeY < TextLine[y - 1].y + LargeTextHeight) {
			--y;
		}

		if (y >= 5) {
			if (y >= BackButtonLine() + 1)
				y = BackButtonLine();
			if (GetItemCount(ActiveStore) > 0 && y <= 20 && !TextLine[y].isSelectable()) {
				if (TextLine[y - 2].isSelectable()) {
					y -= 2;
				} else if (TextLine[y - 1].isSelectable()) {
					y--;
				}
			}
			if (TextLine[y].isSelectable() || (GetItemCount(ActiveStore) > 0 && y == BackButtonLine())) {
				CurrentTextLine = y;
				StoreEnter();
			}
		}
	}
}

void ReleaseStoreButton()
{
	CountdownScrollUp = -1;
	CountdownScrollDown = -1;
}

bool IsPlayerInStore()
{
	return ActiveStore != TalkID::None;
}

} // namespace devilution
