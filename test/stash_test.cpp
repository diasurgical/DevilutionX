/**
 * @file stash_test.cpp
 *
 * Tests for the player stash system.
 *
 * These tests verify the functional behaviour of the shared item stash:
 * item placement, removal, page navigation, gold storage, transfer
 * operations between stash and inventory, and the dirty flag.
 *
 * All assertions are on game state (stash contents, grid cells, gold
 * values, dirty flag, inventory contents). No assertions on rendering,
 * pixel positions, or widget layout.
 */

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "inv.h"
#include "items.h"
#include "player.h"
#include "qol/stash.h"

namespace devilution {
namespace {

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class StashTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Start each test with a completely clean stash.
		Stash = {};
		Stash.gold = 0;
		Stash.dirty = false;
	}

	// --- helpers ---

	/** @brief Create a simple 1×1 item (healing potion). */
	static Item MakeSmallItem()
	{
		Item item {};
		InitializeItem(item, IDI_HEAL);
		return item;
	}

	/** @brief Create a sword (larger than 1×1). */
	static Item MakeSword()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iIdentified = true;
		return item;
	}

	/** @brief Create a gold item with the given value. */
	static Item MakeGold(int value)
	{
		Item item {};
		item._itype = ItemType::Gold;
		item._ivalue = value;
		item._iMiscId = IMISC_NONE;
		return item;
	}

	/** @brief Count the number of non-empty cells in a stash grid page. */
	static int CountOccupiedCells(const StashStruct::StashGrid &grid)
	{
		int count = 0;
		for (const auto &row : grid) {
			for (StashStruct::StashCell cell : row) {
				if (cell != 0)
					count++;
			}
		}
		return count;
	}

	/** @brief Fill a stash page completely with 1×1 items. */
	void FillStashPage(unsigned page)
	{
		for (int x = 0; x < 10; x++) {
			for (int y = 0; y < 10; y++) {
				Item item = MakeSmallItem();
				Stash.SetPage(page);
				ASSERT_TRUE(AutoPlaceItemInStash(item, true))
				    << "Failed to place item at logical position on page " << page;
			}
		}
	}
};

// ---------------------------------------------------------------------------
// AutoPlaceItemInStash
// ---------------------------------------------------------------------------

TEST_F(StashTest, PlaceItem_EmptyStash)
{
	Item item = MakeSmallItem();

	bool placed = AutoPlaceItemInStash(item, true);

	EXPECT_TRUE(placed);
	EXPECT_FALSE(Stash.stashList.empty());
	EXPECT_EQ(Stash.stashList.size(), 1u);
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, PlaceItem_DryRunDoesNotMutate)
{
	Item item = MakeSmallItem();

	bool canPlace = AutoPlaceItemInStash(item, false);

	EXPECT_TRUE(canPlace);
	EXPECT_TRUE(Stash.stashList.empty()) << "Dry-run should not add item to stashList";
	EXPECT_FALSE(Stash.dirty) << "Dry-run should not set dirty flag";
}

TEST_F(StashTest, PlaceItem_GridCellOccupied)
{
	Item item = MakeSmallItem();

	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	// The item should occupy at least one cell on the current page.
	const auto &grid = Stash.stashGrids[Stash.GetPage()];
	EXPECT_GT(CountOccupiedCells(grid), 0);
}

TEST_F(StashTest, PlaceItem_MultipleItemsOnSamePage)
{
	Item item1 = MakeSmallItem();
	Item item2 = MakeSmallItem();
	Item item3 = MakeSword();

	EXPECT_TRUE(AutoPlaceItemInStash(item1, true));
	EXPECT_TRUE(AutoPlaceItemInStash(item2, true));
	EXPECT_TRUE(AutoPlaceItemInStash(item3, true));

	EXPECT_EQ(Stash.stashList.size(), 3u);
}

