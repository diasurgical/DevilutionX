#include "lua/modules/towners.hpp"

#include <optional>
#include <utility>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "player.h"
#include "towners.h"

namespace devilution {
namespace {

const char *const TownerTableNames[NUM_TOWNER_TYPES] {
	"griswold",
	"pepin",
	"deadguy",
	"ogden",
	"cain",
	"farnham",
	"adria",
	"gillian",
	"wirt",
	"cow",
	"lester",
	"celia",
	"nut",
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
	for (uint8_t townerId = TOWN_SMITH; townerId < NUM_TOWNER_TYPES; ++townerId) {
		sol::table townerTable = lua.create_table();
		PopulateTownerTable(static_cast<_talker_id>(townerId), townerTable);
		LuaSetDoc(table, TownerTableNames[townerId], /*signature=*/"", TownerLongNames[townerId], std::move(townerTable));
	}
	return table;
}

} // namespace devilution
