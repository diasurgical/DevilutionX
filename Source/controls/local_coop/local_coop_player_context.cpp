/**
 * @file local_coop_player_context.cpp
 *
 * Implementation of RAII helper for temporarily switching player context.
 */
#include "controls/local_coop/local_coop_player_context.hpp"

#include "player.h"

namespace devilution {

LocalCoopPlayerContext::LocalCoopPlayerContext(uint8_t playerId)
    : savedMyPlayer_(MyPlayer)
    , savedMyPlayerId_(MyPlayerId)
    , savedInspectPlayer_(InspectPlayer)
{
	if (playerId < Players.size()) {
		MyPlayer = &Players[playerId];
		MyPlayerId = playerId;
		InspectPlayer = &Players[playerId];
	}
}

LocalCoopPlayerContext::~LocalCoopPlayerContext()
{
	MyPlayer = savedMyPlayer_;
	MyPlayerId = savedMyPlayerId_;
	InspectPlayer = savedInspectPlayer_;
}

} // namespace devilution
