#include <fmt/format.h>
#include <gtest/gtest.h>

#include "drlg_test.hpp"
#include "levels/gendung.h"

using namespace devilution;

namespace {

TEST(Drlg_l2, CreateL2Dungeon_diablo_5_1677631846)
{
	LoadExpectedLevelData("diablo/5-1677631846.dun");

	LoadCoreArchives();
	LoadQuestData();

	InitQuests();
	Quests[Q_BLOOD]._qactive = QUEST_NOTAVAIL;

	TestCreateDungeon(5, 1677631846, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(27, 28));
	TestCreateDungeon(5, 1677631846, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(26, 62));
	TestCreateDungeon(5, 1677631846, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(33, 56));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_5_68685319)
{
	LoadExpectedLevelData("diablo/5-68685319.dun");

	TestInitGame();
	Quests[Q_BLOOD]._qactive = QUEST_INIT;

	TestCreateDungeon(5, 68685319, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(37, 36));
	TestCreateDungeon(5, 68685319, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(44, 28));
	TestCreateDungeon(5, 68685319, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(45, 76));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_6_2034738122)
{
	LoadExpectedLevelData("diablo/6-2034738122.dun");

	TestInitGame();
	Quests[Q_SCHAMB]._qactive = QUEST_NOTAVAIL;

	TestCreateDungeon(6, 2034738122, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(33, 26));
	TestCreateDungeon(6, 2034738122, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(34, 52));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_6_1824554527)
{
	LoadExpectedLevelData("diablo/6-1824554527.dun");

	TestInitGame();
	Quests[Q_SCHAMB]._qactive = QUEST_INIT;

	TestCreateDungeon(6, 1824554527, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(35, 36));
	TestCreateDungeon(6, 1824554527, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(28, 76));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_6_2033265779)
{
	LoadExpectedLevelData("diablo/6-2033265779.dun");

	TestInitGame();
	Quests[Q_SCHAMB]._qactive = QUEST_INIT;

	TestCreateDungeon(6, 2033265779, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(27, 28));
	TestCreateDungeon(6, 2033265779, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(64, 64));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_7_680552750)
{
	LoadExpectedLevelData("diablo/7-680552750.dun");

	TestInitGame();
	Quests[Q_BLIND]._qactive = QUEST_NOTAVAIL;

	TestCreateDungeon(7, 680552750, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(27, 26));
	TestCreateDungeon(7, 680552750, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(78, 52));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_7_1607627156)
{
	LoadExpectedLevelData("diablo/7-1607627156.dun");

	TestInitGame();
	Quests[Q_BLIND]._qactive = QUEST_INIT;

	TestCreateDungeon(7, 1607627156, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(53, 26));
	TestCreateDungeon(7, 1607627156, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(50, 88));
}

TEST(Drlg_l2, CreateL2Dungeon_diablo_8_1999936419)
{
	LoadExpectedLevelData("diablo/8-1999936419.dun");

	TestInitGame();

	TestCreateDungeon(8, 1999936419, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(39, 74));
	TestCreateDungeon(8, 1999936419, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(48, 46));
}

} // namespace
