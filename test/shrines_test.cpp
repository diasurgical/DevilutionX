/**
 * @file shrines_test.cpp
 *
 * Regression tests for shrine effects driven by shrines.tsv, checking that the
 * data-driven implementation reproduces the original (vanilla) behaviour.
 */
#include <algorithm>
#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "automap.h"
#include "engine/random.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "missiles.h"
#include "objects.h"
#include "player.h"
#include "shrines.hpp"
#include "tables/shrinedat.hpp"
#include "tables/spelldat.h"
#include "ui_test.hpp"

namespace devilution {
namespace {

class ShrinesTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();
		if (IsSkipped())
			return;
		LoadMissileData();
		LoadShrineData();
		StripPlayer();
	}

	static const ShrineData &FindShrine(ShrineData::OperateFn operateFn)
	{
		const auto it = std::find_if(ShrinesData.begin(), ShrinesData.end(),
		    [operateFn](const ShrineData &data) { return data.operateFn == operateFn; });
		EXPECT_NE(it, ShrinesData.end());
		return *it;
	}

	static int Operate(const ShrineData &shrine, uint32_t seed)
	{
		DiabloGenerator rng(seed);
		return shrine.operateFn(rng, *MyPlayer, MyPlayer->position.tile, shrine.powerParam1, shrine.powerParam2);
	}

	static int Operate(Player &player, const ShrineData &shrine, uint32_t seed)
	{
		DiabloGenerator rng(seed);
		return shrine.operateFn(rng, player, player.position.tile, shrine.powerParam1, shrine.powerParam2);
	}

	static void Equip(inv_body_loc loc, int durability, int maxDurability)
	{
		Item &item = MyPlayer->InvBody[loc];
		item = {};
		item._itype = ItemType::LightArmor;
		item._iDurability = durability;
		item._iMaxDur = maxDurability;
	}
};

TEST_F(ShrinesTest, MysteriousAddsFiveToOneAttributeAndMinusOneToTheRest)
{
	const ShrineData &mysterious = FindShrine(OperateShrineMysterious);

	for (uint32_t seed = 0; seed < 32; ++seed) {
		MyPlayer->_pBaseStr = MyPlayer->_pStrength = 10;
		MyPlayer->_pBaseMag = MyPlayer->_pMagic = 12;
		MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 9;
		MyPlayer->_pBaseVit = MyPlayer->_pVitality = 11;

		const int beforeStr = MyPlayer->_pBaseStr;
		const int beforeMag = MyPlayer->_pBaseMag;
		const int beforeDex = MyPlayer->_pBaseDex;
		const int beforeVit = MyPlayer->_pBaseVit;

		EXPECT_EQ(Operate(mysterious, seed), 1);

		const int strDelta = MyPlayer->_pBaseStr - beforeStr;
		const int magDelta = MyPlayer->_pBaseMag - beforeMag;
		const int dexDelta = MyPlayer->_pBaseDex - beforeDex;
		const int vitDelta = MyPlayer->_pBaseVit - beforeVit;

		EXPECT_TRUE(strDelta == 5 || strDelta == -1) << "seed " << seed;
		EXPECT_TRUE(magDelta == 5 || magDelta == -1) << "seed " << seed;
		EXPECT_TRUE(dexDelta == 5 || dexDelta == -1) << "seed " << seed;
		EXPECT_TRUE(vitDelta == 5 || vitDelta == -1) << "seed " << seed;

		const int bonusCount = (strDelta == 5) + (magDelta == 5) + (dexDelta == 5) + (vitDelta == 5);
		EXPECT_EQ(bonusCount, 1) << "seed " << seed;
	}
}

TEST_F(ShrinesTest, MysteriousDoesNotReduceStatsBelowZero)
{
	const ShrineData &mysterious = FindShrine(OperateShrineMysterious);

	for (uint32_t seed = 0; seed < 32; ++seed) {
		MyPlayer->_pBaseStr = MyPlayer->_pStrength = 0;
		MyPlayer->_pBaseMag = MyPlayer->_pMagic = 0;
		MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 0;
		MyPlayer->_pBaseVit = MyPlayer->_pVitality = 0;

		EXPECT_EQ(Operate(mysterious, seed), 1);

		EXPECT_GE(MyPlayer->_pBaseStr, 0);
		EXPECT_GE(MyPlayer->_pBaseMag, 0);
		EXPECT_GE(MyPlayer->_pBaseDex, 0);
		EXPECT_GE(MyPlayer->_pBaseVit, 0);
	}
}

TEST_F(ShrinesTest, MysteriousRespectsStatCaps)
{
	const ShrineData &mysterious = FindShrine(OperateShrineMysterious);

	const int maxStr = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength);
	const int maxMag = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Magic);
	const int maxDex = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Dexterity);
	const int maxVit = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Vitality);

	for (uint32_t seed = 0; seed < 32; ++seed) {
		MyPlayer->_pBaseStr = MyPlayer->_pStrength = maxStr - 1;
		MyPlayer->_pBaseMag = MyPlayer->_pMagic = maxMag - 2;
		MyPlayer->_pBaseDex = MyPlayer->_pDexterity = maxDex - 3;
		MyPlayer->_pBaseVit = MyPlayer->_pVitality = maxVit - 4;

		EXPECT_EQ(Operate(mysterious, seed), 1);

		EXPECT_LE(MyPlayer->_pBaseStr, maxStr);
		EXPECT_LE(MyPlayer->_pBaseMag, maxMag);
		EXPECT_LE(MyPlayer->_pBaseDex, maxDex);
		EXPECT_LE(MyPlayer->_pBaseVit, maxVit);
	}
}

