#include "lua/modules/level.hpp"

#include <sol/sol.hpp>

#include "levels/gendung.h"
#include "lua/metadoc.hpp"

namespace devilution {

sol::table LuaLevelModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "type", "() -> integer",
	    "Returns the current level type (TOWN, CATHEDRAL, etc.)",
	    []() {
		    return leveltype;
	    });

	// Dungeon type constants
	table["TOWN"] = DTYPE_TOWN;
	table["CATHEDRAL"] = DTYPE_CATHEDRAL;
	table["CATACOMBS"] = DTYPE_CATACOMBS;
	table["CAVES"] = DTYPE_CAVES;
	table["HELL"] = DTYPE_HELL;
	table["NEST"] = DTYPE_NEST;
	table["CRYPT"] = DTYPE_CRYPT;

	return table;
}

} // namespace devilution
