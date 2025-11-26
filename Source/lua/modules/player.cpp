#include "lua/modules/player.hpp"

#include <optional>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "msg.h"
#include "multi.h"
#include "player.h"

namespace devilution {
namespace {
void InitPlayerUserType(sol::state_view &lua)
{
	sol::usertype<Player> playerType = lua.new_usertype<Player>(sol::no_constructor);
	LuaSetDocReadonlyProperty(playerType, "name", "string",
	    "Player's name (readonly)",
	    &Player::name);
	LuaSetDocFn(playerType, "addExperience", "(experience: integer, monsterLevel: integer = nil)",
	    "Adds experience to this player based on the current game mode",
	    [](Player &player, uint32_t experience, std::optional<int> monsterLevel) {
		    if (monsterLevel.has_value()) {
			    player.addExperience(experience, *monsterLevel);
		    } else {
			    player.addExperience(experience);
		    }
	    });
	LuaSetDocProperty(playerType, "characterLevel", "number",
	    "Character level (writeable)",
	    &Player::getCharacterLevel, &Player::setCharacterLevel);
	LuaSetDocProperty(playerType, "manaShield", "boolean", "Whether mana shield is active (writeable)", [](const Player &player) { return player.pManaShield; }, [](Player &player, bool value) {
		    if (value == player.pManaShield)
			    return;
		    player.pManaShield = value;
		    if (gbIsMultiplayer && &player == MyPlayer)
			    NetSendCmd(true, value ? CMD_SETSHIELD : CMD_REMSHIELD); });
}
} // namespace

sol::table LuaPlayerModule(sol::state_view &lua)
{
	InitPlayerUserType(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "self", "()",
	    "The current player",
	    []() {
		    return MyPlayer;
	    });
	LuaSetDocFn(table, "walk_to", "(x: integer, y: integer)",
	    "Walk to the given coordinates",
	    [](int x, int y) {
		    NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, Point { x, y });
	    });
	return table;
}

} // namespace devilution
