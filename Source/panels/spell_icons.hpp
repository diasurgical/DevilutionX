#pragma once

#include <cstdint>
#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "tables/spelldat.h"

#define SPLICONLENGTH 56

namespace devilution {

enum class SpellIcon : uint8_t {
	Firebolt,
	Healing,
	Lightning,
	Flash,
	Identify,
	FireWall,
	TownPortal,
	StoneCurse,
	Infravision,
	HealOther,
	Nova,
	Fireball,
	ManaShield,
	FlameWave,
	Inferno,
	ChainLightning,
	Sentinel, // unused
	DoomSerpents,
	BloodRitual,  // unused
	Invisibility, // unused
	Golem,
	Etherealize,
	BloodBoil,
	Teleport,
	Apocalypse,
	ItemRepair,
	Empty,
	Phasing,
	StaffRecharge,
	BoneSpirit,
	RedSkull,  // unused
	Pentagram, // unused
	FireCloud, // unused
	LongHorn,  // unused
	PentaStar, // unused
	BloodStar,
	TrapDisarm,
	Elemental,
	ChargedBolt,
	Telekinesis,
	Resurrect,
	HolyBolt,
	Warp,
	Search,
	Reflect,
	LightningWall,
	Immolation,
	Berserk,
	RingOfFire,
	Jester,
	Mana,
};

/**
 * Draw a large (56x56) spell icon onto the given buffer.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawLargeSpellIcon(const Surface &out, Point position, SpellID spell);

/**
 * Draw a small (37x38) spell icon onto the given buffer.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawSmallSpellIcon(const Surface &out, Point position, SpellID spell);

/**
 * Draw an inset 2px border for a large (56x56) spell icon.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawLargeSpellIconBorder(const Surface &out, Point position, uint8_t color);

/**
 * Draw an inset 2px border for a small (37x38) spell icon.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawSmallSpellIconBorder(const Surface &out, Point position);

/**
 * @brief Set the color mapping for the `Draw(Small|Large)SpellIcon(Border)` calls.
 */
void SetSpellTrans(SpellType t);

tl::expected<void, std::string> LoadLargeSpellIcons();
void FreeLargeSpellIcons();

tl::expected<void, std::string> LoadSmallSpellIcons();
void FreeSmallSpellIcons();

} // namespace devilution
