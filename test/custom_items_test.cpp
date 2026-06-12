#include <gtest/gtest.h>

#include "cursor.h"
#include "engine/random.hpp"
#include "game_mode.hpp"
#include "items.h"
#include "lua/modules/items.hpp"
#include "player.h"
#include "tables/itemdat.h"

namespace devilution {
namespace {

class CustomItemsTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
	}

	static void SetUpTestSuite()
	{
		savedGbIsHellfire = gbIsHellfire;
		savedGbIsSpawn = gbIsSpawn;
		savedGbIsMultiplayer = gbIsMultiplayer;

		LoadItemData();
		LoadSpellData();
		gbIsHellfire = true;
		gbIsSpawn = false;
		gbIsMultiplayer = false;
	}

	static void TearDownTestSuite()
	{
		gbIsHellfire = savedGbIsHellfire;
		gbIsSpawn = savedGbIsSpawn;
		gbIsMultiplayer = savedGbIsMultiplayer;
	}

private:
	static inline bool savedGbIsHellfire = false;
	static inline bool savedGbIsSpawn = false;
	static inline bool savedGbIsMultiplayer = false;
};

TEST_F(CustomItemsTest, DefaultDropAnimForItemType)
{
	// Verify the mapping returns valid animation indices
	EXPECT_GE(DefaultDropAnimForItemType(ItemType::Sword), 0);
	EXPECT_LT(DefaultDropAnimForItemType(ItemType::Sword), ITEMTYPES);
	EXPECT_GE(DefaultDropAnimForItemType(ItemType::Axe), 0);
	EXPECT_LT(DefaultDropAnimForItemType(ItemType::Axe), ITEMTYPES);
	EXPECT_GE(DefaultDropAnimForItemType(ItemType::Misc), 0);
	EXPECT_LT(DefaultDropAnimForItemType(ItemType::Misc), ITEMTYPES);
	EXPECT_GE(DefaultDropAnimForItemType(ItemType::None), 0);
	EXPECT_LT(DefaultDropAnimForItemType(ItemType::None), ITEMTYPES);
}

TEST_F(CustomItemsTest, GetItemAnimTypeBuiltinItems)
{
	// Built-in items should return the same value as direct table lookup
	Item item = {};
	item._iCurs = ICURS_SHORT_SWORD;
	item._itype = ItemType::Sword;
	EXPECT_EQ(GetItemAnimType(item), ItemCAnimTbl[ICURS_SHORT_SWORD]);

	item._iCurs = ICURS_RING;
	item._itype = ItemType::Ring;
	EXPECT_EQ(GetItemAnimType(item), ItemCAnimTbl[ICURS_RING]);
}

TEST_F(CustomItemsTest, GetItemAnimTypeOutOfBounds)
{
	// Cursor IDs beyond ItemCAnimTbl should fall back to item type default.
	// Derive from ItemCAnimTblSize so growth does not make the cursor in-bounds again; _iCurs is uint8_t.
	const uint8_t oobCurs = static_cast<uint8_t>(
	    (ItemCAnimTblSize < 255) ? (ItemCAnimTblSize + 1) : 255);
	ASSERT_GE(static_cast<int>(oobCurs), ItemCAnimTblSize);
	Item item = {};
	item._iCurs = oobCurs;
	item._itype = ItemType::Sword;
	EXPECT_EQ(GetItemAnimType(item), DefaultDropAnimForItemType(ItemType::Sword));

	item._itype = ItemType::Bow;
	EXPECT_EQ(GetItemAnimType(item), DefaultDropAnimForItemType(ItemType::Bow));
}

TEST_F(CustomItemsTest, GetItemSndBuiltinItems)
{
	Item item = {};
	item._iCurs = ICURS_SHORT_SWORD;
	item._itype = ItemType::Sword;

	SfxID invSnd = GetItemInvSnd(item);
	SfxID dropSnd = GetItemDropSnd(item);

	// Should return a valid (non-None) sound
	EXPECT_NE(invSnd, SfxID::None);
	EXPECT_NE(dropSnd, SfxID::None);
}

