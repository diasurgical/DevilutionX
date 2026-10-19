#include <gtest/gtest.h>

#include "data/file.hpp"
#include "monster.h"
#include "tables/monstdat.h"
#include "utils/paths.h"

namespace devilution {
namespace {

class MonsterStatTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		paths::SetAssetsPath(paths::BasePath() + "/test/fixtures/");
		auto data = DataFile::load("txtdata/unique_monster_stats.tsv");
		ASSERT_TRUE(data.has_value());
		UniqueMonstersData.clear();
		LoadUniqueMonstDatFromFile(*data, "unique_monster_stats.tsv");
		MonstersData.resize(NUM_DEFAULT_MTYPES);
		MonstersData[MT_NZOMBIE].toHit = 20;
		MonstersData[MT_NZOMBIE].toHitSpecial = 30;
		LevelMonsterTypes[0].type = MT_NZOMBIE;
	}

	void TearDown() override
	{
		UniqueMonstersData.clear();
		MonstersData.clear();
		paths::SetAssetsPath(paths::BasePath() + "/assets/");
	}
};

TEST_F(MonsterStatTest, NumericTreasureUsesTheSelectedUniqueItem)
{
	auto data = DataFile::load("txtdata/monster_treasure.tsv");
	ASSERT_TRUE(data.has_value());
	LoadMonstDatFromFile(*data, "monster_treasure.tsv", false);
	EXPECT_EQ(MonstersData[MT_NZOMBIE].treasure, T_UNIQ | 64);
	EXPECT_EQ(MonstersData[MT_BZOMBIE].treasure, T_UNIQ);
}

TEST_F(MonsterStatTest, FinalZeroIsNotTheDefaultStat)
{
	ASSERT_EQ(UniqueMonstersData.size(), 3U);
	EXPECT_EQ(UniqueMonstersData[0].finalArmorClass, 0);
	Monster monster {};
	monster.uniqueType = static_cast<UniqueMonsterType>(0);
	for (const auto difficulty : { DIFF_NORMAL, DIFF_NIGHTMARE, DIFF_HELL }) {
		EXPECT_EQ(monster.toHit(difficulty), 0U);
		EXPECT_EQ(monster.toHitSpecial(difficulty), 0U);
	}
}

TEST_F(MonsterStatTest, FinalStatDoesNotReceiveDifficultyBonuses)
{
	EXPECT_EQ(UniqueMonstersData[1].finalArmorClass, 45);
	Monster monster {};
	monster.uniqueType = static_cast<UniqueMonsterType>(1);
	for (const auto difficulty : { DIFF_NORMAL, DIFF_NIGHTMARE, DIFF_HELL }) {
		EXPECT_EQ(monster.toHit(difficulty), 100U);
		EXPECT_EQ(monster.toHitSpecial(difficulty), 100U);
	}
}

TEST_F(MonsterStatTest, NumericOverridesRetainDifficultyScaling)
{
	EXPECT_EQ(UniqueMonstersData[2].finalToHit, -1);
	EXPECT_EQ(UniqueMonstersData[2].finalArmorClass, -1);
	EXPECT_EQ(UniqueMonstersData[2].customArmorClass, 45);
	Monster monster {};
	monster.uniqueType = static_cast<UniqueMonsterType>(2);
	EXPECT_EQ(monster.toHit(DIFF_NORMAL), 100U);
	EXPECT_EQ(monster.toHitSpecial(DIFF_NORMAL), 100U);
	EXPECT_GT(monster.toHit(DIFF_NIGHTMARE), 100U);
	EXPECT_GT(monster.toHitSpecial(DIFF_HELL), 100U);
}

} // namespace
} // namespace devilution