TEST_F(StashTest, PlaceItem_FullPageOverflowsToNextPage)
{
	Stash.SetPage(0);
	Stash.dirty = false;

	FillStashPage(0);

	size_t itemsAfterPage0 = Stash.stashList.size();

	// Page 0 should be completely full now. Placing another item should go to page 1.
	Item overflow = MakeSmallItem();
	Stash.SetPage(0); // Reset to page 0 so AutoPlace starts searching from page 0.
	EXPECT_TRUE(AutoPlaceItemInStash(overflow, true));

	EXPECT_EQ(Stash.stashList.size(), itemsAfterPage0 + 1);

	// The overflow item should be on page 1 (or later), not page 0.
	// Page 0 should still have only the original cells occupied.
	const auto &grid0 = Stash.stashGrids[0];
	EXPECT_EQ(CountOccupiedCells(grid0), 100) << "Page 0 should remain fully occupied";

	// Page 1 should have the overflow item.
	EXPECT_TRUE(Stash.stashGrids.count(1) > 0) << "Page 1 should have been created";
	EXPECT_GT(CountOccupiedCells(Stash.stashGrids[1]), 0) << "Overflow item should be on page 1";
}

TEST_F(StashTest, PlaceItem_SwordOccupiesCorrectArea)
{
	Item sword = MakeSword();
	const Size swordSize = GetInventorySize(sword);

	ASSERT_TRUE(AutoPlaceItemInStash(sword, true));

	const auto &grid = Stash.stashGrids[Stash.GetPage()];
	int occupiedCells = CountOccupiedCells(grid);
	EXPECT_EQ(occupiedCells, swordSize.width * swordSize.height)
	    << "Sword should occupy exactly " << swordSize.width << "×" << swordSize.height << " cells";
}

// ---------------------------------------------------------------------------
// Gold in stash
// ---------------------------------------------------------------------------

TEST_F(StashTest, PlaceGold_AddsToStashGold)
{
	Item gold = MakeGold(5000);

	EXPECT_TRUE(AutoPlaceItemInStash(gold, true));

	EXPECT_EQ(Stash.gold, 5000);
	EXPECT_TRUE(Stash.stashList.empty()) << "Gold should not be added to stashList";
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, PlaceGold_DryRunDoesNotMutate)
{
	Item gold = MakeGold(3000);

	EXPECT_TRUE(AutoPlaceItemInStash(gold, false));

	EXPECT_EQ(Stash.gold, 0) << "Dry-run should not change stash gold";
	EXPECT_FALSE(Stash.dirty);
}

TEST_F(StashTest, PlaceGold_AccumulatesMultipleDeposits)
{
	Item gold1 = MakeGold(1000);
	Item gold2 = MakeGold(2500);

	ASSERT_TRUE(AutoPlaceItemInStash(gold1, true));
	ASSERT_TRUE(AutoPlaceItemInStash(gold2, true));

	EXPECT_EQ(Stash.gold, 3500);
}

TEST_F(StashTest, PlaceGold_RejectsOverflow)
{
	Stash.gold = std::numeric_limits<int>::max() - 100;

	Item gold = MakeGold(200);

	EXPECT_FALSE(AutoPlaceItemInStash(gold, true))
	    << "Should reject gold that would cause integer overflow";
	EXPECT_EQ(Stash.gold, std::numeric_limits<int>::max() - 100)
	    << "Stash gold should be unchanged after rejected deposit";
}

// ---------------------------------------------------------------------------
// RemoveStashItem
// ---------------------------------------------------------------------------

TEST_F(StashTest, RemoveItem_ClearsGridAndList)
{
	Item item = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));
	ASSERT_EQ(Stash.stashList.size(), 1u);

	Stash.dirty = false;
	Stash.RemoveStashItem(0);

	EXPECT_TRUE(Stash.stashList.empty());
	EXPECT_EQ(CountOccupiedCells(Stash.stashGrids[Stash.GetPage()]), 0)
	    << "Grid cells should be cleared after removing item";
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, RemoveItem_LastItemSwap)
{
	// Place two items, then remove the first. The second item should be
	// moved to index 0 in stashList, and grid references updated.
	Item item1 = MakeSmallItem();
	Item item2 = MakeSword();

	ASSERT_TRUE(AutoPlaceItemInStash(item1, true));
	ASSERT_TRUE(AutoPlaceItemInStash(item2, true));
	ASSERT_EQ(Stash.stashList.size(), 2u);

	// Remember the type of the second item.
	const ItemType secondItemType = Stash.stashList[1]._itype;

	Stash.RemoveStashItem(0);

	ASSERT_EQ(Stash.stashList.size(), 1u);
	// The former item at index 1 should now be at index 0.
	EXPECT_EQ(Stash.stashList[0]._itype, secondItemType);

	// Grid should reference the moved item correctly (cell value = index + 1 = 1).
	const auto &grid = Stash.stashGrids[Stash.GetPage()];
	bool foundReference = false;
	for (const auto &row : grid) {
		for (StashStruct::StashCell cell : row) {
			if (cell == 1) { // index 0 + 1
				foundReference = true;
			}
		}
	}
	EXPECT_TRUE(foundReference)
	    << "Grid should have updated references to the swapped item";
}

