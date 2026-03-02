/**
 * @file qol/visual_store.cpp
 *
 * Implementation of visual grid-based store UI.
 */
#include "qol/visual_store.h"

#include <algorithm>
#include <cstdint>
#include <span>

#include "control/control.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "inv.h"
#include "items.h"
#include "minitext.h"
#include "options.h"
#include "panels/info_box.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {

bool IsVisualStoreOpen;
VisualStoreState VisualStore;
int16_t pcursstoreitem = -1;
int16_t pcursstorebtn = -1;

namespace {

OptionalOwnedClxSpriteList VisualStorePanelArt;
OptionalOwnedClxSpriteList VisualStoreNavButtonArt;
OptionalOwnedClxSpriteList VisualStoreRepairAllButtonArt;
OptionalOwnedClxSpriteList VisualStoreRepairButtonArt;

int VisualStoreButtonPressed = -1;

constexpr Size ButtonSize { 27, 16 };

/** Contains mappings for the buttons in the visual store (page nav, tabs, leave) */
constexpr Rectangle VisualStoreButtonRect[] = {
	// Page navigation buttons
	//{ { 19, 19 }, ButtonSize },  // 10 left
	//{ { 56, 19 }, ButtonSize },  // 1 left
	//{ { 242, 19 }, ButtonSize }, // 1 right
	//{ { 279, 19 }, ButtonSize }, // 10 right
	// Tab buttons (Smith only) - positioned below title
	{ { 14, 21 }, { 72, 22 } },      // Basic tab
	{ { 14 + 73, 21 }, { 72, 22 } }, // Premium tab
	{ { 233, 315 }, { 48, 24 } },    // Repair All Btn
	{ { 286, 315 }, { 24, 24 } },    // Repair Btn
};

// constexpr int NavButton10Left = 0;
// constexpr int NavButton1Left = 1;
// constexpr int NavButton1Right = 2;
// constexpr int NavButton10Right = 3;
constexpr int TabButtonBasic = 0;
constexpr int TabButtonPremium = 1;
constexpr int RepairAllBtn = 2;
constexpr int RepairBtn = 3;

/** @brief Get the items array for a specific vendor/tab combination. */
std::span<Item> GetVendorItems(VisualStoreVendor vendor, VisualStoreTab tab)
{
	switch (vendor) {
	case VisualStoreVendor::Smith: {
		if (tab == VisualStoreTab::Premium) {
			return { PremiumItems.data(), static_cast<size_t>(PremiumItems.size()) };
		}
		return { SmithItems.data(), static_cast<size_t>(SmithItems.size()) };
	}
	case VisualStoreVendor::Witch: {
		return { WitchItems.data(), static_cast<size_t>(WitchItems.size()) };
	}
	case VisualStoreVendor::Healer: {
		return { HealerItems.data(), static_cast<size_t>(HealerItems.size()) };
	}
	case VisualStoreVendor::Boy: {
		if (BoyItem.isEmpty()) {
			return {};
		}
		return { &BoyItem, 1 };
	}
	}
	return {};
}

/** @brief Check if the current vendor has tabs (Smith only). */
bool VendorHasTabs()
{
	return VisualStore.vendor == VisualStoreVendor::Smith;
}

/** @brief Check if the current vendor accepts items for sale. */
bool VendorAcceptsSale()
{
	switch (VisualStore.vendor) {
	case VisualStoreVendor::Smith:
	case VisualStoreVendor::Witch: {
		return true;
	}
	case VisualStoreVendor::Healer:
	case VisualStoreVendor::Boy: {
		return false;
	}
	}
	return false;
}

/** @brief Calculate the sell price for an item (1/4 of value). */
int GetSellPrice(const Item &item)
{
	int value = item._ivalue;
	if (item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified)
		value = item._iIvalue;
	return std::max(value / 4, 1);
}

/** @brief Check if Griswold will buy this item. */
bool SmithWillBuy(const Item &item)
{
	if (item.isEmpty())
		return false;

	// Oils are accepted
	if (item._iMiscId > IMISC_OILFIRST && item._iMiscId < IMISC_OILLAST)
		return true;

	if (item._itype == ItemType::Misc)
		return false;
	if (item._itype == ItemType::Gold)
		return false;
	if (item._itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(item._iSpell)))
		return false;
	if (item._iClass == ICLASS_QUEST)
		return false;
	if (item.IDidx == IDI_LAZSTAFF)
		return false;

