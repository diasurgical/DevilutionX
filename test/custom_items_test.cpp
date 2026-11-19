#include <gtest/gtest.h>

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
