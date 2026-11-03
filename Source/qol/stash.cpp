#include "qol/stash.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

#ifdef USE_SDL3
#include <SDL3/SDL_keyboard.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/text_input.hpp"
#include "control/control.hpp"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "inv.h"
#include "loadsave.h"
#include "lua/lua_event.hpp"
#include "minitext.h"
#include "pfile.h"
#include "stores.h"
#include "utils/display.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

bool IsStashOpen;
StashStruct Stash;
bool IsWithdrawGoldOpen;

namespace {

// Lua mod support: a mod-registered stash and the settings it was registered with.
struct LuaModStash {
	StashStruct stash;
	unsigned pageCount;
	bool allowsGold;
	bool loaded = false;
};

// std::map keeps entries at stable addresses, so ActiveLuaModStash stays valid across registrations.
std::map<std::string, LuaModStash, std::less<>> LuaModStashes;
LuaModStash *ActiveLuaModStash = nullptr;
std::string ActiveLuaModStashName;

char GoldWithdrawText[21];
TextInputCursorState GoldWithdrawCursor;
std::optional<NumberInputState> GoldWithdrawInputState;

constexpr Size ButtonSize { 27, 16 };
/** Contains mappings for the buttons in the stash (2 navigation buttons, withdraw gold buttons, 2 navigation buttons) */
constexpr Rectangle StashButtonRect[] = {
	// clang-format off
	{ {  19, 19 }, ButtonSize }, // 10 left
	{ {  56, 19 }, ButtonSize }, // 1 left
	{ {  93, 19 }, ButtonSize }, // withdraw gold
	{ { 242, 19 }, ButtonSize }, // 1 right
	{ { 279, 19 }, ButtonSize }  // 10 right
	// clang-format on
};

OptionalOwnedClxSpriteList StashPanelArt;
OptionalOwnedClxSpriteList StashNavButtonArt;

/**
 * @param page The stash page index.
 * @param position Position to add the item to.
 * @param stashListIndex The item's StashList index
 * @param itemSize Size of item
 */
void AddItemToStashGrid(unsigned page, Point position, uint16_t stashListIndex, Size itemSize)
{
	for (const Point point : PointsInRectangle(Rectangle { position, itemSize })) {
		GetActiveStash().stashGrids[page][point.x][point.y] = stashListIndex + 1;
	}
}

std::optional<Point> FindTargetSlotUnderItemCursor(Point cursorPosition, Size itemSize)
{
	for (auto point : StashGridRange) {
		const Rectangle cell {
			GetStashSlotCoord(point),
			InventorySlotSizeInPixels + 1
		};

		if (cell.contains(cursorPosition)) {
			// When trying to paste into the stash we need to determine the top left cell of the nearest area that could fit the item, not the slot under the center/hot pixel.
			if (itemSize.height <= 1 && itemSize.width <= 1) {
				// top left cell of a 1x1 item is the same cell as the hot pixel, no work to do
				return point;
			}
			// Otherwise work out how far the central cell is from the top-left cell
			Displacement hotPixelCellOffset = { (itemSize.width - 1) / 2, (itemSize.height - 1) / 2 };
			// For even dimension items we need to work out if the cursor is in the left/right (or top/bottom) half of the central cell and adjust the offset so the item lands in the area most covered by the cursor.
			if (itemSize.width % 2 == 0 && cell.contains(cursorPosition + Displacement { INV_SLOT_HALF_SIZE_PX, 0 })) {
				// hot pixel was in the left half of the cell, so we want to increase the offset to preference the column to the left
				hotPixelCellOffset.deltaX++;
			}
			if (itemSize.height % 2 == 0 && cell.contains(cursorPosition + Displacement { 0, INV_SLOT_HALF_SIZE_PX })) {
				// hot pixel was in the top half of the cell, so we want to increase the offset to preference the row above
				hotPixelCellOffset.deltaY++;
			}
			// Then work out the top left cell of the nearest area that could fit this item (as pasting on the edge of the stash would otherwise put it out of bounds)
			point.y = std::clamp(point.y - hotPixelCellOffset.deltaY, 0, StashGridSize.height - itemSize.height);
			point.x = std::clamp(point.x - hotPixelCellOffset.deltaX, 0, StashGridSize.width - itemSize.width);
			return point;
		}
	}

	return {};
}

bool IsItemAllowedInStash(const Item &item)
{
	return lua::OnItemAllowedInStash(&item, item._iMiscId != IMISC_ARENAPOT); // Lua mod support
}

void CheckStashPaste(Point cursorPosition)
{
	Player &player = *MyPlayer;
	StashStruct &stash = GetActiveStash();

	if (!IsItemAllowedInStash(player.HoldItem))
		return;

	if (player.HoldItem._itype == ItemType::Gold) {
		if (!stash.allowsGold)
			return;
		if (stash.gold > std::numeric_limits<int>::max() - player.HoldItem._ivalue)
			return;
		stash.gold += player.HoldItem._ivalue;
		player.HoldItem.clear();
		PlaySFX(SfxID::ItemGold);
		stash.dirty = true;
		NewCursor(CURSOR_HAND);
		return;
	}

	const Size itemSize = GetInventorySize(player.HoldItem);

	std::optional<Point> targetSlot = FindTargetSlotUnderItemCursor(cursorPosition, itemSize);
	if (!targetSlot)
		return;

	const Point firstSlot = *targetSlot;

	// Check that no more than 1 item is replaced by the move
	StashStruct::StashCell stashIndex = StashStruct::EmptyCell;
	for (const Point point : PointsInRectangle(Rectangle { firstSlot, itemSize })) {
		const StashStruct::StashCell iv = stash.GetItemIdAtPosition(point);
		if (iv == StashStruct::EmptyCell || stashIndex == iv)
			continue;
		if (stashIndex == StashStruct::EmptyCell) {
			stashIndex = iv; // Found first item
			continue;
		}
		return; // Found a second item
	}

	PlaySFX(ItemInvSnds[ItemCAnimTbl[player.HoldItem._iCurs]]);

	// Need to set the item anchor position to the bottom left so drawing code functions correctly.
	player.HoldItem.position = firstSlot + Displacement { 0, itemSize.height - 1 };

	if (stashIndex == StashStruct::EmptyCell) {
		stash.stashList.emplace_back(player.HoldItem.pop());
		// stashList will have at most 10 000 items, up to 65 535 are supported with uint16_t indexes
		stashIndex = static_cast<uint16_t>(stash.stashList.size() - 1);
	} else {
		// swap the held item and whatever was in the stash at this position
		std::swap(stash.stashList[stashIndex], player.HoldItem);
		// then clear the space occupied by the old item
		for (auto &row : stash.GetCurrentGrid()) {
			for (auto &itemId : row) {
				if (itemId - 1 == stashIndex)
					itemId = 0;
			}
		}
	}

	// Finally mark the area now occupied by the pasted item in the current page/grid.
	AddItemToStashGrid(stash.GetPage(), firstSlot, stashIndex, itemSize);

	stash.dirty = true;

	NewCursor(player.HoldItem);
}

void CheckStashCut(Point cursorPosition, bool automaticMove)
{
	Player &player = *MyPlayer;

	CloseGoldWithdraw();

	Point slot = InvalidStashPoint;

	for (auto point : StashGridRange) {
		const Rectangle cell {
			GetStashSlotCoord(point),
			InventorySlotSizeInPixels + 1
		};

		// check which inventory rectangle the mouse is in, if any
		if (cell.contains(cursorPosition)) {
			slot = point;
			break;
		}
	}

	if (slot == InvalidStashPoint) {
		return;
	}

	Item &holdItem = player.HoldItem;
	holdItem.clear();

	bool automaticallyMoved = false;
	const bool automaticallyEquipped = false;

	StashStruct &stash = GetActiveStash();
	const StashStruct::StashCell iv = stash.GetItemIdAtPosition(slot);
	if (iv != StashStruct::EmptyCell) {
		holdItem = stash.stashList[iv];
		if (automaticMove) {
			if (CanBePlacedOnBelt(player, holdItem)) {
				automaticallyMoved = AutoPlaceItemInBelt(player, holdItem, true, true);
			} else {
				automaticallyMoved = AutoEquip(player, holdItem, true, true);
			}
		}

		if (!automaticMove || automaticallyMoved) {
			stash.RemoveStashItem(iv);
		}
	}

	if (!holdItem.isEmpty()) {
		CalcPlrInv(player, true);
		holdItem._iStatFlag = player.CanUseItem(holdItem);
		if (automaticallyEquipped) {
			PlaySFX(ItemInvSnds[ItemCAnimTbl[holdItem._iCurs]]);
		} else if (!automaticMove || automaticallyMoved) {
			PlaySFX(SfxID::GrabItem);
		}

		if (automaticMove) {
			if (!automaticallyMoved) {
				if (CanBePlacedOnBelt(player, holdItem)) {
					player.SaySpecific(HeroSpeech::IHaveNoRoom);
				} else {
					player.SaySpecific(HeroSpeech::ICantDoThat);
				}
			}

			holdItem.clear();
		} else {
			NewCursor(holdItem);
		}
	}
}

void WithdrawGold(Player &player, int amount)
{
	AddGoldToInventory(player, amount);
	StashStruct &stash = GetActiveStash();
	stash.gold -= amount;
	stash.dirty = true;
}

} // namespace

