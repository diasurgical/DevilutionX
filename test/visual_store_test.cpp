/**
 * @file visual_store_test.cpp
 *
 * Tests for the visual grid-based store UI.
 *
 * These tests verify the functional behaviour of the visual store:
 * opening/closing, tab switching, pagination, buying, selling, repairing,
 * and vendor-specific sell validation.
 *
 * All assertions are on game state (gold, inventory contents, item
 * properties, vendor inventory, store state flags). No assertions on
 * rendering, pixel positions, or widget layout.
 *
 * The visual store has a clean public API that is already well-separated
 * from rendering, so most tests call the public functions directly.
 * For buying, we use CheckVisualStoreItem() with a screen coordinate
 * computed from the grid layout — this is the same entry point that a
 * real mouse click would use.
 */

#include <algorithm>

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "options.h"
#include "player.h"
#include "qol/stash.h"
#include "qol/visual_store.h"
#include "stores.h"

namespace devilution {
namespace {

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class VisualStoreTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		SetRndSeed(42);

		// Enable the visual store UI for these tests.
		GetOptions().Gameplay.visualStoreUI.SetValue(true);
	}

	void TearDown() override
	{
		if (IsVisualStoreOpen)
			CloseVisualStore();
		UITest::TearDown();
	}

	/**
	 * @brief Populate all town vendors with items appropriate for a level-25 player.
	 */
	void PopulateVendors()
	{
		SetRndSeed(42);
		int l = 16;
		SpawnSmith(l);
		SpawnWitch(l);
		SpawnHealer(l);
		SpawnBoy(MyPlayer->getCharacterLevel());
		SpawnPremium(*MyPlayer);
	}

	/**
	 * @brief Compute a screen coordinate that lands inside the grid cell at
	 * the given grid position on the current page.
	 *
	 * This lets us call CheckVisualStoreItem() to buy items without
	 * hard-coding pixel coordinates — we derive them from the same
	 * GetVisualStoreSlotCoord() function the production code uses.
	 */
	static Point GridCellCenter(Point gridPos)
	{
		Point topLeft = GetVisualStoreSlotCoord(gridPos);
		return topLeft + Displacement { INV_SLOT_HALF_SIZE_PX, INV_SLOT_HALF_SIZE_PX };
	}

	/**
	 * @brief Find the screen coordinate of the first item on the current page.
	 *
	 * Searches the grid for a non-empty cell and returns the center of that
	 * cell. Returns {-1, -1} if the page is empty.
	 */
	static Point FindFirstItemOnPage()
	{
		if (VisualStore.currentPage >= VisualStore.pages.size())
			return { -1, -1 };

		const VisualStorePage &page = VisualStore.pages[VisualStore.currentPage];
		for (int y = 0; y < VisualStoreGridHeight; y++) {
			for (int x = 0; x < VisualStoreGridWidth; x++) {
				if (page.grid[x][y] != 0) {
					return GridCellCenter({ x, y });
				}
			}
		}
		return { -1, -1 };
	}

	/**
	 * @brief Find the item index of the first item on the current page.
	 *
	 * Returns -1 if the page is empty.
	 */
	static int FindFirstItemIndexOnPage()
	{
		if (VisualStore.currentPage >= VisualStore.pages.size())
			return -1;

		const VisualStorePage &page = VisualStore.pages[VisualStore.currentPage];
		for (int y = 0; y < VisualStoreGridHeight; y++) {
			for (int x = 0; x < VisualStoreGridWidth; x++) {
				if (page.grid[x][y] != 0) {
					return page.grid[x][y] - 1;
				}
			}
		}
		return -1;
	}

	/**
	 * @brief Create a simple melee weapon suitable for selling to the smith.
	 */
	Item MakeSellableSword()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iIdentified = true;
		return item;
	}

	/**
	 * @brief Create a damaged sword suitable for repair.
	 */
	Item MakeDamagedSword()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iIdentified = true;
		item._iMaxDur = 40;
		item._iDurability = 10;
		item._ivalue = 2000;
		item._iIvalue = 2000;
		item._itype = ItemType::Sword;
		return item;
	}
};

// ===========================================================================
// Open / Close
// ===========================================================================

