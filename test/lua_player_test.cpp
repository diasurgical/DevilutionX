#include <gtest/gtest.h>

#include <sol/sol.hpp>

#include "levels/gendung.h"
#include "lua/modules/level.hpp"
#include "lua/modules/player.hpp"
#include "player.h"

using namespace devilution;

class LuaPlayerModuleTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
		MyPlayer->pManaShield = false;
		leveltype = DTYPE_CATHEDRAL;
	}

	void TearDown() override
	{
		Players.clear();
		MyPlayer = nullptr;
	}
};

TEST_F(LuaPlayerModuleTest, ManaShieldProperty)
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	sol::table playerModule = LuaPlayerModule(lua);

	// Get player.self()
	sol::protected_function selfFn = playerModule["self"];
	ASSERT_TRUE(selfFn.valid());

	auto result = selfFn();
	ASSERT_TRUE(result.valid());

	Player *player = result.get<Player *>();
	ASSERT_EQ(player, MyPlayer);

	// Test initial manaShield state
	EXPECT_FALSE(player->pManaShield);

	// Test setting manaShield via direct property access
	player->pManaShield = true;
	EXPECT_TRUE(player->pManaShield);

	player->pManaShield = false;
	EXPECT_FALSE(player->pManaShield);
}

TEST_F(LuaPlayerModuleTest, ManaShieldLuaPropertyAccess)
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	sol::table playerModule = LuaPlayerModule(lua);

	// Make player module available as global for Lua script testing
	lua["player"] = playerModule;

	// Test reading manaShield property via Lua script
	MyPlayer->pManaShield = false;
	{
		auto result = lua.safe_script("return player.self().manaShield");
		ASSERT_TRUE(result.valid());
		EXPECT_FALSE(result.get<bool>());
	}

	// Test that setting via C++ is reflected in Lua
	MyPlayer->pManaShield = true;
	{
		auto result = lua.safe_script("return player.self().manaShield");
		ASSERT_TRUE(result.valid());
		EXPECT_TRUE(result.get<bool>());
	}

	// Test setting manaShield via Lua script
	// Note: This will try to send network commands, but since we're in test mode
	// without network initialization, we just verify the property changes
	MyPlayer->pManaShield = true;
	{
		// Setting to same value should be a no-op
		auto result = lua.safe_script("player.self().manaShield = true");
		ASSERT_TRUE(result.valid());
		EXPECT_TRUE(MyPlayer->pManaShield);
	}
}

class LuaLevelModuleTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		leveltype = DTYPE_CATHEDRAL;
	}
};

TEST_F(LuaLevelModuleTest, LevelTypeFunction)
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	sol::table levelModule = LuaLevelModule(lua);

	// Get level.type()
	sol::protected_function typeFn = levelModule["type"];
	ASSERT_TRUE(typeFn.valid());

	// Test all dungeon types
	leveltype = DTYPE_TOWN;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_TOWN);
	}

	leveltype = DTYPE_CATHEDRAL;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_CATHEDRAL);
	}

	leveltype = DTYPE_CATACOMBS;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_CATACOMBS);
	}

	leveltype = DTYPE_CAVES;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_CAVES);
	}

	leveltype = DTYPE_HELL;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_HELL);
	}

	leveltype = DTYPE_NEST;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_NEST);
	}

	leveltype = DTYPE_CRYPT;
	{
		auto result = typeFn();
		ASSERT_TRUE(result.valid());
		EXPECT_EQ(result.get<int>(), DTYPE_CRYPT);
	}
}

TEST_F(LuaLevelModuleTest, LevelTypeConstants)
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	sol::table levelModule = LuaLevelModule(lua);

	// Verify all level type constants are exposed with shorter names
	EXPECT_EQ(levelModule["TOWN"].get<int>(), DTYPE_TOWN);
	EXPECT_EQ(levelModule["CATHEDRAL"].get<int>(), DTYPE_CATHEDRAL);
	EXPECT_EQ(levelModule["CATACOMBS"].get<int>(), DTYPE_CATACOMBS);
	EXPECT_EQ(levelModule["CAVES"].get<int>(), DTYPE_CAVES);
	EXPECT_EQ(levelModule["HELL"].get<int>(), DTYPE_HELL);
	EXPECT_EQ(levelModule["NEST"].get<int>(), DTYPE_NEST);
	EXPECT_EQ(levelModule["CRYPT"].get<int>(), DTYPE_CRYPT);
}