TEST_F(ShrinesTest, HiddenSingleItemLosesTenDurability)
{
	const ShrineData &hidden = FindShrine(OperateShrineHidden);

	// Vanilla: +10 to every item, then -20 to one random item, so a lone item nets -10.
	Equip(INVLOC_CHEST, 30, 30);
	EXPECT_EQ(Operate(hidden, 42), 1);
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_CHEST]._iDurability, 20);
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_CHEST]._iMaxDur, 20);
}

TEST_F(ShrinesTest, HiddenClampsDurabilityToOne)
{
	const ShrineData &hidden = FindShrine(OperateShrineHidden);

	// 5/30 -> 15/40 -> -5/20, and vanilla clamps durability to 1.
	Equip(INVLOC_CHEST, 5, 30);
	Operate(hidden, 42);
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_CHEST]._iDurability, 1);
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_CHEST]._iMaxDur, 20);
}

TEST_F(ShrinesTest, HiddenOneItemLosesTheOthersGain)
{
	const ShrineData &hidden = FindShrine(OperateShrineHidden);

	for (uint32_t seed = 0; seed < 16; seed++) {
		Equip(INVLOC_CHEST, 30, 30);
		Equip(INVLOC_HEAD, 40, 50);
		Operate(hidden, seed);

		const Item &chest = MyPlayer->InvBody[INVLOC_CHEST];
		const Item &head = MyPlayer->InvBody[INVLOC_HEAD];
		const bool chestPicked = chest._iMaxDur == 20;
		if (chestPicked) {
			EXPECT_EQ(chest._iDurability, 20) << "seed " << seed;
			EXPECT_EQ(head._iDurability, 50) << "seed " << seed;
			EXPECT_EQ(head._iMaxDur, 60) << "seed " << seed;
		} else {
			EXPECT_EQ(chest._iDurability, 40) << "seed " << seed;
			EXPECT_EQ(chest._iMaxDur, 40) << "seed " << seed;
			EXPECT_EQ(head._iDurability, 30) << "seed " << seed;
			EXPECT_EQ(head._iMaxDur, 40) << "seed " << seed;
		}
	}
}

TEST_F(ShrinesTest, GloomyRaisesArmorClassAndReducesWeaponMaxDamage)
{
	const ShrineData &gloomy = FindShrine(OperateShrineGloomy);

	Item &armor = MyPlayer->InvBody[INVLOC_CHEST];
	armor = {};
	armor._itype = ItemType::LightArmor;
	armor._iAC = 10;

	Item &weapon = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	weapon = {};
	weapon._itype = ItemType::Sword;
	weapon._iMinDam = 4;
	weapon._iMaxDam = 10;

	EXPECT_EQ(Operate(gloomy, 42), 1);

	EXPECT_EQ(armor._iAC, 12);
	EXPECT_EQ(weapon._iMaxDam, 9);
}

TEST_F(ShrinesTest, GloomyDoesNotModifyNonArmorOrWeaponItems)
{
	const ShrineData &gloomy = FindShrine(OperateShrineGloomy);

	Item &ring = MyPlayer->InvBody[INVLOC_RING_LEFT];
	ring = {};
	ring._itype = ItemType::Ring;
	ring._iAC = 42;

	Item &weapon = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	weapon = {};
	weapon._itype = ItemType::Sword;
	weapon._iMinDam = 5;
	weapon._iMaxDam = 12;

	EXPECT_EQ(Operate(gloomy, 42), 1);

	EXPECT_EQ(ring._iAC, 42);
	EXPECT_EQ(weapon._iMaxDam, 11);
}

TEST_F(ShrinesTest, GloomyNeverDropsWeaponBelowMinimumDamage)
{
	const ShrineData &gloomy = FindShrine(OperateShrineGloomy);

	Item &weapon = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	weapon = {};
	weapon._itype = ItemType::Axe;
	weapon._iMinDam = 7;
	weapon._iMaxDam = 6;

	EXPECT_EQ(Operate(gloomy, 42), 1);
	EXPECT_EQ(weapon._iMaxDam, 7);
}

TEST_F(ShrinesTest, WeirdRaisesMaxDamageOfEquippedWeapons)
{
	const ShrineData &weird = FindShrine(OperateShrineWeird);

	Item &left = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	left = {};
	left._itype = ItemType::Sword;
	left._iMinDam = 3;
	left._iMaxDam = 8;

	Item &right = MyPlayer->InvBody[INVLOC_HAND_RIGHT];
	right = {};
	right._itype = ItemType::Axe;
	right._iMinDam = 5;
	right._iMaxDam = 12;

	EXPECT_EQ(Operate(weird, 42), 1);

	EXPECT_EQ(left._iMinDam, 3);
	EXPECT_EQ(left._iMaxDam, 9);
	EXPECT_EQ(right._iMinDam, 5);
	EXPECT_EQ(right._iMaxDam, 13);
}

TEST_F(ShrinesTest, WeirdDoesNotModifyShieldsOrArmor)
{
	const ShrineData &weird = FindShrine(OperateShrineWeird);

	Item &shield = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	shield = {};
	shield._itype = ItemType::Shield;
	shield._iMinDam = 2;
	shield._iMaxDam = 7;

	Item &armor = MyPlayer->InvBody[INVLOC_CHEST];
	armor = {};
	armor._itype = ItemType::LightArmor;
	armor._iAC = 10;
	armor._iMinDam = 1;
	armor._iMaxDam = 4;

	EXPECT_EQ(Operate(weird, 42), 1);

	EXPECT_EQ(shield._iMaxDam, 7);
	EXPECT_EQ(armor._iAC, 10);
	EXPECT_EQ(armor._iMaxDam, 4);
}

