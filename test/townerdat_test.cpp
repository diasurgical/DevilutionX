#include <gtest/gtest.h>

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "engine/assets.hpp"
#include "townerdat.hpp"
#include "towners.h"
#include "utils/paths.h"

namespace devilution {

namespace {

void SetTestAssetsPath()
{
	const std::string assetsPath = paths::BasePath() + "/assets/";
	paths::SetAssetsPath(assetsPath);
}

void InitializeSDL()
{
#ifdef USE_SDL3
	if (!SDL_Init(SDL_INIT_EVENTS)) {
		// SDL_Init returns 0 on success in SDL3
		return;
	}
#elif !defined(USE_SDL1)
	if (SDL_Init(SDL_INIT_EVENTS) >= 0) {
		return;
	}
#else
	if (SDL_Init(0) >= 0) {
		return;
	}
#endif
	// If we get here, SDL initialization failed
	// In tests, we'll continue anyway as file operations might still work
}

/**
 * @brief Helper to find a towner data entry by type.
 */
const TownerDataEntry *FindTownerDataByType(_talker_id type)
{
	for (const auto &entry : TownersDataEntries) {
		if (entry.type == type) {
			return &entry;
		}
	}
	return nullptr;
}

} // namespace

TEST(TownerDat, LoadTownerData)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Verify we loaded the expected number of towners from assets
	ASSERT_GE(TownersDataEntries.size(), 4u) << "Should load at least 4 towners from assets";

	// Check Griswold (TOWN_SMITH)
	const TownerDataEntry *smith = FindTownerDataByType(TOWN_SMITH);
	ASSERT_NE(smith, nullptr) << "Should find TOWN_SMITH data";
	EXPECT_EQ(smith->type, TOWN_SMITH);
	EXPECT_EQ(smith->name, "Griswold the Blacksmith");
	EXPECT_EQ(smith->position.x, 62);
	EXPECT_EQ(smith->position.y, 63);
	EXPECT_EQ(smith->direction, Direction::SouthWest);
	EXPECT_EQ(smith->animWidth, 96);
	EXPECT_EQ(smith->animPath, "towners\\smith\\smithn");
	EXPECT_EQ(smith->animFrames, 16);
	EXPECT_EQ(smith->animDelay, 3);
	EXPECT_EQ(smith->gossipTexts.size(), 11u);
	EXPECT_EQ(smith->gossipTexts[0], TEXT_GRISWOLD2);
	EXPECT_EQ(smith->gossipTexts[10], TEXT_GRISWOLD13);
	ASSERT_GE(smith->animOrder.size(), 4u);
	EXPECT_EQ(smith->animOrder[0], 4);
	EXPECT_EQ(smith->animOrder[3], 7);

	// Check Pepin (TOWN_HEALER)
	const TownerDataEntry *healer = FindTownerDataByType(TOWN_HEALER);
	ASSERT_NE(healer, nullptr) << "Should find TOWN_HEALER data";
	EXPECT_EQ(healer->type, TOWN_HEALER);
	EXPECT_EQ(healer->name, "Pepin the Healer");
	EXPECT_EQ(healer->position.x, 55);
	EXPECT_EQ(healer->position.y, 79);
	EXPECT_EQ(healer->direction, Direction::SouthEast);
	EXPECT_EQ(healer->animFrames, 20);
	EXPECT_EQ(healer->gossipTexts.size(), 9u);
	ASSERT_GE(healer->animOrder.size(), 3u);

	// Check Dead Guy (TOWN_DEADGUY) - has empty gossip texts and animOrder
	const TownerDataEntry *deadguy = FindTownerDataByType(TOWN_DEADGUY);
	ASSERT_NE(deadguy, nullptr) << "Should find TOWN_DEADGUY data";
	EXPECT_EQ(deadguy->type, TOWN_DEADGUY);
	EXPECT_EQ(deadguy->name, "Wounded Townsman");
	EXPECT_EQ(deadguy->direction, Direction::North);
	EXPECT_TRUE(deadguy->gossipTexts.empty()) << "Dead guy should have no gossip texts";
	EXPECT_TRUE(deadguy->animOrder.empty()) << "Dead guy should have no custom anim order";

	// Check Cow (TOWN_COW) - has empty animPath but animFrames and animDelay are set
	const TownerDataEntry *cow = FindTownerDataByType(TOWN_COW);
	ASSERT_NE(cow, nullptr) << "Should find TOWN_COW data";
	EXPECT_EQ(cow->type, TOWN_COW);
	EXPECT_EQ(cow->name, "Cow");
	EXPECT_EQ(cow->position.x, 58);
	EXPECT_EQ(cow->position.y, 16);
	EXPECT_EQ(cow->direction, Direction::SouthWest);
	EXPECT_EQ(cow->animWidth, 128);
	EXPECT_TRUE(cow->animPath.empty()) << "Cow should have empty animPath";
	EXPECT_EQ(cow->animFrames, 12);
	EXPECT_EQ(cow->animDelay, 3);
	EXPECT_TRUE(cow->gossipTexts.empty()) << "Cow should have no gossip texts";
	EXPECT_TRUE(cow->animOrder.empty()) << "Cow should have no custom anim order";
}

