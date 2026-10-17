#include "player_test.h"

#include <gtest/gtest.h>

#include "cursor.h"
#include "engine/assets.hpp"
#include "init.hpp"
#include "tables/monstdat.h"
#include "tables/objdat.h"
#include "tables/playerdat.hpp"
#include "utils/fixed_point.hpp"

using namespace devilution;

namespace devilution {
extern bool TestPlayerDoGotHit(Player &player);
}

int RunBlockTest(int frames, ItemSpecialEffect flags)
{
	devilution::Player &player = Players[0];

	player._pHFrames = frames;
	player._pIFlags = flags;
	// StartPlrHit compares damage (a 6 bit fixed point value) to character level to determine if the player shrugs off the hit.
	// We don't initialise player so this comparison can't be relied on, instead we use forcehit to ensure the player enters hit mode
	StartPlrHit(player, Fixed26_6::fromInt(0), true);

	int i = 1;
	for (; i < 100; i++) {
		TestPlayerDoGotHit(player);
		if (player._pmode != PM_GOTHIT)
			break;
		player.AnimInfo.currentFrame++;
	}

	return i;
}

constexpr ItemSpecialEffect Normal = ItemSpecialEffect::None;
constexpr ItemSpecialEffect Balance = ItemSpecialEffect::FastHitRecovery;
constexpr ItemSpecialEffect Stability = ItemSpecialEffect::FasterHitRecovery;
constexpr ItemSpecialEffect Harmony = ItemSpecialEffect::FastestHitRecovery;
constexpr ItemSpecialEffect BalanceStability = Balance | Stability;
constexpr ItemSpecialEffect BalanceHarmony = Balance | Harmony;
constexpr ItemSpecialEffect StabilityHarmony = Stability | Harmony;

constexpr int Warrior = 6;
constexpr int Rogue = 7;
constexpr int Sorcerer = 8;

struct BlockTestCase {
	int expectedRecoveryFrame;
	int maxRecoveryFrame;
	ItemSpecialEffect itemFlags;
};

BlockTestCase BlockData[] = {
	{ 6, Warrior, Normal },
	{ 7, Rogue, Normal },
	{ 8, Sorcerer, Normal },

	{ 5, Warrior, Balance },
	{ 6, Rogue, Balance },
	{ 7, Sorcerer, Balance },

	{ 4, Warrior, Stability },
	{ 5, Rogue, Stability },
	{ 6, Sorcerer, Stability },

	{ 3, Warrior, Harmony },
	{ 4, Rogue, Harmony },
	{ 5, Sorcerer, Harmony },

	{ 4, Warrior, BalanceStability },
	{ 5, Rogue, BalanceStability },
	{ 6, Sorcerer, BalanceStability },

	{ 3, Warrior, BalanceHarmony },
	{ 4, Rogue, BalanceHarmony },
	{ 5, Sorcerer, BalanceHarmony },

	{ 3, Warrior, StabilityHarmony },
	{ 4, Rogue, StabilityHarmony },
	{ 5, Sorcerer, StabilityHarmony },
};

TEST(Player, PM_DoGotHit)
{
	LoadCoreArchives();
	LoadGameArchives();
	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}
	LoadPlayerDataFiles();

	Players.resize(1);
	MyPlayer = &Players[0];
	for (size_t i = 0; i < sizeof(BlockData) / sizeof(*BlockData); i++) {
		EXPECT_EQ(BlockData[i].expectedRecoveryFrame, RunBlockTest(BlockData[i].maxRecoveryFrame, BlockData[i].itemFlags));
	}
}