TEST_F(ShrinesTest, WeirdClampsMaxDamageTo255)
{
	const ShrineData &weird = FindShrine(OperateShrineWeird);

	Item &weapon = MyPlayer->InvBody[INVLOC_HAND_RIGHT];
	weapon = {};
	weapon._itype = ItemType::Sword;
	weapon._iMinDam = 10;
	weapon._iMaxDam = 255;

	EXPECT_EQ(Operate(weird, 42), 1);

	EXPECT_EQ(weapon._iMinDam, 10);
	EXPECT_EQ(weapon._iMaxDam, 255);
}

TEST_F(ShrinesTest, MagicalCastsManaShield)
{
	const ShrineData &magical = FindShrine(OperateShrineMagical);
	const size_t missileCount = Missiles.size();

	EXPECT_EQ(Operate(magical, 42), 1);

	bool foundManaShield = false;
	size_t index = 0;
	for (const Missile &missile : Missiles) {
		if (index++ < missileCount)
			continue;

		EXPECT_EQ(missile._mitype, MissileID::ManaShield);
		foundManaShield = true;
	}
	EXPECT_TRUE(foundManaShield);
}

TEST_F(ShrinesTest, MagicalUsesConfiguredSpellLevel)
{
	const ShrineData &magical = FindShrine(OperateShrineMagical);
	const size_t missileCount = Missiles.size();

	Operate(magical, 42);

	bool foundManaShield = false;
	size_t index = 0;
	for (const Missile &missile : Missiles) {
		if (index++ < missileCount)
			continue;

		if (missile._mitype != MissileID::ManaShield)
			continue;

		EXPECT_EQ(missile._mispllvl, magical.powerParam1 * leveltype + magical.powerParam2);
		foundManaShield = true;
	}
	EXPECT_TRUE(foundManaShield);
}

TEST_F(ShrinesTest, StoneRechargesAllStaves)
{
	const ShrineData &stone = FindShrine(OperateShrineStone);

	Item &staff1 = MyPlayer->InvList[0];
	staff1 = {};
	staff1._itype = ItemType::Staff;
	staff1._iCharges = 0;
	staff1._iMaxCharges = 15;

	Item &staff2 = MyPlayer->InvList[1];
	staff2 = {};
	staff2._itype = ItemType::Staff;
	staff2._iCharges = 3;
	staff2._iMaxCharges = 20;

	MyPlayer->_pNumInv = 2;

	EXPECT_EQ(Operate(stone, 42), 1);
	EXPECT_EQ(staff1._iCharges, staff1._iMaxCharges);
	EXPECT_EQ(staff2._iCharges, staff2._iMaxCharges);
}

TEST_F(ShrinesTest, StoneDoesNotRechargeNonStaffItems)
{
	const ShrineData &stone = FindShrine(OperateShrineStone);

	Item &staff = MyPlayer->InvList[0];
	staff = {};
	staff._itype = ItemType::Staff;
	staff._iCharges = 1;
	staff._iMaxCharges = 10;

	Item &sword = MyPlayer->InvList[1];
	sword = {};
	sword._itype = ItemType::Sword;
	sword._iCharges = 2;
	sword._iMaxCharges = 10;

	MyPlayer->_pNumInv = 2;

	EXPECT_EQ(Operate(stone, 42), 1);

	EXPECT_EQ(staff._iCharges, 10);
	EXPECT_EQ(sword._iCharges, 2);
}

TEST_F(ShrinesTest, StoneRechargesEquippedStaff)
{
	const ShrineData &stone = FindShrine(OperateShrineStone);

	Item &staff = MyPlayer->InvBody[INVLOC_HAND_LEFT];
	staff = {};
	staff._itype = ItemType::Staff;
	staff._iCharges = 0;
	staff._iMaxCharges = 12;

	EXPECT_EQ(Operate(stone, 42), 1);
	EXPECT_EQ(staff._iCharges, 12);
}

TEST_F(ShrinesTest, ReligiousRepairsInventoryAndEquippedItems)
{
	const ShrineData &religious = FindShrine(OperateShrineReligious);

	Item &equipped = MyPlayer->InvBody[INVLOC_CHEST];
	equipped = {};
	equipped._itype = ItemType::LightArmor;
	equipped._iDurability = 5;
	equipped._iMaxDur = 30;

	Item &inventory = MyPlayer->InvList[0];
	inventory = {};
	inventory._itype = ItemType::Sword;
	inventory._iDurability = 2;
	inventory._iMaxDur = 20;

	MyPlayer->_pNumInv = 1;

	EXPECT_EQ(Operate(religious, 42), 1);
	EXPECT_EQ(equipped._iDurability, equipped._iMaxDur);
	EXPECT_EQ(inventory._iDurability, inventory._iMaxDur);
}

TEST_F(ShrinesTest, EnchantedRaisesAllSpellsButOne)
{
	const ShrineData &enchanted = FindShrine(OperateShrineEnchanted);

	constexpr std::array<SpellID, 3> Spells { SpellID::Firebolt, SpellID::Healing, SpellID::ChargedBolt };
	constexpr std::array<uint8_t, 3> Levels { 5, 0, MaxSpellLevel };

	for (uint32_t seed = 0; seed < 16; seed++) {
		MyPlayer->_pMemSpells = 0;
		for (size_t i = 0; i < Spells.size(); i++) {
			MyPlayer->_pMemSpells |= GetSpellBitmask(Spells[i]);
			MyPlayer->_pSplLvl[static_cast<int8_t>(Spells[i])] = Levels[i];
		}

		EXPECT_EQ(Operate(enchanted, seed), 1);

		// Vanilla: one known spell loses a level (not below 0), every other known spell gains one (not above max).
		int reducedCount = 0;
		for (size_t reduced = 0; reduced < Spells.size(); reduced++) {
			bool matches = true;
			for (size_t i = 0; i < Spells.size(); i++) {
				const int level = Levels[i];
				const int expected = i == reduced ? std::max(level - 1, 0) : std::min(level + 1, static_cast<int>(MaxSpellLevel));
				if (MyPlayer->_pSplLvl[static_cast<int8_t>(Spells[i])] != expected)
					matches = false;
			}
			if (matches)
				reducedCount++;
		}
		EXPECT_EQ(reducedCount, 1) << "seed " << seed;
	}
}

