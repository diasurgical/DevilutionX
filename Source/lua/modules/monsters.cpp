#include "lua/modules/monsters.hpp"

#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "data/file.hpp"
#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "monster.h"
#include "tables/monstdat.h"
#include "utils/language.h"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

void AddMonsterDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LoadMonstDatFromFile(dataFile, path, true);
}

void AddUniqueMonsterDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LoadUniqueMonstDatFromFile(dataFile, path);
}

void InitMonsterUserType(sol::state_view &lua)
{
	sol::usertype<Monster> monsterType = lua.new_usertype<Monster>(sol::no_constructor);
	LuaSetDocReadonlyProperty(monsterType, "position", "Point",
	    "Monster's current position (readonly)",
	    [](const Monster &monster) {
		    return Point { monster.position.tile };
	    });
	LuaSetDocReadonlyProperty(monsterType, "id", "integer",
	    "Monster's unique ID (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(reinterpret_cast<uintptr_t>(&monster));
	    });
}

} // namespace

sol::table LuaMonstersModule(sol::state_view &lua)
{
	InitMonsterUserType(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "addMonsterDataFromTsv", "(path: string)", AddMonsterDataFromTsv);
	LuaSetDocFn(table, "addUniqueMonsterDataFromTsv", "(path: string)", AddUniqueMonsterDataFromTsv);
	return table;
}

} // namespace devilution