StashStruct &GetActiveStash()
{
	return ActiveLuaModStash != nullptr ? ActiveLuaModStash->stash : Stash;
}

void OpenStash()
{
	IsStashOpen = true;
	GetActiveStash().RefreshItemStatFlags();
	invflag = true;
	if (ControlMode != ControlTypes::KeyboardAndMouse) {
		if (pcurs == CURSOR_DISARM)
			NewCursor(CURSOR_HAND);
		FocusOnInventory();
	}
}

bool LuaRegisterModStash(std::string_view name, unsigned pageCount, bool allowsGold) // Lua mod support
{
	// The name becomes a save file name next to the hero saves, so only allow a plain identifier and
	// refuse names the engine's own save files use.
	if (name.empty() || name.size() > 32)
		return false;
	for (const char c : name) {
		const bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
		if (!valid)
			return false;
	}
	const std::string lower = AsciiStrToLower(name);
	if (lower.starts_with("stash") || lower.starts_with("single_") || lower.starts_with("multi_")
	    || lower.starts_with("spawn_") || lower.starts_with("share_") || lower.starts_with("demo_"))
		return false;
	if (pageCount == 0 || pageCount > DefaultStashPageCount)
		return false;

	LuaModStash &entry = LuaModStashes[std::string(name)];
	entry.pageCount = pageCount;
	entry.allowsGold = allowsGold;
	if (entry.loaded) {
		entry.stash.pageCount = pageCount;
		entry.stash.allowsGold = allowsGold;
	}
	return true;
}