TEST_F(ShrinesTest, ThaumaturgicRefillsClosedChest)
{
	const ShrineData &thaumaturgic = FindShrine(OperateShrineThaumaturgic);

	ActiveObjectCount = 1;
	ActiveObjects[0] = 0;

	Object &chest = Objects[0];
	chest = {};
	chest._otype = _object_id::OBJ_CHEST1;
	chest.selectionRegion = SelectionRegion::None;
	chest._oAnimFrame = 10;
	chest._oRndSeed = 1;

	EXPECT_EQ(Operate(thaumaturgic, 42), 1);

	EXPECT_EQ(chest.selectionRegion, SelectionRegion::Bottom);
	EXPECT_EQ(chest._oAnimFrame, 8);
}

TEST_F(ShrinesTest, ThaumaturgicLeavesInteractableChestUnchanged)
{
	const ShrineData &thaumaturgic = FindShrine(OperateShrineThaumaturgic);

	ActiveObjectCount = 1;
	ActiveObjects[0] = 0;

	Object &chest = Objects[0];
	chest = {};
	chest._otype = _object_id::OBJ_CHEST1;
	chest.selectionRegion = SelectionRegion::Top;
	chest._oAnimFrame = 10;
	chest._oRndSeed = 1;

	EXPECT_EQ(Operate(thaumaturgic, 42), 1);

	EXPECT_EQ(chest.selectionRegion, SelectionRegion::Top);
	EXPECT_EQ(chest._oAnimFrame, 10);
	EXPECT_EQ(chest._oRndSeed, 1);
}

TEST_F(ShrinesTest, FascinatingRaisesFireboltAndReducesMaxMana)
{
	const ShrineData &fascinating = FindShrine(OperateShrineFascinating);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)] = 3;

	const int maxMana = MyPlayer->_pMaxManaBase.raw();

	EXPECT_EQ(Operate(fascinating, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)], 5);
	EXPECT_LT(MyPlayer->_pMaxManaBase.raw(), maxMana);
}

TEST_F(ShrinesTest, FascinatingClampsFireboltLevelToMaximum)
{
	const ShrineData &fascinating = FindShrine(OperateShrineFascinating);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)] = MaxSpellLevel - 1;

	EXPECT_EQ(Operate(fascinating, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)], MaxSpellLevel);
}

TEST_F(ShrinesTest, FascinatingAddsFireboltToKnownSpells)
{
	const ShrineData &fascinating = FindShrine(OperateShrineFascinating);

	MyPlayer->_pMemSpells &= ~GetSpellBitmask(SpellID::Firebolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)] = 0;

	EXPECT_EQ(Operate(fascinating, 42), 1);

	EXPECT_NE(MyPlayer->_pMemSpells & GetSpellBitmask(SpellID::Firebolt), 0);
	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::Firebolt)], 2);
}

TEST_F(ShrinesTest, CrypticRestoresManaToMaximum)
{
	const ShrineData &cryptic = FindShrine(OperateShrineCryptic);

	MyPlayer->_pMana = Fixed26_6::fromInt(1);
	MyPlayer->_pManaBase = Fixed26_6::fromInt(1);

	EXPECT_EQ(Operate(cryptic, 42), 1);

	EXPECT_EQ(MyPlayer->_pMana, MyPlayer->_pMaxMana);
	EXPECT_EQ(MyPlayer->_pManaBase, MyPlayer->_pMaxManaBase);
}

TEST_F(ShrinesTest, CrypticCastsNova)
{
	const ShrineData &cryptic = FindShrine(OperateShrineCryptic);

	Operate(cryptic, 42);

	Missile *nova = nullptr;
	for (Missile &missile : Missiles) {
		if (missile._mitype == MissileID::Nova) {
			nova = &missile;
			break;
		}
	}

	ASSERT_NE(nova, nullptr);
	EXPECT_EQ(nova->_misource, MyPlayer->getId());
	EXPECT_EQ(nova->_mispllvl, cryptic.powerParam1 * leveltype + cryptic.powerParam2);
}

TEST_F(ShrinesTest, EldritchTurnsHealthAndManaPotionsIntoRejuvenation)
{
	const ShrineData &eldritch = FindShrine(OperateShrineEldritch);

	MyPlayer->_pNumInv = 2;

	Item &health = MyPlayer->InvList[0];
	InitializeItem(health, ItemMiscIdIdx(IMISC_HEAL));

	Item &mana = MyPlayer->InvList[1];
	InitializeItem(mana, ItemMiscIdIdx(IMISC_MANA));

	EXPECT_EQ(Operate(eldritch, 42), 1);
	EXPECT_EQ(health._iMiscId, IMISC_REJUV);
	EXPECT_EQ(mana._iMiscId, IMISC_REJUV);
}

TEST_F(ShrinesTest, EldritchTurnsFullPotionsIntoFullRejuvenation)
{
	const ShrineData &eldritch = FindShrine(OperateShrineEldritch);

	MyPlayer->_pNumInv = 2;

	Item &fullHealth = MyPlayer->InvList[0];
	InitializeItem(fullHealth, ItemMiscIdIdx(IMISC_FULLHEAL));

	Item &fullMana = MyPlayer->InvList[1];
	InitializeItem(fullMana, ItemMiscIdIdx(IMISC_FULLMANA));

	EXPECT_EQ(Operate(eldritch, 42), 1);
	EXPECT_EQ(fullHealth._iMiscId, IMISC_FULLREJUV);
	EXPECT_EQ(fullMana._iMiscId, IMISC_FULLREJUV);
}

