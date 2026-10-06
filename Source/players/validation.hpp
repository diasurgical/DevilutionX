/**
 * @file players/validation.hpp
 *
 * Interface of functions for validation of player data.
 */
#pragma once

#include <cstddef>

namespace devilution {

struct Player;
bool IsNetPlayerValid(const Player &player);
bool IsNetPlayerValid(size_t playerId);

} // namespace devilution
