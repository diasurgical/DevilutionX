#include "player_test.hh"

#include <gtest/gtest.h>

#include "cursor.h"
#include "debug.h"
#include "engine/assets.hpp"
#include "init.hpp"
#include "inv.h"
#include "itemdat.h"
#include "items.h"
#include "monster.h"
#include "monstdat.h"
#include "objdat.h"
#include "playerdat.hpp"
#include "pfile.h"

using namespace devilution;

namespace devilution {
extern bool TestPlayerDoGotHit(Player &player);
extern bool TestPlayerPlrHitMonst(Player &player, Monster &monster);
}

int RunBlockTest(int frames, ItemSpecialEffect flags)
{
	devilution::Player &player = Players[0];

	player._pHFrames = frames;
	player._pIFlags = flags;
	// StartPlrHit compares damage (a 6 bit fixed point value) to character level to determine if the player shrugs off the hit.
	// We don't initialise player so this comparison can't be relied on, instead we use forcehit to ensure the player enters hit mode
	StartPlrHit(player, 0, true);

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
	ASSERT_TRUE(HaveMainData());
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
	ASSERT_EQ(player._pMaxHPBase, 2880);
	ASSERT_EQ(player._pHPBase, 2880);
	ASSERT_EQ(player.getBaseToBlock(), 20);
	ASSERT_EQ(player._pMaxManaBase, 1440);
	ASSERT_EQ(player._pManaBase, 1440);
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
	ASSERT_EQ(player._pHitPoints, 2880);
	ASSERT_EQ(player._pMaxHP, 2880);
	ASSERT_EQ(player._pMana, 1440);
	ASSERT_EQ(player._pMaxMana, 1440);
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

TEST(Player, CreatePlayer)
{
	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	// Please provide them so that the tests can run successfully
	ASSERT_TRUE(HaveMainData());

	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	AssertPlayer(Players[0]);
}

TEST(Player, PlrHitMonst_DualWield)
{
	// Setup
	LoadCoreArchives();
	LoadGameArchives();
	ASSERT_TRUE(HaveMainData());
	LoadPlayerDataFiles();
	LoadMonsterData();
	InitItemGFX(); // Needed for item attributes
	Players.resize(1);
	MyPlayer = &Players[0];
	MyPlayerId = 0;
	DebugGodMode = true;

	CreatePlayer(*MyPlayer, HeroClass::Warrior);
	MyPlayer->position.tile = { 50, 50 };

	// Set fixed damage to a predictable value
	MyPlayer->_pIMinDam = 10;
	MyPlayer->_pIMaxDam = 10;
	MyPlayer->_pIBonusDam = 0;
	MyPlayer->_pIBonusDamMod = 0;
	MyPlayer->_pDamageMod = 0;

	// Create a zombie
	auto monsterId = AddMonster({ 50, 51 }, Direction::South, MT_ZOMBIE, true);
	ASSERT_TRUE(monsterId);
	Monster &zombie = Monsters[*monsterId];
	int initialHP = zombie.hitPoints;

	// Create weapons
	Item sword, mace;
	GetItemAttrs(sword, IDI_SHORTSWORD, 1);
	GetItemAttrs(mace, IDI_SPIKEDCLUB, 1);

	// Case 1: Sword in right hand (main), Mace in left. Damage to undead should be halved.
	MyPlayer->InvBody[INVLOC_HAND_RIGHT] = sword;
	MyPlayer->InvBody[INVLOC_HAND_LEFT] = mace;
	CalcPlrInv(*MyPlayer, true);

	TestPlayerPlrHitMonst(*MyPlayer, zombie);

	// Base damage is 10. Sword vs Undead is 0.5x damage. So 5 damage.
	// The damage value is stored as a 6-bit fixed point number, so we shift it.
	int expectedDamage1 = 5 << 6;
	EXPECT_EQ(zombie.hitPoints, initialHP - expectedDamage1);

	// Case 2: Mace in right hand (main), Sword in left. Damage to undead should be 1.5x.
	zombie.hitPoints = initialHP; // Reset HP
	MyPlayer->InvBody[INVLOC_HAND_RIGHT] = mace;
	MyPlayer->InvBody[INVLOC_HAND_LEFT] = sword;
	CalcPlrInv(*MyPlayer, true);

	TestPlayerPlrHitMonst(*MyPlayer, zombie);

	// Base damage is 10. Mace vs Undead is 1.5x damage. So 15 damage.
	int expectedDamage2 = 15 << 6;
	EXPECT_EQ(zombie.hitPoints, initialHP - expectedDamage2);

	DebugGodMode = false;
}