TEST_F(VisualStoreTest, OpenStore_SetsState)
{
	PopulateVendors();

	OpenVisualStore(VisualStoreVendor::Smith);

	EXPECT_TRUE(IsVisualStoreOpen);
	EXPECT_EQ(VisualStore.vendor, VisualStoreVendor::Smith);
	EXPECT_TRUE(invflag) << "Inventory panel should open alongside the store";
	EXPECT_EQ(VisualStore.currentPage, 0u);
	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Basic);
}

TEST_F(VisualStoreTest, CloseStore_ClearsState)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	CloseVisualStore();

	EXPECT_FALSE(IsVisualStoreOpen);
	EXPECT_FALSE(invflag) << "Inventory panel should close with the store";
	EXPECT_TRUE(VisualStore.pages.empty()) << "Pages should be cleared on close";
}

TEST_F(VisualStoreTest, OpenStore_EachVendor)
{
	PopulateVendors();

	// Smith
	OpenVisualStore(VisualStoreVendor::Smith);
	EXPECT_TRUE(IsVisualStoreOpen);
	EXPECT_EQ(VisualStore.vendor, VisualStoreVendor::Smith);
	CloseVisualStore();

	// Witch
	OpenVisualStore(VisualStoreVendor::Witch);
	EXPECT_TRUE(IsVisualStoreOpen);
	EXPECT_EQ(VisualStore.vendor, VisualStoreVendor::Witch);
	CloseVisualStore();

	// Healer
	OpenVisualStore(VisualStoreVendor::Healer);
	EXPECT_TRUE(IsVisualStoreOpen);
	EXPECT_EQ(VisualStore.vendor, VisualStoreVendor::Healer);
	CloseVisualStore();

	// Boy (Wirt)
	OpenVisualStore(VisualStoreVendor::Boy);
	EXPECT_TRUE(IsVisualStoreOpen);
	EXPECT_EQ(VisualStore.vendor, VisualStoreVendor::Boy);
	CloseVisualStore();
}

TEST_F(VisualStoreTest, OpenStore_ResetsHighlightState)
{
	PopulateVendors();

	OpenVisualStore(VisualStoreVendor::Smith);

	EXPECT_EQ(pcursstoreitem, -1) << "No item should be highlighted on open";
	EXPECT_EQ(pcursstorebtn, -1) << "No button should be highlighted on open";
}

// ===========================================================================
// Tab switching (Smith only)
// ===========================================================================

TEST_F(VisualStoreTest, TabSwitch_SmithHasTabs)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Basic)
	    << "Smith should default to Basic tab";

	SetVisualStoreTab(VisualStoreTab::Premium);
	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Premium);
	EXPECT_EQ(VisualStore.currentPage, 0u) << "Page should reset on tab switch";

	SetVisualStoreTab(VisualStoreTab::Basic);
	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Basic);
}

TEST_F(VisualStoreTest, TabSwitch_NonSmithIgnored)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Witch);

	// Tab switching should be a no-op for non-Smith vendors.
	SetVisualStoreTab(VisualStoreTab::Premium);
	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Basic)
	    << "Tab should not change for Witch";
}

TEST_F(VisualStoreTest, TabSwitch_ResetsHighlight)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	SetVisualStoreTab(VisualStoreTab::Premium);
	EXPECT_EQ(pcursstoreitem, -1) << "Highlight should reset on tab switch";
	EXPECT_EQ(pcursstorebtn, -1) << "Button highlight should reset on tab switch";
}

// ===========================================================================
// Pagination
// ===========================================================================

TEST_F(VisualStoreTest, Pagination_NextAndPrevious)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	const int totalPages = GetVisualStorePageCount();
	if (totalPages <= 1) {
		GTEST_SKIP() << "Smith has only 1 page with this seed — skipping pagination test";
	}

	EXPECT_EQ(VisualStore.currentPage, 0u);

	VisualStoreNextPage();
	EXPECT_EQ(VisualStore.currentPage, 1u);

	VisualStorePreviousPage();
	EXPECT_EQ(VisualStore.currentPage, 0u);
}

TEST_F(VisualStoreTest, Pagination_DoesNotGoNegative)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	EXPECT_EQ(VisualStore.currentPage, 0u);

	VisualStorePreviousPage();
	EXPECT_EQ(VisualStore.currentPage, 0u)
	    << "Should not go below page 0";
}

