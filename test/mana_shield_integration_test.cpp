#include "player_test.h"

#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "game_mode.hpp"
#include "init.hpp"
#include "levels/gendung.h"
#include "lua/lua_global.hpp"
#include "options.h"
#include "player.h"
#include "playerdat.hpp"
#include "spelldat.h"
#include "utils/paths.h"

namespace devilution {
namespace {

// Set up a minimal player for testing
void SetupTestPlayer(Player &player)
{
	player = {};
	strcpy(player._pName, "ManaShieldTest");
	player._pClass = HeroClass::Sorcerer;
	player.pManaShield = true;
	player._pSplLvl[static_cast<int8_t>(SpellID::ManaShield)] = 5;
}

TEST(ManaShieldIntegration, ManaShieldPersistsAcrossDungeonLevels)
{
	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	paths::SetPrefPath(paths::BasePath());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = false;
	leveltype = DTYPE_CATHEDRAL;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadItemData();

	// Set up player with mana shield enabled
	SetupTestPlayer(*MyPlayer);
	ASSERT_TRUE(MyPlayer->pManaShield) << "Player should start with mana shield enabled";

	// Initialize options and enable the persist_mana_shield mod
	InitKeymapActions();
	LoadOptions();
	GetOptions().Mods.AddModEntry("persist_mana_shield");
	GetOptions().Mods.SetModEnabled("persist_mana_shield", true);

	// Initialize Lua system (this loads the mod)
	LuaInitialize();

	// Simulate level change: LeavingLevel event fires first (mod saves mana shield state)
	LuaEvent("LeavingLevel");

	// Then C++ clears mana shield (as InitLevelChange does)
	MyPlayer->pManaShield = false;
	ASSERT_FALSE(MyPlayer->pManaShield) << "Mana shield should be cleared after InitLevelChange";

	// Change to a new dungeon level (not town)
	leveltype = DTYPE_CATACOMBS;

	// EnterLevel event fires after entering new level (mod restores mana shield)
	LuaEvent("EnterLevel");

	// With the persist_mana_shield mod enabled, mana shield should be restored
	EXPECT_TRUE(MyPlayer->pManaShield) << "Mana shield should persist from Cathedral to Catacombs";

	// Clean up
	LuaShutdown();
	Players.clear();
	MyPlayer = nullptr;
}

TEST(ManaShieldIntegration, ManaShieldDoesNotPersistIntoTown)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	paths::SetPrefPath(paths::BasePath());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = false;
	leveltype = DTYPE_CATHEDRAL;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadItemData();

	SetupTestPlayer(*MyPlayer);
	ASSERT_TRUE(MyPlayer->pManaShield);

	// Initialize options and enable the persist_mana_shield mod
	InitKeymapActions();
	LoadOptions();
	GetOptions().Mods.AddModEntry("persist_mana_shield");
	GetOptions().Mods.SetModEnabled("persist_mana_shield", true);

	// Initialize Lua system (this loads the mod)
	LuaInitialize();

	// Simulate leaving dungeon (mod saves mana shield state)
	LuaEvent("LeavingLevel");

	// C++ clears mana shield (as InitLevelChange does)
	MyPlayer->pManaShield = false;

	// Enter town - mana shield should NOT be restored even with mod enabled
	leveltype = DTYPE_TOWN;
	LuaEvent("EnterLevel");

	// The mod should NOT restore mana shield in town
	EXPECT_FALSE(MyPlayer->pManaShield) << "Mana shield should not persist into town";

	LuaShutdown();
	Players.clear();
	MyPlayer = nullptr;
}

} // namespace
} // namespace devilution