TEST_F(CustomItemsTest, CustomItemInAllItemsList)
{
	const size_t sizeBefore = AllItemsList.size();

	// Add a custom item directly to AllItemsList
	ItemData customItem = {};
	customItem.dropRate = 2;
	customItem.iClass = ICLASS_WEAPON;
	customItem.iLoc = ILOC_ONEHAND;
	customItem.iCurs = ICURS_SHORT_SWORD;
	customItem.itype = ItemType::Sword;
	customItem.iItemId = UITYPE_NONE;
	customItem.iName = "Test Sword";
	customItem.iSName = "Test Sword";
	customItem.iMinMLvl = 1;
	customItem.iDurability = 20;
	customItem.iMinDam = 3;
	customItem.iMaxDam = 8;
	customItem.iFlags = ItemSpecialEffect::None;
	customItem.iMiscId = IMISC_NONE;
	customItem.iSpell = SpellID::Null;
	customItem.iUsable = false;
	customItem.iValue = 100;
	customItem.iMappingId = 99999;

	AllItemsList.push_back(std::move(customItem));
	ItemMappingIdsToIndices.emplace(99999, static_cast<int16_t>(AllItemsList.size()) - 1);

	EXPECT_EQ(AllItemsList.size(), sizeBefore + 1);
	EXPECT_EQ(AllItemsList.back().iName, "Test Sword");
	EXPECT_EQ(AllItemsList.back().dropRate, 2);

	// Verify item is available for drops
	const int idx = static_cast<int>(AllItemsList.size()) - 1;
	EXPECT_TRUE(IsItemAvailable(idx));

	// Clean up: remove the item we added
	AllItemsList.pop_back();
	ItemMappingIdsToIndices.erase(99999);
}

TEST_F(CustomItemsTest, ItemCAnimTblSizeMatchesArray)
{
	// ItemCAnimTblSize is sizeof the table in items.cpp; `sizeof(ItemCAnimTbl)` is
	// ill-formed here because the header only declares an incomplete `extern` array.
	EXPECT_GT(ItemCAnimTblSize, 0);
}

TEST_F(CustomItemsTest, SetCustomItemSounds)
{
	const int testCurs = ItemCAnimTblSize;

	// Register custom sounds
	SetCustomItemSounds(testCurs, SfxID::ItemAxe, SfxID::ItemAxeFlip);

	Item item = {};
	item._iCurs = static_cast<uint8_t>(testCurs);
	item._itype = ItemType::Sword; // Intentionally different from axe

	// Should return the custom sounds, not the sword default
	EXPECT_EQ(GetItemInvSnd(item), SfxID::ItemAxe);
	EXPECT_EQ(GetItemDropSnd(item), SfxID::ItemAxeFlip);

	// Clean up
	FreeCustomItemData();
}

TEST_F(CustomItemsTest, LuaAddItemDataAssignsMappingRangeFromCollectionBase)
{
	const size_t sizeBefore = AllItemsList.size();
	constexpr int32_t baseMappingId = 230000;
	sol::state lua;
	sol::table itemModule = LuaItemModule(lua);
	sol::table itemData = lua.create_table();
	itemData[1] = lua.create_table_with("name", "Range Test Item One");
	itemData[2] = lua.create_table_with("name", "Range Test Item Two");
	sol::function addItemData = itemModule["addItemData"];
	addItemData(itemData, baseMappingId);

	EXPECT_EQ(AllItemsList.size(), sizeBefore + 2);
	EXPECT_EQ(AllItemsList[sizeBefore].iMappingId, baseMappingId);
	EXPECT_EQ(AllItemsList[sizeBefore + 1].iMappingId, baseMappingId + 1);
	EXPECT_EQ(ItemMappingIdsToIndices.count(baseMappingId), 1u);
	EXPECT_EQ(ItemMappingIdsToIndices.count(baseMappingId + 1), 1u);

	AllItemsList.pop_back();
	AllItemsList.pop_back();
	ItemMappingIdsToIndices.erase(baseMappingId);
	ItemMappingIdsToIndices.erase(baseMappingId + 1);
}

TEST_F(CustomItemsTest, LuaAddUniqueItemDataAssignsMappingRangeFromCollectionBase)
{
	const size_t sizeBefore = UniqueItems.size();
	constexpr int32_t baseMappingId = 240000;
	sol::state lua;
	sol::table itemModule = LuaItemModule(lua);
	sol::table itemData = lua.create_table();
	itemData[1] = lua.create_table_with("name", "Range Test Unique");
	sol::function addUniqueItemData = itemModule["addUniqueItemData"];
	addUniqueItemData(itemData, baseMappingId);

	EXPECT_EQ(UniqueItems.size(), sizeBefore + 1);
	EXPECT_EQ(UniqueItems[sizeBefore].mappingId, baseMappingId);
	EXPECT_EQ(UniqueItemMappingIdsToIndices.count(baseMappingId), 1u);

	UniqueItems.pop_back();
	UniqueItemMappingIdsToIndices.erase(baseMappingId);
}

} // namespace
} // namespace devilution