	return true;
}

/** @brief Check if Adria will buy this item. */
bool WitchWillBuy(const Item &item)
{
	if (item.isEmpty())
		return false;

	bool rv = false;

	if (item._itype == ItemType::Misc)
		rv = true;
	if (item._iMiscId > 29 && item._iMiscId < 41)
		rv = false;
	if (item._iClass == ICLASS_QUEST)
		rv = false;
	if (item._itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(item._iSpell)))
		rv = true;
	if (item.IDidx >= IDI_FIRSTQUEST && item.IDidx <= IDI_LASTQUEST)
		rv = false;
	if (item.IDidx == IDI_LAZSTAFF)
		rv = false;

	return rv;
}

/** @brief Rebuild the grid layout for the current vendor/tab. */
void RefreshVisualStoreLayout()
{
	VisualStore.pages.clear();
	std::span<Item> items = GetVisualStoreItems();

	if (items.empty()) {
		VisualStore.pages.emplace_back();
		VisualStorePage &page = VisualStore.pages.back();
		memset(page.grid, 0, sizeof(page.grid));
		return;
	}

	auto createNewPage = [&]() -> VisualStorePage & {
		VisualStore.pages.emplace_back();
		VisualStorePage &page = VisualStore.pages.back();
		memset(page.grid, 0, sizeof(page.grid));
		return page;
	};

	VisualStorePage *currentPage = &createNewPage();

	for (uint16_t i = 0; i < static_cast<uint16_t>(items.size()); i++) {
		const Item &item = items[i];
		if (item.isEmpty())
			continue;

		const Size itemSize = GetInventorySize(item);
		bool placed = false;

		// Try to place in current page
		for (auto stashPosition : PointsInRectangle(Rectangle { { 0, 0 }, Size { VisualStoreGridWidth - (itemSize.width - 1), VisualStoreGridHeight - (itemSize.height - 1) } })) {
			bool isSpaceFree = true;
			for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
				if (currentPage->grid[itemPoint.x][itemPoint.y] != 0) {
					isSpaceFree = false;
					break;
				}
			}

			if (isSpaceFree) {
				for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
					currentPage->grid[itemPoint.x][itemPoint.y] = i + 1;
				}
				currentPage->items.push_back({ i, stashPosition + Displacement { 0, itemSize.height - 1 } });
				placed = true;
				break;
			}
		}

		if (!placed) {
			// Start new page
			currentPage = &createNewPage();
			// Try placing again in new page
			for (auto stashPosition : PointsInRectangle(Rectangle { { 0, 0 }, Size { VisualStoreGridWidth - (itemSize.width - 1), VisualStoreGridHeight - (itemSize.height - 1) } })) {
				bool isSpaceFree = true;
				for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
					if (currentPage->grid[itemPoint.x][itemPoint.y] != 0) {
						isSpaceFree = false;
						break;
					}
				}

				if (isSpaceFree) {
					for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
						currentPage->grid[itemPoint.x][itemPoint.y] = i + 1;
					}
					currentPage->items.push_back({ i, stashPosition + Displacement { 0, itemSize.height - 1 } });
					placed = true;
					break;
				}
			}
		}
	}

	if (VisualStore.currentPage >= VisualStore.pages.size())
		VisualStore.currentPage = VisualStore.pages.empty() ? 0 : static_cast<unsigned>(VisualStore.pages.size() - 1);
}

} // namespace

