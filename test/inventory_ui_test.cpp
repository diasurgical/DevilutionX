/**
 * @file inventory_ui_test.cpp
 *
 * Tests for inventory operations that are not covered by inv_test.cpp.
 *
 * Covers: AutoEquip, AutoPlaceItemInInventory, CanFitItemInInventory,
 * belt placement, RemoveEquipment, ReorganizeInventory, and
 * TransferItemToStash.
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

class InventoryUITest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Each test starts with a completely stripped player and clean stash.
		StripPlayer();
		Stash = {};
		Stash.gold = 0;
		Stash.dirty = false;
	}

	// --- helpers ---

	/** @brief Create a sword (2×1 one-hand weapon, requires 15 Str). */
	static Item MakeSword()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iIdentified = true;
		return item;
	}

	/** @brief Create a healing potion (1×1, beltable). */
	static Item MakePotion()
	{
		Item item {};
		InitializeItem(item, IDI_HEAL);
		return item;
	}

	/** @brief Create a short staff (1×3 two-hand weapon, requires 0 Str). */
	static Item MakeStaff()
	{
		Item item {};
		InitializeItem(item, IDI_SHORTSTAFF);
		item._iIdentified = true;
		return item;
	}

	/**
	 * @brief Fill the entire inventory grid with 1×1 healing potions.
	 *
	 * After this call every one of the 40 inventory cells is occupied.
	 */
	void FillInventory()
	{
		for (int i = 0; i < InventoryGridCells; i++) {
			Item potion = MakePotion();
			ASSERT_TRUE(AutoPlaceItemInInventory(*MyPlayer, potion))
			    << "Failed to place potion at cell " << i;
		}
	}

	/** @brief Fill all 8 belt slots with healing potions. */
	void FillBelt()
	{
		for (int i = 0; i < MaxBeltItems; i++) {
			Item potion = MakePotion();
			ASSERT_TRUE(AutoPlaceItemInBelt(*MyPlayer, potion, /*persistItem=*/true))
			    << "Failed to place potion in belt slot " << i;
		}
	}
};

// ===========================================================================
// AutoEquip tests
// ===========================================================================

TEST_F(InventoryUITest, AutoEquip_SwordGoesToHand)
{
	Item sword = MakeSword();
	sword._iStatFlag = true; // Player meets stat requirements

	bool equipped = AutoEquip(*MyPlayer, sword);

	EXPECT_TRUE(equipped) << "AutoEquip should succeed for a usable sword";
	EXPECT_FALSE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty())
	    << "Sword should be placed in the left hand slot";
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_HAND_LEFT].IDidx, sword.IDidx)
	    << "Equipped item should match the sword we created";
}

TEST_F(InventoryUITest, AutoEquip_ReturnsFalseForUnusableItem)
{
	Item sword = MakeSword();
	sword._iStatFlag = false; // Player does NOT meet stat requirements

	bool equipped = AutoEquip(*MyPlayer, sword);

	EXPECT_FALSE(equipped)
	    << "AutoEquip should return false when _iStatFlag is false";
	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty())
	    << "Left hand slot should remain empty";
	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_HAND_RIGHT].isEmpty())
	    << "Right hand slot should remain empty";
}

TEST_F(InventoryUITest, AutoEquip_FailsWhenSlotOccupied)
{
	// Equip the first sword.
	Item sword1 = MakeSword();
	sword1._iStatFlag = true;
	ASSERT_TRUE(AutoEquip(*MyPlayer, sword1));

	// Also fill the right hand so neither hand slot is free.
	Item sword2 = MakeSword();
	sword2._iStatFlag = true;
	MyPlayer->InvBody[INVLOC_HAND_RIGHT] = sword2;

	// Now try to auto-equip a third sword — both hand slots are occupied,
	// so CanEquip should reject it (AutoEquip does NOT swap).
	Item sword3 = MakeSword();
	sword3._iStatFlag = true;

	bool equipped = AutoEquip(*MyPlayer, sword3);

	EXPECT_FALSE(equipped)
	    << "AutoEquip should return false when all valid body slots are occupied";
}

// ===========================================================================
// AutoPlaceItemInInventory / CanFitItemInInventory tests
// ===========================================================================

TEST_F(InventoryUITest, AutoPlaceItem_EmptyInventory)
{
	Item sword = MakeSword();

	bool placed = AutoPlaceItemInInventory(*MyPlayer, sword);

	EXPECT_TRUE(placed) << "Should be able to place an item in an empty inventory";
	EXPECT_EQ(MyPlayer->_pNumInv, 1)
	    << "Inventory count should be 1 after placing one item";

	// Verify the item is actually stored.
	EXPECT_EQ(MyPlayer->InvList[0].IDidx, sword.IDidx)
	    << "InvList[0] should contain the sword";

	// Verify at least one grid cell is set (non-zero).
	bool gridSet = false;
	for (int i = 0; i < InventoryGridCells; i++) {
		if (MyPlayer->InvGrid[i] != 0) {
			gridSet = true;
			break;
		}
	}
	EXPECT_TRUE(gridSet) << "At least one InvGrid cell should be non-zero";
}

TEST_F(InventoryUITest, AutoPlaceItem_FullInventory)
{
	FillInventory();

	// Inventory is full — a new item should not fit.
	Item extraPotion = MakePotion();
	bool placed = AutoPlaceItemInInventory(*MyPlayer, extraPotion);

	EXPECT_FALSE(placed)
	    << "Should not be able to place an item in a completely full inventory";
}

TEST_F(InventoryUITest, CanFitItem_EmptyInventory)
{
	Item sword = MakeSword();

	EXPECT_TRUE(CanFitItemInInventory(*MyPlayer, sword))
	    << "An empty inventory should have room for a sword";
}

