#include <gtest/gtest.h>

#include "cursor.h"
#include "engine/assets.hpp"
#include "inv.h"
#include "player.h"
#include "storm/storm_net.hpp"

namespace devilution {
namespace {

class InvTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
	}

	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();

		// The tests need spawn.mpq or diabdat.mpq
		// Please provide them so that the tests can run successfully
		ASSERT_TRUE(HaveMainData());

		InitCursor();
		LoadSpellData();
		LoadItemData();
	}
};

/* Set up a given item as a spell scroll, allowing for its usage. */
void set_up_scroll(Item &item, SpellID spell)
{
	pcurs = CURSOR_HAND;
	leveltype = DTYPE_CATACOMBS;
	MyPlayer->_pRSpell = static_cast<SpellID>(spell);
	item._itype = ItemType::Misc;
	item._iMiscId = IMISC_SCROLL;
	item._iSpell = spell;
}

/* Clear the inventory of MyPlayerId. */
void clear_inventory()
{
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i] = {};
		MyPlayer->InvGrid[i] = 0;
	}
	MyPlayer->_pNumInv = 0;
}

// Test that the scroll is used in the inventory in correct conditions
TEST_F(InvTest, UseScroll_from_inventory)
{
	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->_pNumInv = 5;
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test that the scroll is used in the belt in correct conditions
TEST_F(InvTest, UseScroll_from_belt)
{
	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test that the scroll is not used in the inventory for each invalid condition
TEST_F(InvTest, UseScroll_from_inventory_invalid_conditions)
{
	// Empty the belt to prevent using a scroll from the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}

	// Adjust inventory size
	MyPlayer->_pNumInv = 5;

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->_pRSpell = SpellID::Healing;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Healing));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->InvList[2]._iMiscId = IMISC_STAFF;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->InvList[2].clear();
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test that the scroll is not used in the belt for each invalid condition
TEST_F(InvTest, UseScroll_from_belt_invalid_conditions)
{
	// Disable the inventory to prevent using a scroll from the inventory
	MyPlayer->_pNumInv = 0;

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->_pRSpell = SpellID::Healing;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Healing));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->SpdList[2]._iMiscId = IMISC_STAFF;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->SpdList[2].clear();
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test gold calculation
TEST_F(InvTest, CalculateGold)
{
	MyPlayer->_pNumInv = 10;
	// Set up 4 slots of gold in the inventory
	MyPlayer->InvList[1]._itype = ItemType::Gold;
	MyPlayer->InvList[5]._itype = ItemType::Gold;
	MyPlayer->InvList[2]._itype = ItemType::Gold;
	MyPlayer->InvList[3]._itype = ItemType::Gold;
	// Set the gold amount to arbitrary values
	MyPlayer->InvList[1]._ivalue = 100;
	MyPlayer->InvList[5]._ivalue = 200;
	MyPlayer->InvList[2]._ivalue = 3;
	MyPlayer->InvList[3]._ivalue = 30;

	EXPECT_EQ(CalculateGold(*MyPlayer), 333);
}

// Test automatic gold placing
TEST_F(InvTest, GoldAutoPlace)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Empty the inventory
	clear_inventory();

	// Put gold into the inventory:
	// | 1000 | ... | ...
	MyPlayer->InvList[0]._itype = ItemType::Gold;
	MyPlayer->InvList[0]._ivalue = 1000;
	MyPlayer->_pNumInv = 1;
	// Put (max gold - 100) gold, which is 4900, into the player's hand
	MyPlayer->HoldItem._itype = ItemType::Gold;
	MyPlayer->HoldItem._ivalue = GOLD_MAX_LIMIT - 100;

	GoldAutoPlace(*MyPlayer, MyPlayer->HoldItem);
	// We expect the inventory:
	// | 5000 | 900 | ...
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, GOLD_MAX_LIMIT);
	EXPECT_EQ(MyPlayer->InvList[1]._ivalue, 900);
}

// Test removing an item from inventory with no other items.
TEST_F(InvTest, RemoveInvItem)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	clear_inventory();
	// Put a two-slot misc item into the inventory:
	// | (item) | (item) | ... | ...
	MyPlayer->_pNumInv = 1;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvGrid[1] = -1;
	MyPlayer->InvList[0]._itype = ItemType::Misc;

	MyPlayer->RemoveInvItem(0);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->InvGrid[1], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing an item from middle of inventory list.
TEST_F(InvTest, RemoveInvItem_shiftsListFromMiddle)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	clear_inventory();
	// Put a two-slot misc item and a ring into the inventory, followed by another two-slot misc item:
	// | (item) | (item) | (ring) | (item) | (item) | ...
	MyPlayer->_pNumInv = 3;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvGrid[1] = -1;
	MyPlayer->InvList[0]._itype = ItemType::Misc;

	MyPlayer->InvGrid[2] = 2;
	MyPlayer->InvList[1]._itype = ItemType::Ring;

	MyPlayer->InvGrid[3] = 3;
	MyPlayer->InvGrid[4] = -3;
	MyPlayer->InvList[2]._itype = ItemType::Misc;

	MyPlayer->RemoveInvItem(1);
	EXPECT_EQ(MyPlayer->InvGrid[0], 1);
	EXPECT_EQ(MyPlayer->InvGrid[1], -1);
	EXPECT_EQ(MyPlayer->InvGrid[2], 0);
	EXPECT_EQ(MyPlayer->InvGrid[3], 2);
	EXPECT_EQ(MyPlayer->InvGrid[4], -2);

	EXPECT_EQ(MyPlayer->InvList[0]._itype, ItemType::Misc);
	EXPECT_EQ(MyPlayer->InvList[1]._itype, ItemType::Misc);

	EXPECT_EQ(MyPlayer->_pNumInv, 2);
}