TEST_F(ShrinesTest, EldritchLeavesOtherInventoryItemsUnchanged)
{
	const ShrineData &eldritch = FindShrine(OperateShrineEldritch);

	MyPlayer->_pNumInv = 1;

	Item &sword = MyPlayer->InvList[0];
	sword = {};
	sword._itype = ItemType::Sword;

	EXPECT_EQ(Operate(eldritch, 42), 1);
	EXPECT_EQ(sword._itype, ItemType::Sword);
}

TEST_F(ShrinesTest, EerieRaisesMagicByTwo)
{
	const ShrineData &eerie = FindShrine(OperateShrineEerie);

	MyPlayer->_pBaseMag = 10;
	MyPlayer->_pMagic = 10;

	EXPECT_EQ(Operate(eerie, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseMag, 12);
	EXPECT_EQ(MyPlayer->_pMagic, 12);
}

TEST_F(ShrinesTest, EerieDoesNotIncreaseMagicPastMaximum)
{
	const ShrineData &eerie = FindShrine(OperateShrineEerie);

	const int maximum = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Magic);
	MyPlayer->_pBaseMag = maximum - 1;
	MyPlayer->_pMagic = maximum - 1;

	EXPECT_EQ(Operate(eerie, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseMag, maximum);
	EXPECT_EQ(MyPlayer->_pMagic, maximum);
}

TEST_F(ShrinesTest, EerieDoesNotChangeOtherAttributes)
{
	const ShrineData &eerie = FindShrine(OperateShrineEerie);

	MyPlayer->_pBaseStr = MyPlayer->_pStrength = 10;
	MyPlayer->_pBaseMag = MyPlayer->_pMagic = 10;
	MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 10;
	MyPlayer->_pBaseVit = MyPlayer->_pVitality = 10;

	EXPECT_EQ(Operate(eerie, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseStr, 10);
	EXPECT_EQ(MyPlayer->_pBaseDex, 10);
	EXPECT_EQ(MyPlayer->_pBaseVit, 10);
}

TEST_F(ShrinesTest, DivineRestoresHealthAndMana)
{
	currlevel = 1; // Below the shrine threshold; eligible Full Mana/Full Healing potions exist.
	const ShrineData &divine = FindShrine(OperateShrineDivine);
	DiabloGenerator rng(42);
	const Point spawnPosition { 50, 50 };
	MyPlayer->position.tile = spawnPosition;

	MyPlayer->_pHitPoints = Fixed26_6::fromInt(20);
	MyPlayer->_pHPBase = Fixed26_6::fromInt(10);
	MyPlayer->_pMaxHP = Fixed26_6::fromInt(120);
	MyPlayer->_pMaxHPBase = Fixed26_6::fromInt(110);
	MyPlayer->_pMana = Fixed26_6::fromInt(40);
	MyPlayer->_pManaBase = Fixed26_6::fromInt(30);
	MyPlayer->_pMaxMana = Fixed26_6::fromInt(140);
	MyPlayer->_pMaxManaBase = Fixed26_6::fromInt(130);

	EXPECT_EQ(Operate(divine, 42), 1);

	EXPECT_EQ(MyPlayer->_pMana, MyPlayer->_pMaxMana);
	EXPECT_EQ(MyPlayer->_pManaBase, MyPlayer->_pMaxManaBase);
	EXPECT_EQ(MyPlayer->_pHitPoints, MyPlayer->_pMaxHP);
	EXPECT_EQ(MyPlayer->_pHPBase, MyPlayer->_pMaxHPBase);
}

TEST_F(ShrinesTest, DivineSpawnsConfiguredNumberOfPotions)
{
	currlevel = 1; // Below the shrine threshold; eligible Full Mana/Full Healing potions exist.

	const ShrineData &divine = FindShrine(OperateShrineDivine);

	const int itemCount = ActiveItemCount;
	EXPECT_EQ(Operate(divine, 42), 1);

	EXPECT_EQ(ActiveItemCount - itemCount, divine.powerParam2);
}

TEST_F(ShrinesTest, DivineSpawnsFullManaLifeAtBelowLevelThreshold)
{
	const ShrineData &divine = FindShrine(OperateShrineDivine);

	currlevel = divine.powerParam1 - 1;

	const int itemCount = ActiveItemCount;
	EXPECT_EQ(Operate(divine, 42), 1);

	ASSERT_EQ(ActiveItemCount - itemCount, divine.powerParam2);
	for (int i = itemCount; i < ActiveItemCount; ++i) {
		EXPECT_NE(Items[ActiveItems[i]]._iMiscId, IMISC_FULLREJUV);
	}
}

TEST_F(ShrinesTest, DivineSpawnsFullRejuvenationAtOrAboveLevelThreshold)
{
	const ShrineData &divine = FindShrine(OperateShrineDivine);

	currlevel = divine.powerParam1;

	const int itemCount = ActiveItemCount;
	EXPECT_EQ(Operate(divine, 42), 1);

	ASSERT_EQ(ActiveItemCount - itemCount, divine.powerParam2);
	for (int i = itemCount; i < ActiveItemCount; ++i) {
		EXPECT_EQ(Items[ActiveItems[i]]._iMiscId, IMISC_FULLREJUV);
	}
}

TEST_F(ShrinesTest, HolyCastsPhasing)
{
	const ShrineData &holy = FindShrine(OperateShrineHoly);
	const Point playerTile = MyPlayer->position.tile;
	const size_t missileCount = Missiles.size();

	EXPECT_EQ(Operate(holy, 42), 1);

	bool foundPhasing = false;
	size_t index = 0;
	for (const Missile &missile : Missiles) {
		if (index++ < missileCount || missile._mitype != MissileID::Phasing)
			continue;

		EXPECT_EQ(missile._misource, MyPlayer->getId());
		EXPECT_EQ(missile.position.start, playerTile);
		EXPECT_EQ(missile._mispllvl, holy.powerParam1 * leveltype + holy.powerParam2);
		foundPhasing = true;
	}

	EXPECT_TRUE(foundPhasing);
}

TEST_F(ShrinesTest, SacredRaisesChargedBoltAndReducesMaxMana)
{
	const ShrineData &sacred = FindShrine(OperateShrineSacred);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::ChargedBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)] = 3;

	const int maxMana = MyPlayer->_pMaxManaBase.raw();

	EXPECT_EQ(Operate(sacred, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)], 5);
	EXPECT_LT(MyPlayer->_pMaxManaBase.raw(), maxMana);
}