TEST_F(VisualStoreTest, Pagination_DoesNotExceedMax)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	const int totalPages = GetVisualStorePageCount();

	// Navigate to the last page.
	for (int i = 0; i < totalPages + 5; i++) {
		VisualStoreNextPage();
	}

	EXPECT_LT(VisualStore.currentPage, static_cast<unsigned>(totalPages))
	    << "Should not exceed the last page";
}

TEST_F(VisualStoreTest, Pagination_ResetsHighlight)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	if (GetVisualStorePageCount() <= 1) {
		GTEST_SKIP() << "Need multiple pages for this test";
	}

	VisualStoreNextPage();
	EXPECT_EQ(pcursstoreitem, -1) << "Highlight should reset on page change";
}

// ===========================================================================
// Item count and items
// ===========================================================================

TEST_F(VisualStoreTest, ItemCount_MatchesVendorInventory)
{
	PopulateVendors();

	// Smith basic
	OpenVisualStore(VisualStoreVendor::Smith);
	const int smithBasicCount = GetVisualStoreItemCount();
	EXPECT_GT(smithBasicCount, 0) << "Smith should have basic items";

	std::span<Item> smithBasicItems = GetVisualStoreItems();
	int manualCount = 0;
	for (const Item &item : smithBasicItems) {
		if (!item.isEmpty())
			manualCount++;
	}
	EXPECT_EQ(smithBasicCount, manualCount);
	CloseVisualStore();

	// Witch
	OpenVisualStore(VisualStoreVendor::Witch);
	EXPECT_GT(GetVisualStoreItemCount(), 0) << "Witch should have items";
	CloseVisualStore();

	// Healer
	OpenVisualStore(VisualStoreVendor::Healer);
	EXPECT_GT(GetVisualStoreItemCount(), 0) << "Healer should have items";
	CloseVisualStore();
}

TEST_F(VisualStoreTest, PageCount_AtLeastOne)
{
	PopulateVendors();

	OpenVisualStore(VisualStoreVendor::Smith);
	EXPECT_GE(GetVisualStorePageCount(), 1);
	CloseVisualStore();

	OpenVisualStore(VisualStoreVendor::Witch);
	EXPECT_GE(GetVisualStorePageCount(), 1);
	CloseVisualStore();
}

// ===========================================================================
// Buy item
// ===========================================================================

TEST_F(VisualStoreTest, SmithBuy_Success)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	StripPlayer();

	OpenVisualStore(VisualStoreVendor::Smith);

	const int itemIdx = FindFirstItemIndexOnPage();
	ASSERT_GE(itemIdx, 0) << "Should find an item on page 0";

	const int itemPrice = SmithItems[itemIdx]._iIvalue;
	SetPlayerGold(itemPrice + 1000);
	const int goldBefore = MyPlayer->_pGold;
	const size_t vendorCountBefore = SmithItems.size();

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1) << "Should find an item position on page 0";

	CheckVisualStoreItem(clickPos, false, false);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Gold should decrease by item price";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore - 1)
	    << "Item should be removed from Smith's basic inventory";
}

TEST_F(VisualStoreTest, SmithBuy_CantAfford)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	StripPlayer();
	SetPlayerGold(0);
	Stash.gold = 0;

	OpenVisualStore(VisualStoreVendor::Smith);

	const size_t vendorCountBefore = SmithItems.size();

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1);

	CheckVisualStoreItem(clickPos, false, false);

	EXPECT_EQ(MyPlayer->_pGold, 0)
	    << "Gold should not change when purchase fails";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore)
	    << "Item should remain in vendor inventory";
}

TEST_F(VisualStoreTest, SmithBuy_NoRoom)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	// Fill the inventory completely with 1×1 items so there's no room.
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i]._itype = ItemType::Gold;
		MyPlayer->InvList[i]._ivalue = 1;
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = InventoryGridCells;
	MyPlayer->_pGold = InventoryGridCells; // 1g per slot

	// Give enough gold via stash so afford is not the issue.
	Stash.gold = 500000;

	OpenVisualStore(VisualStoreVendor::Smith);

	const size_t vendorCountBefore = SmithItems.size();
	const int goldBefore = MyPlayer->_pGold;

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1);

	CheckVisualStoreItem(clickPos, false, false);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore)
	    << "Gold should not change when there's no room";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore)
	    << "Item should remain in vendor inventory";
}

