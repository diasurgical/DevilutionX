#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

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

std::expected<void, std::string> LoadLargeSpellIcons();
void FreeLargeSpellIcons();

std::expected<void, std::string> LoadSmallSpellIcons();
void FreeSmallSpellIcons();

/** Lua mod support: parse a SpellIcon name string to its frame index. Returns Empty on unknown names. */
uint8_t LuaParseSpellIconName(std::string_view name);

/** Lua mod support: register a custom icon frame for a dynamic spell ID. */
void LuaRegisterDynamicSpellIcon(int spellId, uint8_t iconFrame);

/** Lua mod support: clear all dynamic spell icon registrations. Call at the start of each data reload. */
void LuaClearDynamicSpellIcons();

} // namespace devilution
