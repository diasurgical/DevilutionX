#include "lua/modules/towners.hpp"

#include <optional>
#include <unordered_map>
#include <utility>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "player.h"
#include "towners.h"

namespace devilution {
namespace {

// Map from towner type enum to Lua table name
const std::unordered_map<_talker_id, const char *> TownerTableNames = {
	{ TOWN_SMITH, "griswold" },
	{ TOWN_HEALER, "pepin" },
	{ TOWN_DEADGUY, "deadguy" },
	{ TOWN_TAVERN, "ogden" },
	{ TOWN_STORY, "cain" },
	{ TOWN_DRUNK, "farnham" },
	{ TOWN_WITCH, "adria" },
	{ TOWN_BMAID, "gillian" },
	{ TOWN_PEGBOY, "wirt" },
	{ TOWN_COW, "cow" },
	{ TOWN_FARMER, "lester" },
	{ TOWN_GIRL, "celia" },
	{ TOWN_COWFARM, "nut" },
};

void PopulateTownerTable(_talker_id townerId, sol::table &out)
{
	LuaSetDocFn(out, "position", "()",
	    "Returns towner coordinates",
	    [townerId]() -> std::optional<std::pair<int, int>> {
		    const Towner *towner = GetTowner(townerId);
		    if (towner == nullptr) return std::nullopt;
		    return std::make_pair(towner->position.x, towner->position.y);
	    });
}
} // namespace

sol::table LuaTownersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	// Iterate over all towner types found in TSV data
	for (const auto &[townerId, name] : TownerLongNames) {
		auto tableNameIt = TownerTableNames.find(townerId);
		if (tableNameIt == TownerTableNames.end())
			continue; // Skip if no table name mapping

		sol::table townerTable = lua.create_table();
		PopulateTownerTable(townerId, townerTable);
		LuaSetDoc(table, tableNameIt->second, /*signature=*/"", name.c_str(), std::move(townerTable));
	}
	return table;
}

} // namespace devilution
