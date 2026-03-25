/**
 * @file inv.h
 *
 * Interface of player inventory.
 */
#pragma once

#include <cstdint>

#include "engine/palette.h"
#include "engine/point.hpp"
#include "inv_iterators.hpp"
#include "items.h"
#include "player.h"
#include "utils/algorithm/container.hpp"

namespace devilution {

#define INV_SLOT_SIZE_PX 28
#define INV_SLOT_HALF_SIZE_PX (INV_SLOT_SIZE_PX / 2)
constexpr Size InventorySizeInSlots { 10, 4 };
#define INV_ROW_SLOT_SIZE InventorySizeInSlots.width
constexpr Size InventorySlotSizeInPixels { INV_SLOT_SIZE_PX };

enum inv_item : int8_t {
	// clang-format off
	INVITEM_HEAD       = 0,
	INVITEM_RING_LEFT  = 1,
	INVITEM_RING_RIGHT = 2,
	INVITEM_AMULET     = 3,
	INVITEM_HAND_LEFT  = 4,
	INVITEM_HAND_RIGHT = 5,
	INVITEM_CHEST      = 6,
	INVITEM_INV_FIRST  = 7,
	INVITEM_INV_LAST   = 46,
	INVITEM_BELT_FIRST = 47,
	INVITEM_BELT_LAST  = 54,
	// clang-format on
};

/**
 * identifiers for each of the inventory squares
 * @see InvRect
 */
enum inv_xy_slot : uint8_t {
	// clang-format off
	SLOTXY_HEAD           = 0,
	SLOTXY_EQUIPPED_FIRST = SLOTXY_HEAD,
	SLOTXY_RING_LEFT      = 1,
	SLOTXY_RING_RIGHT     = 2,
	SLOTXY_AMULET         = 3,
	SLOTXY_HAND_LEFT      = 4,
	SLOTXY_HAND_RIGHT     = 5,
	SLOTXY_CHEST          = 6,
	SLOTXY_EQUIPPED_LAST  = SLOTXY_CHEST,

	// regular inventory
	SLOTXY_INV_FIRST      = 7,
	SLOTXY_INV_ROW1_FIRST = SLOTXY_INV_FIRST,
	SLOTXY_INV_ROW1_LAST  = 16,
	SLOTXY_INV_ROW2_FIRST = 17,
	SLOTXY_INV_ROW2_LAST  = 26,
	SLOTXY_INV_ROW3_FIRST = 27,
	SLOTXY_INV_ROW3_LAST  = 36,
	SLOTXY_INV_ROW4_FIRST = 37,
	SLOTXY_INV_ROW4_LAST  = 46,
	SLOTXY_INV_LAST       = SLOTXY_INV_ROW4_LAST,