TEST(TownerDat, LoadQuestDialogTable)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Check Smith quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_BUTCHER), TEXT_BUTCH5);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_LTBANNER), TEXT_BANNER6);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_SKELKING), TEXT_KING7);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_ROCK), TEXT_INFRA6);

	// Check Healer quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_BUTCHER), TEXT_BUTCH3);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_LTBANNER), TEXT_BANNER4);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_SKELKING), TEXT_KING5);

	// Check Dead guy quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_DEADGUY, Q_BUTCHER), TEXT_NONE);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_DEADGUY, Q_LTBANNER), TEXT_NONE);
}

TEST(TownerDat, SetTownerQuestDialog)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Verify initial value from assets
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM), TEXT_MUSH6);

	// Modify it
	SetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM, TEXT_MUSH1);

	// Verify it changed
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM), TEXT_MUSH1);

	// Reset to original value for other tests
	SetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM, TEXT_MUSH6);
}

TEST(TownerDat, GetQuestDialogInvalidType)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Invalid towner type should return TEXT_NONE
	// Use a value that's guaranteed to be invalid (beyond enum range)
	_talker_id invalidType = static_cast<_talker_id>(255);
	_speech_id result = GetTownerQuestDialog(invalidType, Q_BUTCHER);
	EXPECT_EQ(result, TEXT_NONE) << "Should return TEXT_NONE for invalid towner type";
}

TEST(TownerDat, GetQuestDialogInvalidQuest)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Invalid quest ID should return TEXT_NONE
	_speech_id result = GetTownerQuestDialog(TOWN_SMITH, static_cast<quest_id>(-1));
	EXPECT_EQ(result, TEXT_NONE) << "Should return TEXT_NONE for invalid quest ID";

	result = GetTownerQuestDialog(TOWN_SMITH, static_cast<quest_id>(MAXQUESTS));
	EXPECT_EQ(result, TEXT_NONE) << "Should return TEXT_NONE for out-of-range quest ID";
}

TEST(TownerDat, TownerLongNamesPopulated)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Build TownerLongNames as InitTowners() does
	TownerLongNames.clear();
	for (const auto &entry : TownersDataEntries) {
		TownerLongNames.try_emplace(entry.type, entry.name);
	}

	// Verify TownerLongNames is populated correctly
	EXPECT_FALSE(TownerLongNames.empty()) << "TownerLongNames should not be empty after loading";

	// Check specific entries
	auto smithIt = TownerLongNames.find(TOWN_SMITH);
	ASSERT_NE(smithIt, TownerLongNames.end()) << "Should find TOWN_SMITH in TownerLongNames";
	EXPECT_EQ(smithIt->second, "Griswold the Blacksmith");

	auto healerIt = TownerLongNames.find(TOWN_HEALER);
	ASSERT_NE(healerIt, TownerLongNames.end()) << "Should find TOWN_HEALER in TownerLongNames";
	EXPECT_EQ(healerIt->second, "Pepin the Healer");
}

TEST(TownerDat, GetNumTownerTypes)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Build TownerLongNames as InitTowners() does
	TownerLongNames.clear();
	for (const auto &entry : TownersDataEntries) {
		TownerLongNames.try_emplace(entry.type, entry.name);
	}

	// GetNumTownerTypes should return the number of unique towner types
	size_t numTypes = GetNumTownerTypes();
	EXPECT_GT(numTypes, 0u) << "Should have at least one towner type";
	EXPECT_EQ(numTypes, TownerLongNames.size()) << "GetNumTownerTypes should match TownerLongNames size";
}

TEST(TownerDat, MultipleCowsOnlyOneType)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Count how many TOWN_COW entries exist in the data
	size_t cowCount = 0;
	for (const auto &entry : TownersDataEntries) {
		if (entry.type == TOWN_COW) {
			cowCount++;
		}
	}

	// There should be multiple cows but only one type entry
	EXPECT_GT(cowCount, 1u) << "TSV should have multiple cow entries";

	// Build TownerLongNames
	TownerLongNames.clear();
	for (const auto &entry : TownersDataEntries) {
		TownerLongNames.try_emplace(entry.type, entry.name);
	}

	// But only one entry in TownerLongNames for TOWN_COW
	auto cowIt = TownerLongNames.find(TOWN_COW);
	ASSERT_NE(cowIt, TownerLongNames.end()) << "Should find TOWN_COW in TownerLongNames";
	EXPECT_EQ(cowIt->second, "Cow");
}

TEST(TownerDat, QuestDialogOptionalColumns)
{
	InitializeSDL();
	SetTestAssetsPath();
	LoadTownerData();

	// Verify that missing quest columns default to TEXT_NONE
	// Q_FARMER, Q_GIRL, Q_DEFILER, Q_NAKRUL, Q_CORNSTN, Q_JERSEY may not be in base TSV
	// but the code should handle them gracefully
	_speech_id result = GetTownerQuestDialog(TOWN_SMITH, Q_FARMER);
	// Should be TEXT_NONE since TOWN_SMITH doesn't have farmer quest dialog
	EXPECT_EQ(result, TEXT_NONE) << "Should return TEXT_NONE for unused quest columns";
}

} // namespace devilution
