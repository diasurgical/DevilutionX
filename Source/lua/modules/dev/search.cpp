#ifdef _DEBUG
#include "lua/modules/dev/quests.hpp"

#include <string>

#include <sol/sol.hpp>

#include "debug.h"
#include "lua/metadoc.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

std::string DebugCmdSearchMonster(std::string_view name)
{
	if (name.empty()) return "Missing monster name!";
	AddDebugAutomapMonsterHighlight(AsciiStrToLower(name));
	return StrCat("Added automap marker for monster ", name, ".");
}

std::string DebugCmdSearchItem(std::string_view name)
{
	if (name.empty()) return "Missing item name!";
	AddDebugAutomapItemHighlight(AsciiStrToLower(name));
	return StrCat("Added automap marker for item ", name, ".");
}

std::string DebugCmdSearchObject(std::string_view name)
{
	if (name.empty()) return "Missing object name!";
	AddDebugAutomapObjectHighlight(AsciiStrToLower(name));
	return StrCat("Added automap marker for object ", name, ".");
}

std::string DebugCmdClearSearch()
{
	ClearDebugAutomapHighlights();
	return "Removed all automap search markers.";
}

} // namespace

sol::table LuaDevSearchModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "clear", "()", "Clear search results from the map.", &DebugCmdClearSearch);
	LuaSetDocFn(table, "item", "(name: string)", "Search the map for an item.", &DebugCmdSearchItem);
	LuaSetDocFn(table, "monster", "(name: string)", "Search the map for a monster.", &DebugCmdSearchMonster);
	LuaSetDocFn(table, "object", "(name: string)", "Search the map for an object.", &DebugCmdSearchObject);
	return table;
}

} // namespace devilution
#endif // _DEBUG
