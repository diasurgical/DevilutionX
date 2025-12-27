#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "quests.h"

#include "objdat.h" // For quest IDs

namespace devilution {
void ResetQuests()
{
	for (auto &quest : Quests)
		quest._qactive = QUEST_INIT;
}

std::vector<quest_state> GetActiveFlagsForSlice(std::initializer_list<quest_id> ids)
{
	std::vector<quest_state> temp;

	for (auto id : ids)
		temp.push_back(Quests[id]._qactive);

	return temp;
}

TEST(QuestTest, SinglePlayerBadPools)
{
	ResetQuests();

	// (INT_MIN >> 16) % 2 = 0, so the times when the RNG calls GenerateRnd(2) don't end up with a negative value.
	InitialiseQuestPools(1457187811, Quests);
	EXPECT_EQ(Quests[Q_SKELKING]._qactive, QUEST_NOTAVAIL) << "Skeleton King quest is deactivated with 'bad' seed";
	ResetQuests();

	// Having this seed for level 15 requires starting a game at 1977-12-28 07:44:42 PM or 2087-02-18 10:43:02 PM
	// Given Diablo was released in 1996 and it's currently 2024 this will never have been naturally hit. It's not
	//  possible to hit this case on a pre-NT kernel windows system but it may be possible on macos or winnt?
	InitialiseQuestPools(988045466, Quests);
	EXPECT_THAT(GetActiveFlagsForSlice({ Q_BUTCHER, Q_LTBANNER, Q_GARBUD }), ::testing::Each(QUEST_INIT)) << "All quests in pool 2 remain active with 'bad' seed";
	ResetQuests();

	// This seed can only be reached by editing a save file or modifying the game. Given quest state (including
	//  availability) is saved as part of the game state there's no vanilla compatibility concerns here.
	InitialiseQuestPools(4203210069U, Quests);
	// If we wanted to retain the behaviour that vanilla Diablo would've done we should instead deactivate
	//  Quests[QuestGroup2[-2]]. This would hit QuestGroup1[1] (Ogden's Sign), however that's already marked
	//  as unavailable with this seed due to the previous quest group's roll.
	EXPECT_EQ(Quests[Q_LTBANNER]._qactive, QUEST_NOTAVAIL) << "Ogden's Sign should be deactivated with 'bad' seed";
	EXPECT_EQ(Quests[Q_BLIND]._qactive, QUEST_NOTAVAIL) << "Halls of the Blind should also be deactivated with 'bad' seed";
	ResetQuests();

	// This seed can only be reached by editing a save file or modifying the game. Given quest state (including
	//  availability) is saved as part of the game state there's no vanilla compatibility concerns here.
	InitialiseQuestPools(2557708932U, Quests);
	// If we wanted to retain the behaviour that vanilla Diablo would've done we should instead deactivate
	//  Quests[QuestGroup3[-2]]. This would hit QuestGroup2[1] (Magic Rock), however that's already marked
	//  as unavailable with this seed due to the previous quest group's roll.
	EXPECT_EQ(Quests[Q_ROCK]._qactive, QUEST_NOTAVAIL) << "Magic Rock should be deactivated with 'bad' seed";
	EXPECT_EQ(Quests[Q_MUSHROOM]._qactive, QUEST_NOTAVAIL) << "Black Mushroom should also be deactivated with 'bad' seed";
	ResetQuests();

	InitialiseQuestPools(1272442071, Quests);
	EXPECT_EQ(Quests[Q_VEIL]._qactive, QUEST_NOTAVAIL) << "Lachdan quest is deactivated with 'bad' seed";
}

TEST(QuestTest, SinglePlayerGoodPools)
{
	ResetQuests();

	InitialiseQuestPools(509604, Quests);
	EXPECT_EQ(Quests[Q_SKELKING]._qactive, QUEST_INIT) << "Expected Skeleton King quest to be available with the given seed";
	EXPECT_EQ(Quests[Q_PWATER]._qactive, QUEST_NOTAVAIL) << "Expected Poison Water quest to be deactivated with the given seed";

	EXPECT_EQ(Quests[Q_BUTCHER]._qactive, QUEST_INIT) << "Expected Butcher quest to be available with the given seed";
	EXPECT_EQ(Quests[Q_LTBANNER]._qactive, QUEST_INIT) << "Expected Ogden's Sign quest to be available with the given seed";
	EXPECT_EQ(Quests[Q_GARBUD]._qactive, QUEST_NOTAVAIL) << "Expected Gharbad the Weak quest to be deactivated with the given seed";

	EXPECT_EQ(Quests[Q_BLIND]._qactive, QUEST_INIT) << "Expected Halls of the Blind quest to be available with the given seed";
	EXPECT_EQ(Quests[Q_ROCK]._qactive, QUEST_NOTAVAIL) << "Expected Magic Rock quest to be deactivated with the given seed";
	EXPECT_EQ(Quests[Q_BLOOD]._qactive, QUEST_INIT) << "Expected Valor quest to be available with the given seed";

	EXPECT_EQ(Quests[Q_MUSHROOM]._qactive, QUEST_INIT) << "Expected Black Mushroom quest to be available with the given seed";
	EXPECT_EQ(Quests[Q_ZHAR]._qactive, QUEST_NOTAVAIL) << "Expected Zhar the Mad quest to be deactivated with the given seed";
	EXPECT_EQ(Quests[Q_ANVIL]._qactive, QUEST_INIT) << "Expected Anvil of Fury quest to be available with the given seed";

	EXPECT_EQ(Quests[Q_VEIL]._qactive, QUEST_NOTAVAIL) << "Expected Lachdanan quest to be deactivated with the given seed";
	EXPECT_EQ(Quests[Q_WARLORD]._qactive, QUEST_INIT) << "Expected Warlord of Blood quest to be available with the given seed";
}
} // namespace devilution