bool LuaOpenModStash(std::string_view name) // Lua mod support
{
	const auto it = LuaModStashes.find(name);
	if (it == LuaModStashes.end())
		return false;

	CloseStash();

	LuaModStash &entry = it->second;
	if (!entry.loaded) {
		LuaLoadModStash(entry.stash, it->first);
		entry.stash.pageCount = entry.pageCount;
		entry.stash.allowsGold = entry.allowsGold;
		entry.stash.SetPage(entry.stash.GetPage());
		entry.stash.dirty = false;
		entry.loaded = true;
	}

	ActiveLuaModStash = &entry;
	ActiveLuaModStashName = it->first;
	OpenStash();
	return true;
}

std::string_view LuaActiveModStashName() // Lua mod support
{
	return ActiveLuaModStash != nullptr ? std::string_view(ActiveLuaModStashName) : std::string_view();
}

void LuaCloseModStash() // Lua mod support
{
	ActiveLuaModStash = nullptr;
	ActiveLuaModStashName.clear();
}

void LuaWriteModStashes() // Lua mod support
{
	for (auto &[name, entry] : LuaModStashes) {
		if (!entry.loaded || !entry.stash.dirty)
			continue;
		LuaSaveModStash(entry.stash, name);
		entry.stash.dirty = false;
	}
}

void LuaUnloadModStashes() // Lua mod support
{
	LuaCloseModStash();
	for (auto &[name, entry] : LuaModStashes) {
		entry.stash = {};
		entry.loaded = false;
	}
}

Point GetStashSlotCoord(Point slot)
{
	constexpr int StashNextCell = INV_SLOT_SIZE_PX + 1; // spacing between each cell

	return GetPanelPosition(UiPanels::Stash, slot * StashNextCell + Displacement { 17, 48 });
}

void FreeStashGFX()
{
	StashNavButtonArt = std::nullopt;
	StashPanelArt = std::nullopt;
}

void InitStash()
{
	if (!HeadlessMode) {
		StashPanelArt = LoadClx("data\\stash.clx");
		StashNavButtonArt = LoadClx("data\\stashnavbtns.clx");
	}
}

void TransferItemToInventory(Player &player, uint16_t itemId)
{
	if (itemId == StashStruct::EmptyCell) {
		return;
	}

	StashStruct &stash = GetActiveStash();
	const Item &item = stash.stashList[itemId];
	if (item.isEmpty()) {
		return;
	}

	if (!AutoPlaceItemInInventory(player, item)) {
		player.SaySpecific(HeroSpeech::IHaveNoRoom);
		return;
	}

	PlaySFX(ItemInvSnds[ItemCAnimTbl[item._iCurs]]);

	stash.RemoveStashItem(itemId);
}

