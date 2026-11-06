#include "lua/modules/system.hpp"

#include <sol/sol.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <cstdint>
#include <string>

#include "game_mode.hpp"
#include "lua/metadoc.hpp"
#include "msg.h"
#include "player.h"

namespace devilution {

sol::table LuaSystemModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "get_ticks", "() -> integer", "Returns the number of milliseconds since the game started.",
	    []() { return static_cast<int>(SDL_GetTicks()); });

	LuaSetDocFn(table, "isMultiplayer", "() -> boolean",
	    "Returns true if the current game is multiplayer, false for single-player.",
	    []() { return gbIsMultiplayer; });

	// Generic net pipe. Send an opaque payload to other clients; it arrives via the NetMessage
	// event. The engine does not interpret the payload; encode/decode it in Lua. Default target is
	// every other player; pass an explicit player bitmask to override. No-op in single-player.
	LuaSetDocFn(table, "netSend", "(payload: string, mask: integer|nil)",
	    "Send an opaque payload to other clients over the generic Lua net pipe; received via the NetMessage event. Defaults to all other players. No-op in single-player.",
	    [](const std::string &payload, sol::optional<uint32_t> mask) {
		    // The outgoing packet header is built from the local player, so a send is only valid once
		    // it exists. A mod may reach this from a setup callback that runs before the local player
		    // is established; drop the send rather than dereference a null MyPlayer.
		    if (!gbIsMultiplayer || MyPlayer == nullptr) return;
		    const uint32_t pmask = mask.value_or(0xFFFFFF & ~(1U << MyPlayerId));
		    NetSendCmdLuaMessage(pmask, payload.data(), payload.size());
	    });

	return table;
}

} // namespace devilution