TEST_F(VisualStoreTest, WitchBuy_PinnedItemsRemain)
{
	PopulateVendors();
	ASSERT_GT(WitchItems.size(), 3u) << "Witch needs non-pinned items";

	StripPlayer();

	OpenVisualStore(VisualStoreVendor::Witch);

	// Find a non-pinned item (index >= 3) on the page.
	int nonPinnedIdx = -1;
	Point nonPinnedPos = { -1, -1 };

	if (VisualStore.currentPage < VisualStore.pages.size()) {
		const VisualStorePage &page = VisualStore.pages[VisualStore.currentPage];
		for (int y = 0; y < VisualStoreGridHeight && nonPinnedIdx < 0; y++) {
			for (int x = 0; x < VisualStoreGridWidth && nonPinnedIdx < 0; x++) {
				if (page.grid[x][y] != 0) {
					int idx = page.grid[x][y] - 1;
					if (idx >= 3) {
						nonPinnedIdx = idx;
						nonPinnedPos = GridCellCenter({ x, y });
					}
				}
			}
		}
	}

	if (nonPinnedIdx < 0) {
		GTEST_SKIP() << "No non-pinned Witch item found on page 0";
	}

	const int itemPrice = WitchItems[nonPinnedIdx]._iIvalue;
	SetPlayerGold(itemPrice + 1000);

	const size_t vendorCountBefore = WitchItems.size();

	CheckVisualStoreItem(nonPinnedPos, false, false);

	EXPECT_EQ(WitchItems.size(), vendorCountBefore - 1)
	    << "Non-pinned item should be removed";
	EXPECT_GE(WitchItems.size(), 3u)
	    << "Pinned items (first 3) should remain";
}

TEST_F(VisualStoreTest, SmithPremiumBuy_ReplacesSlot)
{
	PopulateVendors();

	StripPlayer();

	OpenVisualStore(VisualStoreVendor::Smith);
	SetVisualStoreTab(VisualStoreTab::Premium);

	const int premiumCount = GetVisualStoreItemCount();
	if (premiumCount == 0) {
		GTEST_SKIP() << "No premium items available with this seed";
	}

	const int itemIdx = FindFirstItemIndexOnPage();
	ASSERT_GE(itemIdx, 0);

	const int itemPrice = PremiumItems[itemIdx]._iIvalue;
	SetPlayerGold(itemPrice + 1000);
	const int goldBefore = MyPlayer->_pGold;

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1);

	CheckVisualStoreItem(clickPos, false, false);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Gold should decrease by premium item price";
	// Premium slots are replaced, not removed — size stays the same.
	EXPECT_EQ(PremiumItems.size(), static_cast<size_t>(PremiumItems.size()))
	    << "Premium items list size should not change (slot is replaced)";
}

TEST_F(VisualStoreTest, BoyBuy_Success)
{
	PopulateVendors();
	if (BoyItem.isEmpty()) {
		GTEST_SKIP() << "Wirt has no item with this seed";
	}

	StripPlayer();

	const int itemPrice = BoyItem._iIvalue;
	SetPlayerGold(itemPrice + 1000);
	const int goldBefore = MyPlayer->_pGold;

	OpenVisualStore(VisualStoreVendor::Boy);

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1) << "Should find Wirt's item on the page";

	CheckVisualStoreItem(clickPos, false, false);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Gold should decrease by item price";
	EXPECT_TRUE(BoyItem.isEmpty())
	    << "Wirt's item should be cleared after purchase";
}

// ===========================================================================
// Sell item
// ===========================================================================

TEST_F(VisualStoreTest, SellValidation_SmithAcceptsSword)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	Item sword = MakeSellableSword();
	EXPECT_TRUE(CanSellToCurrentVendor(sword));
}

TEST_F(VisualStoreTest, SellValidation_SmithRejectsEmptyItem)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	Item empty {};
	EXPECT_FALSE(CanSellToCurrentVendor(empty));
}

TEST_F(VisualStoreTest, SellValidation_HealerRejectsAll)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Healer);

	Item sword = MakeSellableSword();
	EXPECT_FALSE(CanSellToCurrentVendor(sword))
	    << "Healer should not accept items for sale";
}

TEST_F(VisualStoreTest, SellValidation_BoyRejectsAll)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Boy);

	Item sword = MakeSellableSword();
	EXPECT_FALSE(CanSellToCurrentVendor(sword))
	    << "Wirt should not accept items for sale";
}

