#ifdef _DEBUG
#include "lua/modules/dev/player/gold.hpp"

#include <cstdint>
#include <optional>
#include <string>

#include <sol/sol.hpp>

#include "items.h"
#include "lua/metadoc.hpp"
#include "player.h"

namespace devilution {
namespace {

std::string DebugCmdGiveGoldCheat(std::optional<int> amount)
{
	int goldToAdd = amount.value_or(GOLD_MAX_LIMIT * InventoryGridCells);
	if (goldToAdd <= 0) return "amount must be positive";
	Player &myPlayer = *MyPlayer;
	const int goldAmountBefore = myPlayer._pGold;
	for (int8_t &itemIndex : myPlayer.InvGrid) {
		if (itemIndex < 0)
			continue;

		Item &item = myPlayer.InvList[itemIndex != 0 ? itemIndex - 1 : myPlayer._pNumInv];

		if (itemIndex != 0) {
			if ((!item.isGold() && !item.isEmpty()) || (item.isGold() && item._ivalue == GOLD_MAX_LIMIT))
				continue;
		} else {
			if (item.isEmpty()) {
				MakeGoldStack(item, 0);
				myPlayer._pNumInv++;
				itemIndex = myPlayer._pNumInv;
			}
		}

		int goldThatCanBeAdded = (GOLD_MAX_LIMIT - item._ivalue);
		if (goldThatCanBeAdded >= goldToAdd) {
			item._ivalue += goldToAdd;
			myPlayer._pGold += goldToAdd;
			break;
		}

		item._ivalue += goldThatCanBeAdded;
		goldToAdd -= goldThatCanBeAdded;
		myPlayer._pGold += goldThatCanBeAdded;
	}

	CalcPlrInv(myPlayer, true);

	return StrCat("Set your gold to ", myPlayer._pGold, ", added ", myPlayer._pGold - goldAmountBefore, ".");
}

std::string DebugCmdTakeGoldCheat(std::optional<int> amount)
{
	Player &myPlayer = *MyPlayer;
	int goldToRemove = amount.value_or(GOLD_MAX_LIMIT * InventoryGridCells);
	if (goldToRemove <= 0) return "amount must be positive";

	const int goldAmountBefore = myPlayer._pGold;
	for (auto itemIndex : myPlayer.InvGrid) {
		itemIndex -= 1;

		if (itemIndex < 0)
			continue;

		Item &item = myPlayer.InvList[itemIndex];
		if (!item.isGold())
			continue;

		if (item._ivalue >= goldToRemove) {
			myPlayer._pGold -= goldToRemove;
			item._ivalue -= goldToRemove;
			if (item._ivalue == 0)
				myPlayer.RemoveInvItem(itemIndex);
			break;
		}

		myPlayer._pGold -= item._ivalue;
		goldToRemove -= item._ivalue;
		myPlayer.RemoveInvItem(itemIndex);
	}

	return StrCat("Set your gold to ", myPlayer._pGold, ", removed ", goldAmountBefore - myPlayer._pGold, ".");
}

} // namespace

sol::table LuaDevPlayerGoldModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "give", "(amount: number = MAX)", "Gives the player gold.", &DebugCmdGiveGoldCheat);
	LuaSetDocFn(table, "take", "(amount: number = MAX)", "Takes the player's gold away.", &DebugCmdTakeGoldCheat);
	return table;
}

} // namespace devilution
#endif // _DEBUG
