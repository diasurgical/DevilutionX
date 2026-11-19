#include <filesystem>
#include <string_view>

#include <gtest/gtest.h>
#include <sol/sol.hpp>

#include "diablo.h"
#include "engine/assets.hpp"
#include "engine/sound.h"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "items.h"
#include "levels/dun_tile_data.hpp"
#include "levels/gendung.h"
#include "lua/lua_global.hpp"
#include "options.h"
#include "player.h"
#include "sound_effect_enums.h"
#include "tables/itemdat.h"
#include "tables/questdat.hpp"
#include "utils/paths.h"

namespace devilution {
namespace {

constexpr const char MissingMpqAssetsSkipReason[] = "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test suite";

class LuaGameApiTest : public ::testing::Test {
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
		const std::filesystem::path folder = std::filesystem::path(::testing::TempDir()) / "lua_game_api_test";
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
		savedDiabloQuestState_ = Quests[Q_DIABLO]._qactive;
		LuaInitialize();
	}

	void TearDown() override
	{
		if (missingMpqAssets_)
			return;
		LuaShutdown();
		Quests[Q_DIABLO]._qactive = savedDiabloQuestState_;
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
	quest_state savedDiabloQuestState_ = QUEST_NOTAVAIL;
};

TEST_F(LuaGameApiTest, GameModuleReportsQuestState)
{
	Quests[Q_DIABLO]._qactive = QUEST_ACTIVE;
	EXPECT_EQ(RunLua(R"lua(return require("devilutionx.game").QuestID.DIABLO)lua").as<int>(), static_cast<int>(Q_DIABLO));
	EXPECT_FALSE(RunLua(R"lua(local game = require("devilutionx.game") return game.isQuestDone(game.QuestID.DIABLO))lua").as<bool>());
	Quests[Q_DIABLO]._qactive = QUEST_DONE;
	EXPECT_TRUE(RunLua(R"lua(local game = require("devilutionx.game") return game.isQuestDone(game.QuestID.DIABLO))lua").as<bool>());
	EXPECT_FALSE(RunLua(R"lua(return require("devilutionx.game").isQuestDone(-1))lua").as<bool>());
}

TEST_F(LuaGameApiTest, PlayerModuleExposesHeroSpeech)
{
	EXPECT_EQ(RunLua(R"lua(return require("devilutionx.player").HeroSpeech.ICantUseThisYet)lua").as<int>(),
	    static_cast<int>(HeroSpeech::ICantUseThisYet));
}

TEST_F(LuaGameApiTest, PlayerIsOnLevel)
{
	MyPlayer->setLevel(5);
	EXPECT_TRUE(RunLua(R"lua(return require("devilutionx.player").self():isOnLevel(5))lua").as<bool>());
	EXPECT_FALSE(RunLua(R"lua(return require("devilutionx.player").self():isOnLevel(6))lua").as<bool>());
	EXPECT_FALSE(RunLua(R"lua(return require("devilutionx.player").self():isOnLevel(-1))lua").as<bool>());
}

TEST_F(LuaGameApiTest, GetItemIndexFindsItemsByMappingId)
{
	const int mappingId = AllItemsList[IDI_HEAL].iMappingId;
	const sol::protected_function lookUp = RunLua(R"lua(
		local items = require("devilutionx.items")
		return function(mappingId) return { found = items.getItemIndex(mappingId), missing = items.getItemIndex(-12345) } end
	)lua");
	const sol::table result = lookUp(mappingId);

	EXPECT_EQ(result.get<int>("found"), static_cast<int>(IDI_HEAL));
	EXPECT_EQ(result.get<sol::object>("missing").get_type(), sol::type::lua_nil);
}

TEST_F(LuaGameApiTest, SpawnQuestItemDropsTheItemOnAFreeTile)
{
	const uint8_t savedCurrlevel = currlevel;
	currlevel = 16;
	setlevel = false;
	InitItems();

	const sol::protected_function spawn = RunLua(R"lua(
		local items = require("devilutionx.items")
		local position = require("devilutionx.player").self().position
		return function(index) return items.spawnQuestItem(index, position, false) end
	)lua");
	MyPlayer->position.tile = { 50, 50 };

	EXPECT_FALSE(spawn(-1).get<bool>());
	EXPECT_EQ(ActiveItemCount, 0);
	EXPECT_TRUE(spawn(static_cast<int>(IDI_HEAL)).get<bool>());
	ASSERT_EQ(ActiveItemCount, 1);
	EXPECT_EQ(Items[ActiveItems[0]].IDidx, IDI_HEAL);
	// The second item does not replace the first one but lands next to it.
	EXPECT_TRUE(spawn(static_cast<int>(IDI_HEAL)).get<bool>());
	ASSERT_EQ(ActiveItemCount, 2);
	EXPECT_NE(Items[ActiveItems[0]].position, Items[ActiveItems[1]].position);

	currlevel = savedCurrlevel;
}

} // namespace
} // namespace devilution