int StashButtonPressed = -1;

void CheckStashButtonRelease(Point mousePosition)
{
	if (StashButtonPressed == -1)
		return;

	Rectangle stashButton = StashButtonRect[StashButtonPressed];
	stashButton.position = GetPanelPosition(UiPanels::Stash, stashButton.position);
	if (stashButton.contains(mousePosition)) {
		switch (StashButtonPressed) {
		case 0:
			GetActiveStash().PreviousPage(10);
			break;
		case 1:
			GetActiveStash().PreviousPage();
			break;
		case 2:
			StartGoldWithdraw();
			break;
		case 3:
			GetActiveStash().NextPage();
			break;
		case 4:
			GetActiveStash().NextPage(10);
			break;
		}
	}

	StashButtonPressed = -1;
}

void CheckStashButtonPress(Point mousePosition)
{
	Rectangle stashButton;

	for (int i = 0; i < 5; i++) {
		stashButton = StashButtonRect[i];
		stashButton.position = GetPanelPosition(UiPanels::Stash, stashButton.position);
		if (stashButton.contains(mousePosition)) {
			StashButtonPressed = i;
			return;
		}
	}

	StashButtonPressed = -1;
}

void DrawStash(const Surface &out)
{
	RenderClxSprite(out, (*StashPanelArt)[0], GetPanelPosition(UiPanels::Stash));

	if (StashButtonPressed != -1) {
		const Point stashButton = GetPanelPosition(UiPanels::Stash, StashButtonRect[StashButtonPressed].position);
		RenderClxSprite(out, (*StashNavButtonArt)[StashButtonPressed], stashButton);
	}

	constexpr Displacement offset { 0, INV_SLOT_SIZE_PX - 1 };
	StashStruct &stash = GetActiveStash();

	for (auto slot : StashGridRange) {
		const StashStruct::StashCell itemId = stash.GetItemIdAtPosition(slot);
		if (itemId == StashStruct::EmptyCell) {
			continue; // No item in the given slot
		}
		const Item &item = stash.stashList[itemId];
		InvDrawSlotBack(out, GetStashSlotCoord(slot) + offset, InventorySlotSizeInPixels, item._iMagical);
	}

	for (auto slot : StashGridRange) {
		const StashStruct::StashCell itemId = stash.GetItemIdAtPosition(slot);
		if (itemId == StashStruct::EmptyCell) {
			continue; // No item in the given slot
		}

		const Item &item = stash.stashList[itemId];
		if (item.position != slot) {
			continue; // Not the first slot of the item
		}

		const int frame = item._iCurs + CURSOR_FIRSTITEM;

		const Point position = GetStashSlotCoord(item.position) + offset;
		const ClxSprite sprite = GetInvItemSprite(frame);

		if (pcursstashitem == itemId) {
			const uint8_t color = GetOutlineColor(item, true);
			ClxDrawOutline(out, color, position, sprite);
		}

		DrawItem(item, out, position, sprite);
	}

	const Point position = GetPanelPosition(UiPanels::Stash);
	const UiFlags style = UiFlags::VerticalCenter | UiFlags::ColorWhite;
	const int textboxHeight = 13;

	DrawString(out, StrCat(stash.GetPage() + 1), { position + Displacement { 132, 0 }, { 57, textboxHeight } },
	    { .flags = UiFlags::AlignCenter | style });
	if (stash.allowsGold) {
		DrawString(out, FormatInteger(stash.gold), { position + Displacement { 122, 19 }, { 107, textboxHeight } },
		    { .flags = UiFlags::AlignRight | style });
	}
}

void CheckStashItem(Point mousePosition, bool isShiftHeld, bool isCtrlHeld)
{
	if (!MyPlayer->HoldItem.isEmpty()) {
		CheckStashPaste(mousePosition);
	} else if (isCtrlHeld) {
		TransferItemToInventory(*MyPlayer, pcursstashitem);
	} else {
		CheckStashCut(mousePosition, isShiftHeld);
	}
}

