/**
 * @file spelldat.h
 *
 * Interface of all spell data.
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <expected.hpp>

#include "sound_effect_enums.h"
#include "utils/enum_traits.h"

namespace devilution {

enum class SpellType : uint8_t {
	Skill,
	FIRST = Skill,
	Spell,
	Scroll,
	Charges,
	LAST = Charges,
	Invalid,
};

enum class SpellID : int8_t {
	Null,
	FIRST = Null,
	Firebolt,
	Healing,
	Lightning,
	Flash,
	Identify,
	FireWall,
	TownPortal,
	StoneCurse,
	Infravision,
	Phasing,
	ManaShield,
	Fireball,
	Guardian,
	ChainLightning,
	FlameWave,
	DoomSerpents,
	BloodRitual,
	Nova,
	Invisibility,
	Inferno,
	Golem,
	Rage,
	Teleport,
	Apocalypse,
	Etherealize,
	ItemRepair,
	StaffRecharge,
	TrapDisarm,
	Elemental,
	ChargedBolt,
	HolyBolt,
	Resurrect,
	Telekinesis,
	HealOther,
	BloodStar,
	BoneSpirit,
	LastDiablo = BoneSpirit,
	Mana,
	Magi,
	Jester,
	LightningWall,
	Immolation,
	Warp,
	Reflect,
	Berserk,
	RingOfFire,
	Search,
	RuneOfFire,
	RuneOfLight,
	RuneOfNova,
	RuneOfImmolation,
	RuneOfStone,

	LAST = RuneOfStone,
	Invalid = -1,
};

tl::expected<SpellID, std::string> ParseSpellId(std::string_view value);

enum class MagicType : uint8_t {
	Fire,
	Lightning,
	Magic,
};

enum class MissileID : int8_t {
	// clang-format off
	Arrow,
	Firebolt,
	Guardian,
	Phasing,
	NovaBall,
	FireWall,
	Fireball,
	LightningControl,
	Lightning,
	MagmaBallExplosion,
	TownPortal,
	FlashBottom,
	FlashTop,
	ManaShield,
	FlameWave,
	ChainLightning,
	ChainBall, // unused
	BloodHit, // unused
	BoneHit, // unused
	MetalHit, // unused
	Rhino,
	MagmaBall,
	ThinLightningControl,
	ThinLightning,
	BloodStar,
	BloodStarExplosion,
	Teleport,
	FireArrow,
	DoomSerpents, // unused
	FireOnly, // unused
	StoneCurse,
	BloodRitual, // unused
	Invisibility, // unused
	Golem,
	Etherealize,
	Spurt, // unused
	ApocalypseBoom,
	Healing,
	FireWallControl,
	Infravision,
	Identify,
	FlameWaveControl,
	Nova,
	Rage, // BloodBoil in Diablo
	Apocalypse,
	ItemRepair,
	StaffRecharge,
	TrapDisarm,
	Inferno,
	InfernoControl,
	FireMan, // unused
	Krull, // unused
	ChargedBolt,
	HolyBolt,
	Resurrect,
	Telekinesis,
	LightningArrow,
	Acid,
	AcidSplat,
	AcidPuddle,
	HealOther,
	Elemental,
	ResurrectBeam,
	BoneSpirit,
	WeaponExplosion,
	RedPortal,
	DiabloApocalypseBoom,
	DiabloApocalypse,
	LastDiablo = DiabloApocalypse,
	Mana,
	Magi,
	LightningWall,
	LightningWallControl,
	Immolation,
	SpectralArrow,
	FireballBow,
	LightningBow,
	ChargedBoltBow,
	HolyBoltBow,
	Warp,
	Reflect,
	Berserk,
	RingOfFire,
	StealPotions,
	StealMana,
	RingOfLightning, // unused
	Search,
	Aura, // unused
	Aura2, // unused
	SpiralFireball, // unused
	RuneOfFire,
	RuneOfLight,
	RuneOfNova,
	RuneOfImmolation,
	RuneOfStone,
	BigExplosion,
	HorkSpawn,
	Jester,
	OpenNest,
	OrangeFlare,
	BlueFlare,
	RedFlare,
	YellowFlare,
	BlueFlare2,
	YellowExplosion,
	RedExplosion,
	BlueExplosion,
	BlueExplosion2,
	OrangeExplosion,

	LAST = OrangeExplosion,
	Null = -1,
	// clang-format on
};

enum class SpellDataFlags : uint8_t {
	// The lower 2 bytes are used to store MagicType.
	Fire = static_cast<uint8_t>(MagicType::Fire),
	Lightning = static_cast<uint8_t>(MagicType::Lightning),
	Magic = static_cast<uint8_t>(MagicType::Magic),
	Targeted = 1U << 2,
	AllowedInTown = 1U << 3,
};
use_enum_as_flags(SpellDataFlags);

struct SpellData {
	std::string sNameText;
	SfxID sSFX;
	uint16_t bookCost10;
	uint8_t staffCost10;
	uint8_t sManaCost;
	SpellDataFlags flags;
	int8_t sBookLvl;
	int8_t sStaffLvl;
	uint8_t minInt;
	MissileID sMissiles[2];
	uint8_t sManaAdj;
	uint8_t sMinMana;
	uint8_t sStaffMin;
	uint8_t sStaffMax;

	[[nodiscard]] MagicType type() const
	{
		return static_cast<MagicType>(static_cast<std::underlying_type<SpellDataFlags>::type>(flags) & 0b11U);
	}

	[[nodiscard]] uint32_t bookCost() const
	{
		return bookCost10 * 10;
	}

	[[nodiscard]] uint16_t staffCost() const
	{
		return staffCost10 * 10;
	}

	[[nodiscard]] bool isTargeted() const
	{
		return HasAnyOf(flags, SpellDataFlags::Targeted);
	}

	[[nodiscard]] bool isAllowedInTown() const
	{
		return HasAnyOf(flags, SpellDataFlags::AllowedInTown);
	}
};

extern std::vector<SpellData> SpellsData;

inline const SpellData &GetSpellData(SpellID spellId)
{
	return SpellsData[static_cast<std::underlying_type<SpellID>::type>(spellId)];
}

void LoadSpellData();

} // namespace devilution
