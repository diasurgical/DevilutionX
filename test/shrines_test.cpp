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

#include "engine/random.hpp"
#include "items.h"
#include "objects.h"
#include "player.h"
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

	static void Equip(inv_body_loc loc, int durability, int maxDurability)
	{
		Item &item = MyPlayer->InvBody[loc];
		item = {};
		item._itype = ItemType::LightArmor;
		item._iDurability = durability;
		item._iMaxDur = maxDurability;
	}
};

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

} // namespace
} // namespace devilution