void InitVisualStore()
{
	if (HeadlessMode)
		return;

	// For now, reuse the stash panel art as a placeholder
	// In the future, create dedicated visual store assets
	VisualStorePanelArt = LoadClx("data\\store.clx");
	VisualStoreNavButtonArt = LoadClx("data\\tabBtnUp.clx");
	VisualStoreRepairAllButtonArt = LoadClx("data\\repairAllBtn.clx");
	VisualStoreRepairButtonArt = LoadClx("data\\repairSingleBtn.clx");
}

void FreeVisualStoreGFX()
{
	VisualStoreNavButtonArt = std::nullopt;
	VisualStorePanelArt = std::nullopt;
}

void OpenVisualStore(VisualStoreVendor vendor)
{
	IsVisualStoreOpen = true;
	invflag = true; // Open inventory panel alongside

	VisualStore.vendor = vendor;
	VisualStore.activeTab = VisualStoreTab::Basic;
	VisualStore.currentPage = 0;

	pcursstoreitem = -1;
	pcursstorebtn = -1;

	// Refresh item stat flags for current player
	std::span<Item> items = GetVisualStoreItems();
	for (Item &item : items) {
		item._iStatFlag = MyPlayer->CanUseItem(item);
	}

	RefreshVisualStoreLayout();

	// Initialize controller focus to the visual store grid
	FocusOnVisualStore();
}

void CloseVisualStore()
{
	if (IsVisualStoreOpen) {
		IsVisualStoreOpen = false;
		invflag = false;
		pcursstoreitem = -1;
		pcursstorebtn = -1;
		VisualStoreButtonPressed = -1;
		VisualStore.pages.clear();
	}
}

void SetVisualStoreTab(VisualStoreTab tab)
{
	/*if (!VendorHasTabs())
	    return;*/

	VisualStore.activeTab = tab;
	VisualStore.currentPage = 0;
	pcursstoreitem = -1;
	pcursstorebtn = -1;

	// Refresh item stat flags
	std::span<Item> items = GetVisualStoreItems();
	for (Item &item : items) {
		item._iStatFlag = MyPlayer->CanUseItem(item);
	}

	RefreshVisualStoreLayout();
}

void VisualStoreNextPage()
{
	if (VisualStore.currentPage + 1 < VisualStore.pages.size()) {
		VisualStore.currentPage++;
		pcursstoreitem = -1;
		pcursstorebtn = -1;
	}
}

void VisualStorePreviousPage()
{
	if (VisualStore.currentPage > 0) {
		VisualStore.currentPage--;
		pcursstoreitem = -1;
		pcursstorebtn = -1;
	}
}

int GetRepairCost(const Item &item)
{
	if (item.isEmpty() || item._iDurability == item._iMaxDur || item._iMaxDur == DUR_INDESTRUCTIBLE)
		return 0;

	const int due = item._iMaxDur - item._iDurability;
	if (item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified) {
		return 30 * item._iIvalue * due / (item._iMaxDur * 100 * 2);
	} else {
		return std::max(item._ivalue * due / (item._iMaxDur * 2), 1);
	}
}

void VisualStoreRepairAll()
{
	Player &myPlayer = *MyPlayer;
	int totalCost = 0;

	// Check body items
	for (auto &item : myPlayer.InvBody) {
		totalCost += GetRepairCost(item);
	}

	// Check inventory items
	for (int i = 0; i < myPlayer._pNumInv; i++) {
		totalCost += GetRepairCost(myPlayer.InvList[i]);
	}

	if (totalCost == 0)
		return;

	if (!PlayerCanAfford(totalCost)) {
		// Optional: add a message that player can't afford
		return;
	}

	// Execute repairs
	TakePlrsMoney(totalCost);

	for (auto &item : myPlayer.InvBody) {
		if (!item.isEmpty() && item._iMaxDur != DUR_INDESTRUCTIBLE)
			item._iDurability = item._iMaxDur;
	}

	for (int i = 0; i < myPlayer._pNumInv; i++) {
		Item &item = myPlayer.InvList[i];
		if (!item.isEmpty() && item._iMaxDur != DUR_INDESTRUCTIBLE)
			item._iDurability = item._iMaxDur;
	}

	PlaySFX(SfxID::ItemGold);
	CalcPlrInv(myPlayer, true);
}

