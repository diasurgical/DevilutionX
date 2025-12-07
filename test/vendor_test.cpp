#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "items.h"
#include "player.h"
#include "playerdat.hpp"
#include "stores.h"

#include "engine/assets.hpp"
#include "engine/random.hpp"

namespace devilution {
namespace {

using ::testing::AnyOf;
using ::testing::Eq;

constexpr int SEED = 75357;
constexpr const char MissingMpqAssetsSkipReason[] = "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test suite";

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
		if (missingMpqAssets_) {
			GTEST_SKIP() << MissingMpqAssetsSkipReason;
		}

		Players.resize(1);
		MyPlayer = &Players[0];
		gbIsHellfire = false;
		PremiumItemLevel = 1;
		CreatePlayer(*MyPlayer, HeroClass::Warrior);
		SetRndSeed(SEED);
	}

	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();
		missingMpqAssets_ = !HaveMainData();
		if (missingMpqAssets_) {
			return;
		}
		LoadPlayerDataFiles();
		LoadItemData();
		LoadSpellData();
	}

private:
	static bool missingMpqAssets_;
};

bool VendorTest::missingMpqAssets_ = false;

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

TEST_F(VendorTest, SmithGen)
{
	MyPlayer->setCharacterLevel(25);
	SmithItems.clear();
	SpawnSmith(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItems);
	EXPECT_EQ(SmithItems.size(), N_ITEMS);
	EXPECT_LE(SmithItems.size(), NumSmithBasicItems);

	for (size_t i = 0; i < SmithItems.size(); i++) {
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatch(i));
	}
}

TEST_F(VendorTest, SmithGenHf)
{
	MyPlayer->setCharacterLevel(25);
	SmithItems.clear();
	gbIsHellfire = true;
	SpawnSmith(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumSmithBasicItemsHf);
	EXPECT_EQ(SmithItems.size(), N_ITEMS);
	EXPECT_LE(SmithItems.size(), NumSmithBasicItemsHf);

	for (size_t i = 0; i < SmithItems.size(); i++) {
		EXPECT_THAT(SmithItems[i]._itype, SmithTypeMatchHf(i));
	}
}

TEST_F(VendorTest, PremiumQlvl1to5)
{
	// Test starting the game as a level 1 character
	MyPlayer->setCharacterLevel(1);
	PremiumItems.clear();
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 1, 1, 1, 1, 2, 3 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 4, 4, 5, 5, 6, 7 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, PremiumQlvl25)
{
	constexpr int QLVLS[] = { 24, 24, 25, 25, 26, 27 };

	// Test starting the game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	PremiumItems.clear();
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 3);
	ReplacePremium(*MyPlayer, 5);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, PremiumQlvl30Plus)
{
	constexpr int QLVLS[] = { 30, 30, 30, 30, 30, 30 };

	// Finally test level 30+ characters
	MyPlayer->setCharacterLevel(31);
	PremiumItems.clear();
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 3);
	ReplacePremium(*MyPlayer, 5);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test 30+ levelling
	MyPlayer->setCharacterLevel(35);
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 3);
	ReplacePremium(*MyPlayer, 5);
	EXPECT_EQ(PremiumItems.size(), NumSmithItems);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatch(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, HfPremiumQlvl1to5)
{
	// Test level 1 character item qlvl
	MyPlayer->setCharacterLevel(1);
	PremiumItems.clear();
	gbIsHellfire = true;
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test level ups
	MyPlayer->setCharacterLevel(5);
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, HfPremiumQlvl25)
{
	// Test starting game as a level 25 character
	MyPlayer->setCharacterLevel(25);
	PremiumItems.clear();
	gbIsHellfire = true;
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 28 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 7);
	ReplacePremium(*MyPlayer, 14);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 24, 23, 23, 24, 24, 24, 25, 26, 25, 26, 26, 26, 27, 27, 28 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, HfPremiumQlvl30Plus)
{
	// Finally test level 30+ characters
	MyPlayer->setCharacterLevel(31);
	PremiumItems.clear();
	gbIsHellfire = true;
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 29, 29, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 7);
	ReplacePremium(*MyPlayer, 14);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		constexpr int QLVLS[] = { 30, 29, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	constexpr int QLVLS[] = { 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };

	// Test 30+ levelling
	MyPlayer->setCharacterLevel(35);
	SpawnPremium(*MyPlayer);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}

	// Test buying select items
	ReplacePremium(*MyPlayer, 0);
	ReplacePremium(*MyPlayer, 7);
	ReplacePremium(*MyPlayer, 14);
	EXPECT_EQ(PremiumItems.size(), NumSmithItemsHf);

	for (size_t i = 0; i < PremiumItems.size(); i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, QLVLS[i]) << "Index: " << i;
		EXPECT_THAT(PremiumItems[i]._itype, AnyOf(SmithTypeMatchHf(i), PremiumTypeMatch(i)));
	}
}

