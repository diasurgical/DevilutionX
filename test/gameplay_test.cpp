#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "engine/random.hpp"
#include "items.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "panels/spell_icons.hpp"
#include "player.h"
#include "tables/gameplay.h"

namespace devilution {
namespace {

class GameplayTest : public ::testing::Test {
protected:
	GameplayRules savedRules = Gameplay;
	int savedMaxGold = MaxGold;

	void SetUp() override
	{
		Gameplay = {};
		Players.resize(1);
		MyPlayerId = 0;
		MyPlayer = &Players[0];
		*MyPlayer = {};
	}

	void TearDown() override
	{
		Gameplay = savedRules;
		MaxGold = savedMaxGold;
	}
};

TEST_F(GameplayTest, MissingOverrideRestoresStandardLimits)
{
	UnloadModArchives();
	Gameplay.maxResistance = 70;
	Gameplay.goldStackLimit = 65000;
	Gameplay.vendorValueLimit = 2340000;
	Gameplay.boyValueLimit = 2000000;
	MaxGold = 65000;

	LoadGameplayRules();

	EXPECT_EQ(MaxResistance, 75);
	EXPECT_EQ(GOLD_MAX_LIMIT, 5000);
	EXPECT_EQ(MaxGold, 5000);
	EXPECT_EQ(MaxVendorValue, 140000);
	EXPECT_EQ(MaxBoyValue, 90000);
}

TEST_F(GameplayTest, AuricGoldStacksFitTheSaveFormat)
{
	LoadPlayerDataFiles();
	Item &amulet = MyPlayer->InvBody[INVLOC_AMULET];
	amulet._itype = ItemType::Amulet;
	amulet.IDidx = IDI_AURIC;
	CalcPlrItemVals(*MyPlayer, false);
	EXPECT_EQ(MaxGold, 10000);

	Gameplay.goldStackLimit = 65000;
	CalcPlrItemVals(*MyPlayer, false);
	EXPECT_EQ(MaxGold, 65535);
}

TEST_F(GameplayTest, SpellIconOverridesResetWhenDataReloads)
{
	LoadSpellData();
	EXPECT_EQ(GetSpellIconFrame(SpellID::BoneSpirit), static_cast<uint8_t>(SpellIcon::BoneSpirit));
	SpellsData[static_cast<size_t>(SpellID::BoneSpirit)].iconFrame = 30;
	EXPECT_EQ(GetSpellIconFrame(SpellID::BoneSpirit), 30);
	LoadSpellData();
	EXPECT_EQ(GetSpellIconFrame(SpellID::BoneSpirit), static_cast<uint8_t>(SpellIcon::BoneSpirit));
}

TEST_F(GameplayTest, NormalizedStaffChargesPreserveValueAndRechargeFraction)
{
	SpellData original {};
	original.staffCost10 = 200;
	SpellData normalized = original;
	normalized.staffValueMultiplier = 3;
	for (int casts = 20; casts <= 40; casts++) {
		const int originalValue = original.staffValue(3 * casts);
		const int normalizedValue = normalized.staffValue(casts);
		EXPECT_EQ(originalValue, normalizedValue);
		// Adria adds the same flat spell cost and charges for the missing fraction.
		const int originalRecharge = (originalValue + original.staffCost()) * 9 / (3 * casts * 2);
		const int normalizedRecharge = (normalizedValue + normalized.staffCost()) * 3 / (casts * 2);
		EXPECT_EQ(originalRecharge, normalizedRecharge);
	}
}

TEST_F(GameplayTest, FireballDescriptionTracksTuningAndSpellScaling)
{
	DamageRange damage = GetDamageAmt(SpellID::Fireball, 2);
	EXPECT_EQ(damage.min, 6);
	EXPECT_EQ(damage.max, 52);

	Gameplay.fireballDamageMultiplier = 1;
	Gameplay.fireballDamageBonus = 25;
	damage = GetDamageAmt(SpellID::Fireball, 2);
	EXPECT_EQ(damage.min, 32);
	EXPECT_EQ(damage.max, 55);

	// Elemental retains its own formula; changing Fireball must not tune it.
	damage = GetDamageAmt(SpellID::Elemental, 0);
	EXPECT_EQ(damage.min, 6);
	EXPECT_EQ(damage.max, 42);
}

TEST_F(GameplayTest, FireballProjectilesRespectTheDisplayedRange)
{
	LoadMissileData();
	InitLighting();
	Gameplay.fireballDamageMultiplier = 1;
	Gameplay.fireballDamageBonus = 25;
	const DamageRange damage = GetDamageAmt(SpellID::Fireball, 2);
	for (int seed = 0; seed < 100; seed++) {
		SetRndSeed(seed);
		Missile missile {};
		missile._miAnimType = MissileGraphicID::None;
		missile._micaster = TARGET_MONSTERS;
		missile._misource = 0;
		missile._mispllvl = 2;
		missile.position.start = { 20, 20 };
		missile.position.tile = { 20, 20 };
		AddMissileParameter parameter { { 25, 25 }, Direction::South, nullptr, false };

		AddFireball(missile, parameter);

		EXPECT_GE(missile._midam, damage.min);
		EXPECT_LE(missile._midam, damage.max);
		AddUnLight(missile._mlid);
	}
}

TEST_F(GameplayTest, MatterTrapStaysOnTheCasterTileAndExpires)
{
	LoadMissileData();
	const Point position { 30, 30 };
	dMonster[position.x][position.y] = 0;
	Missile missile {};
	missile._miAnimType = MissileGraphicID::None;
	missile._mitype = MissileID::BloodRitual;
	missile._micaster = TARGET_MONSTERS;
	missile._misource = 0;
	missile.position.start = position;
	AddMissileParameter parameter { { 40, 40 }, Direction::South, nullptr, false };
	AddMatterTrap(missile, parameter);
	EXPECT_GE(missile._midam, 1);
	EXPECT_LE(missile._midam, 6);
	for (int tick = 0; tick < 254; tick++) {
		ProcessMatterTrap(missile);
		EXPECT_EQ(missile.position.tile, position);
		EXPECT_FALSE(missile._miDelFlag);
	}
	ProcessMatterTrap(missile);
	EXPECT_TRUE(missile._miDelFlag);
}

TEST_F(GameplayTest, EtherealizeExpiresWithoutPermanentImmunity)
{
	LoadMissileData();
	Missile missile {};
	missile._mitype = MissileID::Etherealize;
	missile._miAnimType = MissileGraphicID::None;
	missile._misource = 0;
	AddMissileParameter parameter {};
	MyPlayer->_pHitPoints = Fixed26_6::fromInt(10);
	MyPlayer->position.tile = { 30, 30 };
	AddEtherealize(missile, parameter);
	EXPECT_TRUE(HasAnyOf(MyPlayer->_pSpellFlags, SpellFlag::Etherealize));
	EXPECT_EQ(missile.duration, 8);
	for (int tick = 0; tick < 8; tick++)
		ProcessEtherealize(missile);
	EXPECT_TRUE(missile._miDelFlag);
	EXPECT_FALSE(HasAnyOf(MyPlayer->_pSpellFlags, SpellFlag::Etherealize));
}

TEST_F(GameplayTest, MonsterChargeAnimationIsRestoredAfterLoading)
{
	const uint8_t spriteList[] = { 1, 0, 0, 0 };
	CMonster &type = LevelMonsterTypes[0];
	type.type = MT_ZOMBIE;
	AnimStruct &walk = type.anims[static_cast<int>(MonsterGraphic::Walk)];
	const auto savedSprites = walk.sprites;
	walk.sprites = ClxSpriteListOrSheet { spriteList, 0 };
	Monsters[4].levelType = 0;
	Missiles.clear();
	for (MissileID id : { MissileID::FireMan, MissileID::Rhino }) {
		Missile &missile = Missiles.emplace_back();
		missile._mitype = id;
		missile._miAnimType = MissileGraphicID::None;
		missile._misource = 4;
		missile.setDirection(Direction::South);
		missile.duration = 17;
		missile.var1 = 2;
		EXPECT_FALSE(static_cast<bool>(missile._miAnimData));
	}
	SetUpMissileAnimationData();
	for (const Missile &missile : Missiles) {
		ASSERT_TRUE(static_cast<bool>(missile._miAnimData));
		EXPECT_EQ(missile._miAnimData->numSprites(), 1U);
		EXPECT_EQ(missile.duration, 17);
		EXPECT_EQ(missile.var1, 2);
	}
	Missiles.clear();
	walk.sprites = savedSprites;
}

TEST_F(GameplayTest, MatterTrapTriggersOnceWhenAMonsterEnters)
{
	LoadMissileData();
	LoadMonsterData();
	LoadPlayerDataFiles();
	const Point position { 30, 30 };
	LevelMonsterTypes[0].type = MT_ZOMBIE;
	Monster &monster = Monsters[4];
	monster = {};
	monster.levelType = 0;
	monster.uniqueType = UniqueMonsterType::None;
	monster.talkMsg = TEXT_NONE;
	monster.mode = MonsterMode::Petrified;
	monster.hitPoints = Fixed26_6::fromInt(100);
	monster.maxHitPoints = monster.hitPoints;
	monster.position.tile = position;
	dMonster[position.x][position.y] = 0;
	Missile missile {};
	missile._miAnimType = MissileGraphicID::None;
	missile._mitype = MissileID::BloodRitual;
	missile._micaster = TARGET_MONSTERS;
	missile._misource = 0;
	missile.position.start = position;
	AddMissileParameter parameter {};
	AddMatterTrap(missile, parameter);
	ProcessMatterTrap(missile);
	EXPECT_EQ(monster.hitPoints.whole(), 100);
	EXPECT_FALSE(missile._miDelFlag);
	dMonster[position.x][position.y] = 5;
	ProcessMatterTrap(missile);
	EXPECT_TRUE(missile._miDelFlag);
	EXPECT_EQ(monster.hitPoints.whole(), 100 - missile._midam);
	EXPECT_EQ(missile.position.tile, position);
	dMonster[position.x][position.y] = 0;
}

TEST_F(GameplayTest, EtherealizeRefreshReplacesTheOldTimer)
{
	LoadMissileData();
	Missiles.clear();
	MyPlayer->_pHitPoints = Fixed26_6::fromInt(10);
	MyPlayer->position.tile = { 30, 30 };
	AddMissileParameter parameter {};
	auto &first = Missiles.emplace_back();
	first._mitype = MissileID::Etherealize;
	first._misource = 0;
	AddEtherealize(first, parameter);
	first.duration = 1;
	auto &second = Missiles.emplace_back();
	second._mitype = MissileID::Etherealize;
	second._miAnimType = MissileGraphicID::None;
	second._misource = 0;
	AddEtherealize(second, parameter);
	EXPECT_TRUE(first._miDelFlag);
	ProcessEtherealize(second);
	EXPECT_EQ(second.duration, 7);
	EXPECT_TRUE(HasAnyOf(MyPlayer->_pSpellFlags, SpellFlag::Etherealize));
	second.duration = 1;
	ProcessEtherealize(second);
	EXPECT_FALSE(HasAnyOf(MyPlayer->_pSpellFlags, SpellFlag::Etherealize));
	Missiles.clear();
}

TEST_F(GameplayTest, EscapeWaitsForCastAndDoesNotRepeatCompletedWarp)
{
	Players.resize(2);
	MyPlayer = &Players[0];
	Player &remote = Players[1];
	remote = {};
	remote.setLevel(currlevel);
	remote._pHitPoints = Fixed26_6::fromInt(10);
	remote._pmode = PM_SPELL;
	Missile missile {};
	missile._misource = 1;
	missile.duration = 255;
	ProcessEscape(missile);
	EXPECT_FALSE(missile._miDelFlag);
	remote._pmode = PM_STAND;
	ProcessEscape(missile);
	EXPECT_TRUE(missile._miDelFlag);
	// A completed controller must not send another warp even if processed again.
	missile._misource = 0;
	MyPlayer->_pmode = PM_STAND;
	ProcessEscape(missile);
	EXPECT_EQ(MyPlayer->_pmode, PM_STAND);
}

} // namespace
} // namespace devilution
