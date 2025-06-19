/**
 * @file inv.cpp
 *
 * Implementation of player inventory.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "hwcursor.hpp"
#include "inv_iterators.hpp"
#include "levels/tile_properties.hpp"
#include "levels/town.h"
#include "minitext.h"
#include "options.h"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "plrmsg.h"
#include "qol/stash.h"
#include "stores.h"
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/sdl_geometry.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

bool invflag;

/**
 * Maps from inventory slot to screen position. The inventory slots are
 * arranged as follows:
 *
 * @code{.unparsed}
 *                          00 00
 *                          00 00   03
 *
 *              04 04       06 06       05 05
 *              04 04       06 06       05 05
 *              04 04       06 06       05 05
 *
 *                 01                   02
 *
 *              07 08 09 10 11 12 13 14 15 16
 *              17 18 19 20 21 22 23 24 25 26
 *              27 28 29 30 31 32 33 34 35 36
 *              37 38 39 40 41 42 43 44 45 46
 *
 * 47 48 49 50 51 52 53 54
 * @endcode
 */
const Rectangle InvRect[] = {
	// clang-format off
	//{   X,   Y }, {  W,  H }
	{ { 132,   2 }, { 58, 59 } }, // helmet
	{ {  47, 177 }, { 28, 29 } }, // left ring
	{ { 248, 177 }, { 28, 29 } }, // right ring
	{ { 205,  32 }, { 28, 29 } }, // amulet
	{ {  17,  75 }, { 58, 86 } }, // left hand
	{ { 248,  75 }, { 58, 87 } }, // right hand
	{ { 132,  75 }, { 58, 87 } }, // chest
	{ {  17, 222 }, { 29, 29 } }, // inv row 1
	{ {  46, 222 }, { 29, 29 } }, // inv row 1
	{ {  75, 222 }, { 29, 29 } }, // inv row 1
	{ { 104, 222 }, { 29, 29 } }, // inv row 1
	{ { 133, 222 }, { 29, 29 } }, // inv row 1
	{ { 162, 222 }, { 29, 29 } }, // inv row 1
	{ { 191, 222 }, { 29, 29 } }, // inv row 1
	{ { 220, 222 }, { 29, 29 } }, // inv row 1
	{ { 249, 222 }, { 29, 29 } }, // inv row 1
	{ { 278, 222 }, { 29, 29 } }, // inv row 1
	{ {  17, 251 }, { 29, 29 } }, // inv row 2
	{ {  46, 251 }, { 29, 29 } }, // inv row 2
	{ {  75, 251 }, { 29, 29 } }, // inv row 2
	{ { 104, 251 }, { 29, 29 } }, // inv row 2
	{ { 133, 251 }, { 29, 29 } }, // inv row 2
	{ { 162, 251 }, { 29, 29 } }, // inv row 2
	{ { 191, 251 }, { 29, 29 } }, // inv row 2
	{ { 220, 251 }, { 29, 29 } }, // inv row 2
	{ { 249, 251 }, { 29, 29 } }, // inv row 2
	{ { 278, 251 }, { 29, 29 } }, // inv row 2
	{ {  17, 280 }, { 29, 29 } }, // inv row 3
	{ {  46, 280 }, { 29, 29 } }, // inv row 3
	{ {  75, 280 }, { 29, 29 } }, // inv row 3
	{ { 104, 280 }, { 29, 29 } }, // inv row 3
	{ { 133, 280 }, { 29, 29 } }, // inv row 3
	{ { 162, 280 }, { 29, 29 } }, // inv row 3
	{ { 191, 280 }, { 29, 29 } }, // inv row 3
	{ { 220, 280 }, { 29, 29 } }, // inv row 3
	{ { 249, 280 }, { 29, 29 } }, // inv row 3
	{ { 278, 280 }, { 29, 29 } }, // inv row 3
	{ {  17, 309 }, { 29, 29 } }, // inv row 4
	{ {  46, 309 }, { 29, 29 } }, // inv row 4
	{ {  75, 309 }, { 29, 29 } }, // inv row 4
	{ { 104, 309 }, { 29, 29 } }, // inv row 4
	{ { 133, 309 }, { 29, 29 } }, // inv row 4
	{ { 162, 309 }, { 29, 29 } }, // inv row 4
	{ { 191, 309 }, { 29, 29 } }, // inv row 4
	{ { 220, 309 }, { 29, 29 } }, // inv row 4
	{ { 249, 309 }, { 29, 29 } }, // inv row 4
	{ { 278, 309 }, { 29, 29 } }, // inv row 4
	{ { 205,   5 }, { 29, 29 } }, // belt
	{ { 234,   5 }, { 29, 29 } }, // belt
	{ { 263,   5 }, { 29, 29 } }, // belt
	{ { 292,   5 }, { 29, 29 } }, // belt
	{ { 321,   5 }, { 29, 29 } }, // belt
	{ { 350,   5 }, { 29, 29 } }, // belt
	{ { 379,   5 }, { 29, 29 } }, // belt
	{ { 408,   5 }, { 29, 29 } }  // belt
	// clang-format on
};

