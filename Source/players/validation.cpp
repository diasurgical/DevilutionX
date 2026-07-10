/**
 * @file players/validation.cpp
 *
 * Implementation of functions for validation of player data.
 */

#include "players/validation.hpp"

#include <string_view>

#include "diablo.h"
#include "levels/gendung.h"
#include "player.h"
#include "tables/playerdat.hpp"

namespace devilution {

bool IsNetPlayerValid(const Player &player)
{
	// we no longer check character level here, players with out-of-range clevels are not allowed to join the game and we don't observe change clevel messages that would set it out of range
	// (the only code path that would result in _pLevel containing an out of range value in the DevilutionX code is a zero-initialized player, in which case _pName is empty)
	return static_cast<uint8_t>(player._pClass) < GetNumPlayerClasses()
	    && player.plrlevel < NUMLEVELS
	    && InDungeonBounds(player.position.tile)
	    && !std::string_view(player._pName).empty();
}

bool IsNetPlayerValid(size_t playerId)
{
	const Player &player = Players[playerId];
	return IsNetPlayerValid(player);
}

} // namespace devilution
