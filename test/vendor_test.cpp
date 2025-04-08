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

std::string itemtype_str(ItemType type);
std::string misctype_str(item_misc_id type);

MATCHER_P(SmithTypeMatch, i, "Valid Diablo item type from Griswold")
{
	if (arg >= ItemType::Sword && arg <= ItemType::HeavyArmor) return true;

	*result_listener << "At index " << i << ": Invalid item type " << itemtype_str(arg);
	return false;
}

MATCHER_P(SmithTypeMatchHf, i, "Valid Hellfire item type from Griswold")
{
	if (arg >= ItemType::Sword && arg <= ItemType::Staff) return true;

	*result_listener << "At index " << i << ": Invalid item type " << itemtype_str(arg);
	return false;
}

MATCHER_P(PremiumTypeMatch, i, "Valid premium items from Griswold")
{
	if (arg >= ItemType::Ring && arg <= ItemType::Amulet) return true;

	*result_listener << "At index " << i << ": Invalid item type " << itemtype_str(arg);
	return false;
}

MATCHER_P(WitchTypeMatch, i, "Valid item type from Adria")
{
	if (arg == ItemType::Misc || arg == ItemType::Staff) return true;

	*result_listener << "At index " << i << ": Invalid item type " << itemtype_str(arg);
	return false;
}

MATCHER_P(WitchMiscMatch, i, "Valid misc. item type from Adria")
{
	if (arg >= IMISC_ELIXSTR && arg <= IMISC_ELIXVIT) return true;
	if (arg >= IMISC_REJUV && arg <= IMISC_FULLREJUV) return true;
	if (arg >= IMISC_SCROLL && arg <= IMISC_SCROLLT) return true;
	if (arg >= IMISC_RUNEFIRST && arg <= IMISC_RUNELAST) return true;
	if (arg == IMISC_BOOK) return true;

	*result_listener << "At index " << i << ": Invalid misc. item type " << misctype_str(arg);
	return false;
}

MATCHER_P(HealerMiscMatch, i, "Valid misc. item type from Pepin")
{
	if (arg >= IMISC_ELIXSTR && arg <= IMISC_ELIXVIT) return true;
	if (arg >= IMISC_REJUV && arg <= IMISC_FULLREJUV) return true;
	if (arg >= IMISC_SCROLL && arg <= IMISC_SCROLLT) return true;

	*result_listener << "At index " << i << ": Invalid misc. item type " << misctype_str(arg);
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
		ASSERT_TRUE(HaveMainData());
		LoadPlayerDataFiles();
		LoadItemData();
		LoadSpellData();
	}
};

std::string itemtype_str(ItemType type)
{
	const std::string ITEM_TYPES[] = {
		"ItemType::Misc",
		"ItemType::Sword",
		"ItemType::Axe",
		"ItemType::Bow",
		"ItemType::Mace",
		"ItemType::Shield",
		"ItemType::LightArmor",
		"ItemType::Helm",
		"ItemType::MediumArmor",
		"ItemType::HeavyArmor",
		"ItemType::Staff",
		"ItemType::Gold",
		"ItemType::Ring",
		"ItemType::Amulet",
	};

	if (type == ItemType::None) return "ItemType::None";
	if (type < ItemType::Misc || type > ItemType::Amulet) return "ItemType does not exist!";
	return ITEM_TYPES[static_cast<int>(type)];
}

std::string misctype_str(item_misc_id type)
{
	const std::string MISC_TYPES[] = {
		// clang-format off
		"IMISC_NONE",		"IMISC_USEFIRST",	"IMISC_FULLHEAL",	"IMISC_HEAL",
		"IMISC_0x4",		"IMISC_0x5",		"IMISC_MANA",		"IMISC_FULLMANA",
		"IMISC_0x8",		"IMISC_0x9",		"IMISC_ELIXSTR",	"IMISC_ELIXMAG",
		"IMISC_ELIXDEX",	"IMISC_ELIXVIT",	"IMISC_0xE",		"IMISC_0xF",
		"IMISC_0x10",		"IMISC_0x11",		"IMISC_REJUV",		"IMISC_FULLREJUV",
		"IMISC_USELAST",	"IMISC_SCROLL",		"IMISC_SCROLLT",	"IMISC_STAFF",
		"IMISC_BOOK",		"IMISC_RING",		"IMISC_AMULET",		"IMISC_UNIQUE",
		"IMISC_0x1C",		"IMISC_OILFIRST",	"IMISC_OILOF",		"IMISC_OILACC",
		"IMISC_OILMAST",	"IMISC_OILSHARP",	"IMISC_OILDEATH",	"IMISC_OILSKILL",
		"IMISC_OILBSMTH",	"IMISC_OILFORT",	"IMISC_OILPERM",	"IMISC_OILHARD",
		"IMISC_OILIMP",		"IMISC_OILLAST",	"IMISC_MAPOFDOOM",	"IMISC_EAR",
		"IMISC_SPECELIX",	"IMISC_0x2D",		"IMISC_RUNEFIRST",	"IMISC_RUNEF",
		"IMISC_RUNEL",		"IMISC_GR_RUNEL",	"IMISC_GR_RUNEF",	"IMISC_RUNES",
		"IMISC_RUNELAST",	"IMISC_AURIC",		"IMISC_NOTE",		"IMISC_ARENAPOT"
		// clang-format on
	};

	if (type == IMISC_INVALID) return "IMISC_INVALID";
	if (type < IMISC_NONE || type > IMISC_ARENAPOT) return "IMISC does not exist!";
	return MISC_TYPES[static_cast<int>(type)];
}