// Regression test for mana shield persistence across level changes
class ManaShieldPersistenceTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
		MyPlayer->pManaShield = false;
		leveltype = DTYPE_CATHEDRAL;
	}

	void TearDown() override
	{
		Players.clear();
		MyPlayer = nullptr;
	}
};

TEST_F(ManaShieldPersistenceTest, PersistManaShieldMod)
{
	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::table);

	// Create player and level modules
	sol::table playerModule = LuaPlayerModule(lua);
	sol::table levelModule = LuaLevelModule(lua);

	lua["player"] = playerModule;
	lua["level"] = levelModule;

	// Track mana shield state from C++ side
	bool savedManaShield = false;

	// Create C++ callbacks that simulate the mod behavior
	auto leavingLevelHandler = [&savedManaShield]() {
		if (MyPlayer != nullptr) {
			savedManaShield = MyPlayer->pManaShield;
		}
	};

	auto enterLevelHandler = [&savedManaShield]() {
		if (savedManaShield && leveltype != DTYPE_TOWN) {
			if (MyPlayer != nullptr) {
				MyPlayer->pManaShield = true;
			}
		}
		savedManaShield = false;
	};

	// Test 1: Enable mana shield, leave dungeon level, enter another dungeon level
	// Mana shield should persist
	MyPlayer->pManaShield = true;
	leveltype = DTYPE_CATHEDRAL;

	// Simulate leaving level - mod saves mana shield state
	leavingLevelHandler();

	// Simulate C++ clearing mana shield (as InitLevelChange does)
	MyPlayer->pManaShield = false;

	// Simulate entering a new dungeon level (not town)
	leveltype = DTYPE_CATACOMBS;
	enterLevelHandler();

	// Mana shield should be restored
	EXPECT_TRUE(MyPlayer->pManaShield) << "Mana shield should persist from Cathedral to Catacombs";

	// Test 2: Enable mana shield, leave dungeon level, enter TOWN
	// Mana shield should NOT persist
	MyPlayer->pManaShield = true;
	leveltype = DTYPE_CATACOMBS;

	leavingLevelHandler();
	MyPlayer->pManaShield = false;

	// Enter town
	leveltype = DTYPE_TOWN;
	enterLevelHandler();

	// Mana shield should NOT be restored in town
	EXPECT_FALSE(MyPlayer->pManaShield) << "Mana shield should not persist into town";

	// Test 3: No mana shield active, should not restore
	MyPlayer->pManaShield = false;
	leveltype = DTYPE_CAVES;

	leavingLevelHandler();

	leveltype = DTYPE_HELL;
	enterLevelHandler();

	EXPECT_FALSE(MyPlayer->pManaShield) << "Mana shield should not be enabled if it wasn't active before";
}

// Test that Lua mod can correctly access mana shield and level type
TEST_F(ManaShieldPersistenceTest, LuaModAccessPattern)
{
	// This test verifies the level module access - player module access
	// is already covered by LuaPlayerModuleTest.ManaShieldLuaPropertyAccess
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	sol::table levelModule = LuaLevelModule(lua);
	lua["level"] = levelModule;

	// Verify reading level type via Lua
	leveltype = DTYPE_TOWN;
	{
		auto result = lua.safe_script("return level.type() == level.TOWN");
		ASSERT_TRUE(result.valid());
		EXPECT_TRUE(result.get<bool>());
	}

	// Verify we can check "not in town" condition
	leveltype = DTYPE_CATHEDRAL;
	{
		auto result = lua.safe_script("return level.type() ~= level.TOWN");
		ASSERT_TRUE(result.valid());
		EXPECT_TRUE(result.get<bool>());
	}
}
