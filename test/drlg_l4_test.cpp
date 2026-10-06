#include <gtest/gtest.h>

#include "drlg_test.hpp"
#include "levels/gendung.h"

using namespace devilution;

namespace {

TEST(Drlg_l4, CreateL4Dungeon_diablo_13_428074402)
{
	LoadExpectedLevelData("diablo/13-428074402.dun");

	TestInitGame();
	Quests[Q_WARLORD]._qactive = QUEST_NOTAVAIL;

	TestCreateDungeon(13, 428074402, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(26, 64));
	TestCreateDungeon(13, 428074402, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(49, 77));
	TestCreateDungeon(13, 428074402, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(26, 44));
}

TEST(Drlg_l4, CreateL4Dungeon_diablo_13_594689775)
{
	LoadExpectedLevelData("diablo/13-594689775.dun");

	TestInitGame();
	Quests[Q_WARLORD]._qactive = QUEST_INIT;

	TestCreateDungeon(13, 594689775, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(72, 38));
	TestCreateDungeon(13, 594689775, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(33, 41));
	TestCreateDungeon(13, 594689775, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(36, 88));
}

TEST(Drlg_l4, CreateL4Dungeon_diablo_14_717625719)
{
	LoadExpectedLevelData("diablo/14-717625719.dun");

	TestInitGame();

	TestCreateDungeon(14, 717625719, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(90, 64));
	TestCreateDungeon(14, 717625719, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(51, 29));
}

// Level which rooms extend to the edge of the quadrant
TEST(Drlg_l4, CreateL4Dungeon_diablo_14_815743776)
{
	LoadExpectedLevelData("diablo/14-815743776.dun");

	TestInitGame();

	TestCreateDungeon(14, 815743776, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(66, 60));
	TestCreateDungeon(14, 815743776, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(27, 75));
}

TEST(Drlg_l4, CreateL4Dungeon_diablo_15_1583642716)
{
	LoadExpectedLevelData("diablo/15-1583642716.dun");

	TestInitGame();
	Quests[Q_DIABLO]._qactive = QUEST_INIT;

	TestCreateDungeon(15, 1583642716, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(44, 26));
	TestCreateDungeon(15, 1583642716, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(88, 67));

	Quests[Q_BETRAYER]._qactive = QUEST_ACTIVE;
	TestCreateDungeon(15, 1583642716, ENTRY_MAIN); // Betrayer quest does not change level gen
	EXPECT_EQ(ViewPosition, Point(44, 26));
	EXPECT_EQ(Quests[Q_BETRAYER].position, Point(84, 64)) << "Ensure the portal to lazarus has a spawn position if the player has activated the quest";

	LoadExpectedLevelData("diablo/15-1583642716-changed.dun");

	Quests[Q_BETRAYER]._qactive = QUEST_DONE;
	Quests[Q_DIABLO]._qactive = QUEST_ACTIVE;

	TestCreateDungeon(15, 1583642716, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(44, 26));
	EXPECT_EQ(Quests[Q_BETRAYER].position, Point(84, 64)) << "Not really required? current bugfix sets this position anyway";
	TestCreateDungeon(15, 1583642716, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(88, 67));
	EXPECT_EQ(Quests[Q_BETRAYER].position, Point(84, 64)) << "Not really required? current bugfix sets this position anyway";
}

TEST(Drlg_l4, CreateL4Dungeon_diablo_15_1256511996)
{
	LoadExpectedLevelData("diablo/15-1256511996.dun");

	TestInitGame(false);

	TestCreateDungeon(15, 1256511996, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(80, 70));
	TestCreateDungeon(15, 1256511996, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(34, 65));
}

TEST(Drlg_l4, CreateL4Dungeon_diablo_16_741281013)
{
	LoadExpectedLevelData("diablo/16-741281013.dun");

	TestCreateDungeon(16, 741281013, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(58, 42));
}

} // namespace