void VisualStoreRepair()
{
	NewCursor(CURSOR_REPAIR);
}

void VisualStoreRepairItem(int invIndex)
{
	Player &myPlayer = *MyPlayer;
	Item *item = nullptr;

	if (invIndex < INVITEM_INV_FIRST) {
		item = &myPlayer.InvBody[invIndex];
	} else if (invIndex <= INVITEM_INV_LAST) {
		item = &myPlayer.InvList[invIndex - INVITEM_INV_FIRST];
	} else {
		return; // Belt items don't have durability
	}

	if (item->isEmpty())
		return;

	int cost = GetRepairCost(*item);
	if (cost <= 0)
		return;

	if (!PlayerCanAfford(cost)) {
		// PlaySFX(HeroSpeech::ICantUseThisYet); // Or some other error sound
		return;
	}

	TakePlrsMoney(cost);
	item->_iDurability = item->_iMaxDur;
	PlaySFX(SfxID::ItemGold);
	CalcPlrInv(myPlayer, true);
}

Point GetVisualStoreSlotCoord(Point slot)
{
	constexpr int SlotSpacing = INV_SLOT_SIZE_PX + 1;
	// Grid starts below the header area
	// constexpr Displacement GridOffset { 17, 60 + (INV_SLOT_SIZE_PX/2) };

	return GetPanelPosition(UiPanels::Stash, slot * SlotSpacing + Displacement { 17, 44 });
}

Rectangle GetVisualBtnCoord(int btnId)
{
	const Point panelPos = GetPanelPosition(UiPanels::Stash);
	const Rectangle regBtnPos = { panelPos + (VisualStoreButtonRect[btnId].position - Point { 0, 0 }), VisualStoreButtonRect[btnId].size };

	return regBtnPos;
}

int GetVisualStoreItemCount()
{
	std::span<Item> items = GetVisualStoreItems();
	int count = 0;
	for (const Item &item : items) {
		if (!item.isEmpty())
			count++;
	}
	return count;
}

std::span<Item> GetVisualStoreItems()
{
	return GetVendorItems(VisualStore.vendor, VisualStore.activeTab);
}

int GetVisualStorePageCount()
{
	return std::max(1, static_cast<int>(VisualStore.pages.size()));
}

