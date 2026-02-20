#include "lua/modules/game.hpp"

#include <sol/sol.hpp>

#include "lua/metadoc.hpp"
#include "monster.h"
#include "multi.h"
#include "quests.h"
#include "tables/objdat.h"

namespace devilution {

sol::table LuaGameModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "prepDoEnding", "()",
	    "Triggers the game-ending sequence (win condition). Safe to call in multiplayer.",
	    PrepDoEnding);

	LuaSetDocFn(table, "isQuestDone", "(questId: integer) -> boolean",
	    "Returns true if the quest with the given ID has been completed.",
	    [](int questId) {
		    return Quests[questId]._qactive == QUEST_DONE;
	    });

	LuaSetDocFn(table, "isMultiplayer", "() -> boolean",
	    "Returns true when running in a multiplayer session.",
	    []() { return gbIsMultiplayer; });

	return table;
}

} // namespace devilution
