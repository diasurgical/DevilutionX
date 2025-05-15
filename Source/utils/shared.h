#pragma once

#include "utils/ring.h"
#include "player.h"

namespace devilution {

namespace shared {
	extern struct ring_queue   input_queue;
	extern struct ring_queue   events_queue;
	extern uint64_t            game_ticks;
	extern uint64_t            game_saves;
	extern uint64_t            game_loads;
	extern struct player_state player;

	void share_diablo_state(const std::string &path);
}
}
