#include "lua/modules/system.hpp"

#include <algorithm>
#include <string>

#include <sol/sol.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "lua/metadoc.hpp"
#include "options.h"

namespace devilution {

sol::table LuaSystemModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "get_ticks", "() -> integer", "Returns the number of milliseconds since the game started.",
	    []() { return static_cast<int>(SDL_GetTicks()); });

	LuaSetDocFn(table, "is_mod_active", "(name: string) -> boolean",
	    "Returns true if the named mod is currently enabled.",
	    [](const std::string &name) {
		    const auto activeMods = GetOptions().Mods.GetActiveModList();
		    return std::ranges::find(activeMods.begin(), activeMods.end(), name) != activeMods.end();
	    });

	return table;
}

} // namespace devilution