void DrawVisualStore(const Surface &out)
{
	if (!VisualStorePanelArt)
		return;

	// Draw panel background (reusing stash art for now)
	RenderClxSprite(out, (*VisualStorePanelArt)[0], GetPanelPosition(UiPanels::Stash));

	const Point panelPos = GetPanelPosition(UiPanels::Stash);
	const UiFlags styleWhite = UiFlags::VerticalCenter | UiFlags::ColorWhite;
	const UiFlags styleTabPushed = UiFlags::VerticalCenter | UiFlags::ColorButtonpushed;
	constexpr int TextHeight = 13;

	// Draw store title
	/*DrawString(out, GetVendorName(), { panelPos + Displacement { 0, 2 }, { 320, TextHeight } },
	    { .flags = UiFlags::AlignCenter | styleGold });*/

	// Draw tab buttons
	UiFlags basicStyle = VisualStore.activeTab == VisualStoreTab::Basic ? styleWhite : styleTabPushed;
	UiFlags premiumStyle = VisualStore.activeTab == VisualStoreTab::Premium ? styleWhite : styleTabPushed;
	switch (VisualStore.vendor) {
	case VisualStoreVendor::Smith: {
		// Draw pressed button visual
		/*if (VisualStoreButtonPressed >= 0 && VisualStoreButtonPressed <= NavButton10Right && VisualStoreNavButtonArt) {
		    const Point buttonPos = GetPanelPosition(UiPanels::Stash, VisualStoreButtonRect[VisualStoreButtonPressed].position);
		    RenderClxSprite(out, (*VisualStoreNavButtonArt)[VisualStoreButtonPressed], buttonPos);
		}*/

		const Rectangle regBtnPos = { panelPos + (VisualStoreButtonRect[TabButtonBasic].position - Point { 0, 0 }), VisualStoreButtonRect[TabButtonBasic].size };
		RenderClxSprite(out, (*VisualStoreNavButtonArt)[VisualStore.activeTab != VisualStoreTab::Basic], regBtnPos.position);
		DrawString(out, _("Basic"), regBtnPos, { .flags = UiFlags::AlignCenter | basicStyle });

		const Rectangle premBtnPos = { panelPos + (VisualStoreButtonRect[TabButtonPremium].position - Point { 0, 0 }), VisualStoreButtonRect[TabButtonPremium].size };
		RenderClxSprite(out, (*VisualStoreNavButtonArt)[VisualStore.activeTab != VisualStoreTab::Premium], premBtnPos.position);
		DrawString(out, _("Premium"), premBtnPos, { .flags = UiFlags::AlignCenter | premiumStyle });
		break;
	}
	case VisualStoreVendor::Witch:
	case VisualStoreVendor::Boy:
	case VisualStoreVendor::Healer: {
		const Rectangle miscBtnPos = { panelPos + (VisualStoreButtonRect[TabButtonBasic].position - Point { 0, 0 }), VisualStoreButtonRect[TabButtonBasic].size };
		RenderClxSprite(out, (*VisualStoreNavButtonArt)[VisualStoreButtonPressed == TabButtonBasic], miscBtnPos.position);
		DrawString(out, _("Misc"), miscBtnPos, { .flags = UiFlags::AlignCenter | basicStyle });
		break;
	}
	default: {
		break;
	}
	}

	// Draw page number
	/*int pageCount = GetVisualStorePageCount();
	std::string pageText = StrCat(VisualStore.currentPage + 1, "/", pageCount);
	DrawString(out, pageText, { panelPos + Displacement { 132, 40 }, { 57, TextHeight } },
	    { .flags = UiFlags::AlignCenter | styleWhite });*/

	if (VisualStore.currentPage >= VisualStore.pages.size())
		return;

	const VisualStorePage &page = VisualStore.pages[VisualStore.currentPage];
	std::span<Item> allItems = GetVisualStoreItems();

	constexpr Displacement offset { 0, INV_SLOT_SIZE_PX - 1 };

	// First pass: draw item slot backgrounds
	for (int y = 0; y < VisualStoreGridHeight; y++) {
		for (int x = 0; x < VisualStoreGridWidth; x++) {
			const uint16_t itemPlusOne = page.grid[x][y];
			if (itemPlusOne == 0)
				continue;

			const Item &item = allItems[itemPlusOne - 1];
			Point position = GetVisualStoreSlotCoord({ x, y }) + offset;
			InvDrawSlotBack(out, position, InventorySlotSizeInPixels, item._iMagical);
		}
	}

	// Second pass: draw item sprites
	for (const auto &vsItem : page.items) {
		const Item &item = allItems[vsItem.index];
		Point position = GetVisualStoreSlotCoord(vsItem.position) + offset;

		const int frame = item._iCurs + CURSOR_FIRSTITEM;
		const ClxSprite sprite = GetInvItemSprite(frame);

		// Draw highlight outline if this item is hovered
		if (pcursstoreitem == vsItem.index) {
			const uint8_t color = GetOutlineColor(item, true);
			ClxDrawOutline(out, color, position, sprite);
		}

		DrawItem(item, out, position, sprite);
	}

	// Draw player gold at bottom
	uint32_t totalGold = MyPlayer->_pGold + Stash.gold;
	DrawString(out, StrCat(_("Gold: "), FormatInteger(totalGold)),
	    { panelPos + Displacement { 20, 320 }, { 280, TextHeight } },
	    { .flags = styleWhite });

	// Draw Repair All
	if (VisualStore.vendor == VisualStoreVendor::Smith) {
		const Rectangle repairAllBtnPos = { panelPos + (VisualStoreButtonRect[RepairAllBtn].position - Point { 0, 0 }), VisualStoreButtonRect[RepairAllBtn].size };
		RenderClxSprite(out, (*VisualStoreRepairAllButtonArt)[VisualStoreButtonPressed == RepairAllBtn], repairAllBtnPos.position);
		// DrawString(out, _("Repair All"), repairAllBtnPos, { .flags = UiFlags::AlignCenter | styleWhite

		// Draw Repair
		const Rectangle repairBtnPos = { panelPos + (VisualStoreButtonRect[RepairBtn].position - Point { 0, 0 }), VisualStoreButtonRect[RepairBtn].size };
		RenderClxSprite(out, (*VisualStoreRepairButtonArt)[VisualStoreButtonPressed == RepairBtn], repairBtnPos.position);
		// DrawString(out, _("Repair"), repairBtnPos, { .flags = UiFlags::AlignCenter | styleWhite });
	}
}

