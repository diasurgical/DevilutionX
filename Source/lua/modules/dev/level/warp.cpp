#ifdef _DEBUG
#include "lua/modules/dev/level/warp.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include <sol/sol.hpp>

#include "debug.h"
#include "interfac.h"
#include "levels/setmaps.h"
#include "lua/metadoc.hpp"
#include "player.h"
#include "quests.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

std::string DebugCmdWarpToDungeonLevel(uint8_t level)
{
	Player &myPlayer = *MyPlayer;
	if (level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " does not exist!");
	if (!setlevel && myPlayer.isOnLevel(level))
		return StrCat("You are already on level ", level, "!");

	StartNewLvl(myPlayer, (level != 21) ? interface_mode::WM_DIABNEXTLVL : interface_mode::WM_DIABTOWNWARP, level);
	return StrCat("Moved you to level ", level, ".");
}

std::string DebugCmdWarpToQuestLevel(uint8_t level)
{
	if (level < 1)
		return StrCat("Quest level number must be 1 or higher!");
	if (setlevel && setlvlnum == level)
		return StrCat("You are already on quest level", level, "!");

	for (Quest &quest : Quests) {
		if (level != quest._qslvl)
			continue;

		setlvltype = quest._qlvltype;
		StartNewLvl(*MyPlayer, WM_DIABSETLVL, level);

		return StrCat("Moved you to quest level ", QuestLevelNames[level], ".");
	}

	return StrCat("Quest level ", level, " does not exist!");
}

std::string DebugCmdWarpToCustomMap(std::string_view path, int dunType, int x, int y)
{
	if (path.empty()) return "path is required";
	if (dunType < DTYPE_CATHEDRAL || dunType > DTYPE_LAST) return "invalid dunType";

	const Point spawn { x, y };
	if (!InDungeonBounds(spawn)) return "spawn location is out of bounds";

	TestMapPath = StrCat(path, ".dun");
	setlvltype = static_cast<dungeon_type>(dunType);
	ViewPosition = spawn;

	StartNewLvl(*MyPlayer, WM_DIABSETLVL, SL_NONE);

	return StrCat("Moved you to ", TestMapPath, ".");
}

} // namespace

sol::table LuaDevLevelWarpModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "dungeon", "(n: number)", "Go to dungeon level (0 for town).", &DebugCmdWarpToDungeonLevel);
	LuaSetDocFn(table, "map", "(path: string, dunType: number, x: number, y: number)", "Go to custom {path}.dun level", &DebugCmdWarpToCustomMap);
	LuaSetDocFn(table, "quest", "(n: number)", "Go to quest level.", &DebugCmdWarpToQuestLevel);
	return table;
}

} // namespace devilution
#endif // _DEBUG
