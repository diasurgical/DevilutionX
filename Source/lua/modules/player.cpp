#include "lua/modules/player.hpp"

#include <optional>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "player.h"

namespace devilution {
namespace {
void InitPlayerUserType(sol::state_view &lua)
{
	sol::usertype<Player> playerType = lua.new_usertype<Player>(sol::no_constructor);
	SetDocumented(playerType, "name", "",
	    "Player's name (readonly)",
	    sol::readonly_property(&Player::name));
	SetDocumented(playerType, "addExperience", "(experience: integer, monsterLevel: integer = nil)",
	    "Adds experience to this player based on the current game mode",
	    [](Player &player, uint32_t experience, std::optional<int> monsterLevel) {
		    if (monsterLevel.has_value()) {
			    player.addExperience(experience, *monsterLevel);
		    } else {
			    player.addExperience(experience);
		    }
	    });
	SetDocumented(playerType, "characterLevel", "",
	    "Character level (writeable)",
	    sol::property(&Player::getCharacterLevel, &Player::setCharacterLevel));
}
} // namespace

sol::table LuaPlayerModule(sol::state_view &lua)
{
	InitPlayerUserType(lua);
	sol::table table = lua.create_table();
	SetDocumented(table, "self", "()",
	    "The current player",
	    []() {
		    return MyPlayer;
	    });
	SetDocumented(table, "walk_to", "(x: integer, y: integer)",
	    "Walk to the given coordinates",
	    [](int x, int y) {
		    NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, Point { x, y });
	    });
	return table;
}

} // namespace devilution