namespace {

OptionalOwnedClxSpriteList pInvCels;

/**
 * @brief Adds an item to a player's InvGrid array
 * @param player The player reference
 * @param invGridIndex Item's position in InvGrid (this should be the item's topleft grid tile)
 * @param invListIndex The item's InvList index (it's expected this already has +1 added to it since InvGrid can't store a 0 index)
 * @param itemSize Size of item
 */
void AddItemToInvGrid(Player &player, int invGridIndex, int invListIndex, Size itemSize, bool sendNetworkMessage)
{
	const int pitch = 10;
	for (int y = 0; y < itemSize.height; y++) {
		int rowGridIndex = invGridIndex + pitch * y;
		for (int x = 0; x < itemSize.width; x++) {
			if (x == 0 && y == itemSize.height - 1)
				player.InvGrid[rowGridIndex + x] = invListIndex;
			else
				player.InvGrid[rowGridIndex + x] = -invListIndex; // use negative index to denote it's occupied but it's not the top-left cell.
		}
	}

	if (sendNetworkMessage) {
		NetSendCmdChInvItem(false, invGridIndex);
	}
}

/**
 * @brief Checks whether the given item can fit in a belt slot (i.e. the item's size in inventory cells is 1x1).
 * @param item The item to be checked.
 * @return 'True' in case the item can fit a belt slot and 'False' otherwise.
 */
bool FitsInBeltSlot(const Item &item)
{
	return GetInventorySize(item) == Size { 1, 1 };
}

/**
 * @brief Checks whether the given item can be equipped. Since this overload doesn't take player information, it only considers
 * general aspects about the item, like if its requirements are met and if the item's target location is valid for the body.
 * @param item The item to check.
 * @return 'True' in case the item could be equipped in a player, and 'False' otherwise.
 */
bool CanEquip(const Item &item)
{
	return item.isEquipment()
	    && item._iStatFlag;
}

/**
 * @brief A specialized version of 'CanEquip(int, Item&, int)' that specifically checks whether the item can be equipped
 * in one/both of the player's hands.
 * @param player The player whose inventory will be checked for compatibility with the item.
 * @param item The item to check.
 * @return 'True' if the player can currently equip the item in either one of his hands (i.e. the required hands are empty and
 * allow the item), and 'False' otherwise.
 */
bool CanWield(Player &player, const Item &item)
{
	if (!CanEquip(item) || IsNoneOf(player.GetItemLocation(item), ILOC_ONEHAND, ILOC_TWOHAND))
		return false;

	Item &leftHandItem = player.InvBody[INVLOC_HAND_LEFT];
	Item &rightHandItem = player.InvBody[INVLOC_HAND_RIGHT];

	if (leftHandItem.isEmpty() && rightHandItem.isEmpty()) {
		return true;
	}

	if (!leftHandItem.isEmpty() && !rightHandItem.isEmpty()) {
		return false;
	}

	Item &occupiedHand = !leftHandItem.isEmpty() ? leftHandItem : rightHandItem;

	// Bard can dual wield swords and maces, so we allow equiping one-handed weapons in her free slot as long as her occupied
	// slot is another one-handed weapon.
	if (player._pClass == HeroClass::Bard) {
		bool occupiedHandIsOneHandedSwordOrMace = player.GetItemLocation(occupiedHand) == ILOC_ONEHAND
		    && IsAnyOf(occupiedHand._itype, ItemType::Sword, ItemType::Mace);

		bool weaponToEquipIsOneHandedSwordOrMace = player.GetItemLocation(item) == ILOC_ONEHAND
		    && IsAnyOf(item._itype, ItemType::Sword, ItemType::Mace);

		if (occupiedHandIsOneHandedSwordOrMace && weaponToEquipIsOneHandedSwordOrMace) {
			return true;
		}
	}

	return player.GetItemLocation(item) == ILOC_ONEHAND
	    && player.GetItemLocation(occupiedHand) == ILOC_ONEHAND
	    && item._iClass != occupiedHand._iClass;
}

/**
 * @brief Checks whether the specified item can be equipped in the desired body location on the player.
 * @param player The player whose inventory will be checked for compatibility with the item.
 * @param item The item to check.
 * @param bodyLocation The location in the inventory to be checked against.
 * @return 'True' if the player can currently equip the item in the specified body location (i.e. the body location is empty and
 * allows the item), and 'False' otherwise.
 */
bool CanEquip(Player &player, const Item &item, inv_body_loc bodyLocation)
{
	if (!CanEquip(item) || player._pmode > PM_WALK_SIDEWAYS || !player.InvBody[bodyLocation].isEmpty()) {
		return false;
	}

	switch (bodyLocation) {
	case INVLOC_AMULET:
		return item._iLoc == ILOC_AMULET;

	case INVLOC_CHEST:
		return item._iLoc == ILOC_ARMOR;

	case INVLOC_HAND_LEFT:
	case INVLOC_HAND_RIGHT:
		return CanWield(player, item);

	case INVLOC_HEAD:
		return item._iLoc == ILOC_HELM;

	case INVLOC_RING_LEFT:
	case INVLOC_RING_RIGHT:
		return item._iLoc == ILOC_RING;

	default:
		return false;
	}
}

void ChangeEquipment(Player &player, inv_body_loc bodyLocation, const Item &item, bool sendNetworkMessage)
{
	player.InvBody[bodyLocation] = item;

	if (sendNetworkMessage) {
		NetSendCmdChItem(false, bodyLocation, true);
	}
}

bool AutoEquip(Player &player, const Item &item, inv_body_loc bodyLocation, bool persistItem, bool sendNetworkMessage)
{
	if (!CanEquip(player, item, bodyLocation)) {
		return false;
	}

	if (persistItem) {
		ChangeEquipment(player, bodyLocation, item, sendNetworkMessage);

		if (sendNetworkMessage && *GetOptions().Audio.autoEquipSound) {
			PlaySFX(ItemInvSnds[ItemCAnimTbl[item._iCurs]]);
		}

		CalcPlrInv(player, true);
	}

	return true;
}

int FindTargetSlotUnderItemCursor(Point cursorPosition, Size itemSize)
{
	Displacement panelOffset = Point { 0, 0 } - GetRightPanel().position;
	for (int r = SLOTXY_EQUIPPED_FIRST; r <= SLOTXY_EQUIPPED_LAST; r++) {
		if (InvRect[r].contains(cursorPosition + panelOffset))
			return r;
	}
	for (int r = SLOTXY_INV_FIRST; r <= SLOTXY_INV_LAST; r++) {
		if (InvRect[r].contains(cursorPosition + panelOffset)) {
			// When trying to paste into the inventory we need to determine the top left cell of the nearest area that could fit the item, not the slot under the center/hot pixel.
			if (itemSize.height <= 1 && itemSize.width <= 1) {
				// top left cell of a 1x1 item is the same cell as the hot pixel, no work to do
				return r;
			}
			// Otherwise work out how far the central cell is from the top-left cell
			Displacement hotPixelCellOffset = { (itemSize.width - 1) / 2, (itemSize.height - 1) / 2 };
			// For even dimension items we need to work out if the cursor is in the left/right (or top/bottom) half of the central cell and adjust the offset so the item lands in the area most covered by the cursor.
			if (itemSize.width % 2 == 0 && InvRect[r].contains(cursorPosition + panelOffset + Displacement { INV_SLOT_HALF_SIZE_PX, 0 })) {
				// hot pixel was in the left half of the cell, so we want to increase the offset to preference the column to the left
				hotPixelCellOffset.deltaX++;
			}
			if (itemSize.height % 2 == 0 && InvRect[r].contains(cursorPosition + panelOffset + Displacement { 0, INV_SLOT_HALF_SIZE_PX })) {
				// hot pixel was in the top half of the cell, so we want to increase the offset to preference the row above
				hotPixelCellOffset.deltaY++;
			}
			// Then work out the top left cell of the nearest area that could fit this item (as pasting on the edge of the inventory would otherwise put it out of bounds)
			int hotPixelCell = r - SLOTXY_INV_FIRST;
			int targetRow = std::clamp((hotPixelCell / InventorySizeInSlots.width) - hotPixelCellOffset.deltaY, 0, InventorySizeInSlots.height - itemSize.height);
			int targetColumn = std::clamp((hotPixelCell % InventorySizeInSlots.width) - hotPixelCellOffset.deltaX, 0, InventorySizeInSlots.width - itemSize.width);
			return SLOTXY_INV_FIRST + targetRow * InventorySizeInSlots.width + targetColumn;
		}
	}

	panelOffset = Point { 0, 0 } - GetMainPanel().position;
	for (int r = SLOTXY_BELT_FIRST; r <= SLOTXY_BELT_LAST; r++) {
		if (InvRect[r].contains(cursorPosition + panelOffset))
			return r;
	}
	return NUM_XY_SLOTS;
}

void ChangeBodyEquipment(Player &player, int slot, item_equip_type location)
{
	const inv_body_loc bodyLocation = [&slot](item_equip_type location) {
		switch (location) {
		case ILOC_HELM:
			return INVLOC_HEAD;
		case ILOC_RING:
			return (slot == SLOTXY_RING_LEFT ? INVLOC_RING_LEFT : INVLOC_RING_RIGHT);
		case ILOC_AMULET:
			return INVLOC_AMULET;
		case ILOC_ARMOR:
			return INVLOC_CHEST;
		default:
			app_fatal("Unexpected equipment type");
		}
	}(location);
	const Item previouslyEquippedItem = player.InvBody[slot];
	ChangeEquipment(player, bodyLocation, player.HoldItem.pop(), &player == MyPlayer);
	if (!previouslyEquippedItem.isEmpty()) {
		player.HoldItem = previouslyEquippedItem;
	}
}

void ChangeEquippedItem(Player &player, uint8_t slot)
{
	const inv_body_loc selectedHand = slot == SLOTXY_HAND_LEFT ? INVLOC_HAND_LEFT : INVLOC_HAND_RIGHT;
	const inv_body_loc otherHand = slot == SLOTXY_HAND_LEFT ? INVLOC_HAND_RIGHT : INVLOC_HAND_LEFT;

	const bool pasteIntoSelectedHand = (player.InvBody[otherHand].isEmpty() || player.InvBody[otherHand]._iClass != player.HoldItem._iClass)
	    || (player._pClass == HeroClass::Bard && player.InvBody[otherHand]._iClass == ICLASS_WEAPON && player.HoldItem._iClass == ICLASS_WEAPON);

	const bool dequipTwoHandedWeapon = (!player.InvBody[otherHand].isEmpty() && player.GetItemLocation(player.InvBody[otherHand]) == ILOC_TWOHAND);

	const inv_body_loc pasteHand = pasteIntoSelectedHand ? selectedHand : otherHand;
	const Item previouslyEquippedItem = dequipTwoHandedWeapon ? player.InvBody[otherHand] : player.InvBody[pasteHand];
	if (dequipTwoHandedWeapon) {
		RemoveEquipment(player, otherHand, false);
	}
	ChangeEquipment(player, pasteHand, player.HoldItem.pop(), &player == MyPlayer);
	if (!previouslyEquippedItem.isEmpty()) {
		player.HoldItem = previouslyEquippedItem;
	}
}

void ChangeTwoHandItem(Player &player)
{
	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && !player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
		inv_body_loc locationToUnequip = INVLOC_HAND_LEFT;
		if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
			locationToUnequip = INVLOC_HAND_RIGHT;
		}
		if (!AutoPlaceItemInInventory(player, player.InvBody[locationToUnequip])) {
			return;
		}

		if (locationToUnequip == INVLOC_HAND_RIGHT) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
		} else {
			player.InvBody[INVLOC_HAND_LEFT].clear();
		}
	}

	if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
		Item previouslyEquippedItem = player.InvBody[INVLOC_HAND_LEFT];
		ChangeEquipment(player, INVLOC_HAND_LEFT, player.HoldItem.pop(), &player == MyPlayer);
		if (!previouslyEquippedItem.isEmpty()) {
			player.HoldItem = previouslyEquippedItem;
		}
	} else {
		Item previouslyEquippedItem = player.InvBody[INVLOC_HAND_RIGHT];
		RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
		ChangeEquipment(player, INVLOC_HAND_LEFT, player.HoldItem, &player == MyPlayer);
		player.HoldItem = previouslyEquippedItem;
	}
}

int8_t CheckOverlappingItems(int slot, const Player &player, Size itemSize)
{
	// check that the item we're pasting only overlaps one other item (or is going into empty space)
	const unsigned originCell = static_cast<unsigned>(slot - SLOTXY_INV_FIRST);

	int8_t overlappingId = 0;
	for (unsigned rowOffset = 0; rowOffset < static_cast<unsigned>(itemSize.height * InventorySizeInSlots.width); rowOffset += InventorySizeInSlots.width) {

		for (unsigned columnOffset = 0; columnOffset < static_cast<unsigned>(itemSize.width); columnOffset++) {
			unsigned testCell = originCell + rowOffset + columnOffset;
			// FindTargetSlotUnderItemCursor returns the top left slot of the inventory region that fits the item, we can be confident this calculation is not going to read out of range.
			assert(testCell < sizeof(player.InvGrid));
			if (player.InvGrid[testCell] != 0) {
				int8_t iv = std::abs(player.InvGrid[testCell]);
				if (overlappingId != 0) {
					if (overlappingId != iv) {
						// Found two different items that would be displaced by the held item, can't paste the item here.
						return -1;
					}
				} else {
					overlappingId = iv;
				}
			}
		}
	}

	return overlappingId;
}

int8_t GetPrevItemId(int slot, const Player &player, const Size &itemSize)
{
	if (player.HoldItem._itype != ItemType::Gold)
		return CheckOverlappingItems(slot, player, itemSize);
	int8_t item_cell_begin = player.InvGrid[slot - SLOTXY_INV_FIRST];
	if (item_cell_begin == 0)
		return 0;
	if (item_cell_begin <= 0)
		return -item_cell_begin;
	if (player.InvList[item_cell_begin - 1]._itype != ItemType::Gold)
		return item_cell_begin;
	return 0;
}

bool ChangeInvItem(Player &player, int slot, Size itemSize)
{
	int8_t prevItemId = GetPrevItemId(slot, player, itemSize);
	if (prevItemId < 0) return false;

	if (player.HoldItem._itype == ItemType::Gold && prevItemId == 0) {
		const int ii = slot - SLOTXY_INV_FIRST;
		if (player.InvGrid[ii] > 0) {
			const int invIndex = player.InvGrid[ii] - 1;
			const int gt = player.InvList[invIndex]._ivalue;
			int ig = player.HoldItem._ivalue + gt;
			if (ig <= MaxGold) {
				player.InvList[invIndex]._ivalue = ig;
				SetPlrHandGoldCurs(player.InvList[invIndex]);
				player._pGold += player.HoldItem._ivalue;
				player.HoldItem.clear();
			} else {
				ig = MaxGold - gt;
				player._pGold += ig;
				player.HoldItem._ivalue -= ig;
				SetPlrHandGoldCurs(player.HoldItem);
				player.InvList[invIndex]._ivalue = MaxGold;
				player.InvList[invIndex]._iCurs = ICURS_GOLD_LARGE;
			}
		} else {
			const int invIndex = player._pNumInv;
			player._pGold += player.HoldItem._ivalue;
			player.InvList[invIndex] = player.HoldItem.pop();
			player._pNumInv++;
			player.InvGrid[ii] = player._pNumInv;
		}
		if (&player == MyPlayer) {
			NetSendCmdChInvItem(false, ii);
		}
	} else {
		if (prevItemId == 0) {
			player.InvList[player._pNumInv] = player.HoldItem.pop();
			player._pNumInv++;
			prevItemId = player._pNumInv;
		} else {
			const int invIndex = prevItemId - 1;
			if (player.HoldItem._itype == ItemType::Gold)
				player._pGold += player.HoldItem._ivalue;
			std::swap(player.InvList[invIndex], player.HoldItem);
			if (player.HoldItem._itype == ItemType::Gold)
				player._pGold = CalculateGold(player);
			for (int8_t &itemIndex : player.InvGrid) {
				if (itemIndex == prevItemId)
					itemIndex = 0;
				if (itemIndex == -prevItemId)
					itemIndex = 0;
			}
		}

		AddItemToInvGrid(player, slot - SLOTXY_INV_FIRST, prevItemId, itemSize, &player == MyPlayer);
	}

	return true;
}

