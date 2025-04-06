#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "init.h"
#include "items.h"
#include "player.h"
#include "playerdat.hpp"
#include "stores.h"

#include "engine/random.hpp"

namespace devilution {
namespace {

using ::testing::AnyOf;
using ::testing::Eq;

MATCHER(SmithTypeMatch, "Valid Diablo item type from Griswold")
{
	return arg >= ItemType::Sword && arg <= ItemType::HeavyArmor;
}

MATCHER(SmithTypeMatchHf, "Valid Hellfire item type from Griswold")
{
	return arg >= ItemType::Sword && arg <= ItemType::Staff;
}

MATCHER(PremiumTypeMatch, "Valid premium items from Griswold")
{
	return arg >= ItemType::Ring || arg <= ItemType::Amulet;
}

MATCHER(WitchTypeMatch, "Valid item type from Adria")
{
	return arg == ItemType::Misc || arg == ItemType::Staff;
}

MATCHER(WitchMiscMatch, "Valid misc. item type from Adria")
{
	if (arg >= IMISC_ELIXSTR && arg <= IMISC_ELIXVIT) return true;
	if (arg >= IMISC_REJUV && arg <= IMISC_FULLREJUV) return true;
	if (arg >= IMISC_SCROLL && arg <= IMISC_SCROLLT) return true;
	if (arg >= IMISC_RUNEFIRST && arg <= IMISC_RUNELAST) return true;
	return arg == IMISC_BOOK;
}

MATCHER(HealerMiscMatch, "Valid misc. item type from Pepin")
{
	if (arg >= IMISC_ELIXSTR && arg <= IMISC_ELIXVIT) return true;
	if (arg >= IMISC_REJUV && arg <= IMISC_FULLREJUV) return true;
	if (arg >= IMISC_SCROLL && arg <= IMISC_SCROLLT) return true;
	return false;
}

class VendorTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
	}

	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();
		ASSERT_TRUE(HaveSpawn() || HaveDiabdat());
		LoadPlayerDataFiles();
		LoadItemData();
		LoadSpellData();
	}
};

void test_premium_qlvl(int *qlvls, int n_qlvls, int clvl, bool hf)
{
	constexpr int QLVL_DELTAS[] = { -1, -1, 0, 0, 1, 2 };
	constexpr int QLVL_DELTAS_HF[] = { -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3 };

	int lvl = 1;

	for (int i = 0; i < n_qlvls; i++) {
		if (qlvls[i] == 0) {
			qlvls[i] = lvl + (hf ? QLVL_DELTAS_HF[i] : QLVL_DELTAS[i]);
			qlvls[i] = qlvls[i] < 1 ? 1 : qlvls[i];
		}
	}

	while (lvl++ < clvl) {
		if (hf) {
			std::move(qlvls + 3, qlvls + 13, qlvls);
			qlvls[11] = qlvls[13];
			qlvls[13] = qlvls[14];
			qlvls[10] = QLVL_DELTAS_HF[10] + lvl;
			qlvls[12] = QLVL_DELTAS_HF[12] + lvl;
			qlvls[14] = QLVL_DELTAS_HF[14] + lvl;
		} else {
			std::move(qlvls + 2, qlvls + 5, qlvls);
			qlvls[4] = qlvls[5];
			qlvls[3] = QLVL_DELTAS[3] + lvl;
			qlvls[5] = QLVL_DELTAS[5] + lvl;
		}
	}
}

TEST_F(VendorTest, SmithGen)
{
	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Diablo game mode
	for (int i = 0; i < NumSmithBasicItemsHf; i++) {
		SmithItems[i].clear();
	}
	gbIsHellfire = false;

	SetRndSeed(SEED);
	SpawnSmith(25);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItems);
	int n_items = 0;

	for (int i = 0; i < NumSmithBasicItems; i++) {
		if (SmithItems[i].isEmpty()) break;
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatch());
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, SmithGenHf)
{
	ASSERT_TRUE(HaveHellfire());

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumSmithBasicItemsHf; i++) {
		SmithItems[i].clear();
	}
	gbIsHellfire = true;

	SetRndSeed(SEED);
	SpawnSmith(25);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItemsHf);
	int n_items = 0;

	for (int i = 0; i < NumSmithBasicItemsHf; i++) {
		if (SmithItems[i].isEmpty()) break;
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatchHf());
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, PremiumQlvl)
{
	int qlvls[NumSmithItems] = {};

	// Clear global state for test, and force Diablo game mode
	for (int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = false;

	// Test level 1 character item qlvl
	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Clear global state
	for (int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[3] = 0;
	qlvls[5] = 0;

	PremiumItems[0].clear();
	PremiumItems[3].clear();
	PremiumItems[5].clear();
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(), PremiumTypeMatch()));
	}
}