	// belt items
	SLOTXY_BELT_FIRST     = 47,
	SLOTXY_BELT_LAST      = 54,
	NUM_XY_SLOTS          = 55
	// clang-format on
};

enum item_color : uint8_t {
	// clang-format off
	ICOL_YELLOW = PAL16_YELLOW + 5,
	ICOL_WHITE  = PAL16_GRAY   + 5,
	ICOL_BLUE   = PAL16_BLUE   + 5,
	ICOL_RED    = PAL16_RED    + 5,
	// clang-format on
};

extern bool invflag;
extern const Rectangle InvRect[NUM_XY_SLOTS];

void InvDrawSlotBack(const Surface &out, Point targetPosition, Size size, item_quality itemQuality);
/**
 * @brief Checks whether the given item can be placed on the belt. Takes item size as well as characteristics into account. Items
 * that cannot be placed on the belt have to be placed in the inventory instead.
 * @param item The item to be checked.
 * @return 'True' in case the item can be placed on the belt and 'False' otherwise.
 */
bool CanBePlacedOnBelt(const Player &player, const Item &item);

/**
 * @brief Function type which performs an operation on the given item.
 */
using ItemFunc = void (*)(Item &);

void CloseInventory();
void CloseStash();
void FreeInvGFX();
void InitInv();

/**
 * @brief Render the inventory panel to the given buffer.
 */
void DrawInv(const Surface &out);

void DrawInvBelt(const Surface &out);

/**
 * @brief Removes equipment from the specified location on the player's body.
 * @param player The player from which equipment will be removed.
 * @param bodyLocation The location from which equipment will be removed.
 * @param hiPri Priority of the network message to sync player equipment.
 */
void RemoveEquipment(Player &player, inv_body_loc bodyLocation, bool hiPri);

/**
 * @brief Checks whether or not auto-equipping behavior is enabled for the given player and item.
 * @param player The player to check.
 * @param item The item to check.
 * @return 'True' if auto-equipping behavior is enabled for the player and item and 'False' otherwise.
 */
bool AutoEquipEnabled(const Player &player, const Item &item);

/**
 * @brief Automatically attempts to equip the specified item in the most appropriate location in the player's body.
 * @note On success, this will broadcast an equipment_change event to let other players know about the equipment change.
 * @param player The player whose inventory will be checked for compatibility with the item.
 * @param item The item to equip.
 * @param persistItem Indicates whether or not the item should be persisted in the player's body. Pass 'False' to check
 * whether the player can equip the item but you don't want the item to actually be equipped. 'True' by default.
 * @param sendNetworkMessage Set to true if you want an equip sound and network message to be generated if the equipment
 * changes. Should only be set if a local player is equipping an item in a play session (not when creating a new game)
 * @return 'True' if the item was equipped and 'False' otherwise.
 */
bool AutoEquip(Player &player, const Item &item, bool persistItem = true, bool sendNetworkMessage = false);

/**
 * @brief Checks whether the given item can be placed on the specified player's inventory.
 * @param player The player whose inventory will be checked.
 * @param item The item to be checked.
 * @return 'True' in case the item can be placed on the player's inventory and 'False' otherwise.
 */
bool CanFitItemInInventory(const Player &player, const Item &item);

/**
 * @brief Attempts to place the given item in the specified player's inventory.
 * @param player The player whose inventory will be used.
 * @param item The item to be placed.
 * @param sendNetworkMessage Set to true if you want a network message to be generated if the item is persisted.
 * Should only be set if a local player is placing an item in a play session (not when creating a new game)
 * @return 'True' if the item was placed on the player's inventory and 'False' otherwise.
 */
bool AutoPlaceItemInInventory(Player &player, const Item &item, bool sendNetworkMessage = false);

/**
 * @brief Checks whether the given item can be placed on the specified player's belt. Returns 'True' when the item can be placed
 * on belt slots and the player has at least one empty slot in his belt.
 * If 'persistItem' is 'True', the item is also placed in the belt.
 * @param player The player on whose belt will be checked.
 * @param item The item to be checked.
 * @param persistItem Pass 'True' to actually place the item in the belt. The default is 'False'.
 * @param sendNetworkMessage Set to true if you want a network message to be generated if the item is persisted.
 * Should only be set if a local player is placing an item in a play session (not when creating a new game)
 * @return 'True' in case the item can be placed on the player's belt and 'False' otherwise.
 */
bool AutoPlaceItemInBelt(Player &player, const Item &item, bool persistItem = false, bool sendNetworkMessage = false);

/**
 * @brief Sort player inventory.
 */
void ReorganizeInventory(Player &player);

/**
 * @brief Calculate the maximum additional gold that may fit in the user's inventory
 */
int RoomForGold();

/**
 * @return The leftover amount that didn't fit, if any
 */
int AddGoldToInventory(Player &player, int value);
bool GoldAutoPlace(Player &player, Item &goldStack);
void CheckInvSwap(Player &player, inv_body_loc bLoc);
void inv_update_rem_item(Player &player, inv_body_loc iv);
void CheckInvSwap(Player &player, const Item &item, int invGridIndex);
void CheckInvRemove(Player &player, int invGridIndex);
void TransferItemToStash(Player &player, int location);
void CheckInvItem(bool isShiftHeld = false, bool isCtrlHeld = false);

/**
 * Check for interactions with belt
 */
void CheckInvScrn(bool isShiftHeld, bool isCtrlHeld);
void InvGetItem(Player &player, int ii);

/**
 * @brief Returns the first free space that can take an item preferencing tiles in front of the current position
 *
 * The search starts with the adjacent tile in the desired direction and alternates sides until it ends up checking the
 * opposite tile, before finally checking the origin tile
 *
 * @param origin center tile of the search space
 * @param facing direction of the adjacent tile to check first
 * @return the first valid point or an empty optional
 */
std::optional<Point> FindAdjacentPositionForItem(Point origin, Direction facing);
void AutoGetItem(Player &player, Item *itemPointer, int ii);

/**
 * @brief Searches for a dropped item with the same type/createInfo/seed
 * @param iseed The value used to initialise the RNG when generating the item
 * @param idx The overarching type of the target item
 * @param ci Flags used to describe the specific subtype of the target item
 * @return An index into ActiveItems or -1 if no matching item was found
 */
int FindGetItem(uint32_t iseed, _item_indexes idx, uint16_t ci);
void SyncGetItem(Point position, uint32_t iseed, _item_indexes idx, uint16_t ci);

/**
 * @brief Checks if the tile has room for an item
 * @param position tile coordinates
 * @return True if the space is free of obstructions, false if blocked
 */
bool CanPut(Point position);

int ClampDurability(const Item &item, int durability);
int16_t ClampToHit(const Item &item, int16_t toHit);
uint8_t ClampMaxDam(const Item &item, uint8_t maxDam);
int SyncDropItem(Point position, _item_indexes idx, uint16_t icreateinfo, int iseed, int id, int dur, int mdur, int ch, int mch, int ivalue, uint32_t ibuff, int toHit, int maxDam);
int SyncDropEar(Point position, uint16_t icreateinfo, uint32_t iseed, uint8_t cursval, std::string_view heroname);
int8_t CheckInvHLight();
bool CanUseScroll(Player &player, SpellID spell);
void ConsumeStaffCharge(Player &player);
bool CanUseStaff(Player &player, SpellID spellId);
Item &GetInventoryItem(Player &player, int location);
bool UseInvItem(int cii);
void DoTelekinesis();
int CalculateGold(Player &player);

/**
 * @brief Gets the size, in inventory cells, of the given item.
 * @param item The item whose size is to be determined.
 * @return The size, in inventory cells, of the item.
 */
Size GetInventorySize(const Item &item);

/**
 * @brief Checks whether the player has an inventory item matching the predicate.
 */
template <typename Predicate>
bool HasInventoryItem(const Player &player, Predicate &&predicate)
{
	const InventoryPlayerItemsRange items { player };
	return c_find_if(items, std::forward<Predicate>(predicate)) != items.end();
}

/**
 * @brief Checks whether the player has a belt item matching the predicate.
 */
template <typename Predicate>
bool HasBeltItem(const Player &player, Predicate &&predicate)
{
	const BeltPlayerItemsRange items { player };
	return c_find_if(items, std::forward<Predicate>(predicate)) != items.end();
}

/**
 * @brief Checks whether the player has an inventory or a belt item matching the predicate.
 */
template <typename Predicate>
bool HasInventoryOrBeltItem(const Player &player, Predicate &&predicate)
{
	return HasInventoryItem(player, predicate) || HasBeltItem(player, predicate);
}

/**
 * @brief Checks whether the player has an inventory item with the given ID (IDidx).
 */
inline bool HasInventoryItemWithId(const Player &player, _item_indexes id)
{
	return HasInventoryItem(player, [id](const Item &item) {
		return item.IDidx == id;
	});
}

/**
 * @brief Checks whether the player has a belt item with the given ID (IDidx).
 */
inline bool HasBeltItemWithId(const Player &player, _item_indexes id)
{
	return HasBeltItem(player, [id](const Item &item) {
		return item.IDidx == id;
	});
}

/**
 * @brief Checks whether the player has an inventory or a belt item with the given ID (IDidx).
 */
inline bool HasInventoryOrBeltItemWithId(const Player &player, _item_indexes id)
{
	return HasInventoryItemWithId(player, id) || HasBeltItemWithId(player, id);
}

/**
 * @brief Removes the first inventory item matching the predicate.
 *
 * @return Whether an item was found and removed.
 */
template <typename Predicate>
bool RemoveInventoryItem(Player &player, Predicate &&predicate)
{
	const InventoryPlayerItemsRange items { player };
	const auto it = c_find_if(items, std::forward<Predicate>(predicate));
	if (it == items.end())
		return false;
	player.RemoveInvItem(static_cast<int>(it.index()));
	return true;
}

/**
 * @brief Removes the first belt item matching the predicate.
 *
 * @return Whether an item was found and removed.
 */
template <typename Predicate>
bool RemoveBeltItem(Player &player, Predicate &&predicate)
{
	const BeltPlayerItemsRange items { player };
	const auto it = c_find_if(items, std::forward<Predicate>(predicate));
	if (it == items.end())
		return false;
	player.RemoveSpdBarItem(static_cast<int>(it.index()));
	return true;
}

/**
 * @brief Removes the first inventory or belt item matching the predicate.
 *
 * @return Whether an item was found and removed.
 */
template <typename Predicate>
bool RemoveInventoryOrBeltItem(Player &player, Predicate &&predicate)
{
	return RemoveInventoryItem(player, predicate) || RemoveBeltItem(player, predicate);
}

/**
 * @brief Removes the first inventory item with the given id (IDidx).
 *
 * @return Whether an item was found and removed.
 */
inline bool RemoveInventoryItemById(Player &player, _item_indexes id)
{
	return RemoveInventoryItem(player, [id](const Item &item) {
		return item.IDidx == id;
	});
}

/**
 * @brief Removes the first belt item with the given id (IDidx).
 *
 * @return Whether an item was found and removed.
 */
inline bool RemoveBeltItemById(Player &player, _item_indexes id)
{
	return RemoveBeltItem(player, [id](const Item &item) {
		return item.IDidx == id;
	});
}

/**
 * @brief Removes the first inventory or belt item with the given id (IDidx).
 *
 * @return Whether an item was found and removed.
 */
inline bool RemoveInventoryOrBeltItemById(Player &player, _item_indexes id)
{
	return RemoveInventoryItemById(player, id) || RemoveBeltItemById(player, id);
}

/**
 * @brief Removes the first inventory or belt scroll with the player's current spell.
 */
void ConsumeScroll(Player &player);

void CheckInvCut(Player &player, Point cursorPosition, bool automaticMove, bool dropItem);

void CheckInvPaste(Player &player, Point cursorPosition);

/* data */

} // namespace devilution
