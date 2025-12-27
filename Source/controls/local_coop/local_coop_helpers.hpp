/**
 * @file local_coop_helpers.hpp
 *
 * Helper utilities for local co-op code to reduce boilerplate.
 * Phase 2 simplification: Player validation helpers and common patterns.
 */
#pragma once

#include <cstdint>

namespace devilution {

// Forward declarations
struct LocalCoopPlayer;
struct Player;
struct ValidatedPlayer;

ValidatedPlayer GetValidatedPlayer(uint8_t playerId, bool requireInitialized = true, bool requireAlive = true);

namespace local_coop {

/**
 * @brief Helper function to check if player validation succeeded.
 * Reduces boilerplate when you just need to check validity.
 * 
 * @param playerId Game player ID (0-3)
 * @param requireInitialized If true, requires player to be initialized
 * @param requireAlive If true, requires player to be alive
 * @return true if player is valid, false otherwise
 */
inline bool IsPlayerValid(uint8_t playerId, bool requireInitialized = true, bool requireAlive = true)
{
	auto validated = GetValidatedPlayer(playerId, requireInitialized, requireAlive);
	return validated.isValid;
}

} // namespace local_coop

} // namespace devilution

