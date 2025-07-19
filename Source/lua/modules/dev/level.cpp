#ifdef _DEBUG
#include "lua/modules/dev/level.hpp"

#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

#include <sol/sol.hpp>

#include "diablo.h"
#include "levels/gendung.h"
#include "lua/metadoc.hpp"
#include "lua/modules/dev/level/map.hpp"
#include "lua/modules/dev/level/warp.hpp"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "utils/endian_stream.hpp"
#include "utils/file_util.h"

namespace devilution {

namespace {

std::string ExportDun()
{
	const std::string levelName = StrCat(currlevel, "-", DungeonSeeds[currlevel], ".dun");
	FILE *dunFile = OpenFile(levelName.c_str(), "ab");

	WriteLE16(dunFile, DMAXX);
	WriteLE16(dunFile, DMAXY);

	/** Tiles. */
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			WriteLE16(dunFile, dungeon[x][y]);
		}
	}

	/** Padding */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, 0);
		}
	}

	/** Monsters */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t monsterId = 0;
			if (dMonster[x][y] > 0) {
				for (int i = 0; i < 157; i++) {
					if (MonstConvTbl[i] == Monsters[std::abs(dMonster[x][y]) - 1].type().type) {
						monsterId = i + 1;
						break;
					}
				}
			}
			WriteLE16(dunFile, monsterId);
		}
	}

	/** Objects */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t objectId = 0;
			Object *object = FindObjectAtPosition({ x, y }, false);
			if (object != nullptr) {
				for (int i = 0; i < 147; i++) {
					if (ObjTypeConv[i] == object->_otype) {
						objectId = i;
						break;
					}
				}
			}
			WriteLE16(dunFile, objectId);
		}
	}

	/** Transparency */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, dTransVal[x][y]);
		}
	}
	std::fclose(dunFile);

	return StrCat("Successfully exported ", levelName, ".");
}

std::string DebugCmdResetLevel(uint8_t level, std::optional<int> seed)
{
	Player &myPlayer = *MyPlayer;
	if (level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " does not exist!");
	if (myPlayer.isOnLevel(level))
		return "Unable to reset dungeon levels occupied by players!";

	myPlayer._pLvlVisited[level] = false;
	DeltaClearLevel(level);

	if (seed.has_value()) {
		DungeonSeeds[level] = *seed;
		LevelSeeds[level] = std::nullopt;
		return StrCat("Successfully reset level ", level, " with seed ", *seed, ".");
	}
	return StrCat("Successfully reset level ", level, ".");
}

std::string DebugCmdLevelSeed(std::optional<uint8_t> level)
{
	constexpr size_t NumLevels = sizeof(DungeonSeeds) / sizeof(DungeonSeeds[0]);
	if (level.has_value() && *level >= NumLevels) {
		return StrCat("level out of range, max: ", NumLevels - 1);
	}
	return StrCat(DungeonSeeds[level.value_or(currlevel)]);
}

} // namespace

sol::table LuaDevLevelModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "exportDun", "()", "Save the current level as a dun-file.", &ExportDun);
	LuaSetDocFn(table, "map", "", "Automap-related commands.", LuaDevLevelMapModule(lua));
	LuaSetDocFn(table, "reset", "(n: number, seed: number = nil)", "Resets specified level.", &DebugCmdResetLevel);
	LuaSetDocFn(table, "seed", "(level: number = nil)", "Get the seed of the current or given level.", &DebugCmdLevelSeed);
	LuaSetDocFn(table, "warp", "", "Warp to a level or a custom map.", LuaDevLevelWarpModule(lua));
	return table;
}

} // namespace devilution
#endif // _DEBUG
