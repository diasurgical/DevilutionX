#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "engine/assets.hpp"
#include "engine/demomode.h"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "init.hpp"
#include "lua/lua_global.hpp"
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
	if (
#ifdef USE_SDL3
	    !SDL_Init(SDL_INIT_EVENTS)
#elif !defined(USE_SDL1)
	    SDL_Init(SDL_INIT_EVENTS) < 0
#else
	    SDL_Init(0) < 0
#endif
	) {
		ErrSdl();
	}

	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string unitTestFolderCompletePath = paths::BasePath() + "test/fixtures/timedemo/" + timedemoFolderName;
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

	const HeroCompareResult result = pfile_compare_hero_demo(demoNumber, true);
	ASSERT_EQ(result.status, HeroCompareResult::Same) << result.message;
	ASSERT_FALSE(gbRunGame);
	gbRunGame = false;
	init_cleanup();
	LuaShutdown();
	SDL_Quit();
}

} // namespace

TEST(Timedemo, WarriorLevel1to2)
{
	RunTimedemo("WarriorLevel1to2");
}
