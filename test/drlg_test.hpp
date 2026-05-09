/**
 * @file drlg_test.hpp
 *
 * Helpers for dungeon related tests.
 */
#pragma once

#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "engine/load_file.hpp"
#include "levels/themes.h"
#include "multi.h"
#include "player.h"
#include "quests.h"
#include "utils/endian_swap.hpp"
#include "utils/paths.h"

using namespace devilution;

int GetTileCount(dungeon_type levelType)
{
	switch (levelType) {
	case DTYPE_TOWN:
		return 376;
	case DTYPE_CATHEDRAL:
		return 206;
	case DTYPE_CATACOMBS:
		return 160;
	case DTYPE_CAVES:
		return 206;
	case DTYPE_HELL:
		return 137;
	case DTYPE_NEST:
		return 166;
	case DTYPE_CRYPT:
		return 217;
	default:
		app_fatal("Invalid level type");
	}
}

std::unique_ptr<uint16_t[]> DunData;

void LoadExpectedLevelData(const char *fixture)
{
	// Set look up path to the location to load set pieces from later:
	paths::SetPrefPath(paths::BasePath() + "test/fixtures/");
	LoadModArchives({});
	DunData = LoadFileInMem<uint16_t>(fixture);
	ASSERT_NE(DunData, nullptr) << "Unable to load test fixture " << fixture;
	ASSERT_EQ(WorldTileSize(DMAXX, DMAXY), GetDunSize(DunData.get()));
}

void TestInitGame(bool fullQuests = true, bool originalCathedral = true, bool hellfire = false)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	MyPlayer->pOriginalCathedral = originalCathedral;

	sgGameInitInfo.fullQuests = fullQuests ? 1 : 0;
	gbIsMultiplayer = !fullQuests;

	LoadCoreArchives();
	LoadQuestData();

	UnloadModArchives();
	if (hellfire) {
		LoadModArchives({ { "Hellfire" } });
	} else {
		LoadModArchives({});
	}

	InitQuests();
}

void TestCreateDungeon(int level, uint32_t seed, lvl_entry entry)
{
	LevelSeeds[level] = std::nullopt;
	currlevel = level;
	leveltype = GetLevelType(level);

	pMegaTiles = std::make_unique<MegaTile[]>(GetTileCount(leveltype));

	CreateDungeon(seed, entry);
	CreateThemeRooms();

	const uint16_t *tileLayer = &DunData[2];

	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			auto tileId = static_cast<uint8_t>(Swap16LE(*tileLayer));
			tileLayer++;
			ASSERT_EQ(dungeon[x][y], tileId) << "Tiles don't match at " << x << "x" << y;
		}
	}

	const uint16_t *transparentLayer = &DunData[2 + DMAXX * DMAXY * 13];

	for (int y = 16; y < 16 + DMAXY * 2; y++) {
		for (int x = 16; x < 16 + DMAXX * 2; x++) {
			auto sectorId = static_cast<uint8_t>(Swap16LE(*transparentLayer));
			transparentLayer++;
			ASSERT_EQ(dTransVal[x][y], sectorId) << "Room/region indexes don't match at " << x << "x" << y;
		}
	}
}