TEST_F(InventoryUITest, CanFitItem_FullInventory)
{
	FillInventory();

	Item extraPotion = MakePotion();

	EXPECT_FALSE(CanFitItemInInventory(*MyPlayer, extraPotion))
	    << "A completely full inventory should report no room";
}

// ===========================================================================
// Belt tests
// ===========================================================================

TEST_F(InventoryUITest, CanBePlacedOnBelt_Potion)
{
	Item potion = MakePotion();

	EXPECT_TRUE(CanBePlacedOnBelt(*MyPlayer, potion))
	    << "A healing potion (1×1) should be placeable on the belt";
}

TEST_F(InventoryUITest, CanBePlacedOnBelt_Sword)
{
	Item sword = MakeSword();

	EXPECT_FALSE(CanBePlacedOnBelt(*MyPlayer, sword))
	    << "A sword (2×1 weapon) should NOT be placeable on the belt";
}

TEST_F(InventoryUITest, AutoPlaceBelt_Success)
{
	Item potion = MakePotion();

	bool placed = AutoPlaceItemInBelt(*MyPlayer, potion, /*persistItem=*/true);

	EXPECT_TRUE(placed) << "Should be able to place a potion in an empty belt";

	// Verify at least one belt slot contains the item.
	bool found = false;
	for (int i = 0; i < MaxBeltItems; i++) {
		if (!MyPlayer->SpdList[i].isEmpty()) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found) << "Potion should appear in one of the belt slots";
}

TEST_F(InventoryUITest, AutoPlaceBelt_Full)
{
	FillBelt();

	Item extraPotion = MakePotion();
	bool placed = AutoPlaceItemInBelt(*MyPlayer, extraPotion, /*persistItem=*/true);

	EXPECT_FALSE(placed)
	    << "Should not be able to place a potion when all 8 belt slots are occupied";
}

// ===========================================================================
// RemoveEquipment test
// ===========================================================================

TEST_F(InventoryUITest, RemoveEquipment_ClearsBodySlot)
{
	// Equip a sword in the left hand.
	Item sword = MakeSword();
	sword._iStatFlag = true;
	ASSERT_TRUE(AutoEquip(*MyPlayer, sword));
	ASSERT_FALSE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty())
	    << "Precondition: left hand should have the sword";

	RemoveEquipment(*MyPlayer, INVLOC_HAND_LEFT, false);

	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty())
	    << "Left hand slot should be empty after RemoveEquipment";
}

// ===========================================================================
// ReorganizeInventory test
// ===========================================================================

TEST_F(InventoryUITest, ReorganizeInventory_DefragmentsGrid)
{
	// Place three potions via AutoPlace so the grid is properly populated.
	Item p1 = MakePotion();
	Item p2 = MakePotion();
	Item p3 = MakePotion();

	ASSERT_TRUE(AutoPlaceItemInInventory(*MyPlayer, p1));
	ASSERT_TRUE(AutoPlaceItemInInventory(*MyPlayer, p2));
	ASSERT_TRUE(AutoPlaceItemInInventory(*MyPlayer, p3));
	ASSERT_EQ(MyPlayer->_pNumInv, 3);

	// Remove the middle item to create a gap.
	MyPlayer->RemoveInvItem(1);
	ASSERT_EQ(MyPlayer->_pNumInv, 2);

	// Reorganize should keep all remaining items and defragment the grid.
	ReorganizeInventory(*MyPlayer);

	EXPECT_EQ(MyPlayer->_pNumInv, 2)
	    << "Item count should be preserved after reorganization";

	// After reorganization, a potion should still fit (there are 38 free cells).
	Item extra = MakePotion();
	EXPECT_TRUE(CanFitItemInInventory(*MyPlayer, extra))
	    << "Should be able to fit another item after reorganization";

	// Verify no InvList entries in the active range are empty.
	for (int i = 0; i < MyPlayer->_pNumInv; i++) {
		EXPECT_FALSE(MyPlayer->InvList[i].isEmpty())
		    << "InvList[" << i << "] should not be empty within active range";
	}
}

// ===========================================================================
// TransferItemToStash tests
// ===========================================================================

TEST_F(InventoryUITest, TransferToStash_FromInventory)
{
	IsStashOpen = true;

	// Place a sword in the inventory.
	Item sword = MakeSword();
	int idx = PlaceItemInInventory(sword);
	ASSERT_GE(idx, 0) << "Failed to place sword in inventory";
	ASSERT_EQ(MyPlayer->_pNumInv, 1);

	int invLocation = INVITEM_INV_FIRST + idx;

	TransferItemToStash(*MyPlayer, invLocation);

	// Item should now be in the stash.
	EXPECT_FALSE(Stash.stashList.empty())
	    << "Stash should contain the transferred item";

	// Item should be removed from inventory.
	EXPECT_EQ(MyPlayer->_pNumInv, 0)
	    << "Inventory should be empty after transferring the only item";
}

TEST_F(InventoryUITest, TransferToStash_InvalidLocation)
{
	IsStashOpen = true;

	size_t stashSizeBefore = Stash.stashList.size();
	int invCountBefore = MyPlayer->_pNumInv;

	// Passing -1 should be a no-op (early return), not a crash.
	TransferItemToStash(*MyPlayer, -1);

	EXPECT_EQ(Stash.stashList.size(), stashSizeBefore)
	    << "Stash should be unchanged after invalid transfer";
	EXPECT_EQ(MyPlayer->_pNumInv, invCountBefore)
	    << "Inventory should be unchanged after invalid transfer";
}

} // namespace
} // namespace devilution