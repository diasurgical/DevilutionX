#include "lua/modules/game.hpp"

#include <string_view>

#include <magic_enum/magic_enum.hpp>
#include <sol/sol.hpp>

#include "engine/sound.h"
#include "lua/metadoc.hpp"
#include "monster.h"
#include "tables/questdat.hpp"

namespace devilution {

namespace {

void RegisterQuestIDEnum(sol::state_view &lua)
{
	sol::table enumTable = lua.create_table();
	for (const quest_id quest : magic_enum::enum_values<quest_id>()) {
		std::string_view name = magic_enum::enum_name(quest);
		if (quest == Q_INVALID || !name.starts_with("Q_"))
			continue;
		name.remove_prefix(2);
		enumTable[name] = static_cast<int>(quest);
	}
	lua["QuestID"] = enumTable;
}

bool IsQuestDone(int quest)
{
	return quest >= 0 && quest < MAXQUESTS && Quests[quest]._qactive == QUEST_DONE;
}

void EndGame()
{
	// PrepDoEnding restores the sound state saved when Diablo's ending sequence starts.
	sgbSaveSoundOn = gbSoundOn;
	PrepDoEnding();
}

} // namespace

sol::table LuaGameModule(sol::state_view &lua)
{
	RegisterQuestIDEnum(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "isQuestDone", "(quest: QuestID) -> boolean",
	    "Whether the given quest has been completed.",
	    IsQuestDone);
	LuaSetDocFn(table, "endGame", "()",
	    "Ends the game with the victory ending, as when Diablo is defeated. In multiplayer, other players see the ending when they next enter level 16.",
	    EndGame);
	table["QuestID"] = lua["QuestID"];
	return table;
}

} // namespace devilution
