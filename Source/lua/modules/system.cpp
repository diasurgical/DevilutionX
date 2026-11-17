#include "lua/modules/system.hpp"

#include <sol/sol.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "game_mode.hpp"
#include "lua/metadoc.hpp"

namespace devilution {

sol::table LuaSystemModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "get_ticks", "() -> integer", "Returns the number of milliseconds since the game started.",
	    []() { return static_cast<int>(SDL_GetTicks()); });

	LuaSetDocFn(table, "isHellfire", "() -> boolean",
	    "Returns true when the game is running in Hellfire mode.",
	    []() { return gbIsHellfire; });

	LuaSetDocFn(table, "isFriendlyFireEnabled", "() -> boolean",
	    "Returns the game's Friendly Fire setting (the same on every client).",
	    []() { return sgGameInitInfo.bFriendlyFire != 0; });

	return table;
}

} // namespace devilution
