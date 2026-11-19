#include <filesystem>
#include <string_view>

#include <gtest/gtest.h>
#include <sol/sol.hpp>

#include "cursor.h"
#include "diablo.h"
#include "engine/assets.hpp"
#include "engine/sound.h"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "inv.h"
#include "items.h"
#include "lua/lua_event.hpp"
#include "lua/lua_global.hpp"
#include "options.h"
#include "player.h"
#include "tables/itemdat.h"
#include "utils/paths.h"

namespace devilution {
namespace {

constexpr const char MissingMpqAssetsSkipReason[] = "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test suite";

class LuaEventsTest : public ::testing::Test {
public:
	static void SetUpTestSuite()
	{
		savedHeadlessMode_ = HeadlessMode;
		savedIsMultiplayer_ = gbIsMultiplayer;
		LoadCoreArchives();
		LoadGameArchives();
		missingMpqAssets_ = !HaveMainData();
		if (missingMpqAssets_)
			return;

		// Use an empty config and pref folder so that no installed or enabled mods are loaded.
		const std::filesystem::path folder = std::filesystem::path(::testing::TempDir()) / "lua_events_test";
		std::filesystem::remove_all(folder);
		std::filesystem::create_directories(folder);
		paths::SetPrefPath(folder.string());
		paths::SetConfigPath(folder.string());
		InitKeymapActions();
		LoadOptions();
		HeadlessMode = true;
	}

	static void TearDownTestSuite()
	{
		HeadlessMode = savedHeadlessMode_;
		gbIsMultiplayer = savedIsMultiplayer_;
	}

	void SetUp() override
	{
		if (missingMpqAssets_) {
			GTEST_SKIP() << MissingMpqAssetsSkipReason;
		}
		Players.resize(1);
		MyPlayer = &Players[0];
		InspectPlayer = MyPlayer;
		gbIsMultiplayer = false;
		gbSoundOn = false;
		LuaInitialize();
	}

	void TearDown() override
	{
		if (missingMpqAssets_)
			return;
		LuaShutdown();
	}

protected:
	/** Runs a chunk the way a mod's init.lua runs and returns its result. */
	static sol::object RunLua(std::string_view code)
	{
		sol::state &lua = GetLuaState();
		const sol::environment env = CreateLuaSandbox();
		return SafeCallResult(lua.safe_script(code, env, sol::script_pass_on_error), /*optional=*/false);
	}

private:
	static inline bool missingMpqAssets_ = false;
	static inline bool savedHeadlessMode_ = false;
	static inline bool savedIsMultiplayer_ = false;
};

TEST_F(LuaEventsTest, OnItemUseIsNotCancelledWithoutHandlers)
{
	const Item item {};
	EXPECT_FALSE(lua::OnItemUse(MyPlayer, &item));
}

TEST_F(LuaEventsTest, OnItemUseIsCancelledIfAnyHandlerReturnsTrue)
{
	const sol::table state = RunLua(R"lua(
		local events = require("devilutionx.events")
		local state = { calls = 0 }
		events.OnItemUse.add(function(player, item) return item.IDidx == 42 end)
		events.OnItemUse.add(function(player, item) state.calls = state.calls + 1 end)
		return state
	)lua");

	Item item {};
	item.IDidx = static_cast<_item_indexes>(42);
	EXPECT_TRUE(lua::OnItemUse(MyPlayer, &item));
	item.IDidx = static_cast<_item_indexes>(43);
	EXPECT_FALSE(lua::OnItemUse(MyPlayer, &item));
	// Every handler runs, even after one has cancelled.
	EXPECT_EQ(state.get<int>("calls"), 2);
}

TEST_F(LuaEventsTest, CancelledItemUseDoesNotConsumeTheItem)
{
	Player &player = *MyPlayer;
	player = {};
	player._pMaxHP = player._pMaxHPBase = Fixed26_6::fromInt(100);
	player._pHitPoints = player._pHPBase = Fixed26_6::fromInt(10);
	InitializeItem(player.InvList[0], IDI_HEAL);
	player._pNumInv = 1;
	pcurs = CURSOR_HAND;

	RunLua(R"lua(
		local events = require("devilutionx.events")
		local items = require("devilutionx.items")
		events.OnItemUse.add(function(player, item) return item.IDidx == items.ItemIndex.Heal end)
	)lua");
	EXPECT_TRUE(UseInvItem(INVITEM_INV_FIRST));
	EXPECT_EQ(player._pNumInv, 1);
	EXPECT_EQ(player._pHitPoints, Fixed26_6::fromInt(10));

	// Without a cancelling handler the potion is drunk as usual.
	LuaShutdown();
	LuaInitialize();
	EXPECT_TRUE(UseInvItem(INVITEM_INV_FIRST));
	EXPECT_EQ(player._pNumInv, 0);
}

} // namespace
} // namespace devilution
