#include "lua/modules/towners.hpp"

#include <optional>
#include <string>
#include <utility>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "player.h"
#include "stores.h"
#include "towners.h"

namespace devilution {
namespace {

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
		auto shortNameIt = TownerShortNames.find(townerId);
		if (shortNameIt == TownerShortNames.end())
			continue; // Skip if no short name mapping

		sol::table townerTable = lua.create_table();
		PopulateTownerTable(townerId, townerTable);
		LuaSetDoc(table, shortNameIt->second, /*signature=*/"", name.c_str(), std::move(townerTable));
	}

	LuaSetDocFn(table, "addDialogOption",
	    "(townerName: string, getLabel: function, onSelect: function)",
	    "Adds a dynamic dialog option to a towner's talk menu.\n"
	    "getLabel() is called each time the dialog opens; return a non-empty string to show\n"
	    "the option or an empty string/nil to hide it.\n"
	    "onSelect() is called when the player chooses the option.\n"
	    "All options are cleared when mods reload or Lua shuts down; register from mod init.",
	    [](std::string_view townerName, const sol::function &getLabel, const sol::function &onSelect) {
		    RegisterTownerDialogOption(
		        townerName,
		        [getLabel]() -> std::string {
			        sol::object result = getLabel();
			        if (result.get_type() == sol::type::string)
				        return result.as<std::string>();
			        return {};
		        },
		        [onSelect]() {
			        onSelect();
		        });
	    });

	return table;
}

} // namespace devilution