TEST_F(StashTest, RemoveItem_MiddleOfThree)
{
	// Place three items, remove the middle one. The last item should be
	// swapped into slot 1, and stashList should have size 2.
	Item item1 = MakeSmallItem();
	Item item2 = MakeSmallItem();
	Item item3 = MakeSmallItem();

	ASSERT_TRUE(AutoPlaceItemInStash(item1, true));
	ASSERT_TRUE(AutoPlaceItemInStash(item2, true));
	ASSERT_TRUE(AutoPlaceItemInStash(item3, true));
	ASSERT_EQ(Stash.stashList.size(), 3u);

	Stash.RemoveStashItem(1);

	EXPECT_EQ(Stash.stashList.size(), 2u);
}

// ---------------------------------------------------------------------------
// Page navigation
// ---------------------------------------------------------------------------

TEST_F(StashTest, SetPage_SetsCorrectPage)
{
	Stash.SetPage(5);
	EXPECT_EQ(Stash.GetPage(), 5u);

	Stash.SetPage(42);
	EXPECT_EQ(Stash.GetPage(), 42u);
}

TEST_F(StashTest, SetPage_ClampsToLastPage)
{
	// LastStashPage = 99 (CountStashPages - 1).
	Stash.SetPage(200);
	EXPECT_EQ(Stash.GetPage(), 99u);
}

TEST_F(StashTest, SetPage_SetsDirtyFlag)
{
	Stash.dirty = false;
	Stash.SetPage(3);
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, NextPage_AdvancesByOne)
{
	Stash.SetPage(0);
	Stash.dirty = false;

	Stash.NextPage();
	EXPECT_EQ(Stash.GetPage(), 1u);
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, NextPage_AdvancesByOffset)
{
	Stash.SetPage(5);

	Stash.NextPage(10);
	EXPECT_EQ(Stash.GetPage(), 15u);
}

TEST_F(StashTest, NextPage_ClampsAtLastPage)
{
	Stash.SetPage(98);

	Stash.NextPage(5);
	EXPECT_EQ(Stash.GetPage(), 99u) << "Should clamp to last page, not wrap around";
}

TEST_F(StashTest, NextPage_AlreadyAtLastPage)
{
	Stash.SetPage(99);

	Stash.NextPage();
	EXPECT_EQ(Stash.GetPage(), 99u) << "Should stay at last page";
}

TEST_F(StashTest, PreviousPage_GoesBackByOne)
{
	Stash.SetPage(5);
	Stash.dirty = false;

	Stash.PreviousPage();
	EXPECT_EQ(Stash.GetPage(), 4u);
	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, PreviousPage_GoesBackByOffset)
{
	Stash.SetPage(20);

	Stash.PreviousPage(10);
	EXPECT_EQ(Stash.GetPage(), 10u);
}

TEST_F(StashTest, PreviousPage_ClampsAtPageZero)
{
	Stash.SetPage(2);

	Stash.PreviousPage(5);
	EXPECT_EQ(Stash.GetPage(), 0u) << "Should clamp to page 0, not underflow";
}

TEST_F(StashTest, PreviousPage_AlreadyAtPageZero)
{
	Stash.SetPage(0);

	Stash.PreviousPage();
	EXPECT_EQ(Stash.GetPage(), 0u) << "Should stay at page 0";
}

// ---------------------------------------------------------------------------
// Grid query helpers
// ---------------------------------------------------------------------------

TEST_F(StashTest, GetItemIdAtPosition_EmptyCell)
{
	Stash.SetPage(0);

	StashStruct::StashCell id = Stash.GetItemIdAtPosition({ 0, 0 });
	EXPECT_EQ(id, StashStruct::EmptyCell);
}