int16_t CheckVisualStoreHLight(Point mousePosition)
{
	// Check buttons first
	const Point panelPos = GetPanelPosition(UiPanels::Stash);
	if (MyPlayer->HoldItem.isEmpty()) {
		for (int i = 0; i < 4; i++) {
			// Skip tab buttons if vendor doesn't have tabs
			if (!VendorHasTabs() && i != TabButtonBasic)
				continue;

			Rectangle button = VisualStoreButtonRect[i];
			button.position = panelPos + (button.position - Point { 0, 0 });

			if (button.contains(mousePosition)) {
				if (i == TabButtonBasic) {
					if (VendorHasTabs()) {
						InfoString = _("Basic");
						FloatingInfoString = _("Basic");
						AddInfoBoxString(_("Basic items"));
						AddInfoBoxString(_("Basic items"), true);
					} else {
						InfoString = _("Misc");
						FloatingInfoString = _("Misc");
						AddInfoBoxString(_("Miscellaneous items"));
						AddInfoBoxString(_("Miscellaneous items"), true);
					}
					InfoColor = UiFlags::ColorWhite;
					pcursstorebtn = TabButtonBasic;
					return -1;
				} else if (i == TabButtonPremium) {
					InfoString = _("Premium");
					FloatingInfoString = _("Premium");
					AddInfoBoxString(_("Premium items"));
					AddInfoBoxString(_("Premium items"), true);
					InfoColor = UiFlags::ColorWhite;
					pcursstorebtn = TabButtonPremium;
					return -1;
				} else if (i == RepairAllBtn) {
					int totalCost = 0;
					Player &myPlayer = *MyPlayer;
					for (auto &item : myPlayer.InvBody)
						totalCost += GetRepairCost(item);
					for (int j = 0; j < myPlayer._pNumInv; j++)
						totalCost += GetRepairCost(myPlayer.InvList[j]);

					InfoString = _("Repair All");
					FloatingInfoString = _("Repair All");
					if (totalCost > 0) {
						AddInfoBoxString(StrCat(FormatInteger(totalCost), " Gold"));
						AddInfoBoxString(StrCat(FormatInteger(totalCost), " Gold"), true);
					} else {
						AddInfoBoxString(_("Nothing to repair"));
						AddInfoBoxString(_("Nothing to repair"), true);
					}
					InfoColor = UiFlags::ColorWhite;
					pcursstorebtn = RepairAllBtn;
					return -1;
				} else if (i == RepairBtn) {
					InfoString = _("Repair");
					FloatingInfoString = _("Repair");
					AddInfoBoxString(_("Repair a single item"));
					AddInfoBoxString(_("Repair a single item"), true);
					InfoColor = UiFlags::ColorWhite;
					pcursstorebtn = RepairBtn;
					return -1;
				}
			}
		}
	}

	if (VisualStore.currentPage >= VisualStore.pages.size())
		return -1;

	const VisualStorePage &page = VisualStore.pages[VisualStore.currentPage];
	std::span<Item> allItems = GetVisualStoreItems();

	for (int y = 0; y < VisualStoreGridHeight; y++) {
		for (int x = 0; x < VisualStoreGridWidth; x++) {
			const uint16_t itemPlusOne = page.grid[x][y];
			if (itemPlusOne == 0)
				continue;

			const int itemIndex = itemPlusOne - 1;
			const Item &item = allItems[itemIndex];

			const Rectangle cell {
				GetVisualStoreSlotCoord({ x, y }),
				InventorySlotSizeInPixels + 1
			};

			if (cell.contains(mousePosition)) {
				// Set up info display
				InfoColor = item.getTextColor();
				InfoString = item.getName();

				const int price = item._iIvalue;
				const bool canAfford = PlayerCanAfford(price);

				InfoString = item.getName();
				FloatingInfoString = item.getName();
				InfoColor = canAfford ? item.getTextColor() : UiFlags::ColorRed;

				if (item._iIdentified) {
					PrintItemDetails(item);
				} else {
					PrintItemDur(item);
				}

				AddInfoBoxString(StrCat("", FormatInteger(price), " Gold"));

				return static_cast<int16_t>(itemIndex);
			}
		}
	}

	return -1;
}

