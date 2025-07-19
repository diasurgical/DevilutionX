#ifdef _DEBUG
#include "lua/modules/dev/level/map.hpp"

#include <string>

#include <sol/sol.hpp>

#include "automap.h"
#include "lua/metadoc.hpp"

namespace devilution {
namespace {

std::string DebugCmdMapReveal()
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);
	return "Automap fully explored.";
}

std::string DebugCmdMapHide()
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			AutomapView[x][y] = MAP_EXP_NONE;
	return "Automap exploration removed.";
}

} // namespace

sol::table LuaDevLevelMapModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "hide", "()", "Hide the map.", &DebugCmdMapHide);
	LuaSetDocFn(table, "reveal", "()", "Reveal the map.", &DebugCmdMapReveal);
	return table;
}

} // namespace devilution
#endif // _DEBUG
