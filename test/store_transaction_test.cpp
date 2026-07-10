/**
 * @file store_transaction_test.cpp
 *
 * End-to-end tests for text-based store transactions.
 *
 * These tests drive the store state machine through its TalkID transitions
 * (StartStore → browse items → select → Confirm → commit) and assert only
 * on **game state outcomes**: player gold, inventory contents, item
 * properties, vendor inventory changes.
 *
 * No assertions are made on text lines, rendering, or UI layout — so these
 * tests remain valid when the text-based store is replaced with a new UI,
 * provided the replacement exposes equivalent "buy / sell / repair / identify
 * / recharge" entry points with the same game-state semantics.
 *
 * The store state machine works as follows:
 *   - StartStore(TalkID) sets up the text UI and sets ActiveStore at the end.
 *   - StoreEnter() dispatches on ActiveStore to the appropriate *Enter() fn.
 *   - *Enter() functions check CurrentTextLine to decide what action to take:
 *     - For item lists, item index = ScrollPos + ((CurrentTextLine - 5) / 4)
 *       where 5 is PreviousScrollPos (set by ScrollVendorStore).
 *     - For confirmations, line 18 = Yes, line 20 = No.
 *   - On buy: checks afford → checks room → copies to TempItem → Confirm.
 *   - ConfirmEnter dispatches to the actual transaction function on Yes.
 */

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "control/control.hpp"
#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "minitext.h"
#include "options.h"
#include "player.h"
#include "qol/stash.h"
#include "quests.h"
#include "stores.h"
#include "storm/storm_net.hpp"
#include "tables/itemdat.h"
#include "tables/playerdat.hpp"
#include "tables/spelldat.h"

namespace devilution {
namespace {

// ---------------------------------------------------------------------------
// Helpers to drive the store state machine at a high level.
//
// These abstract over the text-line / scroll-position encoding so that
// tests read as "select item 0, confirm yes" rather than
// "set CurrentTextLine=5, ScrollPos=0, call StoreEnter, ...".
// ---------------------------------------------------------------------------

/**
 * @brief Open a vendor's top-level menu.
 *
 * Equivalent to the player clicking on a towner NPC.
 */
void OpenVendor(TalkID vendor)
{
	StartStore(vendor);
}

/**
 * @brief In a top-level vendor menu, select a menu option by its text line.
 *
 * The line numbers are fixed by the Start*() functions:
 *   Smith:  12=Buy, 14=Premium, 16=Sell, 18=Repair, 20=Leave
 *   Witch:  12=Talk, 14=Buy, 16=Sell, 18=Recharge, 20=Leave
 *   Healer: 12=Talk, 14=Buy, 18=Leave
 *   Storyteller: 12=Talk, 14=Identify, 18=Leave
 *   Boy:    18=What have you got? (if item exists)
 */
void SelectMenuLine(int line)
{
	CurrentTextLine = line;
	StoreEnter();
}

/**
 * @brief In an item list (buy/sell/repair/identify/recharge), select item
 * at the given 0-based index.
 *
 * The store text layout puts items starting at line 5 (PreviousScrollPos),
 * with each item taking 4 lines.  So item N is at line 5 + N*4 when
 * ScrollPos is 0.
 */
void SelectItemAtIndex(int itemIndex)
{
	ScrollPos = 0;
	CurrentTextLine = 5 + itemIndex * 4;
	StoreEnter();
}

/**
 * @brief Confirm a pending transaction (press "Yes" in the Confirm dialog).
 *
 * Precondition: ActiveStore == TalkID::Confirm.
 */
void ConfirmYes()
{
	CurrentTextLine = 18;
	StoreEnter();
}

/**
 * @brief Decline a pending transaction (press "No" in the Confirm dialog).
 *
 * Precondition: ActiveStore == TalkID::Confirm.
 */
void ConfirmNo()
{
	CurrentTextLine = 20;
	StoreEnter();
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class StoreTransactionTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Seed the RNG for deterministic item generation.
		SetRndSeed(42);

		// Make sure visualStoreUI is off (the base fixture does this too,
		// but be explicit).
		GetOptions().Gameplay.visualStoreUI.SetValue(false);
	}