void ChangeBeltItem(Player &player, int slot)
{
	const int ii = slot - SLOTXY_BELT_FIRST;
	if (player.SpdList[ii].isEmpty()) {
		player.SpdList[ii] = player.HoldItem.pop();
	} else {
		std::swap(player.SpdList[ii], player.HoldItem);

		if (player.HoldItem._itype == ItemType::Gold)
			player._pGold = CalculateGold(player);
	}
	if (&player == MyPlayer) {
		NetSendCmdChBeltItem(false, ii);
	}
	RedrawComponent(PanelDrawComponent::Belt);
}

item_equip_type GetItemEquipType(const Player &player, int slot, item_equip_type desiredLocation)
{
	if (slot == SLOTXY_HEAD)
		return ILOC_HELM;
	if (slot == SLOTXY_RING_LEFT || slot == SLOTXY_RING_RIGHT)
		return ILOC_RING;
	if (slot == SLOTXY_AMULET)
		return ILOC_AMULET;
	if (slot == SLOTXY_HAND_LEFT || slot == SLOTXY_HAND_RIGHT) {
		if (desiredLocation == ILOC_TWOHAND)
			return ILOC_TWOHAND;
		return ILOC_ONEHAND;
	}
	if (slot == SLOTXY_CHEST)
		return ILOC_ARMOR;
	if (slot >= SLOTXY_BELT_FIRST && slot <= SLOTXY_BELT_LAST)
		return ILOC_BELT;

	return ILOC_UNEQUIPABLE;
}

void CheckInvPaste(Player &player, Point cursorPosition)
{
	const Size itemSize = GetInventorySize(player.HoldItem);

	const int slot = FindTargetSlotUnderItemCursor(cursorPosition, itemSize);
	if (slot == NUM_XY_SLOTS)
		return;

	const item_equip_type desiredLocation = player.GetItemLocation(player.HoldItem);
	const item_equip_type location = GetItemEquipType(player, slot, desiredLocation);

	if (location == ILOC_BELT) {
		if (!CanBePlacedOnBelt(player, player.HoldItem)) return;
	} else if (location != ILOC_UNEQUIPABLE) {
		if (desiredLocation != location) return;
	}

	if (IsNoneOf(location, ILOC_UNEQUIPABLE, ILOC_BELT)) {
		if (!player.CanUseItem(player.HoldItem)) {
			player.Say(HeroSpeech::ICantUseThisYet);
			return;
		}
		if (player._pmode > PM_WALK_SIDEWAYS)
			return;
	}

	if (&player == MyPlayer) {
		PlaySFX(ItemInvSnds[ItemCAnimTbl[player.HoldItem._iCurs]]);
	}

	// Select the parameters that go into
	// ChangeEquipment and add it to post switch
	switch (location) {
	case ILOC_HELM:
	case ILOC_RING:
	case ILOC_AMULET:
	case ILOC_ARMOR:
		ChangeBodyEquipment(player, slot, location);
		break;
	case ILOC_ONEHAND:
		ChangeEquippedItem(player, slot);
		break;
	case ILOC_TWOHAND:
		ChangeTwoHandItem(player);
		break;
	case ILOC_UNEQUIPABLE:
		if (!ChangeInvItem(player, slot, itemSize)) return;
		break;
	case ILOC_BELT:
		ChangeBeltItem(player, slot);
		break;
	case ILOC_NONE:
	case ILOC_INVALID:
		break;
	}

	CalcPlrInv(player, true);
	if (&player == MyPlayer) {
		NewCursor(player.HoldItem);
	}
}

inv_body_loc MapSlotToInvBodyLoc(inv_xy_slot slot)
{
	assert(slot <= SLOTXY_CHEST);
	return static_cast<inv_body_loc>(slot);
}

std::optional<inv_xy_slot> FindSlotUnderCursor(Point cursorPosition)
{

	Point testPosition = static_cast<Point>(cursorPosition - GetRightPanel().position);
	for (std::underlying_type_t<inv_xy_slot> r = SLOTXY_EQUIPPED_FIRST; r != SLOTXY_BELT_FIRST; r++) {
		// check which body/inventory rectangle the mouse is in, if any
		if (InvRect[r].contains(testPosition)) {
			return static_cast<inv_xy_slot>(r);
		}
	}

	testPosition = static_cast<Point>(cursorPosition - GetMainPanel().position);
	for (std::underlying_type_t<inv_xy_slot> r = SLOTXY_BELT_FIRST; r != NUM_XY_SLOTS; r++) {
		// check which belt rectangle the mouse is in, if any
		if (InvRect[r].contains(testPosition)) {
			return static_cast<inv_xy_slot>(r);
		}
	}

	return {};
}

/**
 * @brief Checks whether an item of the given size can be placed on the specified player's inventory slot.
 * @param player The player whose inventory will be checked.
 * @param slotIndex The 0-based index of the slot to put the item on.
 * @param itemSize The size of the item to be checked.
 * @param itemIndexToIgnore can be used to check if an item of the given size would fit if the item with the given (positive) ID was removed.
 * @return 'True' in case the item can be placed on the specified player's inventory slot and 'False' otherwise.
 */
bool CheckItemFitsInInventorySlot(const Player &player, int slotIndex, const Size &itemSize, int itemIndexToIgnore)
{
	int yy = (slotIndex > 0) ? (10 * (slotIndex / 10)) : 0;

	for (int j = 0; j < itemSize.height; j++) {
		if (yy >= InventoryGridCells) {
			return false;
		}
		int xx = (slotIndex > 0) ? (slotIndex % 10) : 0;
		for (int i = 0; i < itemSize.width; i++) {
			if (xx >= 10 || !(player.InvGrid[xx + yy] == 0 || std::abs(player.InvGrid[xx + yy]) - 1 == itemIndexToIgnore)) {
				// The item is too wide to fit in the specified column, or one of the cells is occupied (and not by the item we're planning on removing)
				return false;
			}
			xx++;
		}
		yy += 10;
	}
	return true;
}

/**
 * @brief Finds the first slot that could fit an item of the given size
 * @param player Player whose inventory will be checked.
 * @param itemSize Dimensions of the item.
 * @param itemIndexToIgnore Can be used if you want to find whether the new item would fit with this item removed, without performing unnecessary actions.
 * @return The first slot that could fit the item or an empty optional.
 */
std::optional<int> FindSlotForItem(const Player &player, const Size &itemSize, int itemIndexToIgnore = -1)
{
	if (itemSize.height == 1) {
		for (int i = 30; i <= 39; i++) {
			if (CheckItemFitsInInventorySlot(player, i, itemSize, itemIndexToIgnore))
				return i;
		}
		for (int x = 9; x >= 0; x--) {
			for (int y = 2; y >= 0; y--) {
				if (CheckItemFitsInInventorySlot(player, 10 * y + x, itemSize, itemIndexToIgnore))
					return 10 * y + x;
			}
		}
		return {};
	}

	if (itemSize.height == 2) {
		for (int x = 10 - itemSize.width; x >= 0; x--) {
			for (int y = 0; y < 3; y++) {
				if (CheckItemFitsInInventorySlot(player, 10 * y + x, itemSize, itemIndexToIgnore))
					return 10 * y + x;
			}
		}
		return {};
	}

	if (itemSize == Size { 1, 3 }) {
		for (int i = 0; i < 20; i++) {
			if (CheckItemFitsInInventorySlot(player, i, itemSize, itemIndexToIgnore))
				return i;
		}
		return {};
	}

	if (itemSize == Size { 2, 3 }) {
		for (int i = 0; i < 9; i++) {
			if (CheckItemFitsInInventorySlot(player, i, itemSize, itemIndexToIgnore))
				return i;
		}

		for (int i = 10; i < 19; i++) {
			if (CheckItemFitsInInventorySlot(player, i, itemSize, itemIndexToIgnore))
				return i;
		}
		return {};
	}

	app_fatal(StrCat("Unknown item size: ", itemSize.width, "x", itemSize.height));
}

/**
 * @brief Checks if the given item could be placed on the specified players inventory if the other item was removed.
 * @param player The player whose inventory will be checked.
 * @param item The item to be checked.
 * @param itemIndexToIgnore The inventory index of the item that we assume will be removed.
 * @return 'True' if the item could fit with the other item removed and 'False' otherwise.
 */
bool CouldFitItemInInventory(const Player &player, const Item &item, int itemIndexToIgnore)
{
	return static_cast<bool>(FindSlotForItem(player, GetInventorySize(item), itemIndexToIgnore));
}