TEST_F(VendorTest, WitchGen)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };

	MyPlayer->setCharacterLevel(25);
	WitchItems.clear();
	SpawnWitch(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItems);
	EXPECT_EQ(WitchItems.size(), N_ITEMS);
	EXPECT_LE(WitchItems.size(), NumWitchItems);

	for (size_t i = 0; i < WitchItems.size(); i++) {
		if (i < NumWitchPinnedItems) {
			EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
		} else {
			EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch(i));
			if (WitchItems[i]._itype == ItemType::Misc) {
				EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch(i));
			}
		}
	}
}

TEST_F(VendorTest, WitchGenHf)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };
	constexpr int MAX_PINNED_BOOKS = 4;

	MyPlayer->setCharacterLevel(25);
	WitchItems.clear();
	gbIsHellfire = true;
	SpawnWitch(16);

	SetRndSeed(SEED);
	const int N_PINNED_BOOKS = RandomIntLessThan(MAX_PINNED_BOOKS);
	const int N_ITEMS = RandomIntBetween(10, NumWitchItemsHf);
	EXPECT_EQ(WitchItems.size(), N_ITEMS);
	EXPECT_LE(WitchItems.size(), NumWitchItemsHf);

	int n_books = 0;
	for (size_t i = 0; i < WitchItems.size(); i++) {
		if (i < NumWitchPinnedItems) {
			EXPECT_EQ(WitchItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
		} else {
			EXPECT_THAT(WitchItems[i]._itype, WitchTypeMatch(i));
			if (WitchItems[i]._itype == ItemType::Misc) {
				EXPECT_THAT(WitchItems[i]._iMiscId, WitchMiscMatch(i));
			}
			if (WitchItems[i]._iMiscId == IMISC_BOOK) n_books++;
		}
	}
	EXPECT_GE(n_books, N_PINNED_BOOKS);
}

TEST_F(VendorTest, HealerGen)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };

	MyPlayer->setCharacterLevel(25);
	HealerItems.clear();
	SpawnHealer(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItems);
	EXPECT_EQ(HealerItems.size(), N_ITEMS);
	EXPECT_LE(HealerItems.size(), NumHealerItems);

	for (size_t i = 0; i < HealerItems.size(); i++) {
		if (i < NumHealerPinnedItems) {
			EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
		} else {
			EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
			EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch(i));
		}
	}
}

TEST_F(VendorTest, HealerGenHf)
{
	constexpr _item_indexes PINNED_ITEMS[] = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };

	MyPlayer->setCharacterLevel(25);
	HealerItems.clear();
	gbIsHellfire = true;
	SpawnHealer(16);

	SetRndSeed(SEED);
	const int N_ITEMS = RandomIntBetween(10, NumHealerItemsHf);
	EXPECT_EQ(HealerItems.size(), N_ITEMS);
	EXPECT_LE(HealerItems.size(), NumHealerItemsHf);

	for (size_t i = 0; i < HealerItems.size(); i++) {
		if (i < NumHealerPinnedItems) {
			EXPECT_EQ(HealerItems[i].IDidx, PINNED_ITEMS[i]) << "Index: " << i;
		} else {
			EXPECT_THAT(HealerItems[i]._itype, Eq(ItemType::Misc));
			EXPECT_THAT(HealerItems[i]._iMiscId, HealerMiscMatch(i));
		}
	}
}

} // namespace
} // namespace devilution
