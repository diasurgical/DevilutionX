#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "loadsave.h"
#include "monster.h"
#include "monsters/save_stats.hpp"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"

namespace devilution {
namespace {

class MonsterSaveStats : public ::testing::Test {
protected:
	void SetUp() override
	{
		MonstersData.resize(NUM_DEFAULT_MTYPES);
		LevelMonsterTypes[0].type = MT_NZOMBIE;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		directory = std::filesystem::temp_directory_path()
		    / ("devilutionx-monster-stats-" + std::to_string(stamp));
		ASSERT_TRUE(std::filesystem::create_directory(directory));
	}

	void TearDown() override
	{
		std::filesystem::remove_all(directory);
		MonstersData.clear();
	}

	std::vector<std::byte> save(Monster &monster)
	{
#ifdef UNPACKED_SAVES
		SaveWriter writer { directory.string() + "/" };
#else
		SaveWriter writer { (directory / "test.sv").string() };
#endif
		return SaveMonsterForTest(writer, monster);
	}

	static Monster makeMonster()
	{
		Monster monster {};
		monster.uniqueType = UniqueMonsterType::None;
		monster.goal = MonsterGoal::Normal;
		return monster;
	}

	std::filesystem::path directory;
};

TEST_F(MonsterSaveStats, WideStatsRoundTripThroughActualSaveRecord)
{
	Monster source = makeMonster();
	source.minDamage = 306;
	source.maxDamage = 606;
	source.minDamageSpecial = 255;
	source.maxDamageSpecial = 1026;
	source.armorClass = 265;
	source.talkMsg = TEXT_GARBUD1;
	const auto record = save(source);
	ASSERT_EQ(record.size(), 216U);

	// These positions are independently specified by the legacy memory map.
	EXPECT_EQ(LoadLE32(&record[24]), MonsterStatSaveExtension::MagicVersion1);
	EXPECT_EQ(LoadLE16(&record[164]), 306);
	EXPECT_EQ(LoadLE16(&record[166]), 606);
	EXPECT_EQ(LoadLE16(&record[184]), 255);
	EXPECT_EQ(LoadLE16(&record[186]), 1026);
	EXPECT_EQ(LoadLE16(&record[206]), 265);
	EXPECT_EQ(LoadLE32(&record[208]), static_cast<uint32_t>(TEXT_GARBUD1));
	for (const size_t offset : { 197U, 198U, 200U, 201U, 202U }) {
		EXPECT_EQ(record[offset], std::byte { 255 });
	}

	Monster restored {};
	size_t consumed;
	ASSERT_TRUE(LoadMonsterForTest(record, restored, consumed));
	EXPECT_EQ(consumed, 216U);
	EXPECT_EQ(restored.minDamage, 306);
	EXPECT_EQ(restored.maxDamage, 606);
	EXPECT_EQ(restored.minDamageSpecial, 255);
	EXPECT_EQ(restored.maxDamageSpecial, 1026);
	EXPECT_EQ(restored.armorClass, 265);
	EXPECT_EQ(restored.talkMsg, TEXT_GARBUD1);
}

TEST_F(MonsterSaveStats, LegacyRecordsIgnoreUnusedBytesWithoutSignature)
{
	Monster source = makeMonster();
	source.minDamage = 10;
	source.maxDamage = 20;
	source.armorClass = 30;
	auto record = save(source);
	ASSERT_EQ(record.size(), 216U);
	WriteLE32(&record[24], 0x12345678);
	WriteLE32(&record[164], 0xFFFFFFFF);
	WriteLE32(&record[184], 0xFFFFFFFF);
	WriteLE16(&record[206], 65535);
	Monster restored {};
	size_t consumed;
	ASSERT_TRUE(LoadMonsterForTest(record, restored, consumed));
	EXPECT_EQ(consumed, 216U);
	EXPECT_EQ(restored.minDamage, 10);
	EXPECT_EQ(restored.maxDamage, 20);
	EXPECT_EQ(restored.armorClass, 30);
}

TEST_F(MonsterSaveStats, ByteSizedStatsDoNotWriteAnExtension)
{
	Monster source = makeMonster();
	source.minDamage = 10;
	source.maxDamage = 255;
	source.maxDamageSpecial = 100;
	source.armorClass = 255;
	const auto record = save(source);
	ASSERT_EQ(record.size(), 216U);
	EXPECT_EQ(LoadLE32(&record[24]), 0U);
	EXPECT_EQ(LoadLE32(&record[164]), 0U);
	EXPECT_EQ(LoadLE32(&record[184]), 0U);
	EXPECT_EQ(LoadLE16(&record[206]), 0);
	EXPECT_EQ(record[197], std::byte { 10 });
	EXPECT_EQ(record[198], std::byte { 255 });
	EXPECT_EQ(record[200], std::byte { 0 });
	EXPECT_EQ(record[201], std::byte { 100 });
	EXPECT_EQ(record[202], std::byte { 255 });
}

TEST_F(MonsterSaveStats, WideArmorAlonePreservesAllDamageStats)
{
	Monster source = makeMonster();
	source.minDamage = 0;
	source.maxDamage = 20;
	source.minDamageSpecial = 30;
	source.maxDamageSpecial = 40;
	source.armorClass = 256;
	const auto record = save(source);
	Monster restored {};
	size_t consumed;
	ASSERT_TRUE(LoadMonsterForTest(record, restored, consumed));
	EXPECT_EQ(consumed, 216U);
	EXPECT_EQ(restored.minDamage, 0);
	EXPECT_EQ(restored.maxDamage, 20);
	EXPECT_EQ(restored.minDamageSpecial, 30);
	EXPECT_EQ(restored.maxDamageSpecial, 40);
	EXPECT_EQ(restored.armorClass, 256);
}

} // namespace
} // namespace devilution
