#include "lua/modules/player.hpp"

#include <optional>

#include <sol/sol.hpp>

#include "effects.h"
#include "engine/backbuffer_state.hpp"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "lua/metadoc.hpp"
#include "player.h"

namespace devilution {
namespace {

void InitPlayerUserType(sol::state_view &lua)
{
	sol::usertype<Player> playerType = lua.new_usertype<Player>(sol::no_constructor);
	LuaSetDocReadonlyProperty(playerType, "name", "string",
	    "Player's name (readonly)",
	    &Player::name);
	LuaSetDocReadonlyProperty(playerType, "id", "integer",
	    "Player's unique ID (readonly)",
	    [](const Player &player) {
		    return static_cast<int>(reinterpret_cast<uintptr_t>(&player));
	    });
	LuaSetDocReadonlyProperty(playerType, "position", "Point",
	    "Player's current position (readonly)",
	    [](const Player &player) -> Point {
		    return Point { player.position.tile };
	    });
	LuaSetDocReadonlyProperty(playerType, "headItem", "Item | nil",
	    "Equipped head item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_HEAD].isEmpty() ? nullptr : &player.InvBody[INVLOC_HEAD];
	    });
	LuaSetDocReadonlyProperty(playerType, "chestItem", "Item | nil",
	    "Equipped chest item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_CHEST].isEmpty() ? nullptr : &player.InvBody[INVLOC_CHEST];
	    });
	LuaSetDocReadonlyProperty(playerType, "amuletItem", "Item | nil",
	    "Equipped amulet item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_AMULET].isEmpty() ? nullptr : &player.InvBody[INVLOC_AMULET];
	    });
	LuaSetDocReadonlyProperty(playerType, "leftRingItem", "Item | nil",
	    "Equipped left ring item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_RING_LEFT].isEmpty() ? nullptr : &player.InvBody[INVLOC_RING_LEFT];
	    });
	LuaSetDocReadonlyProperty(playerType, "rightRingItem", "Item | nil",
	    "Equipped right ring item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_RING_RIGHT].isEmpty() ? nullptr : &player.InvBody[INVLOC_RING_RIGHT];
	    });
	LuaSetDocReadonlyProperty(playerType, "leftHandItem", "Item | nil",
	    "Equipped left-hand item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_HAND_LEFT].isEmpty() ? nullptr : &player.InvBody[INVLOC_HAND_LEFT];
	    });
	LuaSetDocReadonlyProperty(playerType, "rightHandItem", "Item | nil",
	    "Equipped right-hand item (readonly)",
	    [](const Player &player) -> const Item * {
		    return player.InvBody[INVLOC_HAND_RIGHT].isEmpty() ? nullptr : &player.InvBody[INVLOC_HAND_RIGHT];
	    });
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
	LuaSetDocFn(playerType, "addItem", "(itemId: integer, count: integer = 1)",
	    "Add an item to the player's inventory",
	    [](Player &player, int itemId, std::optional<int> count) -> bool {
		    const _item_indexes itemIndex = static_cast<_item_indexes>(itemId);
		    const int itemCount = count.value_or(1);
		    for (int i = 0; i < itemCount; i++) {
			    Item tempItem {};
			    SetupAllItems(player, tempItem, itemIndex, AdvanceRndSeed(), 1, 1, true, false);
			    if (!AutoPlaceItemInInventory(player, tempItem, true)) {
				    return false;
			    }
		    }
		    CalcPlrInv(player, true);
		    return true;
	    });
	LuaSetDocFn(playerType, "hasItem", "(itemId: integer)",
	    "Check if the player has an item with the given ID",
	    [](const Player &player, int itemId) -> bool {
		    return HasInventoryOrBeltItemWithId(player, static_cast<_item_indexes>(itemId));
	    });
	LuaSetDocFn(playerType, "removeItem", "(itemId: integer, count: integer = 1)",
	    "Remove an item from the player's inventory",
	    [](Player &player, int itemId, std::optional<int> count) -> int {
		    const _item_indexes targetId = static_cast<_item_indexes>(itemId);
		    const int itemCount = count.value_or(1);
		    int removed = 0;

		    // Remove from inventory
		    for (int i = player._pNumInv - 1; i >= 0 && removed < itemCount; i--) {
			    if (player.InvList[i].IDidx == targetId) {
				    player.RemoveInvItem(i);
				    removed++;
			    }
		    }

		    // Remove from belt if needed
		    for (int i = MaxBeltItems - 1; i >= 0 && removed < itemCount; i--) {
			    if (!player.SpdList[i].isEmpty() && player.SpdList[i].IDidx == targetId) {
				    player.RemoveSpdBarItem(i);
				    removed++;
			    }
		    }

		    if (removed > 0) {
			    CalcPlrInv(player, true);
		    }

		    return removed;
	    });
	LuaSetDocFn(playerType, "restoreFullLife", "()",
	    "Restore player's HP to maximum",
	    [](Player &player) {
		    player._pHitPoints = player._pMaxHP;
		    player._pHPBase = player._pMaxHPBase;
	    });
	LuaSetDocFn(playerType, "restoreFullMana", "()",
	    "Restore player's mana to maximum",
	    [](Player &player) {
		    player._pMana = player._pMaxMana;
		    player._pManaBase = player._pMaxManaBase;
	    });
	LuaSetDocReadonlyProperty(playerType, "mana", "number",
	    "Current mana (readonly)",
	    [](Player &player) { return player._pMana >> 6; });
	LuaSetDocReadonlyProperty(playerType, "maxMana", "number",
	    "Maximum mana (readonly)",
	    [](Player &player) { return player._pMaxMana >> 6; });
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