uint16_t CheckStashHLight(Point mousePosition)
{
	Point slot = InvalidStashPoint;
	for (auto point : StashGridRange) {
		const Rectangle cell {
			GetStashSlotCoord(point),
			InventorySlotSizeInPixels + 1
		};

		if (cell.contains(mousePosition)) {
			slot = point;
			break;
		}
	}

	if (slot == InvalidStashPoint)
		return -1;

	InfoColor = UiFlags::ColorWhite;

	StashStruct &stash = GetActiveStash();
	const StashStruct::StashCell itemId = stash.GetItemIdAtPosition(slot);
	if (itemId == StashStruct::EmptyCell) {
		return -1;
	}

	const Item &item = stash.stashList[itemId];
	if (item.isEmpty()) {
		return -1;
	}

	InfoColor = item.getTextColor();
	InfoString = item.getName();
	FloatingInfoString = item.getName();
	if (item._iIdentified) {
		PrintItemDetails(item);
	} else {
		PrintItemDur(item);
	}

	return itemId;
}

bool UseStashItem(uint16_t c)
{
	if (MyPlayer->_pInvincible && MyPlayer->hasNoLife())
		return true;
	if (pcurs != CURSOR_HAND)
		return true;
	if (IsPlayerInStore())
		return true;

	StashStruct &stash = GetActiveStash();
	Item *item = &stash.stashList[c];

	constexpr int SpeechDelay = 10;
	if (item->IDidx == IDI_MUSHROOM) {
		MyPlayer->Say(HeroSpeech::NowThatsOneBigMushroom, SpeechDelay);
		return true;
	}
	if (item->IDidx == IDI_FUNGALTM) {
		PlaySFX(SfxID::ItemBook);
		MyPlayer->Say(HeroSpeech::ThatDidntDoAnything, SpeechDelay);
		return true;
	}

	if (!item->isUsable())
		return false;

	if (!MyPlayer->CanUseItem(*item)) {
		MyPlayer->Say(HeroSpeech::ICantUseThisYet);
		return true;
	}

	CloseGoldWithdraw();

	if (item->isScroll()) {
		return true;
	}

	if (item->_iMiscId > IMISC_RUNEFIRST && item->_iMiscId < IMISC_RUNELAST && leveltype == DTYPE_TOWN) {
		return true;
	}

	if (item->_iMiscId == IMISC_BOOK)
		PlaySFX(SfxID::ReadBook);
	else
		PlaySFX(ItemInvSnds[ItemCAnimTbl[item->_iCurs]]);

	UseItem(*MyPlayer, item->_iMiscId, item->_iSpell, -1);

	if (stash.stashList[c]._iMiscId == IMISC_MAPOFDOOM)
		return true;
	if (stash.stashList[c]._iMiscId == IMISC_NOTE) {
		InitQTextMsg(TEXT_BOOK9);
		CloseInventory();
		return true;
	}
	stash.RemoveStashItem(c);

	return true;
}

void StashStruct::RemoveStashItem(StashStruct::StashCell iv)
{
	// Iterate through stashGrid and remove every reference to item
	for (auto &row : GetCurrentGrid()) {
		for (StashStruct::StashCell &itemId : row) {
			if (itemId - 1 == iv) {
				itemId = 0;
			}
		}
	}

	if (stashList.empty()) {
		return;
	}

	// If the item at the end of stash array isn't the one we removed, we need to swap its position in the array with the removed item
	const auto lastItemIndex = static_cast<StashStruct::StashCell>(stashList.size() - 1);
	if (lastItemIndex != iv) {
		stashList[iv] = stashList[lastItemIndex];

		for (auto &[_, grid] : stashGrids) {
			for (auto &row : grid) {
				for (StashStruct::StashCell &itemId : row) {
					if (itemId == lastItemIndex + 1) {
						itemId = iv + 1;
					}
				}
			}
		}
	}
	stashList.pop_back();
	dirty = true;
}

void StashStruct::SetPage(unsigned newPage)
{
	const unsigned lastPage = pageCount - 1;
	page = std::min(newPage, lastPage);
	dirty = true;
}

void StashStruct::NextPage(unsigned offset)
{
	const unsigned lastPage = pageCount - 1;
	if (page <= lastPage) {
		page += std::min(offset, lastPage - page);
	} else {
		page = lastPage;
	}
	dirty = true;
}

void StashStruct::PreviousPage(unsigned offset)
{
	const unsigned lastPage = pageCount - 1;
	if (page <= lastPage) {
		page -= std::min(offset, page);
	} else {
		page = lastPage;
	}
	dirty = true;
}

void StashStruct::RefreshItemStatFlags()
{
	for (auto &item : stashList) {
		item.updateRequiredStatsCacheForPlayer(*MyPlayer);
	}
}