void CheckInvCut(Player &player, Point cursorPosition, bool automaticMove, bool dropItem)
{
	if (player._pmode > PM_WALK_SIDEWAYS) {
		return;
	}

	CloseGoldDrop();

	std::optional<inv_xy_slot> maybeSlot = FindSlotUnderCursor(cursorPosition);

	if (!maybeSlot) {
		// not on an inventory slot rectangle
		return;
	}

	inv_xy_slot r = *maybeSlot;

	Item &holdItem = player.HoldItem;
	holdItem.clear();

	bool attemptedMove = false;
	bool automaticallyMoved = false;
	SfxID successSound = SfxID::None;
	HeroSpeech failedSpeech = HeroSpeech::ICantDoThat; // Default message if the player attempts to automove an item that can't go anywhere else

	if (r >= SLOTXY_HEAD && r <= SLOTXY_CHEST) {
		inv_body_loc invloc = MapSlotToInvBodyLoc(r);
		if (!player.InvBody[invloc].isEmpty()) {
			if (automaticMove) {
				attemptedMove = true;
				automaticallyMoved = AutoPlaceItemInInventory(player, player.InvBody[invloc]);
				if (automaticallyMoved) {
					successSound = ItemInvSnds[ItemCAnimTbl[player.InvBody[invloc]._iCurs]];
					RemoveEquipment(player, invloc, false);
				} else {
					failedSpeech = HeroSpeech::IHaveNoRoom;
				}
			} else {
				holdItem = player.InvBody[invloc];
				RemoveEquipment(player, invloc, false);
			}
		}
	}

	if (r >= SLOTXY_INV_FIRST && r <= SLOTXY_INV_LAST) {
		unsigned ig = r - SLOTXY_INV_FIRST;
		int iv = std::abs(player.InvGrid[ig]) - 1;
		if (iv >= 0) {
			if (automaticMove) {
				attemptedMove = true;
				if (CanBePlacedOnBelt(player, player.InvList[iv])) {
					automaticallyMoved = AutoPlaceItemInBelt(player, player.InvList[iv], true, &player == MyPlayer);
					if (automaticallyMoved) {
						successSound = SfxID::GrabItem;
						player.RemoveInvItem(iv, false);
					} else {
						failedSpeech = HeroSpeech::IHaveNoRoom;
					}
				} else if (CanEquip(player.InvList[iv])) {
					failedSpeech = HeroSpeech::IHaveNoRoom; // Default to saying "I have no room" if auto-equip fails

					/*
					 * If the player shift-clicks an item in the inventory we want to swap it with whatever item may be
					 * equipped in the target slot. Lifting the item to the hand unconditionally would be ideal, except
					 * we don't want to leave the item on the hand if the equip attempt failed. We would end up
					 * generating wasteful network messages if we did the lift first. Instead we work out whatever slot
					 * needs to be unequipped (if any):
					 */
					int invloc = NUM_INVLOC;
					switch (player.GetItemLocation(player.InvList[iv])) {
					case ILOC_ARMOR:
						invloc = INVLOC_CHEST;
						break;
					case ILOC_HELM:
						invloc = INVLOC_HEAD;
						break;
					case ILOC_AMULET:
						invloc = INVLOC_AMULET;
						break;
					case ILOC_ONEHAND:
						if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty()
						    && (player.InvList[iv]._iClass == player.InvBody[INVLOC_HAND_LEFT]._iClass
						        || player.GetItemLocation(player.InvBody[INVLOC_HAND_LEFT]) == ILOC_TWOHAND)) {
							// The left hand is not empty and we're either trying to equip the same type of item or
							// it's holding a two handed weapon, so it must be unequipped
							invloc = INVLOC_HAND_LEFT;
						} else if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvList[iv]._iClass == player.InvBody[INVLOC_HAND_RIGHT]._iClass) {
							// The right hand is not empty and we're trying to equip the same type of item, so we need
							// to unequip that item
							invloc = INVLOC_HAND_RIGHT;
						}
						// otherwise one hand is empty (and we can let the auto-equip code put the target item into
						// that hand) or we're playing a bard with two swords equipped and we're trying to auto-equip
						// a shield (in which case the attempt will fail).
						break;
					case ILOC_TWOHAND:
						// Moving a two-hand item from inventory to InvBody requires emptying both hands.
						if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
							// If the right hand is empty then we can simply try equipping this item in the left hand,
							// we'll let the common code take care of unequipping anything held there.
							invloc = INVLOC_HAND_LEFT;
						} else if (player.InvBody[INVLOC_HAND_LEFT].isEmpty()) {
							// We have an item in the right hand but nothing in the left, so let the common code
							// take care of unequipping whatever is held in the right hand. The auto-equip code
							// picks the most appropriate location for the item type (which in this case will be
							// the left hand), invloc isn't used there.
							invloc = INVLOC_HAND_RIGHT;
						} else {
							// Both hands are holding items, we must unequip one of the items and check that there's
							// space for the other before trying to auto-equip
							inv_body_loc mainHand = INVLOC_HAND_LEFT;
							inv_body_loc offHand = INVLOC_HAND_RIGHT;
							if (!AutoPlaceItemInInventory(player, player.InvBody[offHand])) {
								// No space to move right hand item to inventory, can we move the left instead?
								std::swap(mainHand, offHand);
								if (!AutoPlaceItemInInventory(player, player.InvBody[offHand])) {
									break;
								}
							}
							if (!CouldFitItemInInventory(player, player.InvBody[mainHand], iv)) {
								// No space for the main hand item. Move the other item back to the off hand and abort.
								player.InvBody[offHand] = player.InvList[player._pNumInv - 1];
								player.RemoveInvItem(player._pNumInv - 1, false);
								break;
							}
							RemoveEquipment(player, offHand, false);
							invloc = mainHand;
						}
						break;
					default:
						// If the player is trying to equip a ring we want to say "I can't do that" if they don't already have a ring slot free.
						failedSpeech = HeroSpeech::ICantDoThat;
						break;
					}
					// Then empty the identified InvBody slot (invloc) and hand over to AutoEquip
					if (invloc != NUM_INVLOC
					    && !player.InvBody[invloc].isEmpty()
					    && CouldFitItemInInventory(player, player.InvBody[invloc], iv)) {
						holdItem = player.InvBody[invloc].pop();
					}
					automaticallyMoved = AutoEquip(player, player.InvList[iv], true, &player == MyPlayer);
					if (automaticallyMoved) {
						successSound = ItemInvSnds[ItemCAnimTbl[player.InvList[iv]._iCurs]];
						player.RemoveInvItem(iv, false);

						// If we're holding an item at this point we just lifted it from a body slot to make room for the original item, so we need to put it into the inv
						if (!holdItem.isEmpty() && AutoPlaceItemInInventory(player, holdItem)) {
							holdItem.clear();
						} // there should never be a situation where holdItem is not empty but we fail to place it into the inventory given the checks earlier... leave it on the hand in this case.
					} else if (!holdItem.isEmpty()) {
						// We somehow failed to equip the item in the slot we already checked should hold it? Better put this item back...
						player.InvBody[invloc] = holdItem.pop();
					}
				}
			} else {
				holdItem = player.InvList[iv];
				player.RemoveInvItem(iv, false);
			}
		}
	}

	if (r >= SLOTXY_BELT_FIRST) {
		Item &beltItem = player.SpdList[r - SLOTXY_BELT_FIRST];
		if (!beltItem.isEmpty()) {
			if (automaticMove) {
				attemptedMove = true;
				automaticallyMoved = AutoPlaceItemInInventory(player, beltItem);
				if (automaticallyMoved) {
					successSound = SfxID::GrabItem;
					player.RemoveSpdBarItem(r - SLOTXY_BELT_FIRST);
				} else {
					failedSpeech = HeroSpeech::IHaveNoRoom;
				}
			} else {
				holdItem = beltItem;
				player.RemoveSpdBarItem(r - SLOTXY_BELT_FIRST);
			}
		}
	}

	if (!holdItem.isEmpty()) {
		if (holdItem._itype == ItemType::Gold) {
			player._pGold = CalculateGold(player);
		}

		CalcPlrInv(player, true);
		holdItem._iStatFlag = player.CanUseItem(holdItem);

		if (&player == MyPlayer) {
			PlaySFX(SfxID::GrabItem);
			NewCursor(holdItem);
		}
		if (dropItem) {
			TryDropItem();
		}
	} else if (automaticMove) {
		if (automaticallyMoved) {
			CalcPlrInv(player, true);
		}
		if (attemptedMove && &player == MyPlayer) {
			if (automaticallyMoved) {
				PlaySFX(successSound);
			} else {
				player.SaySpecific(failedSpeech);
			}
		}
	}
}

void TryCombineNaKrulNotes(Player &player, Item &noteItem)
{
	int idx = noteItem.IDidx;
	_item_indexes notes[] = { IDI_NOTE1, IDI_NOTE2, IDI_NOTE3 };

	if (IsNoneOf(idx, IDI_NOTE1, IDI_NOTE2, IDI_NOTE3)) {
		return;
	}

	for (_item_indexes note : notes) {
		if (idx != note && !HasInventoryItemWithId(player, note)) {
			return; // the player doesn't have all notes
		}
	}

	MyPlayer->Say(HeroSpeech::JustWhatIWasLookingFor, 10);

	for (_item_indexes note : notes) {
		if (idx != note) {
			RemoveInventoryItemById(player, note);
		}
	}

	Point position = noteItem.position; // copy the position to restore it after re-initialising the item
	noteItem = {};
	GetItemAttrs(noteItem, IDI_FULLNOTE, 16);
	SetupItem(noteItem);
	noteItem.position = position; // this ensures CleanupItem removes the entry in the dropped items lookup table
}

void CheckQuestItem(Player &player, Item &questItem)
{
	Player &myPlayer = *MyPlayer;

	if (Quests[Q_BLIND]._qactive == QUEST_ACTIVE
	    && (questItem.IDidx == IDI_OPTAMULET
	        || (Quests[Q_BLIND].IsAvailable() && questItem.position == (SetPiece.position.megaToWorld() + Displacement { 5, 5 })))) {
		Quests[Q_BLIND]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_BLIND]);
	}

	if (questItem.IDidx == IDI_MUSHROOM && Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE && Quests[Q_MUSHROOM]._qvar1 == QS_MUSHSPAWNED) {
		player.Say(HeroSpeech::NowThatsOneBigMushroom, 10); // BUGFIX: Voice for this quest might be wrong in MP
		Quests[Q_MUSHROOM]._qvar1 = QS_MUSHPICKED;
		NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
	}

	if (questItem.IDidx == IDI_ANVIL && Quests[Q_ANVIL]._qactive != QUEST_NOTAVAIL) {
		if (Quests[Q_ANVIL]._qactive == QUEST_INIT) {
			Quests[Q_ANVIL]._qactive = QUEST_ACTIVE;
			NetSendCmdQuest(true, Quests[Q_ANVIL]);
		}
		if (Quests[Q_ANVIL]._qlog) {
			myPlayer.Say(HeroSpeech::INeedToGetThisToGriswold, 10);
		}
	}

	if (questItem.IDidx == IDI_GLDNELIX && Quests[Q_VEIL]._qactive != QUEST_NOTAVAIL) {
		myPlayer.Say(HeroSpeech::INeedToGetThisToLachdanan, 30);
	}

	if (questItem.IDidx == IDI_ROCK && Quests[Q_ROCK]._qactive != QUEST_NOTAVAIL) {
		if (Quests[Q_ROCK]._qactive == QUEST_INIT) {
			Quests[Q_ROCK]._qactive = QUEST_ACTIVE;
			NetSendCmdQuest(true, Quests[Q_ROCK]);
		}
		if (Quests[Q_ROCK]._qlog) {
			myPlayer.Say(HeroSpeech::ThisMustBeWhatGriswoldWanted, 10);
		}
	}

	if (Quests[Q_BLOOD]._qactive == QUEST_ACTIVE
	    && (questItem.IDidx == IDI_ARMOFVAL
	        || (Quests[Q_BLOOD].IsAvailable() && questItem.position == (SetPiece.position.megaToWorld() + Displacement { 9, 3 })))) {
		Quests[Q_BLOOD]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_BLOOD]);
		myPlayer.Say(HeroSpeech::MayTheSpiritOfArkaineProtectMe, 20);
	}

	if (questItem.IDidx == IDI_MAPOFDOOM) {
		Quests[Q_GRAVE]._qactive = QUEST_ACTIVE;
		if (Quests[Q_GRAVE]._qvar1 != 1) {
			MyPlayer->Say(HeroSpeech::UhHuh, 10);
			Quests[Q_GRAVE]._qvar1 = 1;
		}
	}

	TryCombineNaKrulNotes(player, questItem);
}