TEST_F(ShrinesTest, SacredClampsChargedBoltLevelToMaximum)
{
	const ShrineData &sacred = FindShrine(OperateShrineSacred);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::ChargedBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)] = MaxSpellLevel - 1;

	EXPECT_EQ(Operate(sacred, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)], MaxSpellLevel);
}

TEST_F(ShrinesTest, SacredAddsChargedBoltToKnownSpells)
{
	const ShrineData &sacred = FindShrine(OperateShrineSacred);

	MyPlayer->_pMemSpells &= ~GetSpellBitmask(SpellID::ChargedBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)] = 0;

	EXPECT_EQ(Operate(sacred, 42), 1);

	EXPECT_NE(MyPlayer->_pMemSpells & GetSpellBitmask(SpellID::ChargedBolt), 0);
	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::ChargedBolt)], 2);
}

TEST_F(ShrinesTest, SpiritualFillsEmptyInventorySlotsWithGold)
{
	const ShrineData &spiritual = FindShrine(OperateShrineSpiritual);
	leveltype = DTYPE_CATHEDRAL;

	MyPlayer->_pNumInv = 0;
	for (int i = 0; i < InventoryGridCells; ++i)
		MyPlayer->InvGrid[i] = 0;

	EXPECT_EQ(Operate(spiritual, 42), 1);

	EXPECT_GT(MyPlayer->_pNumInv, 0);
	EXPECT_EQ(MyPlayer->_pGold, 403);
}

TEST_F(ShrinesTest, SpiritualDoesNotOverwriteOccupiedInventorySlots)
{
	const ShrineData &spiritual = FindShrine(OperateShrineSpiritual);
	leveltype = DTYPE_CATHEDRAL;

	MyPlayer->_pNumInv = 1;
	MyPlayer->InvGrid[0] = 1;

	MyPlayer->InvList[0] = {};
	MyPlayer->InvList[0]._itype = ItemType::Sword;

	for (int i = 1; i < InventoryGridCells; ++i)
		MyPlayer->InvGrid[i] = 0;

	const int inventoryCount = MyPlayer->_pNumInv;

	EXPECT_EQ(Operate(spiritual, 42), 1);

	EXPECT_EQ(MyPlayer->InvList[0]._itype, ItemType::Sword);
	EXPECT_GT(MyPlayer->_pNumInv, inventoryCount);
}

TEST_F(ShrinesTest, SpookyEffectsOnlyNotOperatingPlayer)
{
	const ShrineData &spooky = FindShrine(OperateShrineSpooky);

	const Fixed26_6 hp = Fixed26_6::fromInt(20);
	const Fixed26_6 hpMax = Fixed26_6::fromInt(120);
	const Fixed26_6 hpBase = Fixed26_6::fromInt(10);
	const Fixed26_6 hpMaxBase = Fixed26_6::fromInt(110);
	const Fixed26_6 mana = Fixed26_6::fromInt(40);
	const Fixed26_6 manaMax = Fixed26_6::fromInt(140);
	const Fixed26_6 manaBase = Fixed26_6::fromInt(30);
	const Fixed26_6 manaMaxBase = Fixed26_6::fromInt(130);

	MyPlayer->_pHitPoints = hp;
	MyPlayer->_pHPBase = hpBase;
	MyPlayer->_pMaxHP = hpMax;
	MyPlayer->_pMaxHPBase = hpMaxBase;
	MyPlayer->_pMana = mana;
	MyPlayer->_pManaBase = manaBase;
	MyPlayer->_pMaxMana = manaMax;
	MyPlayer->_pMaxManaBase = manaMaxBase;

	Player operatePlayer;
	operatePlayer = {};

	operatePlayer._pHitPoints = hp;
	operatePlayer._pHPBase = hpBase;
	operatePlayer._pMaxHP = hpMax;
	operatePlayer._pMaxHPBase = hpMaxBase;
	operatePlayer._pMana = mana;
	operatePlayer._pManaBase = manaBase;
	operatePlayer._pMaxMana = manaMax;
	operatePlayer._pMaxManaBase = manaMaxBase;

	EXPECT_EQ(Operate(operatePlayer, spooky, 42), 2);
	EXPECT_EQ(MyPlayer->_pHitPoints, hpMax);
	EXPECT_EQ(MyPlayer->_pHPBase, hpMaxBase);
	EXPECT_EQ(MyPlayer->_pMana, manaMax);
	EXPECT_EQ(MyPlayer->_pManaBase, manaMaxBase);
	EXPECT_EQ(operatePlayer._pHitPoints, hp);
	EXPECT_EQ(operatePlayer._pHPBase, hpBase);
	EXPECT_EQ(operatePlayer._pMana, mana);
	EXPECT_EQ(operatePlayer._pManaBase, manaBase);
}

