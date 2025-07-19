#ifdef _DEBUG
#include "lua/modules/dev/player/stats.hpp"

#include <string>

#include <sol/sol.hpp>

#include "engine/backbuffer_state.hpp"
#include "lua/metadoc.hpp"
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

std::string DebugCmdLevelUp(std::optional<int> levels)
{
	const int levelsToAdd = levels.value_or(1);
	if (levelsToAdd <= 0) return "amount must be positive";
	Player &myPlayer = *MyPlayer;
	for (int i = 0; i < levelsToAdd; i++)
		NetSendCmd(true, CMD_CHEAT_EXPERIENCE);
	return StrCat("New character level: ", myPlayer.getCharacterLevel() + levelsToAdd);
}

std::string DebugCmdMaxStats()
{
	Player &myPlayer = *MyPlayer;
	ModifyPlrStr(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength) - myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic) - myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity) - myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality) - myPlayer._pBaseVit);
	return "Set all character base attributes to maximum.";
}

std::string DebugCmdMinStats()
{
	Player &myPlayer = *MyPlayer;
	ModifyPlrStr(myPlayer, -myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, -myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, -myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, -myPlayer._pBaseVit);
	return "Set all character base attributes to minimum.";
}

std::string DebugCmdRefillHealthMana()
{
	Player &myPlayer = *MyPlayer;
	myPlayer.RestoreFullLife();
	myPlayer.RestoreFullMana();
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);
	return StrCat("Restored life and mana to full.");
}

std::string DebugCmdChangeHealth(int change)
{
	Player &myPlayer = *MyPlayer;
	if (change == 0)
		return StrCat("Enter a value not equal to 0 to change life!");

	int newHealth = myPlayer._pHitPoints + (change * 64);
	SetPlayerHitPoints(myPlayer, newHealth);
	if (newHealth <= 0)
		SyncPlrKill(myPlayer, DeathReason::MonsterOrTrap);

	return StrCat("Changed life by ", change);
}

std::string DebugCmdChangeMana(int change)
{
	Player &myPlayer = *MyPlayer;
	if (change == 0)
		return StrCat("Enter a value not equal to 0 to change mana!");

	int newMana = myPlayer._pMana + (change * 64);
	myPlayer._pMana = newMana;
	myPlayer._pManaBase = myPlayer._pMana + myPlayer._pMaxManaBase - myPlayer._pMaxMana;
	RedrawComponent(PanelDrawComponent::Mana);

	return StrCat("Changed mana by ", change);
}

} // namespace

sol::table LuaDevPlayerStatsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "adjustHealth", "(amount: number)", "Adjust HP (amount can be negative)", &DebugCmdChangeHealth);
	LuaSetDocFn(table, "adjustMana", "(amount: number)", "Adjust mana (amount can be negative)", &DebugCmdChangeMana);
	LuaSetDocFn(table, "levelUp", "(amount: number = 1)", "Level the player up.", &DebugCmdLevelUp);
	LuaSetDocFn(table, "rejuvenate", "()", "Refill health", &DebugCmdRefillHealthMana);
	LuaSetDocFn(table, "setAttrToMax", "()", "Set Str, Mag, Dex, and Vit to maximum.", &DebugCmdMaxStats);
	LuaSetDocFn(table, "setAttrToMin", "()", "Set Str, Mag, Dex, and Vit to minimum.", &DebugCmdMinStats);
	return table;
}

} // namespace devilution
#endif // _DEBUG