void StartGoldWithdraw()
{
	if (!GetActiveStash().allowsGold)
		return;

	CloseGoldDrop();

	if (ChatFlag)
		ResetChat();

	const Point start = GetPanelPosition(UiPanels::Stash, { 67, 128 });
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 180, 20);
	SDL_SetTextInputArea(ghMainWnd, &rect, /*cursor=*/0);

	IsWithdrawGoldOpen = true;
	GoldWithdrawText[0] = '\0';
	GoldWithdrawInputState.emplace(NumberInputState::Options {
	    .textOptions {
	        .value = GoldWithdrawText,
	        .cursor = &GoldWithdrawCursor,
	        .maxLength = sizeof(GoldWithdrawText) - 1,
	    },
	    .min = 0,
	    .max = std::min(RoomForGold(), GetActiveStash().gold),
	});
	SDLC_StartTextInput(ghMainWnd);
}

void WithdrawGoldKeyPress(SDL_Keycode vkey)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer.hasNoLife()) {
		CloseGoldWithdraw();
		return;
	}

	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (const int value = GoldWithdrawInputState->value(); value != 0) {
			WithdrawGold(myPlayer, value);
			PlaySFX(SfxID::ItemGold);
		}
		CloseGoldWithdraw();
		break;
	case SDLK_ESCAPE:
		CloseGoldWithdraw();
		break;
	default:
		break;
	}
}

void DrawGoldWithdraw(const Surface &out)
{
	if (!IsWithdrawGoldOpen) {
		return;
	}

	const std::string_view amountText = GoldWithdrawText;
	const TextInputCursorState &cursor = GoldWithdrawCursor;

	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Stash, { dialogX, 178 }), (*GoldBoxBuffer)[0]);

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(_("How many gold pieces do you want to withdraw?"), 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Stash, { dialogX + 31, 75 }), { 200, 50 } },
	    { .flags = UiFlags::ColorWhitegold | UiFlags::AlignCenter, .lineHeight = 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Stash, { dialogX + 37, 128 }),
	    {
	        .flags = UiFlags::ColorWhite | UiFlags::PentaCursor,
	        .cursorPosition = static_cast<int>(cursor.position),
	        .highlightRange = { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
}

void CloseGoldWithdraw()
{
	if (!IsWithdrawGoldOpen)
		return;
	SDLC_StopTextInput(ghMainWnd);
	IsWithdrawGoldOpen = false;
	GoldWithdrawInputState = std::nullopt;
}

bool HandleGoldWithdrawTextInputEvent(const SDL_Event &event)
{
	return HandleNumberInputEvent(event, *GoldWithdrawInputState);
}

bool AutoPlaceItemInStash(const Item &item, bool persistItem)
{
	if (!IsItemAllowedInStash(item))
		return false;

	StashStruct &stash = GetActiveStash();

	if (item._itype == ItemType::Gold) {
		if (!stash.allowsGold)
			return false;
		if (stash.gold > std::numeric_limits<int>::max() - item._ivalue)
			return false;
		if (persistItem) {
			stash.gold += item._ivalue;
			stash.dirty = true;
		}
		return true;
	}

	const Size itemSize = GetInventorySize(item);

	// Try to add the item to the current active page and if it's not possible move forward
	for (unsigned pageCounter = 0; pageCounter < stash.pageCount; pageCounter++) {
		unsigned pageIndex = stash.GetPage() + pageCounter;
		// Wrap around if needed
		if (pageIndex >= stash.pageCount)
			pageIndex -= stash.pageCount;
		// Search all possible position in stash grid
		for (auto stashPosition : PointsInRectangle(Rectangle { { 0, 0 }, Size { 10 - (itemSize.width - 1), 10 - (itemSize.height - 1) } })) {
			// Check that all needed slots are free
			bool isSpaceFree = true;
			for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
				const uint16_t iv = stash.stashGrids[pageIndex][itemPoint.x][itemPoint.y];
				if (iv != 0) {
					isSpaceFree = false;
					break;
				}
			}
			if (!isSpaceFree)
				continue;
			if (persistItem) {
				stash.stashList.push_back(item);
				const auto stashIndex = static_cast<uint16_t>(stash.stashList.size() - 1);
				stash.stashList[stashIndex].position = stashPosition + Displacement { 0, itemSize.height - 1 };
				AddItemToStashGrid(pageIndex, stashPosition, stashIndex, itemSize);
				stash.dirty = true;
			}
			return true;
		}
	}

	return false;
}

} // namespace devilution
