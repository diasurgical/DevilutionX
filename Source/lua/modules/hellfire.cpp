#include "lua/modules/hellfire.hpp"

#include <sol/sol.hpp>

#include "engine/assets.hpp"
#include "lua/metadoc.hpp"

namespace devilution {

sol::table LuaHellfireModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "loadData", "()", LoadHellfireArchives);
	LuaSetDocFn(table, "enable", "()", []() { gbIsHellfire = true; });
	return table;
}

} // namespace devilution
