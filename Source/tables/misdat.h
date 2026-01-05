/**
 * @file misdat.h
 *
 * Interface of data related to missiles.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <expected.hpp>

#include "effects.h"
#include "engine/clx_sprite.hpp"
#include "spelldat.h"
#include "utils/enum_traits.h"

namespace devilution {

enum mienemy_type : uint8_t {
	TARGET_MONSTERS,
	TARGET_PLAYERS,
	TARGET_BOTH,
};

enum class DamageType : uint8_t {
	Physical,
	Fire,
	Lightning,
	Magic,
	Acid,
};

enum class MissileGraphicID : uint8_t {
	Arrow,
	Fireball,
	Guardian,
	Lightning,
	FireWall,
	MagmaBallExplosion,
	TownPortal,
	FlashBottom,
	FlashTop,
	ManaShield,
	BloodHit,
	BoneHit,
	MetalHit,
	FireArrow,
	DoomSerpents,
	Golem,
	Spurt,
	ApocalypseBoom,
	StoneCurseShatter,
	BigExplosion,
	Inferno,
	ThinLightning,
	BloodStar,
	BloodStarExplosion,
	MagmaBall,
	Krull,
	ChargedBolt,
	HolyBolt,
	HolyBoltExplosion,
	LightningArrow,
	FireArrowExplosion,
	Acid,
	AcidSplat,
	AcidPuddle,
	Etherealize,
	Elemental,
	Resurrect,
	BoneSpirit,
	RedPortal,
	DiabloApocalypseBoom,
	BloodStarBlue,
	BloodStarBlueExplosion,
	BloodStarYellow,
	BloodStarYellowExplosion,
	BloodStarRed,
	BloodStarRedExplosion,
	HorkSpawn,
	Reflect,
	OrangeFlare,
	BlueFlare,
	RedFlare,
	YellowFlare,
	Rune,
	YellowFlareExplosion,
	BlueFlareExplosion,
	RedFlareExplosion,
	BlueFlare2,
	OrangeFlareExplosion,
	BlueFlareExplosion2,
	None,
};

/**
 * @brief Specifies what if and how movement distribution is applied
 */
enum class MissileMovementDistribution : uint8_t {
	/**
	 * @brief No movement distribution is calculated. Normally this means the missile doesn't move at all.
	 */
	Disabled,
	/**
	 * @brief The missile moves and if it hits an enemy it stops (for example firebolt)
	 */
	Blockable,
	/**
	 * @brief The missile moves and even it hits an enemy it keeps moving (for example flame wave)
	 */
	Unblockable,
};

struct Missile;
struct AddMissileParameter;

enum class MissileDataFlags : uint8_t {
	// The lower 3 bytes are used to store DamageType.
	Physical = static_cast<uint8_t>(DamageType::Physical),
	Fire = static_cast<uint8_t>(DamageType::Fire),
	Lightning = static_cast<uint8_t>(DamageType::Lightning),
	Magic = static_cast<uint8_t>(DamageType::Magic),
	Acid = static_cast<uint8_t>(DamageType::Acid),
	Arrow = 1 << 4,
	Invisible = 1 << 5,
};
use_enum_as_flags(MissileDataFlags);

/**
 * Represent a more fine-grained direction than the 8 value Direction enum.
 *
 * This is used when rendering projectiles like arrows which have additional sprites for "half-winds" on a 16-point compass.
 * The sprite sheets are typically 0-indexed and use the following layout (relative to the screen projection)
 *
 *      W  WSW   SW  SSW  S
 *               ^
 *     WNW       |       SSE
 *               |
 *     NW -------+------> SE
 *               |
 *     NNW       |       ESE
 *               |
 *      N  NNE   NE  ENE  E
 */
enum class Direction16 : uint8_t {
	South,
	South_SouthWest,
	SouthWest,
	West_SouthWest,
	West,
	West_NorthWest,
	NorthWest,
	North_NorthWest,
	North,
	North_NorthEast,
	NorthEast,
	East_NorthEast,
	East,
	East_SouthEast,
	SouthEast,
	South_SouthEast,
};

struct MissileData {
	using AddFn = void (*)(Missile &, AddMissileParameter &);
	using ProcessFn = void (*)(Missile &);

	AddFn addFn;
	ProcessFn processFn;

	/**
	 * @brief Sound emitted when cast.
	 */
	SfxID castSound;
	/**
	 * @brief Sound emitted on impact.
	 */
	SfxID hitSound;
	MissileGraphicID graphic;
	MissileDataFlags flags;
	MissileMovementDistribution movementDistribution;

	[[nodiscard]] bool isDrawn() const
	{
		return !HasAnyOf(flags, MissileDataFlags::Invisible);
	}

	[[nodiscard]] bool isArrow() const
	{
		return HasAnyOf(flags, MissileDataFlags::Arrow);
	}

	[[nodiscard]] DamageType damageType() const
	{
		return static_cast<DamageType>(static_cast<std::underlying_type<MissileDataFlags>::type>(flags) & 0b111U);
	}
};

enum class MissileGraphicsFlags : uint8_t {
	// clang-format off
	None         = 0,
	MonsterOwned = 1 << 0,
	NotAnimated  = 1 << 1,
	// clang-format on
};

struct MissileFileData {
	OptionalOwnedClxSpriteListOrSheet sprites;
	uint16_t animWidth;
	int8_t animWidth2;
	std::string name;
	uint8_t animFAmt;
	MissileGraphicsFlags flags;
	uint8_t animDelayIdx;
	uint8_t animLenIdx;

	[[nodiscard]] uint8_t animDelay(uint8_t dir) const;
	[[nodiscard]] uint8_t animLen(uint8_t dir) const;

	tl::expected<void, std::string> LoadGFX();

	void FreeGFX()
	{
		sprites = std::nullopt;
	}

	/**
	 * @brief Returns the sprite list for a given direction.
	 *
	 * @param direction One of the 16 directions. Valid range: [0, 15].
	 * @return OptionalClxSpriteList
	 */
	[[nodiscard]] OptionalClxSpriteList spritesForDirection(Direction16 direction) const
	{
		if (!sprites)
			return std::nullopt;
		return sprites->isSheet() ? sprites->sheet()[static_cast<size_t>(direction)] : sprites->list();
	}
};

const MissileData &GetMissileData(MissileID missileId);
MissileFileData &GetMissileSpriteData(MissileGraphicID graphicId);

void LoadMissileData();

tl::expected<void, std::string> InitMissileGFX();
void FreeMissileGFX();

} // namespace devilution