TEST_F(ShrinesTest, AbandonedRaisesDexterityByTwo)
{
	const ShrineData &abandoned = FindShrine(OperateShrineAbandoned);

	MyPlayer->_pBaseDex = 10;
	MyPlayer->_pDexterity = 10;

	EXPECT_EQ(Operate(abandoned, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseDex, 12);
	EXPECT_EQ(MyPlayer->_pDexterity, 12);
}

TEST_F(ShrinesTest, AbandonedDoesNotIncreaseDexterityPastMaximum)
{
	const ShrineData &abandoned = FindShrine(OperateShrineAbandoned);

	const int maximum = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Dexterity);
	MyPlayer->_pBaseDex = maximum - 1;
	MyPlayer->_pDexterity = maximum - 1;

	EXPECT_EQ(Operate(abandoned, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseDex, maximum);
	EXPECT_EQ(MyPlayer->_pDexterity, maximum);
}

TEST_F(ShrinesTest, AbandonedDoesNotChangeOtherAttributes)
{
	const ShrineData &abandoned = FindShrine(OperateShrineAbandoned);

	MyPlayer->_pBaseStr = MyPlayer->_pStrength = 10;
	MyPlayer->_pBaseMag = MyPlayer->_pDexterity = 10;
	MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 10;
	MyPlayer->_pBaseVit = MyPlayer->_pVitality = 10;

	EXPECT_EQ(Operate(abandoned, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseStr, 10);
	EXPECT_EQ(MyPlayer->_pBaseMag, 10);
	EXPECT_EQ(MyPlayer->_pBaseVit, 10);
}

TEST_F(ShrinesTest, CreepyRaisesStrengthByTwo)
{
	const ShrineData &creepy = FindShrine(OperateShrineCreepy);

	MyPlayer->_pBaseStr = 10;
	MyPlayer->_pStrength = 10;

	EXPECT_EQ(Operate(creepy, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseStr, 12);
	EXPECT_EQ(MyPlayer->_pStrength, 12);
}

TEST_F(ShrinesTest, CreepyDoesNotIncreaseStrengthPastMaximum)
{
	const ShrineData &creepy = FindShrine(OperateShrineCreepy);

	const int maximum = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength);
	MyPlayer->_pBaseStr = maximum - 1;
	MyPlayer->_pStrength = maximum - 1;

	EXPECT_EQ(Operate(creepy, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseStr, maximum);
	EXPECT_EQ(MyPlayer->_pStrength, maximum);
}

TEST_F(ShrinesTest, CreepyDoesNotChangeOtherAttributes)
{
	const ShrineData &creepy = FindShrine(OperateShrineCreepy);

	MyPlayer->_pBaseStr = MyPlayer->_pStrength = 10;
	MyPlayer->_pBaseMag = MyPlayer->_pMagic = 10;
	MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 10;
	MyPlayer->_pBaseVit = MyPlayer->_pVitality = 10;

	EXPECT_EQ(Operate(creepy, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseMag, 10);
	EXPECT_EQ(MyPlayer->_pBaseDex, 10);
	EXPECT_EQ(MyPlayer->_pBaseVit, 10);
}

TEST_F(ShrinesTest, QuietRaisesVitalityByTwo)
{
	const ShrineData &quiet = FindShrine(OperateShrineQuiet);

	MyPlayer->_pBaseVit = 10;
	MyPlayer->_pVitality = 10;

	EXPECT_EQ(Operate(quiet, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseVit, 12);
	EXPECT_EQ(MyPlayer->_pVitality, 12);
}

TEST_F(ShrinesTest, QuietDoesNotIncreaseVitalityPastMaximum)
{
	const ShrineData &quiet = FindShrine(OperateShrineQuiet);

	const int maximum = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Vitality);
	MyPlayer->_pBaseVit = maximum - 1;
	MyPlayer->_pVitality = maximum - 1;

	EXPECT_EQ(Operate(quiet, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseVit, maximum);
	EXPECT_EQ(MyPlayer->_pVitality, maximum);
}

TEST_F(ShrinesTest, QuietDoesNotChangeOtherAttributes)
{
	const ShrineData &quiet = FindShrine(OperateShrineQuiet);

	MyPlayer->_pBaseStr = MyPlayer->_pStrength = 10;
	MyPlayer->_pBaseMag = MyPlayer->_pMagic = 10;
	MyPlayer->_pBaseDex = MyPlayer->_pDexterity = 10;
	MyPlayer->_pBaseVit = MyPlayer->_pVitality = 10;

	EXPECT_EQ(Operate(quiet, 42), 1);

	EXPECT_EQ(MyPlayer->_pBaseStr, 10);
	EXPECT_EQ(MyPlayer->_pBaseDex, 10);
	EXPECT_EQ(MyPlayer->_pBaseMag, 10);
}

TEST_F(ShrinesTest, SecludedRevealsEntireAutomap)
{
	const ShrineData &secluded = FindShrine(OperateShrineSecluded);

	for (int x = 0; x < DMAXX; ++x) {
		for (int y = 0; y < DMAXY; ++y)
			AutomapView[x][y] = MAP_EXP_NONE;
	}

	EXPECT_EQ(Operate(secluded, 42), 1);

	for (int x = 0; x < DMAXX; ++x) {
		for (int y = 0; y < DMAXY; ++y)
			EXPECT_EQ(AutomapView[x][y], MAP_EXP_SHRINE);
	}
}

TEST_F(ShrinesTest, OrnateRaisesHolyBoltAndReducesMaxMana)
{
	const ShrineData &ornate = FindShrine(OperateShrineOrnate);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::HolyBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)] = 3;

	const int maxMana = MyPlayer->_pMaxManaBase.raw();

	EXPECT_EQ(Operate(ornate, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)], 5);
	EXPECT_LT(MyPlayer->_pMaxManaBase.raw(), maxMana);
}

TEST_F(ShrinesTest, OrnateClampsHolyBoltLevelToMaximum)
{
	const ShrineData &ornate = FindShrine(OperateShrineOrnate);

	MyPlayer->_pMemSpells |= GetSpellBitmask(SpellID::HolyBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)] = MaxSpellLevel - 1;

	EXPECT_EQ(Operate(ornate, 42), 1);

	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)], MaxSpellLevel);
}

