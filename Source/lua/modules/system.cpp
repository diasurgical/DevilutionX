#include "lua/modules/system.hpp"

#include <sol/sol.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "lua/metadoc.hpp"

namespace devilution {

sol::table LuaSystemModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "get_ticks", "() -> integer", "Returns the number of milliseconds since the game started.",
	    []() { return static_cast<int>(SDL_GetTicks()); });

	return table;
}

} // namespace devilution
