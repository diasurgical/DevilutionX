#include "lua/modules/monsters.hpp"

#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "data/file.hpp"
#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "monster.h"
#include "tables/monstdat.h"

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

int AliveEnemyCount()
{
	return GetAliveEnemyCount();
}

int TotalSpawnedEnemies()
{
	return LevelSpawnedMonsters - ReservedMonsterSlotsForGolems;
}

} // namespace

sol::table LuaMonstersModule(sol::state_view &lua)
{
	InitMonsterUserType(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "addMonsterDataFromTsv", "(path: string)", AddMonsterDataFromTsv);
	LuaSetDocFn(table, "addUniqueMonsterDataFromTsv", "(path: string)", AddUniqueMonsterDataFromTsv);
	LuaSetDocFn(table, "aliveEnemyCount", "() -> integer",
	    "Returns the number of hostile monsters currently alive on the active level (excludes player-summoned golems).",
	    AliveEnemyCount);
	LuaSetDocFn(table, "totalSpawnedEnemies", "() -> integer",
	    "Returns the high-water-mark of hostile monsters ever spawned on the active level (excludes the 4 reserved golem slots). Returns <= 0 when no enemies have spawned this level.",
	    TotalSpawnedEnemies);
	return table;
}

} // namespace devilution
