#include "lua/modules/spells.hpp"

#include <string_view>
#include <type_traits>

#include <sol/sol.hpp>

#include "data/file.hpp"
#include "lua/metadoc.hpp"
#include "panels/spell_icons.hpp"
#include "tables/spelldat.h"

namespace devilution {

namespace {

void AddSpellDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LuaAddSpellDatFromFile(dataFile, path);
}

} // namespace

sol::table LuaSpellsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "addSpellDataFromTsv", "(path: string)",
	    "Add the spells in a spelldat-format TSV file. Call inside a SpellDataLoaded handler. Each spell's `id` column is its name. "
	    "SpellIDs are assigned once every mod has added its spells, sorted by id from a fixed base, so an id resolves to the same "
	    "SpellID in every game mode and mod load order; read it with getSpellId(id) from the SpellsAssigned event onward.",
	    AddSpellDataFromTsv);
	LuaSetDocFn(table, "getSpellId", "(name: string)",
	    "Return the SpellID integer for a spell id (base or mod-added), or nil if unknown. Mod-added ids resolve from the SpellsAssigned event onward.",
	    [](std::string_view name) -> sol::optional<int> {
		    auto id = ParseSpellId(name);
		    if (id.has_value() && *id != SpellID::Null)
			    return static_cast<int>(*id);
		    return sol::nullopt;
	    });
	LuaSetDocFn(table, "setSpellIcon", "(spellId: integer, iconName: string)",
	    "Set the speedbook icon for a dynamic spell by ID (e.g. \"Golem\"). Call after the spell has been assigned (e.g. in a SpellsAssigned handler).",
	    [](int spellId, std::string_view iconName) {
		    LuaRegisterDynamicSpellIcon(spellId, LuaParseSpellIconName(iconName));
	    });
	return table;
}

} // namespace devilution
