#include <fmt/format.h>
#include <gtest/gtest.h>

#include "drlg_test.hpp"
#include "levels/gendung.h"

using namespace devilution;

namespace {

TEST(Drlg_l3, CreateL3Dungeon_diablo_9_262005438)
{
	LoadExpectedLevelData("diablo/9-262005438.dun");

	TestInitGame();

	TestCreateDungeon(9, 262005438, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(41, 73));
	TestCreateDungeon(9, 262005438, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(73, 59));
	TestCreateDungeon(9, 262005438, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(37, 35));
}

TEST(Drlg_l3, CreateL3Dungeon_diablo_10_1630062353)
{
	LoadExpectedLevelData("diablo/10-1630062353.dun");

	TestInitGame();
	Quests[Q_ANVIL]._qactive = QUEST_NOTAVAIL;

	TestCreateDungeon(10, 1630062353, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(55, 37));
	TestCreateDungeon(10, 1630062353, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(19, 47));
}

TEST(Drlg_l3, CreateL3Dungeon_diablo_10_879635115)
{
	LoadExpectedLevelData("diablo/10-879635115.dun");

	TestInitGame();
	Quests[Q_ANVIL]._qlevel = 10;
	Quests[Q_ANVIL]._qactive = QUEST_INIT;

	TestCreateDungeon(10, 879635115, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(75, 41));
	TestCreateDungeon(10, 879635115, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(27, 45));
}

TEST(Drlg_l3, CreateL3Dungeon_diablo_11_384626536)
{
	LoadExpectedLevelData("diablo/11-384626536.dun");

	TestInitGame();

	TestCreateDungeon(11, 384626536, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(29, 19));
	TestCreateDungeon(11, 384626536, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(65, 65));
}

TEST(Drlg_l3, CreateL3Dungeon_diablo_12_2104541047)
{
	LoadExpectedLevelData("diablo/12-2104541047.dun");

	TestInitGame();

	TestCreateDungeon(12, 2104541047, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(35, 23));
	TestCreateDungeon(12, 2104541047, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(21, 83));
}

TEST(Drlg_l3, CreateL3Dungeon_hive_1_19770182)
{
	LoadExpectedLevelData("hellfire/17-19770182.dun");

	TestInitGame(true, true, true);

	TestCreateDungeon(17, 19770182, ENTRY_TWARPDN);
	EXPECT_EQ(ViewPosition, Point(75, 81));
	TestCreateDungeon(17, 19770182, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(59, 41));
}

TEST(Drlg_l3, CreateL3Dungeon_hive_2_1522546307)
{
	LoadExpectedLevelData("hellfire/18-1522546307.dun");

	TestInitGame(true, true, true);

	TestCreateDungeon(18, 1522546307, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(47, 19));
	TestCreateDungeon(18, 1522546307, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(33, 35));
}

TEST(Drlg_l3, CreateL3Dungeon_hive_3_125121312)
{
	LoadExpectedLevelData("hellfire/19-125121312.dun");

	TestInitGame(true, true, true);

	TestCreateDungeon(19, 125121312, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(61, 25));
	TestCreateDungeon(19, 125121312, ENTRY_PREV);
	EXPECT_EQ(ViewPosition, Point(21, 85));
}

TEST(Drlg_l3, CreateL3Dungeon_hive_4_1511478689)
{
	LoadExpectedLevelData("hellfire/20-1511478689.dun");

	TestInitGame(true, true, true);

	TestCreateDungeon(20, 1511478689, ENTRY_MAIN);
	EXPECT_EQ(ViewPosition, Point(65, 41));
}

} // namespace