static void AssertPlayer(devilution::Player &player)
{
	ASSERT_EQ(CountU8(player._pSplLvl, 64), 0);
	ASSERT_EQ(Count8(player.InvGrid, InventoryGridCells), 1);
	ASSERT_EQ(CountItems(player.InvBody, NUM_INVLOC), 1);
	ASSERT_EQ(CountItems(player.InvList, InventoryGridCells), 1);
	ASSERT_EQ(CountItems(player.SpdList, MaxBeltItems), 2);
	ASSERT_EQ(CountItems(&player.HoldItem, 1), 0);

	ASSERT_EQ(player.position.tile.x, 0);
	ASSERT_EQ(player.position.tile.y, 0);
	ASSERT_EQ(player.position.future.x, 0);
	ASSERT_EQ(player.position.future.y, 0);
	ASSERT_EQ(player.plrlevel, 0);
	ASSERT_EQ(player.destAction, 0);
	ASSERT_STREQ(player._pName, "");
	ASSERT_EQ(player._pClass, HeroClass::Rogue);
	ASSERT_EQ(player._pBaseStr, 20);
	ASSERT_EQ(player._pStrength, 20);
	ASSERT_EQ(player._pBaseMag, 15);
	ASSERT_EQ(player._pMagic, 15);
	ASSERT_EQ(player._pBaseDex, 30);
	ASSERT_EQ(player._pDexterity, 30);
	ASSERT_EQ(player._pBaseVit, 20);
	ASSERT_EQ(player._pVitality, 20);
	ASSERT_EQ(player.getCharacterLevel(), 1);
	ASSERT_EQ(player._pStatPts, 0);
	ASSERT_EQ(player._pExperience, 0);
	ASSERT_EQ(player._pGold, 100);
	ASSERT_EQ(player._pMaxHPBase.raw(), 2880);
	ASSERT_EQ(player._pHPBase.raw(), 2880);
	ASSERT_EQ(player.getBaseToBlock(), 20);
	ASSERT_EQ(player._pMaxManaBase.raw(), 1440);
	ASSERT_EQ(player._pManaBase.raw(), 1440);
	ASSERT_EQ(player._pMemSpells, 0);
	ASSERT_EQ(player._pNumInv, 1);
	ASSERT_EQ(player.wReflections, 0);
	ASSERT_EQ(player.pTownWarps, 0);
	ASSERT_EQ(player.pDungMsgs, 0);
	ASSERT_EQ(player.pDungMsgs2, 0);
	ASSERT_EQ(player.pLvlLoad, 0);
	ASSERT_EQ(player.pDiabloKillLevel, 0);
	ASSERT_EQ(player.pManaShield, 0);
	ASSERT_EQ(player.pDamAcFlags, ItemSpecialEffectHf::None);

	ASSERT_EQ(player._pmode, 0);
	ASSERT_EQ(Count8(player.walkpath, MaxPathLengthPlayer), 0);
	ASSERT_EQ(player.queuedSpell.spellId, SpellID::Null);
	ASSERT_EQ(player.queuedSpell.spellType, SpellType::Skill);
	ASSERT_EQ(player.queuedSpell.spellFrom, 0);
	ASSERT_EQ(player.inventorySpell, SpellID::Null);
	ASSERT_EQ(player._pRSpell, SpellID::TrapDisarm);
	ASSERT_EQ(player._pRSplType, SpellType::Skill);
	ASSERT_EQ(player._pSBkSpell, SpellID::Null);
	ASSERT_EQ(player._pAblSpells, 134217728);
	ASSERT_EQ(player._pScrlSpells, 0);
	ASSERT_EQ(player._pSpellFlags, SpellFlag::None);
	ASSERT_EQ(player._pBlockFlag, 0);
	ASSERT_EQ(player._pLightRad, 10);
	ASSERT_EQ(player._pDamageMod, 0);
	ASSERT_EQ(player._pHitPoints.raw(), 2880);
	ASSERT_EQ(player._pMaxHP.raw(), 2880);
	ASSERT_EQ(player._pMana.raw(), 1440);
	ASSERT_EQ(player._pMaxMana.raw(), 1440);
	ASSERT_EQ(player.getNextExperienceThreshold(), 2000);
	ASSERT_EQ(player._pMagResist, 0);
	ASSERT_EQ(player._pFireResist, 0);
	ASSERT_EQ(player._pLghtResist, 0);
	ASSERT_EQ(CountBool(player._pLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(CountBool(player._pSLvlVisited, NUMLEVELS), 0);
	// This test case uses a Rogue, starting loadout is a short bow with damage 1-4
	ASSERT_EQ(player._pIMinDam, 1);
	ASSERT_EQ(player._pIMaxDam, 4);
	ASSERT_EQ(player._pIAC, 0);
	ASSERT_EQ(player._pIBonusDam, 0);
	ASSERT_EQ(player._pIBonusToHit, 0);
	ASSERT_EQ(player._pIBonusAC, 0);
	ASSERT_EQ(player._pIBonusDamMod, 0);
	ASSERT_EQ(player._pISpells, 0);
	ASSERT_EQ(player._pIFlags, ItemSpecialEffect::None);
	ASSERT_EQ(player._pIGetHit, 0);
	ASSERT_EQ(player._pISplLvlAdd, 0);
	ASSERT_EQ(player._pIEnAc, 0);
	ASSERT_EQ(player._pIFMinDam, 0);
	ASSERT_EQ(player._pIFMaxDam, 0);
	ASSERT_EQ(player._pILMinDam, 0);
	ASSERT_EQ(player._pILMaxDam, 0);
}

TEST(Player, ShadowRulesAreLoaded)
{
	LoadCoreArchives();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadObjectData();

	// One of the warrior's sprite files does not reproduce exactly, which the tags record.
	EXPECT_EQ(GetPlayerSpriteDataForClass(HeroClass::Warrior).shadowRule, (ShadowRule { ShadowRuleType::FirstFrame, 0, ShadowRuleFlags::Unexplained | ShadowRuleFlags::Stray }));
	EXPECT_EQ(GetPlayerSpriteDataForClass(HeroClass::Rogue).shadowRule, (ShadowRule { ShadowRuleType::FirstFrame, 0 }));
	EXPECT_EQ(GetPlayerSpriteDataForClass(HeroClass::Monk).shadowRule, (ShadowRule { ShadowRuleType::EachFrame, 0 }));
	// Sprites that were never analysed have an empty value.
	EXPECT_EQ(GetPlayerSpriteDataForClass(HeroClass::Bard).shadowRule, (ShadowRule { ShadowRuleType::Unknown, 0 }));

	// One rule per animation: stand, walk, attack, hit, death, special.
	const MonsterData &zombie = MonstersData[MT_NZOMBIE];
	EXPECT_EQ(zombie.shadowRules[0], (ShadowRule { ShadowRuleType::FirstFrame, 0 }));
	EXPECT_EQ(zombie.shadowRules[5], (ShadowRule { ShadowRuleType::None, 0 }));
	// Flying monsters have their baseline below the sprite, except when they fall to the ground.
	const MonsterData &fiend = MonstersData[MT_FIEND];
	EXPECT_EQ(fiend.shadowRules[0], (ShadowRule { ShadowRuleType::FirstFrame, 15 }));
	EXPECT_EQ(fiend.shadowRules[4], (ShadowRule { ShadowRuleType::FirstFrame, 0 }));
	const MonsterData &golem = MonstersData[MT_GOLEM];
	EXPECT_EQ(golem.shadowRules[1], (ShadowRule { ShadowRuleType::EachFrame, 0 }));
	// Monsters whose sprites are missing from the game data have an empty value.
	EXPECT_EQ(MonstersData[MT_WYRM].shadowRules[0], (ShadowRule { ShadowRuleType::Unknown, 0 }));

	EXPECT_EQ(AllObjects[OBJ_BARREL].shadowRule, (ShadowRule { ShadowRuleType::FirstFrame, 0 }));
	EXPECT_EQ(AllObjects[OBJ_CHEST1].shadowRule, (ShadowRule { ShadowRuleType::EachFrame, 0 }));
	// The cauldron's shadow cannot be told apart from its black side.
	EXPECT_EQ(AllObjects[OBJ_CAULDRON].shadowRule, (ShadowRule { ShadowRuleType::Baked, 0 }));
	EXPECT_EQ(AllObjects[OBJ_L1LIGHT].shadowRule, (ShadowRule { ShadowRuleType::None, 0 }));
	// Doors have black pixels but no cast shadow.
	EXPECT_EQ(AllObjects[OBJ_L1LDOOR].shadowRule, (ShadowRule { ShadowRuleType::None, 0 }));
}

TEST(Player, CreatePlayer)
{
	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	AssertPlayer(Players[0]);
}
