#include <gtest/gtest.h>

#include "init.h"
#include "items.h"
#include "stores.h"
#include "player.h"
#include "playerdat.hpp"

namespace devilution {
namespace {

constexpr int QLVL_DELTAS[] = { -1, -1, 0, 0, 1, 2 };
constexpr int QLVL_DELTAS_HF[] = { -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3 };

void test_premium_qlvl(int *qlvls, int n_qlvls, int clvl, bool hf)
{
	int lvl = 1;

	for(int i = 0; i < n_qlvls; i++) {
		if(qlvls[i] == 0) {
			qlvls[i] = lvl + (hf ? QLVL_DELTAS_HF[i] : QLVL_DELTAS[i]);
			qlvls[i] = qlvls[i] < 1 ? 1 : qlvls[i];
		}
	}

	while(lvl++ < clvl) {
		if(hf) {
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

TEST(PremiumTest, PremiumQlvl)
{
	Player player;

	int qlvls[NumSmithItems] = {};

	LoadCoreArchives();
	LoadGameArchives();
	ASSERT_TRUE(HaveSpawn() || HaveDiabdat());
	LoadPlayerDataFiles();
	LoadItemData();

	// Clear global state for test, and force Diablo game mode
	for(int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = false;

	// Test level 1 character item qlvl
	CreatePlayer(player, HeroClass::Warrior);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItems, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test level ups
	player.setCharacterLevel(5);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItems, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Clear global state
	for(int i = 0; i < NumSmithItems; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	player.setCharacterLevel(25);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItems, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[3] = 0;
	qlvls[5] = 0;

	PremiumItems[0].clear();
	PremiumItems[3].clear();
	PremiumItems[5].clear();
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItems, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItems; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}
}

TEST(PremiumTest, PremiumQlvlHf)
{
	Player player;
	int qlvls[NumSmithItemsHf] = {};

	LoadCoreArchives();
	LoadGameArchives();
	ASSERT_TRUE(HaveHellfire() && (HaveSpawn() || HaveDiabdat()));
	LoadPlayerDataFiles();
	LoadItemData();

	// Clear global state for test, and force Hellfire game mode
	for(int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;
	gbIsHellfire = true;

	// Test level 1 character item qlvl
	CreatePlayer(player, HeroClass::Warrior);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItemsHf, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test level ups
	player.setCharacterLevel(5);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItemsHf, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Clear global state
	for(int i = 0; i < NumSmithItemsHf; i++) {
		PremiumItems[i].clear();
	}
	PremiumItemLevel = 1;

	// Test starting game as a level 25 character
	player.setCharacterLevel(25);
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItemsHf, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}

	// Test buying select items
	qlvls[0] = 0;
	qlvls[7] = 0;
	qlvls[14] = 0;
	PremiumItems[0].clear();
	PremiumItems[7].clear();
	PremiumItems[14].clear();
	SpawnPremium(player);
	test_premium_qlvl(qlvls, NumSmithItemsHf, player.getCharacterLevel(), gbIsHellfire);
	for(int i = 0; i < NumSmithItemsHf; i++) {
		EXPECT_EQ(PremiumItems[i]._iCreateInfo & CF_LEVEL, qlvls[i]);
	}
}

} // namespace
} // namespace devilution