TEST_F(VendorTest, PremiumQlvlHf)
{
	ASSERT_TRUE(HaveHellfire());

	int qlvls[NumSmithItemsHf] = {};

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = true;

	// Test level 1 character item qlvl
	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Clear global state
	for (int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(), PremiumTypeMatch()));
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[7] = 0;
	qlvls[14] = 0;
	PremiumItems[0].clear();
	PremiumItems[7].clear();
	PremiumItems[14].clear();
	SpawnPremium(*MyPlayer);
	test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}
}

TEST_F(VendorTest, WitchGen)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Diablo game mode
	for (int i = 0; i < NumWitchItemsHf; i++) {
		WitchItems[i].clear();
	}
	gbIsHellfire = false;

	SetRndSeed(SEED);
	SpawnWitch(25);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItems);

	int n_items = NumWitchPinnedItems;

	for (int i = 0; i < NumWitchPinnedItems; i++) {
		EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]);
	}

	for (int i = NumWitchPinnedItems; i < NumWitchItems; i++) {
		if (WitchItems[i].isEmpty()) break;
		EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch());

		if (WitchItems[i]._itype == ItemType::Misc) {
			EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch());
		}
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, WitchGenHf)
{
	ASSERT_TRUE(HaveHellfire());

	constexpr _item_indexes PINNED_ITEMS[] = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };
	constexpr int MAX_PINNED_BOOKS = 4;

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumWitchItemsHf; i++) {
		WitchItems[i].clear();
	}
	gbIsHellfire = true;

	SetRndSeed(SEED);
	SpawnWitch(25);

	SetRndSeed(SEED);
	const int N_PINNED_BOOKS = RandomIntLessThan(MAX_PINNED_BOOKS);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItemsHf);

	int n_books = 0;
	int n_items = NumWitchPinnedItems;

	for (int i = 0; i < NumWitchPinnedItems; i++) {
		EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]);
	}

	for (int i = NumWitchPinnedItems; i < NumWitchItemsHf; i++) {
		if (WitchItems[i].isEmpty()) break;
		EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch());

		if (WitchItems[i]._itype == ItemType::Misc) {
			EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch());
		}
		if (WitchItems[i]._iMiscId == IMISC_BOOK) n_books++;
		n_items++;
	}
	EXPECT_GE(n_books, N_PINNED_BOOKS);
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, HealerGen)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Diablo game mode
	for (int i = 0; i < NumHealerItemsHf; i++) {
		HealerItems[i].clear();
	}
	gbIsHellfire = false;

	SetRndSeed(SEED);
	SpawnHealer(25);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItems);
	int n_items = NumHealerPinnedItems;

	for (int i = 0; i < NumHealerPinnedItems; i++) {
		EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]);
	}

	for (int i = NumHealerPinnedItems; i < NumHealerItems; i++) {
		if (HealerItems[i].isEmpty()) break;
		EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
		EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch());
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, HealerGenHf)
{
	ASSERT_TRUE(HaveHellfire());

	constexpr _item_indexes PINNED_ITEMS[] = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumHealerItemsHf; i++) {
		HealerItems[i].clear();
	}
	gbIsHellfire = true;

	SetRndSeed(SEED);
	SpawnHealer(25);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItemsHf);
	int n_items = NumHealerPinnedItems;

	for (int i = 0; i < NumHealerPinnedItems; i++) {
		EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]);
	}

	for (int i = NumHealerPinnedItems; i < NumHealerItemsHf; i++) {
		if (HealerItems[i].isEmpty()) break;
		EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
		EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch());
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

} // namespace
} // namespace devilution