void CleanupItems(int ii)
{
	Item &item = Items[ii];
	dItem[item.position.x][item.position.y] = 0;

	if (CornerStone.isAvailable() && item.position == CornerStone.position) {
		CornerStone.item.clear();
		CornerStone.item.selectionRegion = SelectionRegion::None;
		CornerStone.item.position = { 0, 0 };
		CornerStone.item._iAnimFlag = false;
		CornerStone.item._iIdentified = false;
		CornerStone.item._iPostDraw = false;
	}

	int i = 0;
	while (i < ActiveItemCount) {
		if (ActiveItems[i] == ii) {
			DeleteItem(i);
			i = 0;
			continue;
		}

		i++;
	}
}

bool CanUseStaff(Item &staff, SpellID spell)
{
	return !staff.isEmpty()
	    && IsAnyOf(staff._iMiscId, IMISC_STAFF, IMISC_UNIQUE)
	    && staff._iSpell == spell
	    && staff._iCharges > 0;
}

void StartGoldDrop()
{
	CloseGoldWithdraw();

	const int8_t invIndex = pcursinvitem;

	const Player &myPlayer = *MyPlayer;

	const int max = (invIndex <= INVITEM_INV_LAST)
	    ? myPlayer.InvList[invIndex - INVITEM_INV_FIRST]._ivalue
	    : myPlayer.SpdList[invIndex - INVITEM_BELT_FIRST]._ivalue;

	if (ChatFlag)
		ResetChat();

	Point start = GetPanelPosition(UiPanels::Inventory, { 67, 128 });
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 180, 20);
	SDL_SetTextInputRect(&rect);

	OpenGoldDrop(invIndex, max);
}

int CreateGoldItemInInventorySlot(Player &player, int slotIndex, int value)
{
	if (player.InvGrid[slotIndex] != 0) {
		return value;
	}

	Item &goldItem = player.InvList[player._pNumInv];
	MakeGoldStack(goldItem, std::min(value, MaxGold));
	player._pNumInv++;
	player.InvGrid[slotIndex] = player._pNumInv;
	if (&player == MyPlayer) {
		NetSendCmdChInvItem(false, slotIndex);
	}

	value -= goldItem._ivalue;

	return value;
}

} // namespace

void InvDrawSlotBack(const Surface &out, Point targetPosition, Size size, item_quality itemQuality)
{
	SDL_Rect srcRect = MakeSdlRect(0, 0, size.width, size.height);
	out.Clip(&srcRect, &targetPosition);
	if (size.width <= 0 || size.height <= 0)
		return;

	uint8_t colorShift;
	switch (itemQuality) {
	case ITEM_QUALITY_MAGIC:
		colorShift = PAL16_GRAY - (!IsInspectingPlayer() ? PAL16_BLUE : PAL16_ORANGE) - 1;
		break;
	case ITEM_QUALITY_UNIQUE:
		colorShift = PAL16_GRAY - (!IsInspectingPlayer() ? PAL16_YELLOW : PAL16_ORANGE) - 1;
		break;
	default:
		colorShift = PAL16_GRAY - (!IsInspectingPlayer() ? PAL16_BEIGE : PAL16_ORANGE) - 1;
		break;
	}

	uint8_t *dst = &out[targetPosition];
	const auto dstPitch = out.pitch();
	for (int y = size.height; y != 0; --y, dst -= dstPitch + size.width) {
		for (const uint8_t *end = dst + size.width; dst < end; ++dst) {
			uint8_t &pix = *dst;
			if (pix >= PAL16_GRAY) {
				pix -= colorShift;
			}
		}
	}
}

bool CanBePlacedOnBelt(const Player &player, const Item &item)
{
	return FitsInBeltSlot(item)
	    && item._itype != ItemType::Gold
	    && player.CanUseItem(item)
	    && item.isUsable();
}

void FreeInvGFX()
{
	pInvCels = std::nullopt;
}