// Test removing an item from front of inventory list.
TEST_F(InvTest, RemoveInvItem_shiftsListFromFront)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	clear_inventory();
	// Put a two-slot misc item and a ring into the inventory, followed by another two-slot misc item:
	// | (item) | (item) | (ring) | (item) | (item) | ...
	MyPlayer->_pNumInv = 3;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvGrid[1] = -1;
	MyPlayer->InvList[0]._itype = ItemType::Misc;

	MyPlayer->InvGrid[2] = 2;
	MyPlayer->InvList[1]._itype = ItemType::Ring;

	MyPlayer->InvGrid[3] = 3;
	MyPlayer->InvGrid[4] = -3;
	MyPlayer->InvList[2]._itype = ItemType::Misc;

	MyPlayer->RemoveInvItem(0);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->InvGrid[1], 0);
	EXPECT_EQ(MyPlayer->InvGrid[2], 1);
	EXPECT_EQ(MyPlayer->InvGrid[3], 2);
	EXPECT_EQ(MyPlayer->InvGrid[4], -2);

	EXPECT_EQ(MyPlayer->InvList[0]._itype, ItemType::Ring);
	EXPECT_EQ(MyPlayer->InvList[1]._itype, ItemType::Misc);

	EXPECT_EQ(MyPlayer->_pNumInv, 2);
}

// Test removing an item from the belt
TEST_F(InvTest, RemoveSpdBarItem)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put an item in the belt: | x | x | item | x | x | x | x | x |
	MyPlayer->SpdList[3]._itype = ItemType::Misc;

	MyPlayer->RemoveSpdBarItem(3);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

// Test removing a scroll from the inventory
TEST_F(InvTest, RemoveCurrentSpellScrollFromInventory)
{
	clear_inventory();

	// Put a firebolt scroll into the inventory
	MyPlayer->_pNumInv = 1;
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = INVITEM_INV_FIRST;
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iMiscId = IMISC_SCROLL;
	MyPlayer->InvList[0]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing the first matching scroll from inventory
TEST_F(InvTest, RemoveCurrentSpellScrollFromInventoryFirstMatch)
{
	clear_inventory();

	// Put a firebolt scroll into the inventory
	MyPlayer->_pNumInv = 1;
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = 0; // any matching scroll
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iMiscId = IMISC_SCROLL;
	MyPlayer->InvList[0]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing a scroll from the belt
TEST_F(InvTest, RemoveCurrentSpellScroll_belt)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put a firebolt scroll into the belt
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = INVITEM_BELT_FIRST + 3;
	MyPlayer->SpdList[3]._itype = ItemType::Misc;
	MyPlayer->SpdList[3]._iMiscId = IMISC_SCROLL;
	MyPlayer->SpdList[3]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

// Test removing the first matching scroll from the belt
TEST_F(InvTest, RemoveCurrentSpellScrollFirstMatchFromBelt)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put a firebolt scroll into the belt
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = 0; // any matching scroll
	MyPlayer->SpdList[3]._itype = ItemType::Misc;
	MyPlayer->SpdList[3]._iMiscId = IMISC_SCROLL;
	MyPlayer->SpdList[3]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

TEST_F(InvTest, ItemSizeRuneOfStone)
{
	// Inventory sizes are currently determined by examining the sprite size
	// rune of stone and grey suit are adjacent in the sprite list so provide an easy check for off-by-one errors
	if (!gbIsHellfire) return;
	Item testItem {};
	InitializeItem(testItem, IDI_RUNEOFSTONE);
	EXPECT_EQ(GetInventorySize(testItem), Size(1, 1));
}

TEST_F(InvTest, ItemSizeGreySuit)
{
	if (!gbIsHellfire) return;
	Item testItem {};
	InitializeItem(testItem, IDI_GREYSUIT);
	EXPECT_EQ(GetInventorySize(testItem), Size(2, 2));
}

TEST_F(InvTest, ItemSizeAuric)
{
	// Auric amulet is the first used hellfire sprite, but there's multiple unused sprites before it in the list.
	// unfortunately they're the same size so this is less valuable as a test.
	if (!gbIsHellfire) return;
	Item testItem {};
	InitializeItem(testItem, IDI_AURIC);
	EXPECT_EQ(GetInventorySize(testItem), Size(1, 1));
}

TEST_F(InvTest, ItemSizeLastDiabloItem)
{
	// Short battle bow is the last diablo sprite, off by ones will end up loading a 1x1 unused sprite from hellfire,.
	Item testItem {};
	InitializeItem(testItem, IDI_SHORT_BATTLE_BOW);
	EXPECT_EQ(GetInventorySize(testItem), Size(2, 3));
}

} // namespace
} // namespace devilution
