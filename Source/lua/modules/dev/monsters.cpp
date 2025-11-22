#ifdef _DEBUG
#include "lua/modules/dev/monsters.hpp"

#include <optional>
#include <string>

#include <sol/sol.hpp>

#include "crawl.hpp"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "lighting.h"
#include "lua/metadoc.hpp"
#include "monstdat.h"
#include "monster.h"
#include "player.h"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

std::string DebugCmdSpawnUniqueMonster(std::string name, std::optional<unsigned> countOpt)
{
	if (leveltype == DTYPE_TOWN) return "Can't spawn monsters in town";
	if (name.empty()) return "name is required";
	const unsigned count = countOpt.value_or(1);
	if (count < 1) return "count must be positive";

	AsciiStrToLower(name);

	int mtype = -1;
	UniqueMonsterType uniqueIndex = UniqueMonsterType::None;
	for (size_t i = 0; i < UniqueMonstersData.size(); ++i) {
		const auto &mondata = UniqueMonstersData[i];
		const std::string monsterName = AsciiStrToLower(std::string_view(mondata.mName));
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = mondata.mtype;
		uniqueIndex = static_cast<UniqueMonsterType>(i);
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1) return "Monster not found";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		if (LevelMonsterTypeCount == MaxLvlMTypes)
			LevelMonsterTypeCount--; // we are running out of monster types, so override last used monster type
		tl::expected<size_t, std::string> idResult = AddMonsterType(uniqueIndex, PLACE_SCATTER);
		if (!idResult.has_value()) return std::move(idResult).error();
		id = idResult.value();
		CMonster &monsterType = LevelMonsterTypes[id];
		InitMonsterGFX(monsterType);
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	unsigned spawnedMonster = 0;

	auto ret = Crawl(0, MaxCrawlRadius, [&](Displacement displacement) -> std::optional<std::string> {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return {};
		if (!IsTileWalkable(pos))
			return {};

		Monster *monster = AddMonster(pos, myPlayer._pdir, id, true);
		if (monster == nullptr)
			return StrCat("Spawned ", spawnedMonster, " monsters. (Unable to spawn more)");
		PrepareUniqueMonst(*monster, uniqueIndex, 0, 0, UniqueMonstersData[static_cast<size_t>(uniqueIndex)]);
		monster->corpseId = 1;
		spawnedMonster += 1;

		if (spawnedMonster >= count)
			return StrCat("Spawned ", spawnedMonster, " monsters.");

		return {};
	});

	if (!ret.has_value())
		ret = StrCat("Spawned ", spawnedMonster, " monsters. (Unable to spawn more)");
	return *ret;
}

std::string DebugCmdSpawnMonster(std::string name, std::optional<unsigned> countOpt)
{
	if (leveltype == DTYPE_TOWN) return "Can't spawn monsters in town";
	if (name.empty()) return "name is required";
	const unsigned count = countOpt.value_or(1);
	if (count < 1) return "count must be positive";

	AsciiStrToLower(name);

	int mtype = -1;

	for (size_t i = 0; i < MonstersData.size(); i++) {
		const auto &mondata = MonstersData[i];
		const std::string monsterName = AsciiStrToLower(std::string_view(mondata.name));
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = static_cast<int>(i);
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1) return "Monster not found";
	if (!MyPlayer->isLevelOwnedByLocalClient()) return "You are not the level owner.";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		if (LevelMonsterTypeCount == MaxLvlMTypes)
			LevelMonsterTypeCount--; // we are running out of monster types, so override last used monster type
		tl::expected<size_t, std::string> idResult = AddMonsterType(static_cast<_monster_id>(mtype), PLACE_SCATTER);
		if (!idResult.has_value()) return std::move(idResult).error();
		id = idResult.value();
		CMonster &monsterType = LevelMonsterTypes[id];
		InitMonsterGFX(monsterType);
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	size_t monstersToSpawn = std::min<size_t>(MaxMonsters - ActiveMonsterCount, count);
	if (monstersToSpawn == 0)
		return "Can't spawn any monsters";

	size_t spawnedMonster = 0;
	Crawl(0, MaxCrawlRadius, [&](Displacement displacement) {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return false;
		if (!IsTileWalkable(pos))
			return false;

		SpawnMonster(pos, myPlayer._pdir, id);
		spawnedMonster += 1;

		return spawnedMonster == monstersToSpawn;
	});

	if (monstersToSpawn != count)
		return StrCat("Spawned ", spawnedMonster, " monsters. (Unable to spawn more)");
	return StrCat("Spawned ", spawnedMonster, " monsters.");
}

} // namespace

sol::table LuaDevMonstersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "spawn", "(name: string, count: number = 1)", "Spawn monster(s)", &DebugCmdSpawnMonster);
	LuaSetDocFn(table, "spawnUnique", "(name: string, count: number = 1)", "Spawn unique monster(s)", &DebugCmdSpawnUniqueMonster);
	return table;
}

} // namespace devilution
#endif // _DEBUG
