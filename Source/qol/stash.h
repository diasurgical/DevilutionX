/**
 * @file qol/stash.h
 *
 * Interface of player stash.
 */
#pragma once

#include <cstdint>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "engine/point.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "items.h"

namespace devilution {

class StashStruct {
public:
	using StashCell = uint16_t;
	using StashGrid = std::array<std::array<StashCell, 10>, 10>;
	static constexpr StashCell EmptyCell = -1;

	void RemoveStashItem(StashCell iv);
	ankerl::unordered_dense::map<unsigned, StashGrid> stashGrids;
	std::vector<Item> stashList;
	int gold;
	bool dirty = false;

	unsigned GetPage() const
	{
		return page;
	}

	StashGrid &GetCurrentGrid()
	{
		return stashGrids[GetPage()];
	}

	/**
	 * @brief Returns the 0-based index of the item at the specified position, or EmptyCell if no item occupies that slot
	 * @param gridPosition x,y coordinate of the current stash page
	 * @return a value which can be used to index into stashList or StashStruct::EmptyCell
	 */
	StashCell GetItemIdAtPosition(Point gridPosition)
	{
		// Because StashCell is an unsigned type we can let this underflow
		return GetCurrentGrid()[gridPosition.x][gridPosition.y] - 1;
	}

	bool IsItemAtPosition(Point gridPosition)
	{
		return GetItemIdAtPosition(gridPosition) != EmptyCell;
	}

	void SetPage(unsigned newPage);
	void NextPage(unsigned offset = 1);
	void PreviousPage(unsigned offset = 1);

	/** @brief Updates _iStatFlag for all stash items. */
	void RefreshItemStatFlags();

private:
	/** Current Page */
	unsigned page;
};

constexpr Point InvalidStashPoint { -1, -1 };

extern bool IsStashOpen;
extern StashStruct Stash;

extern bool IsWithdrawGoldOpen;
extern int WithdrawGoldValue;

inline constexpr Size StashGridSize { 10, 10 };
inline constexpr PointsInRectangle<int> StashGridRange { { { 0, 0 }, StashGridSize } };

Point GetStashSlotCoord(Point slot);
void InitStash();
void FreeStashGFX();
void TransferItemToInventory(Player &player, uint16_t itemId);
/**
 * @brief Render the inventory panel to the given buffer.
 */
void DrawStash(const Surface &out);
void CheckStashItem(Point mousePosition, bool isShiftHeld = false, bool isCtrlHeld = false);
bool UseStashItem(uint16_t cii);
uint16_t CheckStashHLight(Point mousePosition);
void CheckStashButtonRelease(Point mousePosition);
void CheckStashButtonPress(Point mousePosition);

void StartGoldWithdraw();
void WithdrawGoldKeyPress(SDL_Keycode vkey);
void DrawGoldWithdraw(const Surface &out);
void CloseGoldWithdraw();
bool HandleGoldWithdrawTextInputEvent(const SDL_Event &event);

/**
 * @brief Checks whether the given item can be placed on the specified player's stash.
 * If 'persistItem' is 'True', the item is also placed in the inventory.
 * @param player The player to check.
 * @param item The item to be checked.
 * @param persistItem Pass 'True' to actually place the item in the inventory. The default is 'False'.
 * @return 'True' in case the item can be placed on the player's inventory and 'False' otherwise.
 */
bool AutoPlaceItemInStash(Player &player, const Item &item, bool persistItem);

} // namespace devilution