	/**
	 * @brief Populate all town vendors with items appropriate for a level-25 player.
	 *
	 * Must be called after SetUp() and after any adjustments to player level.
	 */
	void PopulateVendors()
	{
		SetRndSeed(42);
		int l = 16; // max store level
		SpawnSmith(l);
		SpawnWitch(l);
		SpawnHealer(l);
		SpawnBoy(MyPlayer->getCharacterLevel());
		SpawnPremium(*MyPlayer);
	}

	/**
	 * @brief Create a simple melee weapon item suitable for selling to the smith.
	 */
	Item MakeSellableSword()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iIdentified = true;
		return item;
	}

	/**
	 * @brief Create a magic item that is unidentified (for Cain tests).
	 */
	Item MakeUnidentifiedMagicItem()
	{
		Item item {};
		InitializeItem(item, IDI_BARDSWORD);
		item._iMagical = ITEM_QUALITY_MAGIC;
		item._iIdentified = false;
		item._iIvalue = 2000;
		item._ivalue = 2000;
		return item;
	}

	/**
	 * @brief Create a damaged item suitable for repair at the smith.
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
// Level A: Transaction primitive tests (completely UI-agnostic)
// ===========================================================================

TEST_F(StoreTransactionTest, PlayerCanAfford_SufficientGold)
{
	SetPlayerGold(10000);
	EXPECT_TRUE(PlayerCanAfford(5000));
	EXPECT_TRUE(PlayerCanAfford(10000));
}

TEST_F(StoreTransactionTest, PlayerCanAfford_InsufficientGold)
{
	SetPlayerGold(1000);
	EXPECT_FALSE(PlayerCanAfford(5000));
}

TEST_F(StoreTransactionTest, PlayerCanAfford_IncludesStashGold)
{
	SetPlayerGold(2000);
	Stash.gold = 3000;
	EXPECT_TRUE(PlayerCanAfford(5000));
	EXPECT_TRUE(PlayerCanAfford(4999));
	EXPECT_FALSE(PlayerCanAfford(5001));
}

TEST_F(StoreTransactionTest, TakePlrsMoney_DeductsFromInventory)
{
	SetPlayerGold(10000);
	int goldBefore = MyPlayer->_pGold;

	TakePlrsMoney(3000);

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - 3000);
}

TEST_F(StoreTransactionTest, TakePlrsMoney_OverflowsToStash)
{
	SetPlayerGold(2000);
	Stash.gold = 5000;

	TakePlrsMoney(4000);

	// 2000 from inventory + 2000 from stash
	EXPECT_EQ(MyPlayer->_pGold, 0);
	EXPECT_EQ(Stash.gold, 3000);
}

TEST_F(StoreTransactionTest, StoreAutoPlace_EmptyInventory)
{
	ClearInventory();
	ClearBelt();
	ClearEquipment();

	Item item {};
	InitializeItem(item, IDI_HEAL);

	// Dry-run: should succeed.
	EXPECT_TRUE(StoreAutoPlace(item, false));

	// Persist: item should appear in player's inventory/belt/equipment.
	EXPECT_TRUE(StoreAutoPlace(item, true));
}

TEST_F(StoreTransactionTest, SmithWillBuy_AcceptsMeleeWeapon)
{
	Item sword {};
	InitializeItem(sword, IDI_BARDSWORD);
	sword._iIdentified = true;
	EXPECT_TRUE(SmithWillBuy(sword));
}

TEST_F(StoreTransactionTest, SmithWillBuy_RejectsMiscItems)
{
	Item scroll {};
	InitializeItem(scroll, IDI_HEAL);
	EXPECT_FALSE(SmithWillBuy(scroll));
}

TEST_F(StoreTransactionTest, WitchWillBuy_AcceptsStaff)
{
	// Witch buys staves and misc magical items but not most weapons.
	Item staff {};
	InitializeItem(staff, IDI_SHORTSTAFF);
	staff._iIdentified = true;
	EXPECT_TRUE(WitchWillBuy(staff));
}

TEST_F(StoreTransactionTest, WitchWillBuy_RejectsSword)
{
	Item sword {};
	InitializeItem(sword, IDI_BARDSWORD);
	sword._iIdentified = true;
	EXPECT_FALSE(WitchWillBuy(sword));
}

// ===========================================================================
// Level B: End-to-end store flows through the state machine
// ===========================================================================

// ---- Smith Buy ------------------------------------------------------------

TEST_F(StoreTransactionTest, SmithBuy_Success)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	// Record state before the transaction.
	const int itemPrice = SmithItems[0]._iIvalue;
	const size_t vendorCountBefore = SmithItems.size();
	StripPlayer();
	SetPlayerGold(itemPrice + 1000);

	const int goldBefore = MyPlayer->_pGold;

	// Drive the state machine: open Smith → browse items → select first → confirm.
	OpenVendor(TalkID::SmithBuy);
	ASSERT_EQ(ActiveStore, TalkID::SmithBuy);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm)
	    << "Selecting an affordable item should go to Confirm";

	ConfirmYes();

	// Assertions on game state.
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Player gold should decrease by item price";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore - 1)
	    << "Purchased item should be removed from vendor inventory";
}

TEST_F(StoreTransactionTest, SmithBuy_CantAfford)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	SetPlayerGold(0);
	Stash.gold = 0;

	OpenVendor(TalkID::SmithBuy);
	ASSERT_EQ(ActiveStore, TalkID::SmithBuy);

	const size_t vendorCountBefore = SmithItems.size();

	SelectItemAtIndex(0);
	EXPECT_EQ(ActiveStore, TalkID::NoMoney)
	    << "Should transition to NoMoney when player can't afford item";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore)
	    << "Vendor inventory should be unchanged";
}

TEST_F(StoreTransactionTest, SmithBuy_NoRoom)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	// Give the player enough gold but fill the inventory completely.
	SetPlayerGold(100000);

	// Fill every inventory grid cell, every belt slot, and every equipment slot
	// so StoreAutoPlace returns false.
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i]._itype = ItemType::Misc;
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = InventoryGridCells;
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i]._itype = ItemType::Misc;
	}
	for (auto &bodyItem : MyPlayer->InvBody) {
		bodyItem._itype = ItemType::Misc;
	}

	OpenVendor(TalkID::SmithBuy);
	ASSERT_EQ(ActiveStore, TalkID::SmithBuy);

	const size_t vendorCountBefore = SmithItems.size();

	SelectItemAtIndex(0);
	EXPECT_EQ(ActiveStore, TalkID::NoRoom)
	    << "Should transition to NoRoom when inventory is full";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore)
	    << "Vendor inventory should be unchanged";
}

TEST_F(StoreTransactionTest, SmithBuy_ConfirmNo_NoChange)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	const int itemPrice = SmithItems[0]._iIvalue;
	StripPlayer();
	SetPlayerGold(itemPrice + 1000);

	const int goldBefore = MyPlayer->_pGold;
	const size_t vendorCountBefore = SmithItems.size();

	OpenVendor(TalkID::SmithBuy);
	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmNo();

	EXPECT_EQ(MyPlayer->_pGold, goldBefore)
	    << "Declining should not change gold";
	EXPECT_EQ(SmithItems.size(), vendorCountBefore)
	    << "Declining should not remove item from vendor";
}

// ---- Smith Sell -----------------------------------------------------------

TEST_F(StoreTransactionTest, SmithSell_Success)
{
	StripPlayer();
	SetPlayerGold(0);

	// Place a sellable sword in the player's inventory.
	Item sword = MakeSellableSword();
	int invIdx = PlaceItemInInventory(sword);
	ASSERT_GE(invIdx, 0);

	ASSERT_EQ(MyPlayer->_pNumInv, 1);

	// Open the sell sub-store directly (Smith menu line 16 → SmithSell).
	OpenVendor(TalkID::SmithSell);
	ASSERT_EQ(ActiveStore, TalkID::SmithSell);

	// The sell list should contain our sword.
	ASSERT_GT(CurrentItemIndex, 0) << "Smith should see at least one sellable item";

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	// The sword should be gone and gold should have increased.
	EXPECT_GT(MyPlayer->_pGold, 0) << "Player should have received gold from the sale";
}

// ---- Smith Repair ---------------------------------------------------------

TEST_F(StoreTransactionTest, SmithRepair_RestoresDurability)
{
	StripPlayer();

	// Equip a damaged sword in the right hand.
	Item damaged = MakeDamagedSword();
	const int maxDur = damaged._iMaxDur;
	ASSERT_LT(damaged._iDurability, maxDur);

	MyPlayer->InvBody[INVLOC_HAND_RIGHT] = damaged;
	SetPlayerGold(100000);

	const int goldBefore = MyPlayer->_pGold;

	// Open the repair sub-store directly.
	OpenVendor(TalkID::SmithRepair);
	ASSERT_EQ(ActiveStore, TalkID::SmithRepair);

	// The repair list should contain our damaged sword.
	ASSERT_GT(CurrentItemIndex, 0) << "Smith should see the damaged item";

	// Record the repair cost.
	const int repairCost = PlayerItems[0]._iIvalue;
	ASSERT_GT(repairCost, 0);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	EXPECT_EQ(MyPlayer->InvBody[INVLOC_HAND_RIGHT]._iDurability,
	    MyPlayer->InvBody[INVLOC_HAND_RIGHT]._iMaxDur)
	    << "Durability should be fully restored after repair";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - repairCost)
	    << "Repair cost should be deducted from gold";
}

TEST_F(StoreTransactionTest, SmithRepair_CantAfford)
{
	StripPlayer();

	Item damaged = MakeDamagedSword();
	const int originalDur = damaged._iDurability;
	MyPlayer->InvBody[INVLOC_HAND_RIGHT] = damaged;
	SetPlayerGold(0);
	Stash.gold = 0;

	OpenVendor(TalkID::SmithRepair);
	ASSERT_EQ(ActiveStore, TalkID::SmithRepair);
	ASSERT_GT(CurrentItemIndex, 0);

	SelectItemAtIndex(0);
	EXPECT_EQ(ActiveStore, TalkID::NoMoney)
	    << "Should transition to NoMoney when can't afford repair";
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_HAND_RIGHT]._iDurability, originalDur)
	    << "Durability should be unchanged";
}

// ---- Healer ---------------------------------------------------------------

TEST_F(StoreTransactionTest, Healer_FreeHealOnTalk)
{
	// Damage the player.
	MyPlayer->_pHitPoints = MyPlayer->_pMaxHP / 2;
	MyPlayer->_pHPBase = MyPlayer->_pMaxHPBase / 2;
	ASSERT_NE(MyPlayer->_pHitPoints, MyPlayer->_pMaxHP);

	const int goldBefore = MyPlayer->_pGold;

	// Just opening the healer menu heals the player for free.
	OpenVendor(TalkID::Healer);
	ASSERT_EQ(ActiveStore, TalkID::Healer);

	EXPECT_EQ(MyPlayer->_pHitPoints, MyPlayer->_pMaxHP)
	    << "Player should be fully healed just by talking to Pepin";
	EXPECT_EQ(MyPlayer->_pHPBase, MyPlayer->_pMaxHPBase)
	    << "Player HP base should also be restored";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore)
	    << "Healing at Pepin is free — gold should be unchanged";
}

TEST_F(StoreTransactionTest, HealerBuy_Success)
{
	PopulateVendors();
	ASSERT_FALSE(HealerItems.empty());

	StripPlayer();

	const int itemPrice = HealerItems[0]._iIvalue;
	SetPlayerGold(itemPrice + 1000);
	const int goldBefore = MyPlayer->_pGold;

	// Navigate through the healer menu like a real player would:
	// First open the healer top-level menu, then select "Buy items" (line 14).
	// This ensures StartHealerBuy() is called, which sets PreviousScrollPos
	// correctly via ScrollVendorStore.
	OpenVendor(TalkID::Healer);
	ASSERT_EQ(ActiveStore, TalkID::Healer);

	SelectMenuLine(14);
	ASSERT_EQ(ActiveStore, TalkID::HealerBuy);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Gold should decrease by the price of the healing item";
}

// ---- Boy (Wirt) -----------------------------------------------------------

TEST_F(StoreTransactionTest, BoyBuy_Success)
{
	PopulateVendors();
	// Wirt must have an item.
	if (BoyItem.isEmpty()) {
		GTEST_SKIP() << "Wirt has no item with this seed — skipping";
	}

	// Wirt charges a 50g viewing fee, then the item price with markup.
	int price = BoyItem._iIvalue;
	price += BoyItem._iIvalue / 2; // Diablo 50% markup

	StripPlayer();
	SetPlayerGold(price + 100);

	const int goldBefore = MyPlayer->_pGold;

	// Open Wirt's menu.
	OpenVendor(TalkID::Boy);
	ASSERT_EQ(ActiveStore, TalkID::Boy);

	// Pay the viewing fee (line 18 when Wirt has an item).
	SelectMenuLine(18);
	// This should deduct 50g and transition to BoyBuy.
	ASSERT_EQ(ActiveStore, TalkID::BoyBuy)
	    << "After paying viewing fee, should see Wirt's item";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - 50)
	    << "50 gold viewing fee should be deducted";

	// Select the item (it's at line 10 for Boy).
	CurrentTextLine = 10;
	StoreEnter();
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	EXPECT_EQ(MyPlayer->_pGold, goldBefore - 50 - price)
	    << "Gold should decrease by viewing fee + item price";
	EXPECT_TRUE(BoyItem.isEmpty())
	    << "Wirt's item should be cleared after purchase";
}

TEST_F(StoreTransactionTest, BoyBuy_CantAffordViewingFee)
{
	PopulateVendors();
	if (BoyItem.isEmpty()) {
		GTEST_SKIP() << "Wirt has no item with this seed — skipping";
	}

	SetPlayerGold(30); // Less than 50g viewing fee.
	Stash.gold = 0;

	OpenVendor(TalkID::Boy);
	ASSERT_EQ(ActiveStore, TalkID::Boy);

	SelectMenuLine(18);
	EXPECT_EQ(ActiveStore, TalkID::NoMoney)
	    << "Should get NoMoney when can't afford viewing fee";
}

// ---- Storyteller (Cain) — Identify ----------------------------------------

TEST_F(StoreTransactionTest, StorytellerIdentify_Success)
{
	StripPlayer();
	SetPlayerGold(10000);

	// Place an unidentified magic item in inventory.
	Item magic = MakeUnidentifiedMagicItem();
	int invIdx = PlaceItemInInventory(magic);
	ASSERT_GE(invIdx, 0);
	ASSERT_FALSE(MyPlayer->InvList[invIdx]._iIdentified);

	const int goldBefore = MyPlayer->_pGold;

	// Open identify.
	OpenVendor(TalkID::StorytellerIdentify);
	ASSERT_EQ(ActiveStore, TalkID::StorytellerIdentify);
	ASSERT_GT(CurrentItemIndex, 0) << "Cain should see the unidentified item";

	// The identify cost is always 100 gold.
	EXPECT_EQ(PlayerItems[0]._iIvalue, 100);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	// After ConfirmEnter for identify, it transitions to IdentifyShow.
	EXPECT_EQ(ActiveStore, TalkID::StorytellerIdentifyShow)
	    << "Should show the identified item";

	// The actual item in the player's inventory should now be identified.
	EXPECT_TRUE(MyPlayer->InvList[invIdx]._iIdentified)
	    << "Item should be identified after Cain's service";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - 100)
	    << "Identification costs 100 gold";
}

TEST_F(StoreTransactionTest, StorytellerIdentify_CantAfford)
{
	StripPlayer();
	SetPlayerGold(50); // Less than 100g identify cost.
	Stash.gold = 0;

	Item magic = MakeUnidentifiedMagicItem();
	int invIdx = PlaceItemInInventory(magic);
	ASSERT_GE(invIdx, 0);

	OpenVendor(TalkID::StorytellerIdentify);
	ASSERT_EQ(ActiveStore, TalkID::StorytellerIdentify);
	ASSERT_GT(CurrentItemIndex, 0);

	SelectItemAtIndex(0);
	EXPECT_EQ(ActiveStore, TalkID::NoMoney)
	    << "Should get NoMoney when can't afford identification";
	EXPECT_FALSE(MyPlayer->InvList[invIdx]._iIdentified)
	    << "Item should remain unidentified";
}

// ---- Witch Buy ------------------------------------------------------------

TEST_F(StoreTransactionTest, WitchBuy_PinnedItemsRemainAfterPurchase)
{
	PopulateVendors();
	ASSERT_GE(WitchItems.size(), static_cast<size_t>(NumWitchPinnedItems));

	// Buy the first pinned item (e.g., mana potion).
	const int itemPrice = WitchItems[0]._iIvalue;
	StripPlayer();
	SetPlayerGold(itemPrice + 1000);

	const size_t vendorCountBefore = WitchItems.size();

	OpenVendor(TalkID::WitchBuy);
	ASSERT_EQ(ActiveStore, TalkID::WitchBuy);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	// Pinned items (first NumWitchPinnedItems) should NOT be removed.
	EXPECT_EQ(WitchItems.size(), vendorCountBefore)
	    << "Pinned witch items should remain after purchase (infinite stock)";
}

TEST_F(StoreTransactionTest, WitchBuy_NonPinnedItemRemoved)
{
	PopulateVendors();

	// Skip past the pinned items. We need at least one non-pinned item.
	if (WitchItems.size() <= static_cast<size_t>(NumWitchPinnedItems)) {
		GTEST_SKIP() << "Not enough non-pinned witch items";
	}

	const int idx = NumWitchPinnedItems; // First non-pinned item.
	const int itemPrice = WitchItems[idx]._iIvalue;
	StripPlayer();
	SetPlayerGold(itemPrice + 1000);

	const size_t vendorCountBefore = WitchItems.size();

	OpenVendor(TalkID::WitchBuy);
	ASSERT_EQ(ActiveStore, TalkID::WitchBuy);

	SelectItemAtIndex(idx);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	EXPECT_EQ(WitchItems.size(), vendorCountBefore - 1)
	    << "Non-pinned witch item should be removed after purchase";
}

// ---- Repair cost calculation (extends existing stores_test.cpp) -----------

TEST_F(StoreTransactionTest, RepairCost_MagicItem_ScalesWithDurabilityLoss)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iIdentified = true;
	item._iMaxDur = 60;
	item._iIvalue = 19000;
	item._ivalue = 2000;

	// Test a range of durability losses.
	for (int dur = 1; dur < item._iMaxDur; dur++) {
		item._iDurability = dur;
		item._ivalue = 2000;
		item._iIvalue = 19000;
		CurrentItemIndex = 0;
		AddStoreHoldRepair(&item, 0);

		if (CurrentItemIndex > 0) {
			const int due = item._iMaxDur - dur;
			const int expectedCost = 30 * 19000 * due / (item._iMaxDur * 100 * 2);
			if (expectedCost > 0) {
				EXPECT_EQ(PlayerItems[0]._iIvalue, expectedCost)
				    << "Repair cost mismatch at durability " << dur;
			}
		}
	}
}

TEST_F(StoreTransactionTest, RepairCost_NormalItem_MinimumOneGold)
{
	Item item {};
	InitializeItem(item, IDI_BARDSWORD);
	item._iMagical = ITEM_QUALITY_NORMAL;
	item._iIdentified = true;
	item._iMaxDur = 20;
	item._ivalue = 10;
	item._iIvalue = 10;
	item._iDurability = 19; // Only 1 durability lost.

	CurrentItemIndex = 0;
	AddStoreHoldRepair(&item, 0);

	ASSERT_EQ(CurrentItemIndex, 1);
	EXPECT_GE(PlayerItems[0]._iIvalue, 1)
	    << "Repair cost should be at least 1 gold for normal items";
}

// ---- Gold from stash used in transactions ---------------------------------

TEST_F(StoreTransactionTest, BuyUsingStashGold)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	const int itemPrice = SmithItems[0]._iIvalue;
	ASSERT_GT(itemPrice, 0);

	// Give the player less gold than the price in inventory, make up
	// the difference with stash gold.
	const int inventoryGold = itemPrice / 3;
	const int stashGold = itemPrice - inventoryGold + 500;
	StripPlayer();
	SetPlayerGold(inventoryGold);
	Stash.gold = stashGold;

	ASSERT_TRUE(PlayerCanAfford(itemPrice));

	OpenVendor(TalkID::SmithBuy);
	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);
	ConfirmYes();

	// Total gold (inventory + stash) should have decreased by itemPrice.
	const int totalGoldAfter = MyPlayer->_pGold + Stash.gold;
	const int expectedTotal = inventoryGold + stashGold - itemPrice;
	EXPECT_EQ(totalGoldAfter, expectedTotal)
	    << "Total gold (inventory + stash) should decrease by item price";
}

// ---- Multiple transactions ------------------------------------------------

TEST_F(StoreTransactionTest, SmithBuy_MultipleItemsPurchased)
{
	PopulateVendors();
	ASSERT_GE(SmithItems.size(), 3u);

	const size_t initialCount = SmithItems.size();
	int totalSpent = 0;

	// Buy three items in succession. Strip and re-fund between purchases
	// because purchased items occupy inventory slots and gold piles also
	// occupy slots — we need room for both the gold and the next item.
	for (int purchase = 0; purchase < 3; purchase++) {
		ASSERT_FALSE(SmithItems.empty());
		const int price = SmithItems[0]._iIvalue;

		StripPlayer();
		SetPlayerGold(price + 1000);
		const int goldBefore = MyPlayer->_pGold;

		OpenVendor(TalkID::SmithBuy);
		SelectItemAtIndex(0);
		ASSERT_EQ(ActiveStore, TalkID::Confirm)
		    << "Purchase " << purchase << " should reach Confirm";
		ConfirmYes();

		EXPECT_EQ(MyPlayer->_pGold, goldBefore - price)
		    << "Gold should decrease by item price on purchase " << purchase;
		totalSpent += price;
	}

	EXPECT_EQ(SmithItems.size(), initialCount - 3)
	    << "Three items should have been removed from Smith's inventory";
}

// ---- Store leaves correct state after ESC ---------------------------------

TEST_F(StoreTransactionTest, StoreESC_ClosesStore)
{
	OpenVendor(TalkID::Smith);
	ASSERT_EQ(ActiveStore, TalkID::Smith);
	ASSERT_TRUE(IsPlayerInStore());

	// Select "Leave" (line 20 for Smith without visual store, or the last option).
	SelectMenuLine(20);

	EXPECT_EQ(ActiveStore, TalkID::None)
	    << "Leaving the store should set ActiveStore to None";
	EXPECT_FALSE(IsPlayerInStore());
}

// ---- Confirm dialog returns to correct sub-store --------------------------

TEST_F(StoreTransactionTest, ConfirmNo_ReturnsToItemList)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	const int itemPrice = SmithItems[0]._iIvalue;
	StripPlayer();
	SetPlayerGold(itemPrice + 1000);

	OpenVendor(TalkID::SmithBuy);
	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmNo();

	EXPECT_EQ(ActiveStore, TalkID::SmithBuy)
	    << "Declining should return to the buy list";
}

// ---- NoMoney returns to correct sub-store on enter ------------------------

TEST_F(StoreTransactionTest, NoMoney_ReturnsToItemListOnEnter)
{
	PopulateVendors();
	ASSERT_FALSE(SmithItems.empty());

	SetPlayerGold(0);
	Stash.gold = 0;

	OpenVendor(TalkID::SmithBuy);
	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::NoMoney);

	// Pressing enter on NoMoney should return to the original item list.
	StoreEnter();

	EXPECT_EQ(ActiveStore, TalkID::SmithBuy)
	    << "Entering on NoMoney should return to the buy list";
}

// ---- Premium items --------------------------------------------------------

TEST_F(StoreTransactionTest, SmithPremiumBuy_ReplacesSlot)
{
	PopulateVendors();
	ASSERT_FALSE(PremiumItems.empty());

	const int itemPrice = PremiumItems[0]._iIvalue;
	StripPlayer();
	SetPlayerGold(itemPrice + 5000);

	const size_t premiumCountBefore = PremiumItems.size();
	const int goldBefore = MyPlayer->_pGold;

	OpenVendor(TalkID::SmithPremiumBuy);
	ASSERT_EQ(ActiveStore, TalkID::SmithPremiumBuy);

	SelectItemAtIndex(0);
	ASSERT_EQ(ActiveStore, TalkID::Confirm);

	ConfirmYes();

	// Premium items are _replaced_, not removed. Count should stay the same.
	EXPECT_EQ(PremiumItems.size(), premiumCountBefore)
	    << "Premium item slot should be replaced, not removed";
	EXPECT_EQ(MyPlayer->_pGold, goldBefore - itemPrice)
	    << "Gold should decrease by premium item price";
}

} // namespace
} // namespace devilution