TEST_F(ShrinesTest, OrnateAddsHolyBoltToKnownSpells)
{
	const ShrineData &ornate = FindShrine(OperateShrineOrnate);

	MyPlayer->_pMemSpells &= ~GetSpellBitmask(SpellID::HolyBolt);
	MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)] = 0;

	EXPECT_EQ(Operate(ornate, 42), 1);

	EXPECT_NE(MyPlayer->_pMemSpells & GetSpellBitmask(SpellID::HolyBolt), 0);
	EXPECT_EQ(MyPlayer->_pSplLvl[static_cast<int8_t>(SpellID::HolyBolt)], 2);
}

TEST_F(ShrinesTest, GlimmeringIdentifiesMagicalInventoryItem)
{
	const ShrineData &glimmering = FindShrine(OperateShrineGlimmering);

	MyPlayer->_pNumInv = 1;
	Item &item = MyPlayer->InvList[0];
	item = {};
	item._itype = ItemType::Sword;
	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iIdentified = false;

	EXPECT_EQ(Operate(glimmering, 42), 1);
	EXPECT_TRUE(item._iIdentified);
}

TEST_F(ShrinesTest, GlimmeringIdentifiesMagicalEquippedItem)
{
	const ShrineData &glimmering = FindShrine(OperateShrineGlimmering);

	Item &item = MyPlayer->InvBody[INVLOC_CHEST];
	item = {};
	item._itype = ItemType::LightArmor;
	item._iMagical = ITEM_QUALITY_UNIQUE;
	item._iIdentified = false;

	EXPECT_EQ(Operate(glimmering, 42), 1);
	EXPECT_TRUE(item._iIdentified);
}

TEST_F(ShrinesTest, GlimmeringDoesNotModifyNormalItems)
{
	const ShrineData &glimmering = FindShrine(OperateShrineGlimmering);

	MyPlayer->_pNumInv = 1;
	Item &item = MyPlayer->InvList[0];
	item = {};
	item._itype = ItemType::Sword;
	item._iMagical = ITEM_QUALITY_NORMAL;
	item._iIdentified = false;

	EXPECT_EQ(Operate(glimmering, 42), 1);
	EXPECT_FALSE(item._iIdentified);
}

TEST_F(ShrinesTest, TaintedAddsOneToOneAttributeAndSubtractsOneFromRest)
{
	const ShrineData &tainted = FindShrine(OperateShrineTainted);

	const int beforeStr = 10;
	const int beforeMag = 12;
	const int beforeDex = 9;
	const int beforeVit = 11;

	Player operatePlayer;
	operatePlayer = {};

	operatePlayer._pBaseStr = operatePlayer._pStrength = beforeStr;
	operatePlayer._pBaseMag = operatePlayer._pMagic = beforeMag;
	operatePlayer._pBaseDex = operatePlayer._pDexterity = beforeDex;
	operatePlayer._pBaseVit = operatePlayer._pVitality = beforeVit;

	for (uint32_t seed = 0; seed < 32; ++seed) {
		MyPlayer->_pBaseStr = MyPlayer->_pStrength = beforeStr;
		MyPlayer->_pBaseMag = MyPlayer->_pMagic = beforeMag;
		MyPlayer->_pBaseDex = MyPlayer->_pDexterity = beforeDex;
		MyPlayer->_pBaseVit = MyPlayer->_pVitality = beforeVit;

		DiabloGenerator rng(seed);
		EXPECT_EQ(Operate(operatePlayer, tainted, 42), 2);

		const int strMyDelta = MyPlayer->_pStrength - beforeStr;
		const int magMyDelta = MyPlayer->_pMagic - beforeMag;
		const int dexMyDelta = MyPlayer->_pDexterity - beforeDex;
		const int vitMyDelta = MyPlayer->_pVitality - beforeVit;

		const int strRemoteDelta = operatePlayer._pStrength - beforeStr;
		const int magRemoteDelta = operatePlayer._pMagic - beforeMag;
		const int dexRemoteDelta = operatePlayer._pDexterity - beforeDex;
		const int vitRemoteDelta = operatePlayer._pVitality - beforeVit;

		EXPECT_TRUE(strMyDelta == 1 || strMyDelta == -1) << "seed " << seed;
		EXPECT_TRUE(magMyDelta == 1 || magMyDelta == -1) << "seed " << seed;
		EXPECT_TRUE(dexMyDelta == 1 || dexMyDelta == -1) << "seed " << seed;
		EXPECT_TRUE(vitMyDelta == 1 || vitMyDelta == -1) << "seed " << seed;
		EXPECT_TRUE(strRemoteDelta == 0) << "seed " << seed;
		EXPECT_TRUE(magRemoteDelta == 0) << "seed " << seed;
		EXPECT_TRUE(dexRemoteDelta == 0) << "seed " << seed;
		EXPECT_TRUE(vitRemoteDelta == 0) << "seed " << seed;

		const int bonusCount = (strMyDelta == 1) + (magMyDelta == 1) + (dexMyDelta == 1) + (vitMyDelta == 1);
		EXPECT_EQ(bonusCount, 1) << "seed " << seed;
	}
}

} // namespace
} // namespace devilution