int test_premium_qlvl(int *qlvls, int n_qlvls, int clvl, int plvl, bool hf)
{
	constexpr int QLVL_DELTAS[] = { -1, -1, 0, 0, 1, 2 };
	constexpr int QLVL_DELTAS_HF[] = { -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3 };

	for (int i = 0; i < n_qlvls; i++) {
		if (qlvls[i] == 0) {
			qlvls[i] = clvl + (hf ? QLVL_DELTAS_HF[i] : QLVL_DELTAS[i]);
			qlvls[i] = qlvls[i] < 1 ? 1 : qlvls[i];
		}
	}

	while (plvl < clvl) {
		plvl++;
		if (hf) {
			std::move(qlvls + 3, qlvls + 13, qlvls);
			qlvls[11] = qlvls[13];
			qlvls[13] = qlvls[14];
			qlvls[10] = QLVL_DELTAS_HF[10] + plvl;
			qlvls[12] = QLVL_DELTAS_HF[12] + plvl;
			qlvls[14] = QLVL_DELTAS_HF[14] + plvl;
		} else {
			std::move(qlvls + 2, qlvls + 5, qlvls);
			qlvls[4] = qlvls[5];
			qlvls[3] = QLVL_DELTAS[3] + plvl;
			qlvls[5] = QLVL_DELTAS[5] + plvl;
		}
	}

	return plvl;
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
	SpawnSmith(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItems);
	int n_items = 0;

	for (int i = 0; i < NumSmithBasicItems; i++) {
		if (SmithItems[i].isEmpty()) break;
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatch(i));
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, SmithGenHf)
{
	if (!HaveHellfire()) return;

	const int SEED = testing::UnitTest::GetInstance()->random_seed();

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->setCharacterLevel(25);

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumSmithBasicItemsHf; i++) {
		SmithItems[i].clear();
	}
	gbIsHellfire = true;

	SetRndSeed(SEED);
	SpawnSmith(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItemsHf);
	int n_items = 0;

	for (int i = 0; i < NumSmithBasicItemsHf; i++) {
		if (SmithItems[i].isEmpty()) break;
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatchHf(i));
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, PremiumQlvl)
{
	int qlvls[NumSmithItems] = {};
	int plvl = 1;

	// Clear global state for test, and force Diablo game mode
	for (int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = false;

	// Test level 1 character item qlvl
	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), 1, gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), plvl, gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Clear global state
	for (int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), 1, gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[3] = 0;
	qlvls[5] = 0;

	PremiumItems[0].clear();
	PremiumItems[3].clear();
	PremiumItems[5].clear();
	PremiumItemCount -= 3;
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItems, MyPlayer->getCharacterLevel(), plvl, gbIsHellfire);
	for (int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, PremiumQlvlHf)
{
	if (!HaveHellfire()) return;

	int qlvls[NumSmithItemsHf] = {};
	int plvl = 1;

	// Clear global state for test, and force Hellfire game mode
	for (int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = true;

	// Test level 1 character item qlvl
	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), 1, gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), plvl, gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Clear global state
	for (int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), 1, gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[7] = 0;
	qlvls[14] = 0;
	PremiumItems[0].clear();
	PremiumItems[7].clear();
	PremiumItems[14].clear();
	PremiumItemCount -= 3;
	SpawnPremium(*MyPlayer);
	plvl = test_premium_qlvl(qlvls, NumSmithItemsHf, MyPlayer->getCharacterLevel(), plvl, gbIsHellfire);
	for (int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
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
	SpawnWitch(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItems);

	int n_items = NumWitchPinnedItems;

	for (int i = 0; i < NumWitchPinnedItems; i++) {
		EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
	}

	for (int i = NumWitchPinnedItems; i < NumWitchItems; i++) {
		if (WitchItems[i].isEmpty()) break;
		EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch(i));

		if (WitchItems[i]._itype == ItemType::Misc) {
			EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch(i));
		}
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, WitchGenHf)
{
	if (!HaveHellfire()) return;

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
	SpawnWitch(16);

	SetRndSeed(SEED);
	const int N_PINNED_BOOKS = RandomIntLessThan(MAX_PINNED_BOOKS);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItemsHf);

	int n_books = 0;
	int n_items = NumWitchPinnedItems;

	for (int i = 0; i < NumWitchPinnedItems; i++) {
		EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
	}

	for (int i = NumWitchPinnedItems; i < NumWitchItemsHf; i++) {
		if (WitchItems[i].isEmpty()) break;
		EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch(i));

		if (WitchItems[i]._itype == ItemType::Misc) {
			EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch(i));
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
	SpawnHealer(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItems);
	int n_items = NumHealerPinnedItems;

	for (int i = 0; i < NumHealerPinnedItems; i++) {
		EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
	}

	for (int i = NumHealerPinnedItems; i < NumHealerItems; i++) {
		if (HealerItems[i].isEmpty()) break;
		EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
		EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch(i));
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

TEST_F(VendorTest, HealerGenHf)
{
	if (!HaveHellfire()) return;

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
	SpawnHealer(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItemsHf);
	int n_items = NumHealerPinnedItems;

	for (int i = 0; i < NumHealerPinnedItems; i++) {
		EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
	}

	for (int i = NumHealerPinnedItems; i < NumHealerItemsHf; i++) {
		if (HealerItems[i].isEmpty()) break;
		EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
		EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch(i));
		n_items++;
	}
	EXPECT_EQ(n_items, N_ITEMS);
}

} // namespace
} // namespace devilution
