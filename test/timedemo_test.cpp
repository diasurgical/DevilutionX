#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>

#include "engine/assets.hpp"
#include "engine/demomode.h"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "init.h"
#include "lua/lua.hpp"
#include "monstdat.h"
#include "options.h"
#include "pfile.h"
#include "playerdat.hpp"
#include "utils/display.h"
#include "utils/paths.h"

using namespace devilution;

namespace {

bool Dummy_GetHeroInfo(_uiheroinfo *pInfo)
{
	return true;
}

void RunTimedemo(std::string timedemoFolderName)
{
	if (SDL_Init(
#ifdef USE_SDL1
	        0
#else
	        SDL_INIT_EVENTS
#endif
	        )
	    <= -1) {
		ErrSdl();
	}

	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	// Please provide them so that the tests can run successfully
	ASSERT_TRUE(HaveMainData());

	std::string unitTestFolderCompletePath = paths::BasePath() + "test/fixtures/timedemo/" + timedemoFolderName;
	paths::SetPrefPath(unitTestFolderCompletePath);
	paths::SetConfigPath(unitTestFolderCompletePath);

	InitKeymapActions();
	LoadOptions();
	demo::OverrideOptions();
	LuaInitialize();

	const int demoNumber = 0;

	Players.resize(1);
	MyPlayerId = demoNumber;
	MyPlayer = &Players[MyPlayerId];
	*MyPlayer = {};

	// Currently only spawn.mpq is present when building on github actions
	gbIsSpawn = true;
	gbIsHellfire = false;
	gbMusicOn = false;
	gbSoundOn = false;
	HeadlessMode = true;
	demo::InitPlayBack(demoNumber, true);

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMissileData();
	LoadMonsterData();
	LoadItemData();
	LoadObjectData();
	pfile_ui_set_hero_infos(Dummy_GetHeroInfo);
	gbLoadGame = true;

	demo::OverrideOptions();

	AdjustToScreenGeometry(forceResolution);

	StartGame(false, true);

	HeroCompareResult result = pfile_compare_hero_demo(demoNumber, true);
	ASSERT_EQ(result.status, HeroCompareResult::Same) << result.message;
	ASSERT_FALSE(gbRunGame);
	gbRunGame = false;
	init_cleanup();
	SDL_Quit();
}

} // namespace

TEST(Timedemo, WarriorLevel1to2)
{
	RunTimedemo("WarriorLevel1to2");
}