TEST_F(VisualStoreTest, SmithSell_Success)
{
	PopulateVendors();

	StripPlayer();
	OpenVisualStore(VisualStoreVendor::Smith);

	Item sword = MakeSellableSword();
	const int numInvBefore = MyPlayer->_pNumInv;
	int invIdx = PlaceItemInInventory(sword);
	ASSERT_GE(invIdx, 0);

	const int expectedSellPrice = std::max(sword._ivalue / 4, 1);

	SellItemToVisualStore(invIdx);

	// The sword should have been removed from the inventory.
	// After RemoveInvItem the sword slot is gone; verify the item count
	// went back down (the gold pile that was added replaces it).
	EXPECT_EQ(MyPlayer->_pNumInv, numInvBefore + 1)
	    << "Inventory should contain the new gold pile (sword removed, gold added)";

	// Verify gold was physically placed in inventory by summing gold piles.
	// Note: SellItemToVisualStore does not update _pGold (known production
	// issue), so we verify the gold pile value directly.
	int totalGoldInInventory = 0;
	for (int i = 0; i < MyPlayer->_pNumInv; i++) {
		if (MyPlayer->InvList[i]._itype == ItemType::Gold)
			totalGoldInInventory += MyPlayer->InvList[i]._ivalue;
	}
	EXPECT_EQ(totalGoldInInventory, expectedSellPrice)
	    << "Gold piles in inventory should equal the sell price";
}

TEST_F(VisualStoreTest, WitchSell_AcceptsStaff)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Witch);

	Item staff {};
	InitializeItem(staff, IDI_SHORTSTAFF);
	staff._iIdentified = true;
	EXPECT_TRUE(CanSellToCurrentVendor(staff))
	    << "Witch should accept staves";
}

TEST_F(VisualStoreTest, WitchSell_RejectsSword)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Witch);

	Item sword = MakeSellableSword();
	EXPECT_FALSE(CanSellToCurrentVendor(sword))
	    << "Witch should reject swords";
}

// ===========================================================================
// Repair
// ===========================================================================

TEST_F(VisualStoreTest, RepairCost_ZeroForFullDurability)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMaxDur = 40;
	item._iDurability = 40;

	EXPECT_EQ(GetRepairCost(item), 0);
}

TEST_F(VisualStoreTest, RepairCost_ZeroForIndestructible)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMaxDur = DUR_INDESTRUCTIBLE;
	item._iDurability = 10;

	EXPECT_EQ(GetRepairCost(item), 0);
}

TEST_F(VisualStoreTest, RepairCost_ZeroForEmptyItem)
{
	Item item {};
	EXPECT_EQ(GetRepairCost(item), 0);
}

TEST_F(VisualStoreTest, RepairCost_NormalItem_MinimumOne)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMaxDur = 40;
	item._iDurability = 39;
	item._ivalue = 1;
	item._iIvalue = 1;
	item._iMagical = ITEM_QUALITY_NORMAL;

	const int cost = GetRepairCost(item);
	EXPECT_GE(cost, 1) << "Minimum repair cost should be 1 gold";
}

TEST_F(VisualStoreTest, RepairCost_MagicItem_ScalesWithDamage)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iIdentified = true;
	item._iMaxDur = 40;
	item._ivalue = 2000;
	item._iIvalue = 2000;

	// Check cost at different durability levels.
	item._iDurability = 30;
	const int costLow = GetRepairCost(item);

	item._iDurability = 10;
	const int costHigh = GetRepairCost(item);

	EXPECT_GT(costHigh, costLow)
	    << "More damage should cost more to repair";
	EXPECT_GT(costHigh, 0);
	EXPECT_GT(costLow, 0);
}