TEST_F(StashTest, IsItemAtPosition_EmptyCell)
{
	Stash.SetPage(0);

	EXPECT_FALSE(Stash.IsItemAtPosition({ 0, 0 }));
}

TEST_F(StashTest, GetItemIdAtPosition_OccupiedCell)
{
	Item item = MakeSmallItem();
	Stash.SetPage(0);
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	// The first item should be placed at (0,0) in an empty stash.
	StashStruct::StashCell id = Stash.GetItemIdAtPosition({ 0, 0 });
	EXPECT_NE(id, StashStruct::EmptyCell);
	EXPECT_EQ(id, 0u) << "First item should have stashList index 0";
}

TEST_F(StashTest, IsItemAtPosition_OccupiedCell)
{
	Item item = MakeSmallItem();
	Stash.SetPage(0);
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	EXPECT_TRUE(Stash.IsItemAtPosition({ 0, 0 }));
}

// ---------------------------------------------------------------------------
// TransferItemToInventory
// ---------------------------------------------------------------------------

TEST_F(StashTest, TransferToInventory_Success)
{
	// Clear inventory so there is room.
	StripPlayer();

	Item item = MakeSmallItem();
	Stash.SetPage(0);
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));
	ASSERT_EQ(Stash.stashList.size(), 1u);

	TransferItemToInventory(*MyPlayer, 0);

	EXPECT_TRUE(Stash.stashList.empty()) << "Item should be removed from stash";
	EXPECT_EQ(CountOccupiedCells(Stash.stashGrids[0]), 0)
	    << "Grid should be cleared";

	// Item should now be in the player's inventory.
	bool foundInInventory = false;
	for (int i = 0; i < MyPlayer->_pNumInv; i++) {
		if (!MyPlayer->InvList[i].isEmpty()) {
			foundInInventory = true;
			break;
		}
	}
	EXPECT_TRUE(foundInInventory) << "Item should appear in player inventory";
}

TEST_F(StashTest, TransferToInventory_EmptyCell)
{
	// Transferring EmptyCell should be a no-op.
	TransferItemToInventory(*MyPlayer, StashStruct::EmptyCell);

	// Nothing should crash and stash should remain unchanged.
	EXPECT_TRUE(Stash.stashList.empty());
}

TEST_F(StashTest, TransferToInventory_InventoryFull)
{
	// Fill inventory completely so there's no room.
	ClearInventory();
	for (int i = 0; i < InventoryGridCells; i++) {
		Item filler = MakeSmallItem();
		MyPlayer->InvList[i] = filler;
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = InventoryGridCells;

	Item item = MakeSmallItem();
	Stash.SetPage(0);
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));
	ASSERT_EQ(Stash.stashList.size(), 1u);

	TransferItemToInventory(*MyPlayer, 0);

	// Item should remain in stash because inventory is full.
	EXPECT_EQ(Stash.stashList.size(), 1u)
	    << "Item should remain in stash when inventory is full";
}

// ---------------------------------------------------------------------------
// TransferItemToStash
// ---------------------------------------------------------------------------

TEST_F(StashTest, TransferToStash_Success)
{
	StripPlayer();
	IsStashOpen = true;

	// Place an item in inventory slot 0.
	Item sword = MakeSword();
	int idx = PlaceItemInInventory(sword);
	ASSERT_GE(idx, 0);

	int invLocation = INVITEM_INV_FIRST + idx;

	TransferItemToStash(*MyPlayer, invLocation);

	// Item should now be in stash.
	EXPECT_FALSE(Stash.stashList.empty()) << "Item should appear in stash";

	// Item should be removed from inventory.
	EXPECT_TRUE(MyPlayer->InvList[idx].isEmpty() || MyPlayer->_pNumInv == 0)
	    << "Item should be removed from inventory";
}

TEST_F(StashTest, TransferToStash_InvalidLocation)
{
	// Transferring from location -1 should be a no-op.
	TransferItemToStash(*MyPlayer, -1);

	EXPECT_TRUE(Stash.stashList.empty());
}

// ---------------------------------------------------------------------------
// Dirty flag
// ---------------------------------------------------------------------------