void CheckVisualStoreItem(Point mousePosition, bool isCtrlHeld, bool isShiftHeld)
{
	// Check if clicking on an item to buy
	int16_t itemIndex = CheckVisualStoreHLight(mousePosition);
	if (itemIndex < 0)
		return;

	std::span<Item> items = GetVisualStoreItems();
	if (itemIndex >= static_cast<int16_t>(items.size()))
		return;

	Item &item = items[itemIndex];
	if (item.isEmpty())
		return;

	// Check if player can afford the item
	int price = item._iIvalue;
	uint32_t totalGold = MyPlayer->_pGold + Stash.gold;
	if (totalGold < static_cast<uint32_t>(price)) {
		// InitDiabloMsg(EMSG_NOT_ENOUGH_GOLD);
		return;
	}

	// Check if player has room for the item
	if (!StoreAutoPlace(item, false)) {
		// InitDiabloMsg(EMSG_INVENTORY_FULL);
		return;
	}

	// Execute the purchase
	TakePlrsMoney(price);
	StoreAutoPlace(item, true);
	PlaySFX(ItemInvSnds[ItemCAnimTbl[item._iCurs]]);

	// Remove item from store (vendor-specific handling)
	switch (VisualStore.vendor) {
	case VisualStoreVendor::Smith: {
		if (VisualStore.activeTab == VisualStoreTab::Premium) {
			// Premium items get replaced
			PremiumItems[itemIndex].clear();
			SpawnPremium(*MyPlayer);
		} else {
			// Basic items are removed
			SmithItems.erase(SmithItems.begin() + itemIndex);
		}
		break;
	}
	case VisualStoreVendor::Witch: {
		// First 3 items are pinned, don't remove them
		if (itemIndex >= 3) {
			WitchItems.erase(WitchItems.begin() + itemIndex);
		}
		break;
	}
	case VisualStoreVendor::Healer: {
		// First 2-3 items are pinned
		if (itemIndex >= (gbIsMultiplayer ? 3 : 2)) {
			HealerItems.erase(HealerItems.begin() + itemIndex);
		}
		break;
	}
	case VisualStoreVendor::Boy: {
		BoyItem.clear();
		break;
	}
	}

	pcursstoreitem = -1;
	RefreshVisualStoreLayout();
}

