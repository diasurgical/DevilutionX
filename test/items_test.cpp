#include <climits>
#include <random>

#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "engine/random.hpp"
#include "game_mode.hpp"
#include "items.h"
#include "player.h"
#include "spells.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

class ItemsTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
		for (auto &flag : UniqueItemFlags)
			flag = false;
	}

	static void SetUpTestSuite()
	{
		LoadItemData();
		LoadSpellData();
		gbIsSpawn = false;
		gbIsMultiplayer = false; // to also test UniqueItemFlags
	}
};

void GenerateAllUniques(bool hellfire, const size_t expectedUniques)
{
	gbIsHellfire = hellfire;
	UnloadModArchives();
	if (hellfire) {
		LoadModArchives({ { "hf" } });
	}
	LoadItemData();

	std::mt19937 betterRng;
	const std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());

	// Get seed for test run from time. If a test run fails, remember the seed and hardcode it here.
	const uint32_t testRunSeed = static_cast<uint32_t>(time(nullptr));
	betterRng.seed(testRunSeed);

	std::set<int> foundUniques;

	constexpr int max_iterations = 1000000;
	int iteration = 0;
	int uniqueIndex = -1;

	for (auto &uniqueItem : UniqueItems) {
		++uniqueIndex;

		if (foundUniques.contains(uniqueIndex))
			continue;

		_item_indexes uniqueBaseIndex = IDI_GOLD;
		for (size_t j = 0; j < AllItemsList.size(); j++) {
			if (!IsItemAvailable(static_cast<int>(j)))
				continue;
			if (AllItemsList[j].iItemId != uniqueItem.UIItemId)
				continue;
			if (AllItemsList[j].dropRate > 0)
				uniqueBaseIndex = static_cast<_item_indexes>(j);
			break;
		}

		if (uniqueBaseIndex == IDI_GOLD)
			continue; // Unique not dropable (Quest-Unique)

		auto &baseItemData = AllItemsList[static_cast<size_t>(uniqueBaseIndex)];

		while (true) {
			iteration += 1;

			if (iteration > max_iterations)
				FAIL() << StrCat("Item ", uniqueItem.UIName, " not found in ", max_iterations, " tries with test run seed ", testRunSeed);

			Item testItem = {};
			testItem._iMiscId = baseItemData.iMiscId;
			std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
			SetRndSeed(dist(betterRng));

			int targetLvl = uniqueItem.UIMinLvl;
			int uper = 1;
			if (uniqueItem.UIMinLvl - 4 > 0) {
				uper = 15;
				targetLvl = uniqueItem.UIMinLvl - 4;
			}

			SetupAllItems(*MyPlayer, testItem, uniqueBaseIndex, AdvanceRndSeed(), targetLvl, uper, true, false);
			TryRandomUniqueItem(testItem, uniqueBaseIndex, targetLvl, uper, true, false);
			SetupItem(testItem);

			if (testItem._iMagical != ITEM_QUALITY_UNIQUE)
				continue;

			foundUniques.insert(testItem._iUid);

			if (testItem._iUid == uniqueIndex)
				break;
		}
	}

	EXPECT_EQ(foundUniques.size(), expectedUniques) << StrCat("test run seed ", testRunSeed);
}

TEST_F(ItemsTest, ForcedUniqueOnNormalBaseSurvivesRecreation)
{
	UniqueItem unique {};
	unique.UIName = "First reward";
	unique.UICurs = static_cast<item_cursor_graphic>(84);
	unique.UIItemId = AllItemsList[IDI_WARRIOR].iItemId;
	unique.UIMinLvl = 1;
	unique.UINumPL = 1;
	unique.powers[0] = { IPL_STR, 7, 11 };
	std::vector<UniqueItem> originalUniques;
	originalUniques.swap(UniqueItems);
	UniqueItems.push_back(unique);
	unique.UIName = "Second reward";
	UniqueItems.push_back(unique);

	Item item {};
	SetupForcedUniqueItem(*MyPlayer, item, IDI_WARRIOR, static_cast<_unique_items>(0));
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_UNIQUE);
	EXPECT_EQ(item._iUid, 0);
	EXPECT_NE(item.dwBuff & CF_UIDOFFSET, 0u);

	Item recreated {};
	RecreateItem(*MyPlayer, recreated, item.IDidx, item._iCreateInfo,
	    item._iSeed, item._ivalue, item.dwBuff);
	EXPECT_EQ(recreated._iMagical, item._iMagical);
	EXPECT_EQ(recreated._iUid, item._iUid);
	EXPECT_EQ(recreated._iPLStr, item._iPLStr);
	EXPECT_EQ(recreated._iCurs, item._iCurs);
	EXPECT_STREQ(recreated._iIName, item._iIName);
	UniqueItems.swap(originalUniques);
}

TEST_F(ItemsTest, UniqueVisionAndHealingPreventionPowers)
{
	UniqueItem unique {};
	unique.UIName = "Test hunter weapon";
	unique.UICurs = static_cast<item_cursor_graphic>(84);
	unique.UIItemId = AllItemsList[IDI_WARRIOR].iItemId;
	unique.UIMinLvl = 1;
	unique.UINumPL = 2;
	unique.powers[0] = { IPL_INFRAVISION, 0, 0 };
	unique.powers[1] = { IPL_NOHEALMON, 0, 0 };

	std::vector<UniqueItem> originalUniques;
	originalUniques.swap(UniqueItems);
	UniqueItems.push_back(unique);

	Item item {};
	SetupAllItems(*MyPlayer, item, IDI_WARRIOR, 12345, 10, 100, true, false);
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_UNIQUE);
	EXPECT_TRUE(HasAnyOf(item._iFlags, ItemSpecialEffect::Infravision));
	EXPECT_TRUE(HasAnyOf(item._iFlags, ItemSpecialEffect::PreventMonsterHealing));
	EXPECT_EQ(item._iCurs, 84);

	UniqueItems.swap(originalUniques);
}

TEST_F(ItemsTest, ItemInfravisionDoesNotChangeSpellDuration)
{
	Player &player = *MyPlayer;
	player._pInfraFlag = false;
	player._pIFlags = ItemSpecialEffect::None;
	EXPECT_FALSE(player.hasInfravision());

	player._pIFlags = ItemSpecialEffect::Infravision;
	EXPECT_TRUE(player.hasInfravision());
	EXPECT_FALSE(player._pInfraFlag);

	player._pInfraFlag = true;
	player._pIFlags = ItemSpecialEffect::None;
	EXPECT_TRUE(player.hasInfravision());

	player._pInfraFlag = false;
	EXPECT_FALSE(player.hasInfravision());
}

TEST_F(ItemsTest, AllDiabloUniquesCanDrop)
{
	GenerateAllUniques(false, 79);
}

TEST_F(ItemsTest, AllHellfireUniquesCanDrop)
{
	GenerateAllUniques(true, 99);
}

} // namespace
} // namespace devilution
