/**
 * @file spells.h
 *
 * Interface of functionality for casting player spells.
 */
#pragma once

#include <cstdint>

#include "engine/world_tile.hpp"
#include "player.h"

namespace devilution {

enum class SpellCheckResult : uint8_t {
	Success,
	Fail_NoMana,
	Fail_Level0,
	Fail_Busy,
};

bool IsValidSpell(SpellID spl);
bool IsValidSpellFrom(int spellFrom);
bool IsWallSpell(SpellID spl);
bool TargetsMonster(SpellID id);
int GetManaAmount(const Player &player, SpellID sn);
void ConsumeSpell(Player &player, SpellID sn);
SpellCheckResult CheckSpell(const Player &player, SpellID sn, SpellType st, bool manaonly);

/**
 * @brief Ensures the player's current readied spell is a valid selection for the character. If the current selection is
 * incompatible with the player's items and spell (for example, if the player does not currently have access to the spell),
 * the selection is cleared.
 * @note Will force a UI redraw in case the values actually change, so that the new spell reflects on the bottom panel.
 * @param player The player whose readied spell is to be checked.
 */
void EnsureValidReadiedSpell(Player &player);
void CastSpell(Player &player, SpellID spl, WorldTilePosition src, WorldTilePosition dst, int spllvl);
void SpawnResurrectBeam(Player &caster, Player &target);
void ApplyResurrect(Player &target);
void DoHealOther(const Player &caster, Player &target);
int GetSpellBookLevel(SpellID s);
int GetSpellStaffLevel(SpellID s);

/**
 * @brief Gets a value that represents the specified spellID in 64bit bitmask format.
 * For example:
 *  - spell ID  1: 0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0001
 *  - spell ID 43: 0000.0000.0000.0000.0000.0100.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000
 * @param spellId The id of the spell to get a bitmask for.
 * @return A 64bit bitmask representation for the specified spell.
 */
constexpr uint64_t GetSpellBitmask(SpellID spellId)
{
	return 1ULL << (static_cast<int8_t>(spellId) - 1);
}

} // namespace devilution