void InitInv()
{
	switch (MyPlayer->_pClass) {
	case HeroClass::Warrior:
	case HeroClass::Barbarian:
		pInvCels = LoadCel("data\\inv\\inv", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Rogue:
	case HeroClass::Bard:
		pInvCels = LoadCel("data\\inv\\inv_rog", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Sorcerer:
		pInvCels = LoadCel("data\\inv\\inv_sor", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Monk:
		pInvCels = LoadCel(!gbIsSpawn ? "data\\inv\\inv_sor" : "data\\inv\\inv", static_cast<uint16_t>(SidePanelSize.width));
		break;
	}
}

void DrawInv(const Surface &out)
{
	ClxDraw(out, GetPanelPosition(UiPanels::Inventory, { 0, 351 }), (*pInvCels)[0]);

	Size slotSize[] = {
		{ 2, 2 }, // head
		{ 1, 1 }, // left ring
		{ 1, 1 }, // right ring
		{ 1, 1 }, // amulet
		{ 2, 3 }, // left hand
		{ 2, 3 }, // right hand
		{ 2, 3 }, // chest
	};

	Point slotPos[] = {
		{ 133, 59 },  // head
		{ 48, 205 },  // left ring
		{ 249, 205 }, // right ring
		{ 205, 60 },  // amulet
		{ 17, 160 },  // left hand
		{ 248, 160 }, // right hand
		{ 133, 160 }, // chest
	};

	Player &myPlayer = *InspectPlayer;

	for (int slot = INVLOC_HEAD; slot < NUM_INVLOC; slot++) {
		if (!myPlayer.InvBody[slot].isEmpty()) {
			int screenX = slotPos[slot].x;
			int screenY = slotPos[slot].y;
			InvDrawSlotBack(out, GetPanelPosition(UiPanels::Inventory, { screenX, screenY }), { slotSize[slot].width * InventorySlotSizeInPixels.width, slotSize[slot].height * InventorySlotSizeInPixels.height }, myPlayer.InvBody[slot]._iMagical);

			const int cursId = myPlayer.InvBody[slot]._iCurs + CURSOR_FIRSTITEM;

			Size frameSize = GetInvItemSize(cursId);

			// calc item offsets for weapons/armor smaller than 2x3 slots
			if (IsAnyOf(slot, INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT, INVLOC_CHEST)) {
				screenX += frameSize.width == InventorySlotSizeInPixels.width ? INV_SLOT_HALF_SIZE_PX : 0;
				screenY += frameSize.height == (3 * InventorySlotSizeInPixels.height) ? 0 : -INV_SLOT_HALF_SIZE_PX;
			}

			const ClxSprite sprite = GetInvItemSprite(cursId);
			const Point position = GetPanelPosition(UiPanels::Inventory, { screenX, screenY });

			if (pcursinvitem == slot) {
				ClxDrawOutline(out, GetOutlineColor(myPlayer.InvBody[slot], true), position, sprite);
			}

			DrawItem(myPlayer.InvBody[slot], out, position, sprite);

			if (slot == INVLOC_HAND_LEFT) {
				if (myPlayer.GetItemLocation(myPlayer.InvBody[slot]) == ILOC_TWOHAND) {
					InvDrawSlotBack(out, GetPanelPosition(UiPanels::Inventory, slotPos[INVLOC_HAND_RIGHT]), { slotSize[INVLOC_HAND_RIGHT].width * InventorySlotSizeInPixels.width, slotSize[INVLOC_HAND_RIGHT].height * InventorySlotSizeInPixels.height }, myPlayer.InvBody[slot]._iMagical);
					const int dstX = GetRightPanel().position.x + slotPos[INVLOC_HAND_RIGHT].x + (frameSize.width == InventorySlotSizeInPixels.width ? INV_SLOT_HALF_SIZE_PX : 0) - 1;
					const int dstY = GetRightPanel().position.y + slotPos[INVLOC_HAND_RIGHT].y;
					ClxDrawBlended(out, { dstX, dstY }, sprite);
				}
			}
		}
	}

	for (int i = 0; i < InventoryGridCells; i++) {
		if (myPlayer.InvGrid[i] != 0) {
			InvDrawSlotBack(
			    out,
			    GetPanelPosition(UiPanels::Inventory, InvRect[i + SLOTXY_INV_FIRST].position) + Displacement { 0, InventorySlotSizeInPixels.height },
			    InventorySlotSizeInPixels,
			    myPlayer.InvList[std::abs(myPlayer.InvGrid[i]) - 1]._iMagical);
		}
	}

	for (int j = 0; j < InventoryGridCells; j++) {
		if (myPlayer.InvGrid[j] > 0) { // first slot of an item
			int ii = myPlayer.InvGrid[j] - 1;
			int cursId = myPlayer.InvList[ii]._iCurs + CURSOR_FIRSTITEM;

			const ClxSprite sprite = GetInvItemSprite(cursId);
			const Point position = GetPanelPosition(UiPanels::Inventory, InvRect[j + SLOTXY_INV_FIRST].position) + Displacement { 0, InventorySlotSizeInPixels.height };
			if (pcursinvitem == ii + INVITEM_INV_FIRST) {
				ClxDrawOutline(out, GetOutlineColor(myPlayer.InvList[ii], true), position, sprite);
			}

			DrawItem(myPlayer.InvList[ii], out, position, sprite);
		}
	}
}

void DrawInvBelt(const Surface &out)
{
	if (ChatFlag) {
		return;
	}

	const Point mainPanelPosition = GetMainPanel().position;

	DrawPanelBox(out, { 205, 21, 232, 28 }, mainPanelPosition + Displacement { 205, 5 });

	Player &myPlayer = *InspectPlayer;

	for (int i = 0; i < MaxBeltItems; i++) {
		if (myPlayer.SpdList[i].isEmpty()) {
			continue;
		}

		const Point position { InvRect[i + SLOTXY_BELT_FIRST].position.x + mainPanelPosition.x, InvRect[i + SLOTXY_BELT_FIRST].position.y + mainPanelPosition.y + InventorySlotSizeInPixels.height };
		InvDrawSlotBack(out, position, InventorySlotSizeInPixels, myPlayer.SpdList[i]._iMagical);
		const int cursId = myPlayer.SpdList[i]._iCurs + CURSOR_FIRSTITEM;

		const ClxSprite sprite = GetInvItemSprite(cursId);

		if (pcursinvitem == i + INVITEM_BELT_FIRST) {
			if (ControlMode == ControlTypes::KeyboardAndMouse || invflag) {
				ClxDrawOutline(out, GetOutlineColor(myPlayer.SpdList[i], true), position, sprite);
			}
		}

		DrawItem(myPlayer.SpdList[i], out, position, sprite);

		if (myPlayer.SpdList[i].isUsable()
		    && myPlayer.SpdList[i]._itype != ItemType::Gold) {
			auto beltKey = StrCat("BeltItem", i + 1);
			std::string_view keyName = ControlMode == ControlTypes::Gamepad
			    ? GetOptions().Padmapper.InputNameForAction(beltKey, true)
			    : GetOptions().Keymapper.KeyNameForAction(beltKey);

			if (keyName.length() > 2)
				keyName = {};

			DrawString(out, keyName, { position - Displacement { 0, 12 }, InventorySlotSizeInPixels },
			    { .flags = UiFlags::ColorWhite | UiFlags::AlignRight });
		}
	}
}

void RemoveEquipment(Player &player, inv_body_loc bodyLocation, bool hiPri)
{
	if (&player == MyPlayer) {
		NetSendCmdDelItem(hiPri, bodyLocation);
	}

	player.InvBody[bodyLocation].clear();
}

bool AutoPlaceItemInBelt(Player &player, const Item &item, bool persistItem, bool sendNetworkMessage)
{
	if (!CanBePlacedOnBelt(player, item)) {
		return false;
	}

	for (Item &beltItem : player.SpdList) {
		if (beltItem.isEmpty()) {
			if (persistItem) {
				beltItem = item;
				player.CalcScrolls();
				RedrawComponent(PanelDrawComponent::Belt);
				if (sendNetworkMessage) {
					const auto beltIndex = static_cast<int>(std::distance<const Item *>(&player.SpdList[0], &beltItem));
					NetSendCmdChBeltItem(false, beltIndex);
				}
			}

			return true;
		}
	}

	return false;
}

bool AutoEquip(Player &player, const Item &item, bool persistItem, bool sendNetworkMessage)
{
	if (!CanEquip(item)) {
		return false;
	}

	for (int bodyLocation = INVLOC_HEAD; bodyLocation < NUM_INVLOC; bodyLocation++) {
		if (AutoEquip(player, item, (inv_body_loc)bodyLocation, persistItem, sendNetworkMessage)) {
			return true;
		}
	}

	return false;
}

bool AutoEquipEnabled(const Player &player, const Item &item)
{
	if (item.isWeapon()) {
		// Monk can use unarmed attack as an encouraged option, thus we do not automatically equip weapons on him so as to not
		// annoy players who prefer that playstyle.
		return player._pClass != HeroClass::Monk && *GetOptions().Gameplay.autoEquipWeapons;
	}

	if (item.isArmor()) {
		return *GetOptions().Gameplay.autoEquipArmor;
	}

	if (item.isHelm()) {
		return *GetOptions().Gameplay.autoEquipHelms;
	}

	if (item.isShield()) {
		return *GetOptions().Gameplay.autoEquipShields;
	}

	if (item.isJewelry()) {
		return *GetOptions().Gameplay.autoEquipJewelry;
	}

	return true;
}

bool CanFitItemInInventory(const Player &player, const Item &item)
{
	return static_cast<bool>(FindSlotForItem(player, GetInventorySize(item)));
}

bool AutoPlaceItemInInventory(Player &player, const Item &item, bool sendNetworkMessage)
{
	Size itemSize = GetInventorySize(item);
	std::optional<int> targetSlot = FindSlotForItem(player, itemSize);

	if (targetSlot) {
		player.InvList[player._pNumInv] = item;
		player._pNumInv++;

		AddItemToInvGrid(player, *targetSlot, player._pNumInv, itemSize, sendNetworkMessage);
		player.CalcScrolls();

		return true;
	}

	return false;
}

std::vector<int> SortItemsBySize(Player &player)
{
	std::vector<std::pair<Size, int>> itemSizes; // Pair of item size and its index in InvList
	itemSizes.reserve(player._pNumInv);          // Reserves space for the number of items in the player's inventory

	for (int i = 0; i < player._pNumInv; i++) {
		Size size = GetInventorySize(player.InvList[i]);
		itemSizes.emplace_back(size, i);
	}

	// Sort items by height first, then by width
	std::sort(itemSizes.begin(), itemSizes.end(), [](const auto &a, const auto &b) {
		if (a.first.height == b.first.height) return a.first.width > b.first.width;
		return a.first.height > b.first.height;
	});

	// Extract sorted indices
	std::vector<int> sortedIndices;
	sortedIndices.reserve(itemSizes.size()); // Pre-allocate the necessary capacity

	for (const auto &itemSize : itemSizes) {
		sortedIndices.push_back(itemSize.second);
	}

	return sortedIndices;
}

void ReorganizeInventory(Player &player)
{
	// Sort items by size
	std::vector<int> sortedIndices = SortItemsBySize(player);

	// Temporary storage for items and a copy of InvGrid
	std::vector<Item> tempStorage(player._pNumInv);
	std::array<int8_t, 40> originalInvGrid;                                                       // Declare an array for InvGrid copy
	std::copy(std::begin(player.InvGrid), std::end(player.InvGrid), std::begin(originalInvGrid)); // Copy InvGrid to originalInvGrid

	// Move items to temporary storage and clear inventory slots
	for (int i = 0; i < player._pNumInv; ++i) {
		tempStorage[i] = player.InvList[i];
		player.InvList[i] = {};
	}
	player._pNumInv = 0;                                                // Reset inventory count
	std::fill(std::begin(player.InvGrid), std::end(player.InvGrid), 0); // Clear InvGrid

	// Attempt to place items back, now from the temp storage
	bool reorganizationFailed = false;
	for (int index : sortedIndices) {
		Item &item = tempStorage[index];
		if (!AutoPlaceItemInInventory(player, item, false)) {
			reorganizationFailed = true;
			break;
		}
	}

	// If reorganization failed, restore items and InvGrid from tempStorage and originalInvGrid
	if (reorganizationFailed) {
		for (Item &item : tempStorage) {
			if (!item.isEmpty()) {
				player.InvList[player._pNumInv++] = item;
			}
		}
		std::copy(std::begin(originalInvGrid), std::end(originalInvGrid), std::begin(player.InvGrid)); // Restore InvGrid
	}
}

int RoomForGold()
{
	int amount = 0;
	for (int8_t &itemIndex : MyPlayer->InvGrid) {
		if (itemIndex < 0) {
			continue;
		}
		if (itemIndex == 0) {
			amount += MaxGold;
			continue;
		}

		Item &goldItem = MyPlayer->InvList[itemIndex - 1];
		if (goldItem._itype != ItemType::Gold || goldItem._ivalue == MaxGold) {
			continue;
		}

		amount += MaxGold - goldItem._ivalue;
	}

	return amount;
}

int AddGoldToInventory(Player &player, int value)
{
	// Top off existing piles
	for (int i = 0; i < player._pNumInv && value > 0; i++) {
		Item &goldItem = player.InvList[i];
		if (goldItem._itype != ItemType::Gold || goldItem._ivalue >= MaxGold) {
			continue;
		}

		if (goldItem._ivalue + value > MaxGold) {
			value -= MaxGold - goldItem._ivalue;
			goldItem._ivalue = MaxGold;
		} else {
			goldItem._ivalue += value;
			value = 0;
		}

		NetSyncInvItem(player, i);
		SetPlrHandGoldCurs(goldItem);
	}

	// Last row right to left
	for (int i = 39; i >= 30 && value > 0; i--) {
		value = CreateGoldItemInInventorySlot(player, i, value);
	}

	// Remaining inventory in columns, bottom to top, right to left
	for (int x = 9; x >= 0 && value > 0; x--) {
		for (int y = 2; y >= 0 && value > 0; y--) {
			value = CreateGoldItemInInventorySlot(player, 10 * y + x, value);
		}
	}

	return value;
}

bool GoldAutoPlace(Player &player, Item &goldStack)
{
	goldStack._ivalue = AddGoldToInventory(player, goldStack._ivalue);
	SetPlrHandGoldCurs(goldStack);

	player._pGold = CalculateGold(player);

	return goldStack._ivalue == 0;
}

void CheckInvSwap(Player &player, inv_body_loc bLoc)
{
	Item &item = player.InvBody[bLoc];

	if (bLoc == INVLOC_HAND_LEFT && player.GetItemLocation(item) == ILOC_TWOHAND) {
		player.InvBody[INVLOC_HAND_RIGHT].clear();
	} else if (bLoc == INVLOC_HAND_RIGHT && player.GetItemLocation(item) == ILOC_TWOHAND) {
		player.InvBody[INVLOC_HAND_LEFT].clear();
	}

	CalcPlrInv(player, true);
}

void inv_update_rem_item(Player &player, inv_body_loc iv)
{
	player.InvBody[iv].clear();

	CalcPlrInv(player, player._pmode != PM_DEATH);
}

void CheckInvSwap(Player &player, const Item &item, int invGridIndex)
{
	Size itemSize = GetInventorySize(item);

	const int pitch = 10;
	int invListIndex = [&]() -> int {
		for (int y = 0; y < itemSize.height; y++) {
			int rowGridIndex = invGridIndex + pitch * y;
			for (int x = 0; x < itemSize.width; x++) {
				int gridIndex = rowGridIndex + x;
				if (player.InvGrid[gridIndex] != 0)
					return std::abs(player.InvGrid[gridIndex]);
			}
		}
		player._pNumInv++;
		return player._pNumInv;
	}();

	if (invListIndex < player._pNumInv) {
		for (int8_t &itemIndex : player.InvGrid) {
			if (itemIndex == invListIndex)
				itemIndex = 0;
			if (itemIndex == -invListIndex)
				itemIndex = 0;
		}
	}

	player.InvList[invListIndex - 1] = item;

	for (int y = 0; y < itemSize.height; y++) {
		int rowGridIndex = invGridIndex + pitch * y;
		for (int x = 0; x < itemSize.width; x++) {
			if (x == 0 && y == itemSize.height - 1)
				player.InvGrid[rowGridIndex + x] = invListIndex;
			else
				player.InvGrid[rowGridIndex + x] = -invListIndex;
		}
	}

	CalcPlrInv(player, true);
}

void CheckInvRemove(Player &player, int invGridIndex)
{
	int invListIndex = std::abs(player.InvGrid[invGridIndex]) - 1;

	if (invListIndex >= 0) {
		player.RemoveInvItem(invListIndex);
	}
}

void TransferItemToStash(Player &player, int location)
{
	if (location == -1) {
		return;
	}

	Item &item = GetInventoryItem(player, location);
	if (!AutoPlaceItemInStash(player, item, true)) {
		player.SaySpecific(HeroSpeech::WhereWouldIPutThis);
		return;
	}

	PlaySFX(ItemInvSnds[ItemCAnimTbl[item._iCurs]]);

	if (location < INVITEM_INV_FIRST) {
		RemoveEquipment(player, static_cast<inv_body_loc>(location), false);
		CalcPlrInv(player, true);
	} else if (location <= INVITEM_INV_LAST)
		player.RemoveInvItem(location - INVITEM_INV_FIRST);
	else
		player.RemoveSpdBarItem(location - INVITEM_BELT_FIRST);
}

void CheckInvItem(bool isShiftHeld, bool isCtrlHeld)
{
	if (IsInspectingPlayer())
		return;
	if (!MyPlayer->HoldItem.isEmpty()) {
		CheckInvPaste(*MyPlayer, MousePosition);
	} else if (IsStashOpen && isCtrlHeld) {
		TransferItemToStash(*MyPlayer, pcursinvitem);
	} else {
		CheckInvCut(*MyPlayer, MousePosition, isShiftHeld, isCtrlHeld);
	}
}

void CheckInvScrn(bool isShiftHeld, bool isCtrlHeld)
{
	const Point mainPanelPosition = GetMainPanel().position;
	if (MousePosition.x > 190 + mainPanelPosition.x && MousePosition.x < 437 + mainPanelPosition.x
	    && MousePosition.y > mainPanelPosition.y && MousePosition.y < 33 + mainPanelPosition.y) {
		CheckInvItem(isShiftHeld, isCtrlHeld);
	}
}

void InvGetItem(Player &player, int ii)
{
	Item &item = Items[ii];
	CloseGoldDrop();

	if (dItem[item.position.x][item.position.y] == 0)
		return;

	item._iCreateInfo &= ~CF_PREGEN;
	CheckQuestItem(player, item);
	item.updateRequiredStatsCacheForPlayer(player);

	if (item._itype == ItemType::Gold && GoldAutoPlace(player, item)) {
		if (MyPlayer == &player) {
			// Non-gold items (or gold when you have a full inventory) go to the hand then provide audible feedback on
			//  paste. To give the same feedback for auto-placed gold we play the sound effect now.
			PlaySFX(SfxID::ItemGold);
		}
	} else {
		// The item needs to go into the players hand
		if (MyPlayer == &player && !player.HoldItem.isEmpty()) {
			// drop whatever the player is currently holding
			NetSendCmdPItem(true, CMD_SYNCPUTITEM, player.position.tile, player.HoldItem);
		}

		// need to copy here instead of move so CleanupItems still has access to the position
		player.HoldItem = item;
		NewCursor(player.HoldItem);
	}

	// This potentially moves items in memory so must be done after we've made a copy
	CleanupItems(ii);
	pcursitem = -1;
}

std::optional<Point> FindAdjacentPositionForItem(Point origin, Direction facing)
{
	if (ActiveItemCount >= MAXITEMS)
		return {};

	if (CanPut(origin + facing))
		return origin + facing;

	if (CanPut(origin + Left(facing)))
		return origin + Left(facing);

	if (CanPut(origin + Right(facing)))
		return origin + Right(facing);

	if (CanPut(origin + Left(Left(facing))))
		return origin + Left(Left(facing));

	if (CanPut(origin + Right(Right(facing))))
		return origin + Right(Right(facing));

	if (CanPut(origin + Left(Left(Left(facing)))))
		return origin + Left(Left(Left(facing)));

	if (CanPut(origin + Right(Right(Right(facing)))))
		return origin + Right(Right(Right(facing)));

	if (CanPut(origin + Opposite(facing)))
		return origin + Opposite(facing);

	if (CanPut(origin))
		return origin;

	return {};
}

void AutoGetItem(Player &player, Item *itemPointer, int ii)
{
	Item &item = *itemPointer;

	CloseGoldDrop();

	if (dItem[item.position.x][item.position.y] == 0)
		return;

	item._iCreateInfo &= ~CF_PREGEN;
	CheckQuestItem(player, item);
	item.updateRequiredStatsCacheForPlayer(player);

	bool done;
	bool autoEquipped = false;

	if (item._itype == ItemType::Gold) {
		done = GoldAutoPlace(player, item);
		if (!done) {
			SetPlrHandGoldCurs(item);
		}
	} else {
		done = AutoEquipEnabled(player, item) && AutoEquip(player, item, true, &player == MyPlayer);
		if (done) {
			autoEquipped = true;
		}

		if (!done) {
			done = AutoPlaceItemInBelt(player, item, true, &player == MyPlayer);
		}
		if (!done) {
			done = AutoPlaceItemInInventory(player, item, &player == MyPlayer);
		}
	}

	if (done) {
		if (!autoEquipped && *GetOptions().Audio.itemPickupSound && &player == MyPlayer) {
			PlaySFX(SfxID::GrabItem);
		}

		CleanupItems(ii);
		return;
	}

	if (&player == MyPlayer) {
		player.Say(HeroSpeech::ICantCarryAnymore);
	}
	RespawnItem(item, true);
	NetSendCmdPItem(true, CMD_SPAWNITEM, item.position, item);
}

int FindGetItem(uint32_t iseed, _item_indexes idx, uint16_t createInfo)
{
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		Item &item = Items[ActiveItems[i]];
		if (item.keyAttributesMatch(iseed, idx, createInfo)) {
			return i;
		}
	}

	return -1;
}

void SyncGetItem(Point position, uint32_t iseed, _item_indexes idx, uint16_t ci)
{
	// Check what the local client has at the target position
	int ii = dItem[position.x][position.y] - 1;

	if (ii >= 0 && ii < MAXITEMS) {
		// If there was an item there, check that it's the same item as the remote player has
		if (!Items[ii].keyAttributesMatch(iseed, idx, ci)) {
			// Key attributes don't match so we must've desynced, ignore this index and try find a matching item via lookup
			ii = -1;
		}
	}

	if (ii == -1) {
		// Either there's no item at the expected position or it doesn't match what is being picked up, so look for an item that matches the key attributes
		ii = FindGetItem(iseed, idx, ci);

		if (ii != -1) {
			// Translate to Items index for CleanupItems, FindGetItem returns an ActiveItems index
			ii = ActiveItems[ii];
		}
	}

	if (ii == -1) {
		// Still can't find the expected item, assume it was collected earlier and this caused the desync
		return;
	}

	CleanupItems(ii);
}

bool CanPut(Point position)
{
	if (!InDungeonBounds(position)) {
		return false;
	}

	if (IsTileSolid(position)) {
		return false;
	}

	if (dItem[position.x][position.y] != 0) {
		return false;
	}

	if (leveltype == DTYPE_TOWN) {
		if (dMonster[position.x][position.y] != 0) {
			return false;
		}
		if (dMonster[position.x + 1][position.y + 1] != 0) {
			return false;
		}
	}

	if (IsItemBlockingObjectAtPosition(position)) {
		return false;
	}

	return true;
}

int ClampDurability(const Item &item, int durability)
{
	if (item._iMaxDur == 0)
		return 0;

	return std::clamp<int>(durability, 1, item._iMaxDur);
}

int16_t ClampToHit(const Item &item, int16_t toHit)
{
	if (toHit < item._iPLToHit || toHit > 51)
		return item._iPLToHit;

	return toHit;
}

uint8_t ClampMaxDam(const Item &item, uint8_t maxDam)
{
	if (maxDam < item._iMaxDam || maxDam - item._iMinDam > 30)
		return item._iMaxDam;

	return maxDam;
}

int SyncDropItem(Point position, _item_indexes idx, uint16_t icreateinfo, int iseed, int id, int dur, int mdur, int ch, int mch, int ivalue, uint32_t ibuff, int toHit, int maxDam)
{
	if (ActiveItemCount >= MAXITEMS)
		return -1;

	Item item;

	RecreateItem(*MyPlayer, item, idx, icreateinfo, iseed, ivalue, ibuff);
	if (id != 0)
		item._iIdentified = true;
	item._iMaxDur = mdur;
	item._iDurability = ClampDurability(item, dur);
	item._iMaxCharges = std::clamp<int>(mch, 0, item._iMaxCharges);
	item._iCharges = std::clamp<int>(ch, 0, item._iMaxCharges);
	if (gbIsHellfire) {
		item._iPLToHit = ClampToHit(item, toHit);
		item._iMaxDam = ClampMaxDam(item, maxDam);
	}

	return PlaceItemInWorld(std::move(item), position);
}

int SyncDropEar(Point position, uint16_t icreateinfo, uint32_t iseed, uint8_t cursval, std::string_view heroname)
{
	if (ActiveItemCount >= MAXITEMS)
		return -1;

	Item item;
	RecreateEar(item, icreateinfo, iseed, cursval, heroname);

	return PlaceItemInWorld(std::move(item), position);
}

int8_t CheckInvHLight()
{
	int8_t r = 0;
	for (; r < NUM_XY_SLOTS; r++) {
		int xo = GetRightPanel().position.x;
		int yo = GetRightPanel().position.y;
		if (r >= SLOTXY_BELT_FIRST) {
			xo = GetMainPanel().position.x;
			yo = GetMainPanel().position.y;
		}

		if (InvRect[r].contains(MousePosition - Displacement(xo, yo))) {
			break;
		}
	}

	if (r >= NUM_XY_SLOTS)
		return -1;

	int8_t rv = -1;
	InfoColor = UiFlags::ColorWhite;
	Item *pi = nullptr;
	Player &myPlayer = *InspectPlayer;

	if (r == SLOTXY_HEAD) {
		rv = INVLOC_HEAD;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_RING_LEFT) {
		rv = INVLOC_RING_LEFT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_RING_RIGHT) {
		rv = INVLOC_RING_RIGHT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_AMULET) {
		rv = INVLOC_AMULET;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_HAND_LEFT) {
		rv = INVLOC_HAND_LEFT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_HAND_RIGHT) {
		pi = &myPlayer.InvBody[INVLOC_HAND_LEFT];
		if (pi->isEmpty() || myPlayer.GetItemLocation(*pi) != ILOC_TWOHAND) {
			rv = INVLOC_HAND_RIGHT;
			pi = &myPlayer.InvBody[rv];
		} else {
			rv = INVLOC_HAND_LEFT;
		}
	} else if (r == SLOTXY_CHEST) {
		rv = INVLOC_CHEST;
		pi = &myPlayer.InvBody[rv];
	} else if (r >= SLOTXY_INV_FIRST && r <= SLOTXY_INV_LAST) {
		int8_t itemId = std::abs(myPlayer.InvGrid[r - SLOTXY_INV_FIRST]);
		if (itemId == 0)
			return -1;
		int ii = itemId - 1;
		rv = ii + INVITEM_INV_FIRST;
		pi = &myPlayer.InvList[ii];
	} else if (r >= SLOTXY_BELT_FIRST) {
		r -= SLOTXY_BELT_FIRST;
		RedrawComponent(PanelDrawComponent::Belt);
		pi = &myPlayer.SpdList[r];
		if (pi->isEmpty())
			return -1;
		rv = r + INVITEM_BELT_FIRST;
	}

	if (pi->isEmpty())
		return -1;

	if (pi->_itype == ItemType::Gold) {
		int nGold = pi->_ivalue;
		InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
	} else {
		InfoColor = pi->getTextColor();
		InfoString = pi->getName();
		if (pi->_iIdentified) {
			PrintItemDetails(*pi);
		} else {
			PrintItemDur(*pi);
		}
	}

	return rv;
}

void ConsumeScroll(Player &player)
{
	const SpellID spellId = player.executedSpell.spellId;

	const auto isCurrentSpell = [spellId](const Item &item) -> bool {
		return item.isScrollOf(spellId) || item.isRuneOf(spellId);
	};

	// Try to remove the scroll from selected inventory slot
	const int8_t itemSlot = player.executedSpell.spellFrom;
	if (itemSlot >= INVITEM_INV_FIRST && itemSlot <= INVITEM_INV_LAST) {
		const int itemIndex = itemSlot - INVITEM_INV_FIRST;
		const Item *item = &player.InvList[itemIndex];
		if (!item->isEmpty() && isCurrentSpell(*item)) {
			player.RemoveInvItem(itemIndex);
			return;
		}
	} else if (itemSlot >= INVITEM_BELT_FIRST && itemSlot <= INVITEM_BELT_LAST) {
		const int itemIndex = itemSlot - INVITEM_BELT_FIRST;
		const Item *item = &player.SpdList[itemIndex];
		if (!item->isEmpty() && isCurrentSpell(*item)) {
			player.RemoveSpdBarItem(itemIndex);
			return;
		}
	} else if (itemSlot != 0) {
		app_fatal(StrCat("ConsumeScroll: Invalid item index ", itemSlot));
	}

	// Didn't find it at the selected slot, take the first one we find
	// This path is always used when the scroll is consumed via spell selection
	RemoveInventoryOrBeltItem(player, isCurrentSpell);
}

bool CanUseScroll(Player &player, SpellID spell)
{
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown())
		return false;

	return HasInventoryOrBeltItem(player, [spell](const Item &item) {
		return item.isScrollOf(spell) || item.isRuneOf(spell);
	});
}

void ConsumeStaffCharge(Player &player)
{
	Item &staff = player.InvBody[INVLOC_HAND_LEFT];

	if (!CanUseStaff(staff, player.executedSpell.spellId))
		return;

	staff._iCharges--;
	CalcPlrInv(player, false);
}

bool CanUseStaff(Player &player, SpellID spellId)
{
	return CanUseStaff(player.InvBody[INVLOC_HAND_LEFT], spellId);
}

Item &GetInventoryItem(Player &player, int location)
{
	if (location < INVITEM_INV_FIRST)
		return player.InvBody[location];

	if (location <= INVITEM_INV_LAST)
		return player.InvList[location - INVITEM_INV_FIRST];

	return player.SpdList[location - INVITEM_BELT_FIRST];
}

bool UseInvItem(int cii)
{
	if (IsInspectingPlayer())
		return false;

	Player &player = *MyPlayer;

	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer)
		return true;
	if (pcurs != CURSOR_HAND)
		return true;
	if (IsPlayerInStore())
		return true;
	if (cii < INVITEM_INV_FIRST)
		return false;

	bool speedlist = false;
	int c;
	Item *item;
	if (cii <= INVITEM_INV_LAST) {
		c = cii - INVITEM_INV_FIRST;
		item = &player.InvList[c];
	} else {
		if (ChatFlag)
			return true;
		c = cii - INVITEM_BELT_FIRST;

		item = &player.SpdList[c];
		speedlist = true;

		// If selected speedlist item exists in InvList, use the InvList item.
		for (int i = 0; i < player._pNumInv && *GetOptions().Gameplay.autoRefillBelt; i++) {
			if (player.InvList[i]._iMiscId == item->_iMiscId && player.InvList[i]._iSpell == item->_iSpell) {
				c = i;
				item = &player.InvList[c];
				cii = c + INVITEM_INV_FIRST;
				speedlist = false;
				break;
			}
		}

		// If speedlist item is not inventory, use same item at the end of the speedlist if exists.
		if (speedlist && *GetOptions().Gameplay.autoRefillBelt) {
			for (int i = INVITEM_BELT_LAST - INVITEM_BELT_FIRST; i > c; i--) {
				Item &candidate = player.SpdList[i];

				if (!candidate.isEmpty() && candidate._iMiscId == item->_iMiscId && candidate._iSpell == item->_iSpell) {
					c = i;
					cii = c + INVITEM_BELT_FIRST;
					item = &candidate;
					break;
				}
			}
		}
	}

	constexpr int SpeechDelay = 10;
	if (item->IDidx == IDI_MUSHROOM) {
		player.Say(HeroSpeech::NowThatsOneBigMushroom, SpeechDelay);
		return true;
	}
	if (item->IDidx == IDI_FUNGALTM) {

		PlaySFX(SfxID::ItemBook);
		player.Say(HeroSpeech::ThatDidntDoAnything, SpeechDelay);
		return true;
	}

	if (player.isOnLevel(0)) {
		if (UseItemOpensHive(*item, player.position.tile)) {
			OpenHive();
			player.RemoveInvItem(c);
			return true;
		}
		if (UseItemOpensGrave(*item, player.position.tile)) {
			OpenGrave();
			player.RemoveInvItem(c);
			return true;
		}
	}

	if (!item->isUsable())
		return false;

	if (!player.CanUseItem(*item)) {
		player.Say(HeroSpeech::ICantUseThisYet);
		return true;
	}

	if (item->_iMiscId == IMISC_NONE && item->_itype == ItemType::Gold) {
		StartGoldDrop();
		return true;
	}

	CloseGoldDrop();

	if (item->isScroll() && leveltype == DTYPE_TOWN && !GetSpellData(item->_iSpell).isAllowedInTown()) {
		return true;
	}

	if (item->_iMiscId > IMISC_RUNEFIRST && item->_iMiscId < IMISC_RUNELAST && leveltype == DTYPE_TOWN) {
		return true;
	}

	if (item->_iMiscId == IMISC_ARENAPOT && !player.isOnArenaLevel()) {
		player.Say(HeroSpeech::ThatWontWorkHere);
		return true;
	}

	int idata = ItemCAnimTbl[item->_iCurs];
	if (item->_iMiscId == IMISC_BOOK)
		PlaySFX(SfxID::ReadBook);
	else if (&player == MyPlayer)
		PlaySFX(ItemInvSnds[idata]);

	UseItem(player, item->_iMiscId, item->_iSpell, cii);

	if (speedlist) {
		if (player.SpdList[c]._iMiscId == IMISC_NOTE) {
			InitQTextMsg(TEXT_BOOK9);
			CloseInventory();
			return true;
		}
		if (!item->isScroll() && !item->isRune())
			player.RemoveSpdBarItem(c);
		return true;
	}
	if (player.InvList[c]._iMiscId == IMISC_MAPOFDOOM)
		return true;
	if (player.InvList[c]._iMiscId == IMISC_NOTE) {
		InitQTextMsg(TEXT_BOOK9);
		CloseInventory();
		return true;
	}
	if (!item->isScroll() && !item->isRune())
		player.RemoveInvItem(c);

	return true;
}

