/**
 * @file stores.cpp
 *
 * Implementation of functionality for stores and towner dialogs.
 */
#include "stores.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

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
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

TalkID ActiveStore;

int CurrentItemIndex;
int8_t PlayerItemIndexes[48];
Item PlayerItems[48];

Item SmithItems[NumSmithBasicItemsHf];
int PremiumItemCount;
int PremiumItemLevel;
Item PremiumItems[NumSmithItemsHf];

Item HealerItems[20];

Item WitchItems[NumWitchItemsHf];

int BoyItemLevel;
Item BoyItem;

namespace {

/** The current towner being interacted with */
_talker_id TownerId;

/** Is the current dialog full size */
bool IsTextFullSize;

/** Number of text lines in the current dialog */
int NumTextLines;
/** Remember currently selected text line from TextLine while displaying a dialog */
int OldTextLine;
/** Currently selected text line from TextLine */
int CurrentTextLine;

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

/** Text lines */
STextStruct TextLine[NumStoreLines];

/** Whether to render the player's gold amount in the top left */
bool RenderGold;

/** Does the current panel have a scrollbar */
bool HasScrollbar;
/** Remember last scroll position */
int OldScrollPos;
/** Scroll position */
int ScrollPos;
/** Next scroll position */
int NextScrollPos;
/** Previous scroll position */
int PreviousScrollPos;
/** Countdown for the push state of the scroll up button */
int8_t CountdownScrollUp;
/** Countdown for the push state of the scroll down button */
int8_t CountdownScrollDown;

/** Remember current store while displaying a dialog */
TalkID OldActiveStore;

/** Temporary item used to hold the item being traded */
Item TempItem;

/** Maps from towner IDs to NPC names. */
const char *const TownerNames[] = {
	N_("Griswold"),
	N_("Pepin"),
	"",
	N_("Ogden"),
	N_("Cain"),
	N_("Farnham"),
	N_("Adria"),
	N_("Gillian"),
	N_("Wirt"),
};

constexpr int PaddingTop = 32;

// For most languages, line height is always 12.
// This includes blank lines and divider line.
constexpr int SmallLineHeight = 12;
constexpr int SmallTextHeight = 12;

// For larger small fonts (Chinese and Japanese), text lines are
// taller and overflow.
// We space out blank lines a bit more to give space to 3-line store items.
constexpr int LargeLineHeight = SmallLineHeight + 1;
constexpr int LargeTextHeight = 18;

/**
 * The line index with the Back / Leave button.
 * This is a special button that is always the last line.
 *
 * For lists with a scrollbar, it is not selectable (mouse-only).
 */
int BackButtonLine()
{
	if (IsSmallFontTall()) {
		return HasScrollbar ? 21 : 20;
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

void DrawSTextBack(const Surface &out)
{
	const Point uiPosition = GetUIRectangle().position;
	ClxDraw(out, { uiPosition.x + 320 + 24, 327 + uiPosition.y }, (*pSTextBoxCels)[0]);
	DrawHalfTransparentRectTo(out, uiPosition.x + 347, uiPosition.y + 28, 265, 297);
}

void DrawSSlider(const Surface &out, int y1, int y2)
{
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
	if (CurrentItemIndex > 1)
		yd3 = 1000 * (ScrollPos + ((yd3 - PreviousScrollPos) / 4)) / (CurrentItemIndex - 1) * (y2 * 12 - y1 * 12 - 24) / 1000;
	else
		yd3 = 0;
	ClxDraw(out, { uiPosition.x + 601, (y1 + 1) * 12 + 44 + uiPosition.y + yd3 }, (*pSTextSlidCels)[12]);
}

void AddSLine(size_t y)
{
	TextLine[y]._sx = 0;
	TextLine[y]._syoff = 0;
	TextLine[y].text.clear();
	TextLine[y].text.shrink_to_fit();
	TextLine[y].type = STextStruct::Divider;
	TextLine[y].cursId = -1;
	TextLine[y].cursIndent = false;
}

void AddSTextVal(size_t y, int val)
{
	TextLine[y]._sval = val;
}

void AddSText(uint8_t x, size_t y, std::string_view text, UiFlags flags, bool sel, int cursId = -1, bool cursIndent = false)
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

void AddOptionsBackButton()
{
	const int line = BackButtonLine();
	AddSText(0, line, _("Back"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	TextLine[line]._syoff = IsSmallFontTall() ? 0 : 6;
}

void AddItemListBackButton(bool selectable = false)
{
	const int line = BackButtonLine();
	std::string_view text = _("Back");
	if (!selectable && IsSmallFontTall()) {
		AddSText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignRight, selectable);
	} else {
		AddSLine(line - 1);
		AddSText(0, line, text, UiFlags::ColorWhite | UiFlags::AlignCenter, selectable);
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
		AddSText(40, l, productLine, flags, false, -1, cursIndent);
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
	AddSText(40, l++, productLine, flags, false, -1, cursIndent);
}

bool StoreAutoPlace(Item &item, bool persistItem)
{
	Player &player = *MyPlayer;

	if (AutoEquipEnabled(player, item) && AutoEquip(player, item, persistItem, true)) {
		return true;
	}

	if (AutoPlaceItemInBelt(player, item, persistItem, true)) {
		return true;
	}

	if (persistItem) {
		return AutoPlaceItemInInventory(player, item, true);
	}

	return CanFitItemInInventory(player, item);
}

void ScrollVendorStore(Item *itemData, int storeLimit, int idx, int selling = true)
{
	ClearSText(5, 21);
	PreviousScrollPos = 5;

	for (int l = 5; l < 20 && idx < storeLimit; l += 4) {
		const Item &item = itemData[idx];
		if (!item.isEmpty()) {
			UiFlags itemColor = item.getTextColorWithStatCheck();
			AddSText(20, l, item.getName(), itemColor, true, item._iCurs, true);
			AddSTextVal(l, item._iIdentified ? item._iIvalue : item._ivalue);
			PrintStoreItem(item, l + 1, itemColor, true);
			NextScrollPos = l;
		} else {
			l -= 4;
		}
		idx++;
	}
	if (selling) {
		if (CurrentTextLine != -1 && !TextLine[CurrentTextLine].isSelectable() && CurrentTextLine != BackButtonLine())
			CurrentTextLine = NextScrollPos;
	} else {
		NumTextLines = std::max(static_cast<int>(storeLimit) - 4, 0);
	}
}

void StartSmith()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Blacksmith's shop"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 7, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 10, _("Talk to Griswold"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 12, _("Buy basic items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Buy premium items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 16, _("Sell items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Repair items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 20, _("Leave the shop"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void ScrollSmithBuy(int idx)
{
	ScrollVendorStore(SmithItems, static_cast<int>(std::size(SmithItems)), idx);
}

uint32_t TotalPlayerGold()
{
	return MyPlayer->_pGold + Stash.gold;
}

// TODO: Change `_iIvalue` to be unsigned instead of passing `int` here.
bool PlayerCanAfford(int price)
{
	return TotalPlayerGold() >= static_cast<uint32_t>(price);
}

void StartSmithBuy()
{
	IsTextFullSize = true;
	HasScrollbar = true;
	ScrollPos = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithBuy(ScrollPos);
	AddItemListBackButton();

	CurrentItemIndex = 0;
	for (Item &item : SmithItems) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		CurrentItemIndex++;
	}

	NumTextLines = std::max(CurrentItemIndex - 4, 0);
}

void ScrollSmithPremiumBuy(int boughtitems)
{
	int idx = 0;
	for (; boughtitems != 0; idx++) {
		if (!PremiumItems[idx].isEmpty())
			boughtitems--;
	}

	ScrollVendorStore(PremiumItems, static_cast<int>(std::size(PremiumItems)), idx);
}

bool StartSmithPremiumBuy()
{
	CurrentItemIndex = 0;
	for (Item &item : PremiumItems) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		CurrentItemIndex++;
	}
	if (CurrentItemIndex == 0) {
		StartStore(TalkID::Smith);
		CurrentTextLine = 14;
		return false;
	}

	IsTextFullSize = true;
	HasScrollbar = true;
	ScrollPos = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these premium items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	AddItemListBackButton();

	NumTextLines = std::max(CurrentItemIndex - 4, 0);

	ScrollSmithPremiumBuy(ScrollPos);

	return true;
}

bool SmithSellOk(int i)
{
	Item *pI;

	if (i >= 0) {
		pI = &MyPlayer->InvList[i];
	} else {
		pI = &MyPlayer->SpdList[-(i + 1)];
	}

	if (pI->isEmpty())
		return false;

	if (pI->_iMiscId > IMISC_OILFIRST && pI->_iMiscId < IMISC_OILLAST)
		return true;

	if (pI->_itype == ItemType::Misc)
		return false;
	if (pI->_itype == ItemType::Gold)
		return false;
	if (pI->_itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(pI->_iSpell)))
		return false;
	if (pI->_iClass == ICLASS_QUEST)
		return false;
	if (pI->IDidx == IDI_LAZSTAFF)
		return false;

	return true;
}

void ScrollSmithSell(int idx)
{
	ScrollVendorStore(PlayerItems, CurrentItemIndex, idx, false);
}

void StartSmithSell()
{
	IsTextFullSize = true;
	bool sellOk = false;
	CurrentItemIndex = 0;

	for (auto &item : PlayerItems) {
		item.clear();
	}

	const Player &myPlayer = *MyPlayer;

	for (int8_t i = 0; i < myPlayer._pNumInv; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (SmithSellOk(i)) {
			sellOk = true;
			PlayerItems[CurrentItemIndex] = myPlayer.InvList[i];

			if (PlayerItems[CurrentItemIndex]._iMagical != ITEM_QUALITY_NORMAL && PlayerItems[CurrentItemIndex]._iIdentified)
				PlayerItems[CurrentItemIndex]._ivalue = PlayerItems[CurrentItemIndex]._iIvalue;

			PlayerItems[CurrentItemIndex]._ivalue = std::max(PlayerItems[CurrentItemIndex]._ivalue / 4, 1);
			PlayerItems[CurrentItemIndex]._iIvalue = PlayerItems[CurrentItemIndex]._ivalue;
			PlayerItemIndexes[CurrentItemIndex] = i;
			CurrentItemIndex++;
		}
	}

	for (int i = 0; i < MaxBeltItems; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (SmithSellOk(-(i + 1))) {
			sellOk = true;
			PlayerItems[CurrentItemIndex] = myPlayer.SpdList[i];

			if (PlayerItems[CurrentItemIndex]._iMagical != ITEM_QUALITY_NORMAL && PlayerItems[CurrentItemIndex]._iIdentified)
				PlayerItems[CurrentItemIndex]._ivalue = PlayerItems[CurrentItemIndex]._iIvalue;

			PlayerItems[CurrentItemIndex]._ivalue = std::max(PlayerItems[CurrentItemIndex]._ivalue / 4, 1);
			PlayerItems[CurrentItemIndex]._iIvalue = PlayerItems[CurrentItemIndex]._ivalue;
			PlayerItemIndexes[CurrentItemIndex] = -(i + 1);
			CurrentItemIndex++;
		}
	}

	if (!sellOk) {
		HasScrollbar = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing I want."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Which item is for sale?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(ScrollPos);
	AddItemListBackButton();
}

bool SmithRepairOk(int i)
{
	const Player &myPlayer = *MyPlayer;
	const Item &item = myPlayer.InvList[i];

	if (item.isEmpty())
		return false;
	if (item._itype == ItemType::Misc)
		return false;
	if (item._itype == ItemType::Gold)
		return false;
	if (item._iDurability == item._iMaxDur)
		return false;
	if (item._iMaxDur == DUR_INDESTRUCTIBLE)
		return false;

	return true;
}

void StartSmithRepair()
{
	IsTextFullSize = true;
	CurrentItemIndex = 0;

	for (auto &item : PlayerItems) {
		item.clear();
	}

	Player &myPlayer = *MyPlayer;

	auto &helmet = myPlayer.InvBody[INVLOC_HEAD];
	if (!helmet.isEmpty() && helmet._iDurability != helmet._iMaxDur) {
		AddStoreHoldRepair(&helmet, -1);
	}

	auto &armor = myPlayer.InvBody[INVLOC_CHEST];
	if (!armor.isEmpty() && armor._iDurability != armor._iMaxDur) {
		AddStoreHoldRepair(&armor, -2);
	}

	auto &leftHand = myPlayer.InvBody[INVLOC_HAND_LEFT];
	if (!leftHand.isEmpty() && leftHand._iDurability != leftHand._iMaxDur) {
		AddStoreHoldRepair(&leftHand, -3);
	}

	auto &rightHand = myPlayer.InvBody[INVLOC_HAND_RIGHT];
	if (!rightHand.isEmpty() && rightHand._iDurability != rightHand._iMaxDur) {
		AddStoreHoldRepair(&rightHand, -4);
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (SmithRepairOk(i)) {
			AddStoreHoldRepair(&myPlayer.InvList[i], i);
		}
	}

	if (CurrentItemIndex == 0) {
		HasScrollbar = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to repair."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Repair which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollSmithSell(ScrollPos);
	AddItemListBackButton();
}

void FillManaPlayer()
{
	if (!*GetOptions().Gameplay.adriaRefillsMana)
		return;

	Player &myPlayer = *MyPlayer;

	if (myPlayer._pMana != myPlayer._pMaxMana) {
		PlaySFX(SfxID::CastHealing);
	}
	myPlayer._pMana = myPlayer._pMaxMana;
	myPlayer._pManaBase = myPlayer._pMaxManaBase;
	RedrawComponent(PanelDrawComponent::Mana);
}

void StartWitch()
{
	FillManaPlayer();
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, _("Witch's shack"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Adria"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Buy items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 16, _("Sell items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Recharge staves"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 20, _("Leave the shack"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void ScrollWitchBuy(int idx)
{
	ScrollVendorStore(WitchItems, static_cast<int>(std::size(WitchItems)), idx);
}

void WitchBookLevel(Item &bookItem)
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

void StartWitchBuy()
{
	IsTextFullSize = true;
	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = 20;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollWitchBuy(ScrollPos);
	AddItemListBackButton();

	CurrentItemIndex = 0;
	for (Item &item : WitchItems) {
		if (item.isEmpty())
			continue;

		WitchBookLevel(item);
		item._iStatFlag = MyPlayer->CanUseItem(item);
		CurrentItemIndex++;
	}
	NumTextLines = std::max(CurrentItemIndex - 4, 0);
}

bool WitchSellOk(int i)
{
	Item *pI;

	bool rv = false;

	if (i >= 0)
		pI = &MyPlayer->InvList[i];
	else
		pI = &MyPlayer->SpdList[-(i + 1)];

	if (pI->_itype == ItemType::Misc)
		rv = true;
	if (pI->_iMiscId > 29 && pI->_iMiscId < 41)
		rv = false;
	if (pI->_iClass == ICLASS_QUEST)
		rv = false;
	if (pI->_itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(pI->_iSpell)))
		rv = true;
	if (pI->IDidx >= IDI_FIRSTQUEST && pI->IDidx <= IDI_LASTQUEST)
		rv = false;
	if (pI->IDidx == IDI_LAZSTAFF)
		rv = false;
	return rv;
}

void StartWitchSell()
{
	IsTextFullSize = true;
	bool sellok = false;
	CurrentItemIndex = 0;

	for (auto &item : PlayerItems) {
		item.clear();
	}

	const Player &myPlayer = *MyPlayer;

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (WitchSellOk(i)) {
			sellok = true;
			PlayerItems[CurrentItemIndex] = myPlayer.InvList[i];

			if (PlayerItems[CurrentItemIndex]._iMagical != ITEM_QUALITY_NORMAL && PlayerItems[CurrentItemIndex]._iIdentified)
				PlayerItems[CurrentItemIndex]._ivalue = PlayerItems[CurrentItemIndex]._iIvalue;

			PlayerItems[CurrentItemIndex]._ivalue = std::max(PlayerItems[CurrentItemIndex]._ivalue / 4, 1);
			PlayerItems[CurrentItemIndex]._iIvalue = PlayerItems[CurrentItemIndex]._ivalue;
			PlayerItemIndexes[CurrentItemIndex] = i;
			CurrentItemIndex++;
		}
	}

	for (int i = 0; i < MaxBeltItems; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (!myPlayer.SpdList[i].isEmpty() && WitchSellOk(-(i + 1))) {
			sellok = true;
			PlayerItems[CurrentItemIndex] = myPlayer.SpdList[i];

			if (PlayerItems[CurrentItemIndex]._iMagical != ITEM_QUALITY_NORMAL && PlayerItems[CurrentItemIndex]._iIdentified)
				PlayerItems[CurrentItemIndex]._ivalue = PlayerItems[CurrentItemIndex]._iIvalue;

			PlayerItems[CurrentItemIndex]._ivalue = std::max(PlayerItems[CurrentItemIndex]._ivalue / 4, 1);
			PlayerItems[CurrentItemIndex]._iIvalue = PlayerItems[CurrentItemIndex]._ivalue;
			PlayerItemIndexes[CurrentItemIndex] = -(i + 1);
			CurrentItemIndex++;
		}
	}

	if (!sellok) {
		HasScrollbar = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing I want."), UiFlags::ColorWhitegold, false);

		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Which item is for sale?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(ScrollPos);
	AddItemListBackButton();
}

bool WitchRechargeOk(int i)
{
	const auto &item = MyPlayer->InvList[i];

	if (item._itype == ItemType::Staff && item._iCharges != item._iMaxCharges) {
		return true;
	}

	if ((item._iMiscId == IMISC_UNIQUE || item._iMiscId == IMISC_STAFF) && item._iCharges < item._iMaxCharges) {
		return true;
	}

	return false;
}

void AddStoreHoldRecharge(Item itm, int8_t i)
{
	PlayerItems[CurrentItemIndex] = itm;
	PlayerItems[CurrentItemIndex]._ivalue += GetSpellData(itm._iSpell).staffCost();
	PlayerItems[CurrentItemIndex]._ivalue = PlayerItems[CurrentItemIndex]._ivalue * (PlayerItems[CurrentItemIndex]._iMaxCharges - PlayerItems[CurrentItemIndex]._iCharges) / (PlayerItems[CurrentItemIndex]._iMaxCharges * 2);
	PlayerItems[CurrentItemIndex]._iIvalue = PlayerItems[CurrentItemIndex]._ivalue;
	PlayerItemIndexes[CurrentItemIndex] = i;
	CurrentItemIndex++;
}

void StartWitchRecharge()
{
	IsTextFullSize = true;
	bool rechargeok = false;
	CurrentItemIndex = 0;

	for (auto &item : PlayerItems) {
		item.clear();
	}

	const Player &myPlayer = *MyPlayer;
	const auto &leftHand = myPlayer.InvBody[INVLOC_HAND_LEFT];

	if ((leftHand._itype == ItemType::Staff || leftHand._iMiscId == IMISC_UNIQUE) && leftHand._iCharges != leftHand._iMaxCharges) {
		rechargeok = true;
		AddStoreHoldRecharge(leftHand, -1);
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (CurrentItemIndex >= 48)
			break;
		if (WitchRechargeOk(i)) {
			rechargeok = true;
			AddStoreHoldRecharge(myPlayer.InvList[i], i);
		}
	}

	if (!rechargeok) {
		HasScrollbar = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to recharge."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Recharge which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);
	ScrollSmithSell(ScrollPos);
	AddItemListBackButton();
}

void StoreNoMoney()
{
	StartStore(OldActiveStore);
	HasScrollbar = false;
	IsTextFullSize = true;
	RenderGold = true;
	ClearSText(5, 23);
	AddSText(0, 14, _("You do not have enough gold"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StoreNoRoom()
{
	StartStore(OldActiveStore);
	HasScrollbar = false;
	ClearSText(5, 23);
	AddSText(0, 14, _("You do not have enough room in inventory"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StoreConfirm(Item &item)
{
	StartStore(OldActiveStore);
	HasScrollbar = false;
	ClearSText(5, 23);

	UiFlags itemColor = item.getTextColorWithStatCheck();
	AddSText(20, 8, item.getName(), itemColor, false);
	AddSTextVal(8, item._iIvalue);
	PrintStoreItem(item, 9, itemColor);

	std::string_view prompt;

	switch (OldActiveStore) {
	case TalkID::BoyBuy:
		prompt = _("Do we have a deal?");
		break;
	case TalkID::StorytellerIdentify:
		prompt = _("Are you sure you want to identify this item?");
		break;
	case TalkID::HealerBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::WitchBuy:
	case TalkID::SmithBuy:
		prompt = _("Are you sure you want to buy this item?");
		break;
	case TalkID::WitchRecharge:
		prompt = _("Are you sure you want to recharge this item?");
		break;
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		prompt = _("Are you sure you want to sell this item?");
		break;
	case TalkID::SmithRepair:
		prompt = _("Are you sure you want to repair this item?");
		break;
	default:
		app_fatal(StrCat("Unknown store dialog ", static_cast<int>(OldActiveStore)));
	}
	AddSText(0, 15, prompt, UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	AddSText(0, 18, _("Yes"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 20, _("No"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StartBoy()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, _("Wirt the Peg-legged boy"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSLine(5);
	if (!BoyItem.isEmpty()) {
		AddSText(0, 8, _("Talk to Wirt"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
		AddSText(0, 12, _("I have something for sale,"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 14, _("but it will cost 50 gold"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 16, _("just to take a look. "), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
		AddSText(0, 18, _("What have you got?"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
		AddSText(0, 20, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	} else {
		AddSText(0, 12, _("Talk to Wirt"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
		AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	}
}

void SStartBoyBuy()
{
	IsTextFullSize = true;
	HasScrollbar = false;

	RenderGold = true;
	AddSText(20, 1, _("I have this item for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	BoyItem._iStatFlag = MyPlayer->CanUseItem(BoyItem);
	UiFlags itemColor = BoyItem.getTextColorWithStatCheck();
	AddSText(20, 10, BoyItem.getName(), itemColor, true, BoyItem._iCurs, true);
	if (gbIsHellfire)
		AddSTextVal(10, BoyItem._iIvalue - (BoyItem._iIvalue / 4));
	else
		AddSTextVal(10, BoyItem._iIvalue + (BoyItem._iIvalue / 2));
	PrintStoreItem(BoyItem, 11, itemColor, true);

	{
		// Add a Leave button. Unlike the other item list back buttons,
		// this one has different text and different layout in LargerSmallFont locales.
		const int line = BackButtonLine();
		AddSLine(line - 1);
		AddSText(0, line, _("Leave"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
		TextLine[line]._syoff = 6;
	}
}

void HealPlayer()
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pHitPoints != myPlayer._pMaxHP) {
		PlaySFX(SfxID::CastHealing);
	}
	myPlayer._pHitPoints = myPlayer._pMaxHP;
	myPlayer._pHPBase = myPlayer._pMaxHPBase;
	RedrawComponent(PanelDrawComponent::Health);
}

void StartHealer()
{
	HealPlayer();
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Healer's home"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Pepin"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Buy items"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Leave Healer's home"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void ScrollHealerBuy(int idx)
{
	ScrollVendorStore(HealerItems, static_cast<int>(std::size(HealerItems)), idx);
}

void StartHealerBuy()
{
	IsTextFullSize = true;
	HasScrollbar = true;
	ScrollPos = 0;

	RenderGold = true;
	AddSText(20, 1, _("I have these items for sale:"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollHealerBuy(ScrollPos);
	AddItemListBackButton();

	CurrentItemIndex = 0;
	for (Item &item : HealerItems) {
		if (item.isEmpty())
			continue;

		item._iStatFlag = MyPlayer->CanUseItem(item);
		CurrentItemIndex++;
	}

	NumTextLines = std::max(CurrentItemIndex - 4, 0);
}

void StartStoryteller()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, _("The Town Elder"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Cain"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Identify an item"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
}

bool IdItemOk(Item *i)
{
	if (i->isEmpty()) {
		return false;
	}
	if (i->_iMagical == ITEM_QUALITY_NORMAL) {
		return false;
	}
	return !i->_iIdentified;
}

void AddStoreHoldId(Item itm, int8_t i)
{
	PlayerItems[CurrentItemIndex] = itm;
	PlayerItems[CurrentItemIndex]._ivalue = 100;
	PlayerItems[CurrentItemIndex]._iIvalue = 100;
	PlayerItemIndexes[CurrentItemIndex] = i;
	CurrentItemIndex++;
}

void StartStorytellerIdentify()
{
	bool idok = false;
	IsTextFullSize = true;
	CurrentItemIndex = 0;

	for (auto &item : PlayerItems) {
		item.clear();
	}

	Player &myPlayer = *MyPlayer;

	auto &helmet = myPlayer.InvBody[INVLOC_HEAD];
	if (IdItemOk(&helmet)) {
		idok = true;
		AddStoreHoldId(helmet, -1);
	}

	auto &armor = myPlayer.InvBody[INVLOC_CHEST];
	if (IdItemOk(&armor)) {
		idok = true;
		AddStoreHoldId(armor, -2);
	}

	auto &leftHand = myPlayer.InvBody[INVLOC_HAND_LEFT];
	if (IdItemOk(&leftHand)) {
		idok = true;
		AddStoreHoldId(leftHand, -3);
	}

	auto &rightHand = myPlayer.InvBody[INVLOC_HAND_RIGHT];
	if (IdItemOk(&rightHand)) {
		idok = true;
		AddStoreHoldId(rightHand, -4);
	}

	auto &leftRing = myPlayer.InvBody[INVLOC_RING_LEFT];
	if (IdItemOk(&leftRing)) {
		idok = true;
		AddStoreHoldId(leftRing, -5);
	}

	auto &rightRing = myPlayer.InvBody[INVLOC_RING_RIGHT];
	if (IdItemOk(&rightRing)) {
		idok = true;
		AddStoreHoldId(rightRing, -6);
	}

	auto &amulet = myPlayer.InvBody[INVLOC_AMULET];
	if (IdItemOk(&amulet)) {
		idok = true;
		AddStoreHoldId(amulet, -7);
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (CurrentItemIndex >= 48)
			break;
		auto &item = myPlayer.InvList[i];
		if (IdItemOk(&item)) {
			idok = true;
			AddStoreHoldId(item, i);
		}
	}

	if (!idok) {
		HasScrollbar = false;

		RenderGold = true;
		AddSText(20, 1, _("You have nothing to identify."), UiFlags::ColorWhitegold, false);
		AddSLine(3);
		AddItemListBackButton(/*selectable=*/true);
		return;
	}

	HasScrollbar = true;
	ScrollPos = 0;
	NumTextLines = myPlayer._pNumInv;

	RenderGold = true;
	AddSText(20, 1, _("Identify which item?"), UiFlags::ColorWhitegold, false);
	AddSLine(3);

	ScrollSmithSell(ScrollPos);
	AddItemListBackButton();
}

void StartStorytellerIdentifyShow(Item &item)
{
	StartStore(OldActiveStore);
	HasScrollbar = false;
	ClearSText(5, 23);

	UiFlags itemColor = item.getTextColorWithStatCheck();

	AddSText(0, 7, _("This item is:"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
	AddSText(20, 11, item.getName(), itemColor, false);
	PrintStoreItem(item, 12, itemColor);
	AddSText(0, 18, _("Done"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
}

void StartTalk()
{
	int la;

	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, fmt::format(fmt::runtime(_("Talk to {:s}")), _(TownerNames[TownerId])), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSLine(5);
	if (gbIsSpawn) {
		AddSText(0, 10, fmt::format(fmt::runtime(_("Talking to {:s}")), _(TownerNames[TownerId])), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 12, _("is not available"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 14, _("in the shareware"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddSText(0, 16, _("version"), UiFlags::ColorWhite | UiFlags::AlignCenter, false);
		AddOptionsBackButton();
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
			AddSText(0, sn, _(QuestsData[quest._qidx]._qlstr), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
			sn += la;
		}
	}
	AddSText(0, sn2, _("Gossip"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddOptionsBackButton();
}

void StartTavern()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 1, _("Welcome to the"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 3, _("Rising Sun"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Ogden"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Leave the tavern"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void StartBarmaid()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, _("Gillian"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Gillian"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 14, _("Access Storage"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Say goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void StartDrunk()
{
	IsTextFullSize = false;
	HasScrollbar = false;
	AddSText(0, 2, _("Farnham the Drunk"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 9, _("Would you like to:"), UiFlags::ColorWhitegold | UiFlags::AlignCenter, false);
	AddSText(0, 12, _("Talk to Farnham"), UiFlags::ColorBlue | UiFlags::AlignCenter, true);
	AddSText(0, 18, _("Say Goodbye"), UiFlags::ColorWhite | UiFlags::AlignCenter, true);
	AddSLine(5);
	CurrentItemIndex = 20;
}

void SmithEnter()
{
	switch (CurrentTextLine) {
	case 10:
		TownerId = TOWN_SMITH;
		OldTextLine = 10;
		OldActiveStore = TalkID::Smith;
		StartStore(TalkID::Gossip);
		break;
	case 12:
		StartStore(TalkID::SmithBuy);
		break;
	case 14:
		StartStore(TalkID::SmithPremiumBuy);
		break;
	case 16:
		StartStore(TalkID::SmithSell);
		break;
	case 18:
		StartStore(TalkID::SmithRepair);
		break;
	case 20:
		ActiveStore = TalkID::None;
		break;
	}
}

/**
 * @brief Purchases an item from the smith.
 */
void SmithBuyItem(Item &item)
{
	TakePlrsMoney(item._iIvalue);
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);
	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	if (idx == NumSmithBasicItemsHf - 1) {
		SmithItems[NumSmithBasicItemsHf - 1].clear();
	} else {
		for (; !SmithItems[idx + 1].isEmpty(); idx++) {
			SmithItems[idx] = std::move(SmithItems[idx + 1]);
		}
		SmithItems[idx].clear();
	}
	CalcPlrInv(*MyPlayer, true);
}

void SmithBuyEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Smith);
		CurrentTextLine = 12;
		return;
	}

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;
	OldActiveStore = TalkID::SmithBuy;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);
	if (!PlayerCanAfford(SmithItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(SmithItems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = SmithItems[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Purchases a premium item from the smith.
 */
void SmithBuyPItem(Item &item)
{
	TakePlrsMoney(item._iIvalue);
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	int xx = 0;
	for (int i = 0; idx >= 0; i++) {
		if (!PremiumItems[i].isEmpty()) {
			idx--;
			xx = i;
		}
	}

	PremiumItems[xx].clear();
	PremiumItemCount--;
	SpawnPremium(*MyPlayer);
}

void SmithPremiumBuyEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Smith);
		CurrentTextLine = 14;
		return;
	}

	OldActiveStore = TalkID::SmithPremiumBuy;
	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int xx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);
	int idx = 0;
	for (int i = 0; xx >= 0; i++) {
		if (!PremiumItems[i].isEmpty()) {
			xx--;
			idx = i;
		}
	}

	if (!PlayerCanAfford(PremiumItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(PremiumItems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = PremiumItems[idx];
	StartStore(TalkID::Confirm);
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
void StoreSellItem()
{
	Player &myPlayer = *MyPlayer;

	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	if (PlayerItemIndexes[idx] >= 0)
		myPlayer.RemoveInvItem(PlayerItemIndexes[idx]);
	else
		myPlayer.RemoveSpdBarItem(-(PlayerItemIndexes[idx] + 1));

	int cost = PlayerItems[idx]._iIvalue;
	CurrentItemIndex--;
	if (idx != CurrentItemIndex) {
		while (idx < CurrentItemIndex) {
			PlayerItems[idx] = PlayerItems[idx + 1];
			PlayerItemIndexes[idx] = PlayerItemIndexes[idx + 1];
			idx++;
		}
	}

	AddGoldToInventory(myPlayer, cost);

	myPlayer._pGold += cost;
}

void SmithSellEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Smith);
		CurrentTextLine = 16;
		return;
	}

	OldTextLine = CurrentTextLine;
	OldActiveStore = TalkID::SmithSell;
	OldScrollPos = ScrollPos;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!StoreGoldFit(PlayerItems[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = PlayerItems[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Repairs an item in the player's inventory or body in the smith.
 */
void SmithRepairItem(int price)
{
	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	PlayerItems[idx]._iDurability = PlayerItems[idx]._iMaxDur;

	int8_t i = PlayerItemIndexes[idx];

	Player &myPlayer = *MyPlayer;

	if (i < 0) {
		if (i == -1)
			myPlayer.InvBody[INVLOC_HEAD]._iDurability = myPlayer.InvBody[INVLOC_HEAD]._iMaxDur;
		if (i == -2)
			myPlayer.InvBody[INVLOC_CHEST]._iDurability = myPlayer.InvBody[INVLOC_CHEST]._iMaxDur;
		if (i == -3)
			myPlayer.InvBody[INVLOC_HAND_LEFT]._iDurability = myPlayer.InvBody[INVLOC_HAND_LEFT]._iMaxDur;
		if (i == -4)
			myPlayer.InvBody[INVLOC_HAND_RIGHT]._iDurability = myPlayer.InvBody[INVLOC_HAND_RIGHT]._iMaxDur;
		TakePlrsMoney(price);
		return;
	}

	myPlayer.InvList[i]._iDurability = myPlayer.InvList[i]._iMaxDur;
	TakePlrsMoney(price);
}

void SmithRepairEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Smith);
		CurrentTextLine = 18;
		return;
	}

	OldActiveStore = TalkID::SmithRepair;
	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!PlayerCanAfford(PlayerItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	TempItem = PlayerItems[idx];
	StartStore(TalkID::Confirm);
}

void WitchEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_WITCH;
		OldActiveStore = TalkID::Witch;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		StartStore(TalkID::WitchBuy);
		break;
	case 16:
		StartStore(TalkID::WitchSell);
		break;
	case 18:
		StartStore(TalkID::WitchRecharge);
		break;
	case 20:
		ActiveStore = TalkID::None;
		break;
	}
}

/**
 * @brief Purchases an item from the witch.
 */
void WitchBuyItem(Item &item)
{
	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);

	if (idx < 3)
		item._iSeed = AdvanceRndSeed();

	TakePlrsMoney(item._iIvalue);
	StoreAutoPlace(item, true);

	if (idx >= 3) {
		if (idx == NumWitchItemsHf - 1) {
			WitchItems[NumWitchItemsHf - 1].clear();
		} else {
			for (; !WitchItems[idx + 1].isEmpty(); idx++) {
				WitchItems[idx] = std::move(WitchItems[idx + 1]);
			}
			WitchItems[idx].clear();
		}
	}

	CalcPlrInv(*MyPlayer, true);
}

void WitchBuyEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Witch);
		CurrentTextLine = 14;
		return;
	}

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;
	OldActiveStore = TalkID::WitchBuy;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!PlayerCanAfford(WitchItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(WitchItems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = WitchItems[idx];
	StartStore(TalkID::Confirm);
}

void WitchSellEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Witch);
		CurrentTextLine = 16;
		return;
	}

	OldTextLine = CurrentTextLine;
	OldActiveStore = TalkID::WitchSell;
	OldScrollPos = ScrollPos;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!StoreGoldFit(PlayerItems[idx])) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = PlayerItems[idx];
	StartStore(TalkID::Confirm);
}

/**
 * @brief Recharges an item in the player's inventory or body in the witch.
 */
void WitchRechargeItem(int price)
{
	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	PlayerItems[idx]._iCharges = PlayerItems[idx]._iMaxCharges;

	Player &myPlayer = *MyPlayer;

	int8_t i = PlayerItemIndexes[idx];
	if (i < 0) {
		myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges = myPlayer.InvBody[INVLOC_HAND_LEFT]._iMaxCharges;
		NetSendCmdChItem(true, INVLOC_HAND_LEFT);
	} else {
		myPlayer.InvList[i]._iCharges = myPlayer.InvList[i]._iMaxCharges;
		NetSyncInvItem(myPlayer, i);
	}

	TakePlrsMoney(price);
	CalcPlrInv(myPlayer, true);
}

void WitchRechargeEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Witch);
		CurrentTextLine = 18;
		return;
	}

	OldActiveStore = TalkID::WitchRecharge;
	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!PlayerCanAfford(PlayerItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	TempItem = PlayerItems[idx];
	StartStore(TalkID::Confirm);
}

void BoyEnter()
{
	if (!BoyItem.isEmpty() && CurrentTextLine == 18) {
		if (!PlayerCanAfford(50)) {
			OldActiveStore = TalkID::Boy;
			OldTextLine = 18;
			OldScrollPos = ScrollPos;
			StartStore(TalkID::NoMoney);
		} else {
			TakePlrsMoney(50);
			StartStore(TalkID::BoyBuy);
		}
		return;
	}

	if ((CurrentTextLine != 8 && !BoyItem.isEmpty()) || (CurrentTextLine != 12 && BoyItem.isEmpty())) {
		ActiveStore = TalkID::None;
		return;
	}

	TownerId = TOWN_PEGBOY;
	OldActiveStore = TalkID::Boy;
	OldTextLine = CurrentTextLine;
	StartStore(TalkID::Gossip);
}

void BoyBuyItem(Item &item, int itemPrice)
{
	TakePlrsMoney(itemPrice);
	StoreAutoPlace(item, true);
	item.clear();
	OldActiveStore = TalkID::Boy;
	CalcPlrInv(*MyPlayer, true);
	OldTextLine = 12;
}

/**
 * @brief Purchases an item from the healer.
 */
void HealerBuyItem(Item &item)
{
	int idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	if (!gbIsMultiplayer) {
		if (idx < 2)
			item._iSeed = AdvanceRndSeed();
	} else {
		if (idx < 3)
			item._iSeed = AdvanceRndSeed();
	}

	TakePlrsMoney(item._iIvalue);
	if (item._iMagical == ITEM_QUALITY_NORMAL)
		item._iIdentified = false;
	StoreAutoPlace(item, true);

	if (!gbIsMultiplayer) {
		if (idx < 2)
			return;
	} else {
		if (idx < 3)
			return;
	}
	idx = OldScrollPos + ((OldTextLine - PreviousScrollPos) / 4);
	if (idx == 19) {
		HealerItems[19].clear();
	} else {
		for (; !HealerItems[idx + 1].isEmpty(); idx++) {
			HealerItems[idx] = std::move(HealerItems[idx + 1]);
		}
		HealerItems[idx].clear();
	}
	CalcPlrInv(*MyPlayer, true);
}

void BoyBuyEnter()
{
	if (CurrentTextLine != 10) {
		ActiveStore = TalkID::None;
		return;
	}

	OldActiveStore = TalkID::BoyBuy;
	OldScrollPos = ScrollPos;
	OldTextLine = 10;
	int price = BoyItem._iIvalue;
	if (gbIsHellfire)
		price -= BoyItem._iIvalue / 4;
	else
		price += BoyItem._iIvalue / 2;

	if (!PlayerCanAfford(price)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(BoyItem, false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = BoyItem;
	TempItem._iIvalue = price;
	StartStore(TalkID::Confirm);
}

void StorytellerIdentifyItem(Item &item)
{
	Player &myPlayer = *MyPlayer;

	int8_t idx = PlayerItemIndexes[((OldTextLine - PreviousScrollPos) / 4) + OldScrollPos];
	if (idx < 0) {
		if (idx == -1)
			myPlayer.InvBody[INVLOC_HEAD]._iIdentified = true;
		if (idx == -2)
			myPlayer.InvBody[INVLOC_CHEST]._iIdentified = true;
		if (idx == -3)
			myPlayer.InvBody[INVLOC_HAND_LEFT]._iIdentified = true;
		if (idx == -4)
			myPlayer.InvBody[INVLOC_HAND_RIGHT]._iIdentified = true;
		if (idx == -5)
			myPlayer.InvBody[INVLOC_RING_LEFT]._iIdentified = true;
		if (idx == -6)
			myPlayer.InvBody[INVLOC_RING_RIGHT]._iIdentified = true;
		if (idx == -7)
			myPlayer.InvBody[INVLOC_AMULET]._iIdentified = true;
	} else {
		myPlayer.InvList[idx]._iIdentified = true;
	}
	item._iIdentified = true;
	TakePlrsMoney(item._iIvalue);
	CalcPlrInv(myPlayer, true);
}

void ConfirmEnter(Item &item)
{
	if (CurrentTextLine == 18) {
		switch (OldActiveStore) {
		case TalkID::SmithBuy:
			SmithBuyItem(item);
			break;
		case TalkID::SmithSell:
		case TalkID::WitchSell:
			StoreSellItem();
			break;
		case TalkID::SmithRepair:
			SmithRepairItem(item._iIvalue);
			break;
		case TalkID::WitchBuy:
			WitchBuyItem(item);
			break;
		case TalkID::WitchRecharge:
			WitchRechargeItem(item._iIvalue);
			break;
		case TalkID::BoyBuy:
			BoyBuyItem(BoyItem, item._iIvalue);
			break;
		case TalkID::HealerBuy:
			HealerBuyItem(item);
			break;
		case TalkID::StorytellerIdentify:
			StorytellerIdentifyItem(item);
			StartStore(TalkID::StorytellerIdentifyShow);
			return;
		case TalkID::SmithPremiumBuy:
			SmithBuyPItem(item);
			break;
		default:
			break;
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

void HealerEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_HEALER;
		OldActiveStore = TalkID::Healer;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		StartStore(TalkID::HealerBuy);
		break;
	case 18:
		ActiveStore = TalkID::None;
		break;
	}
}

void HealerBuyEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Healer);
		CurrentTextLine = 14;
		return;
	}

	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;
	OldActiveStore = TalkID::HealerBuy;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!PlayerCanAfford(HealerItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	if (!StoreAutoPlace(HealerItems[idx], false)) {
		StartStore(TalkID::NoRoom);
		return;
	}

	TempItem = HealerItems[idx];
	StartStore(TalkID::Confirm);
}

void StorytellerEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_STORY;
		OldActiveStore = TalkID::Storyteller;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case 18:
		ActiveStore = TalkID::None;
		break;
	}
}

void StorytellerIdentifyEnter()
{
	if (CurrentTextLine == BackButtonLine()) {
		StartStore(TalkID::Storyteller);
		CurrentTextLine = 14;
		return;
	}

	OldActiveStore = TalkID::StorytellerIdentify;
	OldTextLine = CurrentTextLine;
	OldScrollPos = ScrollPos;

	int idx = ScrollPos + ((CurrentTextLine - PreviousScrollPos) / 4);

	if (!PlayerCanAfford(PlayerItems[idx]._iIvalue)) {
		StartStore(TalkID::NoMoney);
		return;
	}

	TempItem = PlayerItems[idx];
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

void TavernEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_TAVERN;
		OldActiveStore = TalkID::Tavern;
		StartStore(TalkID::Gossip);
		break;
	case 18:
		ActiveStore = TalkID::None;
		break;
	}
}

void BarmaidEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_BMAID;
		OldActiveStore = TalkID::Barmaid;
		StartStore(TalkID::Gossip);
		break;
	case 14:
		ActiveStore = TalkID::None;
		IsStashOpen = true;
		Stash.RefreshItemStatFlags();
		invflag = true;
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			if (pcurs == CURSOR_DISARM)
				NewCursor(CURSOR_HAND);
			FocusOnInventory();
		}
		break;
	case 18:
		ActiveStore = TalkID::None;
		break;
	}
}

void DrunkEnter()
{
	switch (CurrentTextLine) {
	case 12:
		OldTextLine = 12;
		TownerId = TOWN_DRUNK;
		OldActiveStore = TalkID::Drunk;
		StartStore(TalkID::Gossip);
		break;
	case 18:
		ActiveStore = TalkID::None;
		break;
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

void AddStoreHoldRepair(Item *itm, int8_t i)
{
	Item *item;
	int v;

	item = &PlayerItems[CurrentItemIndex];
	PlayerItems[CurrentItemIndex] = *itm;

	int due = item->_iMaxDur - item->_iDurability;
	if (item->_iMagical != ITEM_QUALITY_NORMAL && item->_iIdentified) {
		v = 30 * item->_iIvalue * due / (item->_iMaxDur * 100 * 2);
		if (v == 0)
			return;
	} else {
		v = item->_ivalue * due / (item->_iMaxDur * 2);
		v = std::max(v, 1);
	}
	item->_iIvalue = v;
	item->_ivalue = v;
	PlayerItemIndexes[CurrentItemIndex] = i;
	CurrentItemIndex++;
}

void InitStores()
{
	ClearSText(0, NumStoreLines);
	ActiveStore = TalkID::None;
	IsTextFullSize = false;
	HasScrollbar = false;
	PremiumItemCount = 0;
	PremiumItemLevel = 1;

	for (auto &premiumitem : PremiumItems)
		premiumitem.clear();

	BoyItem.clear();
	BoyItemLevel = 0;
}

void SetupTownStores()
{
	Player &myPlayer = *MyPlayer;

	int l = myPlayer.getCharacterLevel() / 2;
	if (!gbIsMultiplayer) {
		l = 0;
		for (int i = 0; i < NUMLEVELS; i++) {
			if (myPlayer._pLvlVisited[i])
				l = i;
		}
	}

	l = std::clamp(l + 2, 6, 16);
	SpawnSmith(l);
	SpawnWitch(l);
	SpawnHealer(l);
	SpawnBoy(myPlayer.getCharacterLevel());
	SpawnPremium(myPlayer);
}

void FreeStoreMem()
{
	if (*GetOptions().Gameplay.showItemGraphicsInStores) {
		FreeHalfSizeItemSprites();
	}
	ActiveStore = TalkID::None;
	for (STextStruct &entry : TextLine) {
		entry.text.clear();
		entry.text.shrink_to_fit();
	}
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
	if (HasScrollbar && line >= 4 && line <= 20) {
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

void ClearSText(int s, int e)
{
	for (int i = s; i < e; i++) {
		TextLine[i]._sx = 0;
		TextLine[i]._syoff = 0;
		TextLine[i].text.clear();
		TextLine[i].text.shrink_to_fit();
		TextLine[i].flags = UiFlags::None;
		TextLine[i].type = STextStruct::Label;
		TextLine[i]._sval = 0;
	}
}

void StartStore(TalkID s)
{
	if (*GetOptions().Gameplay.showItemGraphicsInStores) {
		CreateHalfSizeItemSprites();
	}
	SpellbookFlag = false;
	CloseInventory();
	CloseCharPanel();
	RenderGold = false;
	QuestLogIsOpen = false;
	CloseGoldDrop();
	ClearSText(0, NumStoreLines);
	ReleaseStoreBtn();
	switch (s) {
	case TalkID::Smith:
		StartSmith();
		break;
	case TalkID::SmithBuy: {
		bool hasAnyItems = false;
		for (int i = 0; !SmithItems[i].isEmpty(); i++) {
			hasAnyItems = true;
			break;
		}
		if (hasAnyItems)
			StartSmithBuy();
		else {
			ActiveStore = TalkID::SmithBuy;
			OldTextLine = 12;
			StoreESC();
			return;
		}
		break;
	}
	case TalkID::SmithSell:
		StartSmithSell();
		break;
	case TalkID::SmithRepair:
		StartSmithRepair();
		break;
	case TalkID::Witch:
		StartWitch();
		break;
	case TalkID::WitchBuy:
		if (CurrentItemIndex > 0)
			StartWitchBuy();
		break;
	case TalkID::WitchSell:
		StartWitchSell();
		break;
	case TalkID::WitchRecharge:
		StartWitchRecharge();
		break;
	case TalkID::NoMoney:
		StoreNoMoney();
		break;
	case TalkID::NoRoom:
		StoreNoRoom();
		break;
	case TalkID::Confirm:
		StoreConfirm(TempItem);
		break;
	case TalkID::Boy:
		StartBoy();
		break;
	case TalkID::BoyBuy:
		SStartBoyBuy();
		break;
	case TalkID::Healer:
		StartHealer();
		break;
	case TalkID::Storyteller:
		StartStoryteller();
		break;
	case TalkID::HealerBuy:
		if (CurrentItemIndex > 0)
			StartHealerBuy();
		break;
	case TalkID::StorytellerIdentify:
		StartStorytellerIdentify();
		break;
	case TalkID::SmithPremiumBuy:
		if (!StartSmithPremiumBuy())
			return;
		break;
	case TalkID::Gossip:
		StartTalk();
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStorytellerIdentifyShow(TempItem);
		break;
	case TalkID::Tavern:
		StartTavern();
		break;
	case TalkID::Drunk:
		StartDrunk();
		break;
	case TalkID::Barmaid:
		StartBarmaid();
		break;
	case TalkID::None:
		break;
	}

	CurrentTextLine = -1;
	for (int i = 0; i < NumStoreLines; i++) {
		if (TextLine[i].isSelectable()) {
			CurrentTextLine = i;
			break;
		}
	}

	ActiveStore = s;
}

void DrawSText(const Surface &out)
{
	if (!IsTextFullSize)
		DrawSTextBack(out);
	else
		DrawQTextBack(out);

	if (HasScrollbar) {
		switch (ActiveStore) {
		case TalkID::SmithBuy:
			ScrollSmithBuy(ScrollPos);
			break;
		case TalkID::SmithSell:
		case TalkID::SmithRepair:
		case TalkID::WitchSell:
		case TalkID::WitchRecharge:
		case TalkID::StorytellerIdentify:
			ScrollSmithSell(ScrollPos);
			break;
		case TalkID::WitchBuy:
			ScrollWitchBuy(ScrollPos);
			break;
		case TalkID::HealerBuy:
			ScrollHealerBuy(ScrollPos);
			break;
		case TalkID::SmithPremiumBuy:
			ScrollSmithPremiumBuy(ScrollPos);
			break;
		default:
			break;
		}
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
		PrintSString(out, 28, 1, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())).c_str(), UiFlags::ColorWhitegold | UiFlags::AlignRight);
	}

	if (HasScrollbar)
		DrawSSlider(out, 4, 20);
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
	case TalkID::Smith:
	case TalkID::Witch:
	case TalkID::Boy:
	case TalkID::BoyBuy:
	case TalkID::Healer:
	case TalkID::Storyteller:
	case TalkID::Tavern:
	case TalkID::Drunk:
	case TalkID::Barmaid:
		ActiveStore = TalkID::None;
		break;
	case TalkID::Gossip:
		StartStore(OldActiveStore);
		CurrentTextLine = OldTextLine;
		break;
	case TalkID::SmithBuy:
		StartStore(TalkID::Smith);
		CurrentTextLine = 12;
		break;
	case TalkID::SmithPremiumBuy:
		StartStore(TalkID::Smith);
		CurrentTextLine = 14;
		break;
	case TalkID::SmithSell:
		StartStore(TalkID::Smith);
		CurrentTextLine = 16;
		break;
	case TalkID::SmithRepair:
		StartStore(TalkID::Smith);
		CurrentTextLine = 18;
		break;
	case TalkID::WitchBuy:
		StartStore(TalkID::Witch);
		CurrentTextLine = 14;
		break;
	case TalkID::WitchSell:
		StartStore(TalkID::Witch);
		CurrentTextLine = 16;
		break;
	case TalkID::WitchRecharge:
		StartStore(TalkID::Witch);
		CurrentTextLine = 18;
		break;
	case TalkID::HealerBuy:
		StartStore(TalkID::Healer);
		CurrentTextLine = 14;
		break;
	case TalkID::StorytellerIdentify:
		StartStore(TalkID::Storyteller);
		CurrentTextLine = 14;
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case TalkID::NoMoney:
	case TalkID::NoRoom:
	case TalkID::Confirm:
		StartStore(OldActiveStore);
		CurrentTextLine = OldTextLine;
		ScrollPos = OldScrollPos;
		break;
	case TalkID::None:
		break;
	}
}

void StoreUp()
{
	PlaySFX(SfxID::MenuMove);
	if (CurrentTextLine == -1) {
		return;
	}

	if (HasScrollbar) {
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

	if (HasScrollbar) {
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
	if (CurrentTextLine != -1 && HasScrollbar) {
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
	if (CurrentTextLine != -1 && HasScrollbar) {
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
	Player &myPlayer = *MyPlayer;

	myPlayer._pGold -= std::min(cost, myPlayer._pGold);

	cost = TakeGold(myPlayer, cost, true);
	if (cost != 0) {
		cost = TakeGold(myPlayer, cost, false);
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
	case TalkID::Smith:
		SmithEnter();
		break;
	case TalkID::SmithPremiumBuy:
		SmithPremiumBuyEnter();
		break;
	case TalkID::SmithBuy:
		SmithBuyEnter();
		break;
	case TalkID::SmithSell:
		SmithSellEnter();
		break;
	case TalkID::SmithRepair:
		SmithRepairEnter();
		break;
	case TalkID::Witch:
		WitchEnter();
		break;
	case TalkID::WitchBuy:
		WitchBuyEnter();
		break;
	case TalkID::WitchSell:
		WitchSellEnter();
		break;
	case TalkID::WitchRecharge:
		WitchRechargeEnter();
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
	case TalkID::Boy:
		BoyEnter();
		break;
	case TalkID::BoyBuy:
		BoyBuyEnter();
		break;
	case TalkID::Healer:
		HealerEnter();
		break;
	case TalkID::Storyteller:
		StorytellerEnter();
		break;
	case TalkID::HealerBuy:
		HealerBuyEnter();
		break;
	case TalkID::StorytellerIdentify:
		StorytellerIdentifyEnter();
		break;
	case TalkID::Gossip:
		TalkEnter();
		break;
	case TalkID::StorytellerIdentifyShow:
		StartStore(TalkID::StorytellerIdentify);
		break;
	case TalkID::Drunk:
		DrunkEnter();
		break;
	case TalkID::Tavern:
		TavernEnter();
		break;
	case TalkID::Barmaid:
		BarmaidEnter();
		break;
	case TalkID::None:
		break;
	}
}

void CheckStoreBtn()
{
	const Point uiPosition = GetUIRectangle().position;
	const Rectangle windowRect { { uiPosition.x + 344, uiPosition.y + PaddingTop - 7 }, { 271, 303 } };
	const Rectangle windowRectFull { { uiPosition.x + 24, uiPosition.y + PaddingTop - 7 }, { 591, 303 } };

	if (!IsTextFullSize) {
		if (!windowRect.contains(MousePosition)) {
			while (IsPlayerInStore())
				StoreESC();
		}
	} else {
		if (!windowRectFull.contains(MousePosition)) {
			while (IsPlayerInStore())
				StoreESC();
		}
	}

	if (qtextflag) {
		qtextflag = false;
		if (leveltype == DTYPE_TOWN)
			stream_stop();
	} else if (CurrentTextLine != -1) {
		const int relativeY = MousePosition.y - (uiPosition.y + PaddingTop);

		if (HasScrollbar && MousePosition.x > 600 + uiPosition.x) {
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
			if (HasScrollbar && y <= 20 && !TextLine[y].isSelectable()) {
				if (TextLine[y - 2].isSelectable()) {
					y -= 2;
				} else if (TextLine[y - 1].isSelectable()) {
					y--;
				}
			}
			if (TextLine[y].isSelectable() || (HasScrollbar && y == BackButtonLine())) {
				CurrentTextLine = y;
				StoreEnter();
			}
		}
	}
}

void ReleaseStoreBtn()
{
	CountdownScrollUp = -1;
	CountdownScrollDown = -1;
}

bool IsPlayerInStore()
{
	return ActiveStore != TalkID::None;
}

} // namespace devilution
