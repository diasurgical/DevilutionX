/**
 * @file local_coop_player_context.hpp
 *
 * RAII helper for temporarily switching player context in local co-op mode.
 */
#pragma once

#include <cstdint>

namespace devilution {

struct Player;

/**
 * @brief RAII helper to temporarily swap MyPlayer, MyPlayerId, and InspectPlayer for local co-op actions.
 *
 * This ensures network commands are sent with the correct player ID and that
 * player-specific state is properly managed during action execution.
 * InspectPlayer is also swapped because UI elements like the spell menu use it.
 *
 * Usage:
 * @code
 * {
 *     LocalCoopPlayerContext context(playerId);
 *     // MyPlayer, MyPlayerId, and InspectPlayer now point to the specified player
 *     // ... perform actions as that player ...
 * } // Context restored to previous state
 * @endcode
 */
class LocalCoopPlayerContext {
public:
	/**
	 * @brief Construct context and switch to specified player.
	 * @param playerId Game player ID (0-3) to switch to
	 */
	explicit LocalCoopPlayerContext(uint8_t playerId);

	/**
	 * @brief Destructor restores previous player context.
	 */
	~LocalCoopPlayerContext();

	// Non-copyable
	LocalCoopPlayerContext(const LocalCoopPlayerContext &) = delete;
	LocalCoopPlayerContext &operator=(const LocalCoopPlayerContext &) = delete;

	// Non-movable (RAII should not be moved)
	LocalCoopPlayerContext(LocalCoopPlayerContext &&) = delete;
	LocalCoopPlayerContext &operator=(LocalCoopPlayerContext &&) = delete;

private:
	Player *savedMyPlayer_;
	uint8_t savedMyPlayerId_;
	Player *savedInspectPlayer_;
};

} // namespace devilution