void CloseInventory()
{
	CloseGoldWithdraw();
	CloseStash();
	invflag = false;
}

void CloseStash()
{
	if (!IsStashOpen)
		return;

	Player &myPlayer = *MyPlayer;
	if (!myPlayer.HoldItem.isEmpty()) {
		std::optional<Point> itemTile = FindAdjacentPositionForItem(myPlayer.position.future, myPlayer._pdir);
		if (itemTile) {
			NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, myPlayer.HoldItem);
		} else {
			if (!AutoPlaceItemInBelt(myPlayer, myPlayer.HoldItem, true, true)
			    && !AutoPlaceItemInInventory(myPlayer, myPlayer.HoldItem, true)
			    && !AutoPlaceItemInStash(myPlayer, myPlayer.HoldItem, true)) {
				// This can fail for max gold, arena potions and a stash that has been arranged
				// to not have room for the item all 3 cases are extremely unlikely
				app_fatal(_("No room for item"));
			}
			PlaySFX(ItemInvSnds[ItemCAnimTbl[myPlayer.HoldItem._iCurs]]);
		}
		myPlayer.HoldItem.clear();
		NewCursor(CURSOR_HAND);
	}

	IsStashOpen = false;
}

void DoTelekinesis()
{
	if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled())
		NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJT, cursPosition);
	if (pcursitem != -1)
		NetSendCmdGItem(true, CMD_REQUESTAGITEM, *MyPlayer, pcursitem);
	if (pcursmonst != -1) {
		Monster &monter = Monsters[pcursmonst];
		if (!M_Talker(monter) && monter.talkMsg == TEXT_NONE)
			NetSendCmdParam1(true, CMD_KNOCKBACK, pcursmonst);
	}
	NewCursor(CURSOR_HAND);
}

int CalculateGold(Player &player)
{
	int gold = 0;

	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i]._itype == ItemType::Gold)
			gold += player.InvList[i]._ivalue;
	}

	return gold;
}

Size GetInventorySize(const Item &item)
{
	Size size = GetInvItemSize(item._iCurs + CURSOR_FIRSTITEM);

	return { size.width / InventorySlotSizeInPixels.width, size.height / InventorySlotSizeInPixels.height };
}

} // namespace devilution