TEST_F(StashTest, DirtyFlag_SetOnPlaceItem)
{
	Stash.dirty = false;

	Item item = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, DirtyFlag_SetOnPlaceGold)
{
	Stash.dirty = false;

	Item gold = MakeGold(100);
	ASSERT_TRUE(AutoPlaceItemInStash(gold, true));

	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, DirtyFlag_SetOnRemoveItem)
{
	Item item = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));
	Stash.dirty = false;

	Stash.RemoveStashItem(0);

	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, DirtyFlag_SetOnPageChange)
{
	Stash.dirty = false;

	Stash.SetPage(1);

	EXPECT_TRUE(Stash.dirty);
}

TEST_F(StashTest, DirtyFlag_NotSetOnDryRun)
{
	Stash.dirty = false;

	Item item = MakeSmallItem();
	AutoPlaceItemInStash(item, false);

	EXPECT_FALSE(Stash.dirty);
}

// ---------------------------------------------------------------------------
// IsStashOpen flag
// ---------------------------------------------------------------------------

TEST_F(StashTest, IsStashOpen_InitiallyClosed)
{
	EXPECT_FALSE(IsStashOpen);
}

TEST_F(StashTest, IsStashOpen_CanBeToggled)
{
	IsStashOpen = true;
	EXPECT_TRUE(IsStashOpen);

	IsStashOpen = false;
	EXPECT_FALSE(IsStashOpen);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(StashTest, PlaceItem_CurrentPagePreferred)
{
	// When the stash is empty, the item should be placed on the current page.
	Stash.SetPage(5);
	Stash.dirty = false;

	Item item = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	EXPECT_TRUE(Stash.stashGrids.count(5) > 0)
	    << "Item should be placed on the current page (5)";
	EXPECT_GT(CountOccupiedCells(Stash.stashGrids[5]), 0);
}

TEST_F(StashTest, PlaceItem_WrapsAroundPages)
{
	// Set page to 99 (last page), fill it, then place another item.
	// It should wrap around to page 0.
	Stash.SetPage(99);
	FillStashPage(99);

	Item overflow = MakeSmallItem();
	Stash.SetPage(99); // Reset to page 99 so search starts there.
	EXPECT_TRUE(AutoPlaceItemInStash(overflow, true));

	// The item should have been placed on page 0 (wrapped around).
	EXPECT_TRUE(Stash.stashGrids.count(0) > 0)
	    << "Item should wrap around to page 0";
	EXPECT_GT(CountOccupiedCells(Stash.stashGrids[0]), 0);
}

TEST_F(StashTest, MultipleItemTypes_CoexistOnSamePage)
{
	Stash.SetPage(0);

	Item potion = MakeSmallItem();
	Item sword = MakeSword();

	ASSERT_TRUE(AutoPlaceItemInStash(potion, true));
	ASSERT_TRUE(AutoPlaceItemInStash(sword, true));

	EXPECT_EQ(Stash.stashList.size(), 2u);

	// Both items should be on page 0.
	const auto &grid = Stash.stashGrids[0];
	const Size swordSize = GetInventorySize(sword);
	int expectedCells = 1 + (swordSize.width * swordSize.height);
	EXPECT_EQ(CountOccupiedCells(grid), expectedCells);
}

TEST_F(StashTest, RemoveItem_ThenPlaceNew)
{
	// Place an item, remove it, then place a new one. The stash should
	// reuse the slot correctly.
	Item item1 = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item1, true));
	ASSERT_EQ(Stash.stashList.size(), 1u);

	Stash.RemoveStashItem(0);
	ASSERT_TRUE(Stash.stashList.empty());

	Item item2 = MakeSword();
	ASSERT_TRUE(AutoPlaceItemInStash(item2, true));
	EXPECT_EQ(Stash.stashList.size(), 1u);
}

TEST_F(StashTest, GoldStorageIndependentOfItems)
{
	// Gold and items use separate storage. Verify they don't interfere.
	Stash.SetPage(0);

	Item gold = MakeGold(5000);
	ASSERT_TRUE(AutoPlaceItemInStash(gold, true));

	Item item = MakeSmallItem();
	ASSERT_TRUE(AutoPlaceItemInStash(item, true));

	EXPECT_EQ(Stash.gold, 5000) << "Gold should be tracked separately";
	EXPECT_EQ(Stash.stashList.size(), 1u) << "Only the non-gold item should be in stashList";
}

} // namespace
} // namespace devilution