#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "townerdat.hpp"
#include "utils/paths.h"

namespace devilution {

namespace {

void SetTestAssetsPath()
{
	const std::string unitTestFolderCompletePath = paths::BasePath() + "/test/fixtures/";
	paths::SetAssetsPath(unitTestFolderCompletePath);
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
	SetTestAssetsPath();
	LoadTownerData();

	// Verify we loaded the expected number of towners from test fixture
	ASSERT_GE(TownersDataEntries.size(), 4u) << "Should load at least 4 towners from test fixture";

	// Check Griswold (TOWN_SMITH)
	const TownerDataEntry *smith = FindTownerDataByType(TOWN_SMITH);
	ASSERT_NE(smith, nullptr) << "Should find TOWN_SMITH data";
	EXPECT_EQ(smith->type, TOWN_SMITH);
	EXPECT_EQ(smith->name, "Griswold");
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
	ASSERT_EQ(smith->animOrder.size(), 4u);
	EXPECT_EQ(smith->animOrder[0], 4);
	EXPECT_EQ(smith->animOrder[3], 7);

	// Check Pepin (TOWN_HEALER)
	const TownerDataEntry *healer = FindTownerDataByType(TOWN_HEALER);
	ASSERT_NE(healer, nullptr) << "Should find TOWN_HEALER data";
	EXPECT_EQ(healer->type, TOWN_HEALER);
	EXPECT_EQ(healer->name, "Pepin");
	EXPECT_EQ(healer->position.x, 55);
	EXPECT_EQ(healer->position.y, 79);
	EXPECT_EQ(healer->direction, Direction::SouthEast);
	EXPECT_EQ(healer->animFrames, 20);
	EXPECT_EQ(healer->gossipTexts.size(), 9u);
	ASSERT_EQ(healer->animOrder.size(), 3u);

	// Check Dead Guy (TOWN_DEADGUY) - has empty gossip texts and animOrder
	const TownerDataEntry *deadguy = FindTownerDataByType(TOWN_DEADGUY);
	ASSERT_NE(deadguy, nullptr) << "Should find TOWN_DEADGUY data";
	EXPECT_EQ(deadguy->type, TOWN_DEADGUY);
	EXPECT_EQ(deadguy->name, "Wounded Townsman");
	EXPECT_EQ(deadguy->direction, Direction::North);
	EXPECT_TRUE(deadguy->gossipTexts.empty()) << "Dead guy should have no gossip texts";
	EXPECT_TRUE(deadguy->animOrder.empty()) << "Dead guy should have no custom anim order";

	// Check Cow (TOWN_COW) - has empty animPath, animFrames, and animDelay (uses special initialization)
	const TownerDataEntry *cow = FindTownerDataByType(TOWN_COW);
	ASSERT_NE(cow, nullptr) << "Should find TOWN_COW data";
	EXPECT_EQ(cow->type, TOWN_COW);
	EXPECT_EQ(cow->name, "Cow");
	EXPECT_EQ(cow->position.x, 58);
	EXPECT_EQ(cow->position.y, 16);
	EXPECT_EQ(cow->direction, Direction::SouthWest);
	EXPECT_EQ(cow->animWidth, 128);
	EXPECT_TRUE(cow->animPath.empty()) << "Cow should have empty animPath (uses special initialization)";
	EXPECT_EQ(cow->animFrames, 0) << "Cow should have animFrames default to 0 when empty";
	EXPECT_EQ(cow->animDelay, 0) << "Cow should have animDelay default to 0 when empty";
	EXPECT_TRUE(cow->gossipTexts.empty()) << "Cow should have no gossip texts";
	EXPECT_TRUE(cow->animOrder.empty()) << "Cow should have no custom anim order";
}

TEST(TownerDat, LoadQuestDialogTable)
{
	SetTestAssetsPath();
	LoadTownerData();

	// Check Smith quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_BUTCHER), TEXT_INFRA5);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_LTBANNER), TEXT_BANNER5);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_SKELKING), TEXT_KING7);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_ROCK), TEXT_NONE);

	// Check Healer quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_BUTCHER), TEXT_INFRA3);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_LTBANNER), TEXT_BANNER4);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_HEALER, Q_SKELKING), TEXT_KING5);

	// Check Dead guy quest dialogs
	EXPECT_EQ(GetTownerQuestDialog(TOWN_DEADGUY, Q_BUTCHER), TEXT_BUTCH9);
	EXPECT_EQ(GetTownerQuestDialog(TOWN_DEADGUY, Q_LTBANNER), TEXT_NONE);
}

TEST(TownerDat, SetTownerQuestDialog)
{
	SetTestAssetsPath();
	LoadTownerData();

	// Verify initial value
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM), TEXT_NONE);

	// Modify it
	SetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM, TEXT_MUSH1);

	// Verify it changed
	EXPECT_EQ(GetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM), TEXT_MUSH1);

	// Reset for other tests
	SetTownerQuestDialog(TOWN_SMITH, Q_MUSHROOM, TEXT_NONE);
}

TEST(TownerDat, GetQuestDialogInvalidType)
{
	SetTestAssetsPath();
	LoadTownerData();

	// Invalid towner type should return TEXT_NONE
	_speech_id result = GetTownerQuestDialog(NUM_TOWNER_TYPES, Q_BUTCHER);
	EXPECT_EQ(result, TEXT_NONE) << "Should return TEXT_NONE for invalid towner type";
}

} // namespace devilution