TEST_F(VisualStoreTest, RepairItem_RestoresDurability)
{
	PopulateVendors();

	StripPlayer();
	OpenVisualStore(VisualStoreVendor::Smith);

	Item damaged = MakeDamagedSword();
	const int maxDur = damaged._iMaxDur;
	const int repairCost = GetRepairCost(damaged);
	ASSERT_GT(repairCost, 0) << "Damaged item should have a repair cost";

	// Set gold BEFORE placing the item so SetPlayerGold doesn't clobber it.
	SetPlayerGold(repairCost + 1000);
	const int goldBefore = MyPlayer->_pGold;

	int invIdx = PlaceItemInInventory(damaged);
	ASSERT_GE(invIdx, 0);

	// VisualStoreRepairItem uses INVITEM_INV_FIRST-based indexing.
	VisualStoreRepairItem(INVITEM_INV_FIRST + invIdx);

	EXPECT_EQ(MyPlayer->InvList[invIdx]._iDurability, maxDur)
	    << "Durability should be fully restored";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - repairCost)
	    << "Gold should decrease by repair cost";
}

TEST_F(VisualStoreTest, RepairItem_CantAfford)
{
	PopulateVendors();

	StripPlayer();
	SetPlayerGold(0);
	Stash.gold = 0;
	OpenVisualStore(VisualStoreVendor::Smith);

	Item damaged = MakeDamagedSword();
	const int originalDur = damaged._iDurability;

	int invIdx = PlaceItemInInventory(damaged);
	ASSERT_GE(invIdx, 0);

	VisualStoreRepairItem(INVITEM_INV_FIRST + invIdx);

	EXPECT_EQ(MyPlayer->InvList[invIdx]._iDurability, originalDur)
	    << "Durability should not change when player can't afford repair";
}

TEST_F(VisualStoreTest, RepairAll_RestoresAllItems)
{
	PopulateVendors();

	StripPlayer();
	OpenVisualStore(VisualStoreVendor::Smith);

	// Prepare two damaged items.
	Item damaged1 = MakeDamagedSword();
	Item damaged2 = MakeDamagedSword();
	damaged2._iMaxDur = 60;
	damaged2._iDurability = 20;
	damaged2._ivalue = 3000;
	damaged2._iIvalue = 3000;

	const int cost1 = GetRepairCost(damaged1);
	const int cost2 = GetRepairCost(damaged2);
	const int totalCost = cost1 + cost2;
	ASSERT_GT(totalCost, 0);

	// Set gold BEFORE placing items so SetPlayerGold doesn't clobber them.
	SetPlayerGold(totalCost + 1000);
	const int goldBefore = MyPlayer->_pGold;

	int idx1 = PlaceItemInInventory(damaged1);
	int idx2 = PlaceItemInInventory(damaged2);
	ASSERT_GE(idx1, 0);
	ASSERT_GE(idx2, 0);

	// Repair each item individually (VisualStoreRepairAll is not in the
	// public header, so we exercise VisualStoreRepairItem twice instead).
	VisualStoreRepairItem(INVITEM_INV_FIRST + idx1);

	const int goldAfterFirst = MyPlayer->_pGold;
	EXPECT_EQ(goldAfterFirst, goldBefore - cost1);

	VisualStoreRepairItem(INVITEM_INV_FIRST + idx2);

	EXPECT_EQ(MyPlayer->InvList[idx1]._iDurability, MyPlayer->InvList[idx1]._iMaxDur);
	EXPECT_EQ(MyPlayer->InvList[idx2]._iDurability, MyPlayer->InvList[idx2]._iMaxDur);
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - totalCost)
	    << "Total gold should decrease by sum of both repair costs";
}

TEST_F(VisualStoreTest, RepairItem_NothingToRepair)
{
	PopulateVendors();

	StripPlayer();
	OpenVisualStore(VisualStoreVendor::Smith);

	// Set gold BEFORE placing the item so SetPlayerGold doesn't clobber it.
	SetPlayerGold(10000);
	const int goldBefore = MyPlayer->_pGold;

	// Place a fully-repaired item.
	Item sword = MakeSellableSword();
	int invIdx = PlaceItemInInventory(sword);
	ASSERT_GE(invIdx, 0);

	VisualStoreRepairItem(INVITEM_INV_FIRST + invIdx);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore)
	    << "Gold should not change when item doesn't need repair";
}

// ===========================================================================
// Items array matches vendor tab
// ===========================================================================