void CheckVisualStorePaste(Point mousePosition)
{
	if (!VendorAcceptsSale())
		return;

	Player &player = *MyPlayer;
	if (player.HoldItem.isEmpty())
		return;

	// Check if the item can be sold to this vendor
	if (!CanSellToCurrentVendor(player.HoldItem)) {
		player.SaySpecific(HeroSpeech::ICantDoThat);
		return;
	}

	// Calculate sell price
	int sellPrice = GetSellPrice(player.HoldItem);

	// Add gold to player
	AddGoldToInventory(player, sellPrice);
	PlaySFX(SfxID::ItemGold);

	// Clear the held item
	player.HoldItem.clear();
	NewCursor(CURSOR_HAND);
}

bool CanSellToCurrentVendor(const Item &item)
{
	if (item.isEmpty())
		return false;

	switch (VisualStore.vendor) {
	case VisualStoreVendor::Smith: {
		return SmithWillBuy(item);
	}
	case VisualStoreVendor::Witch: {
		return WitchWillBuy(item);
	}
	case VisualStoreVendor::Healer:
	case VisualStoreVendor::Boy: {
		return false;
	}
	}
	return false;
}

void SellItemToVisualStore(int invIndex)
{
	if (!VendorAcceptsSale())
		return;

	Player &player = *MyPlayer;
	Item &item = player.InvList[invIndex];

	if (!CanSellToCurrentVendor(item)) {
		player.SaySpecific(HeroSpeech::ICantDoThat);
		return;
	}

	// Calculate sell price
	int sellPrice = GetSellPrice(item);

	// Add gold to player
	AddGoldToInventory(player, sellPrice);
	PlaySFX(SfxID::ItemGold);

	// Remove item from inventory
	player.RemoveInvItem(invIndex);
}

void CheckVisualStoreButtonPress(Point mousePosition)
{
	if (!MyPlayer->HoldItem.isEmpty())
		return;

	for (int i = 0; i < 4; i++) {
		// Skip tab buttons if vendor doesn't have tabs
		if (!VendorHasTabs() && i != TabButtonBasic)
			continue;

		Rectangle button = VisualStoreButtonRect[i];
		button.position = GetPanelPosition(UiPanels::Stash, button.position);

		if (button.contains(mousePosition)) {
			VisualStoreButtonPressed = i;
			return;
		}
	}

	VisualStoreButtonPressed = -1;
}

void CheckVisualStoreButtonRelease(Point mousePosition)
{
	if (VisualStoreButtonPressed == -1)
		return;

	Rectangle button = VisualStoreButtonRect[VisualStoreButtonPressed];
	button.position = GetPanelPosition(UiPanels::Stash, button.position);

	if (button.contains(mousePosition)) {
		switch (VisualStoreButtonPressed) {
		// case NavButton10Left:
		//	for (int i = 0; i < 10 && VisualStore.currentPage > 0; i++)
		//		VisualStorePreviousPage();
		//	break;
		// case NavButton1Left:
		//	VisualStorePreviousPage();
		//	break;
		// case NavButton1Right:
		//	VisualStoreNextPage();
		//	break;
		// case NavButton10Right:
		//	for (int i = 0; i < 10; i++)
		//		VisualStoreNextPage();
		//	break;
		case TabButtonBasic: {
			SetVisualStoreTab(VisualStoreTab::Basic);
			break;
		}
		case TabButtonPremium: {
			SetVisualStoreTab(VisualStoreTab::Premium);
			break;
		}
		case RepairAllBtn: {
			VisualStoreRepairAll();
			break;
		}
		case RepairBtn: {
			VisualStoreRepair();
			break;
		}
		}
	}

	VisualStoreButtonPressed = -1;
}

} // namespace devilution