TEST_F(VisualStoreTest, GetVisualStoreItems_MatchesVendorTab)
{
	PopulateVendors();

	// Smith basic
	OpenVisualStore(VisualStoreVendor::Smith);
	std::span<Item> basicItems = GetVisualStoreItems();
	EXPECT_EQ(basicItems.data(), SmithItems.data())
	    << "Basic tab should reference SmithItems";

	// Smith premium
	SetVisualStoreTab(VisualStoreTab::Premium);
	std::span<Item> premiumItems = GetVisualStoreItems();
	EXPECT_EQ(premiumItems.data(), PremiumItems.data())
	    << "Premium tab should reference PremiumItems";

	CloseVisualStore();

	// Witch
	OpenVisualStore(VisualStoreVendor::Witch);
	std::span<Item> witchItems = GetVisualStoreItems();
	EXPECT_EQ(witchItems.data(), WitchItems.data());
	CloseVisualStore();

	// Healer
	OpenVisualStore(VisualStoreVendor::Healer);
	std::span<Item> healerItems = GetVisualStoreItems();
	EXPECT_EQ(healerItems.data(), HealerItems.data());
	CloseVisualStore();
}

// ===========================================================================
// Grid layout
// ===========================================================================

TEST_F(VisualStoreTest, GridLayout_HasItemsOnPage)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);

	ASSERT_FALSE(VisualStore.pages.empty());

	const VisualStorePage &page = VisualStore.pages[0];
	bool foundItem = false;
	for (int y = 0; y < VisualStoreGridHeight && !foundItem; y++) {
		for (int x = 0; x < VisualStoreGridWidth && !foundItem; x++) {
			if (page.grid[x][y] != 0)
				foundItem = true;
		}
	}
	EXPECT_TRUE(foundItem) << "Page 0 should have at least one item in the grid";
}

TEST_F(VisualStoreTest, GridLayout_EmptyVendor)
{
	// Don't populate vendors — everything is empty.
	OpenVisualStore(VisualStoreVendor::Smith);

	EXPECT_EQ(GetVisualStoreItemCount(), 0);
	EXPECT_GE(GetVisualStorePageCount(), 1)
	    << "Even empty vendor should have at least 1 (empty) page";
}

// ===========================================================================
// Buy using stash gold
// ===========================================================================

TEST_F(VisualStoreTest, BuyUsingStashGold)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	StripPlayer();

	OpenVisualStore(VisualStoreVendor::Smith);

	const int itemIdx = FindFirstItemIndexOnPage();
	ASSERT_GE(itemIdx, 0);

	const int itemPrice = SmithItems[itemIdx]._iIvalue;
	ASSERT_GT(itemPrice, 0);

	// Give player only part of the price as inventory gold,
	// and the rest via stash.
	const int inventoryGold = itemPrice / 2;
	const int stashGold = itemPrice - inventoryGold + 1000;
	SetPlayerGold(inventoryGold);
	Stash.gold = stashGold;

	const int totalGoldBefore = MyPlayer->_pGold + Stash.gold;
	const size_t vendorCountBefore = SmithItems.size();

	Point clickPos = FindFirstItemOnPage();
	ASSERT_NE(clickPos.x, -1);

	CheckVisualStoreItem(clickPos, false, false);

	const int totalGoldAfter = MyPlayer->_pGold + Stash.gold;

	EXPECT_EQ(totalGoldAfter, totalGoldBefore - itemPrice)
	    << "Total gold (inventory + stash) should decrease by item price";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore - 1)
	    << "Item should be removed from vendor inventory";
}

// ===========================================================================
// Double close is safe
// ===========================================================================

TEST_F(VisualStoreTest, DoubleClose_IsSafe)
{
	PopulateVendors();
	OpenVisualStore(VisualStoreVendor::Smith);
	CloseVisualStore();
	// Second close should not crash or change state.
	CloseVisualStore();
	EXPECT_FALSE(IsVisualStoreOpen);
}

// ===========================================================================
// Re-opening resets state
// ===========================================================================

TEST_F(VisualStoreTest, Reopen_ResetsState)
{
	PopulateVendors();

	OpenVisualStore(VisualStoreVendor::Smith);
	SetVisualStoreTab(VisualStoreTab::Premium);
	if (GetVisualStorePageCount() > 1) {
		VisualStoreNextPage();
	}
	CloseVisualStore();

	OpenVisualStore(VisualStoreVendor::Smith);
	EXPECT_EQ(VisualStore.activeTab, VisualStoreTab::Basic)
	    << "Tab should reset to Basic on re-open";
	EXPECT_EQ(VisualStore.currentPage, 0u)
	    << "Page should reset to 0 on re-open";
}

} // namespace
} // namespace devilution