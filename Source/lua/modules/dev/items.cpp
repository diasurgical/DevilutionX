#ifdef _DEBUG
#include "lua/modules/dev/items.hpp"

#include <random>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <sol/sol.hpp>

#include "cursor.h"
#include "engine/random.hpp"
#include "items.h"
#include "lua/metadoc.hpp"
#include "pack.h"
#include "player.h"
#include "utils/is_of.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

const Item *DebugCmdGetItem()
{
	const Player &myPlayer = *MyPlayer;
	if (!myPlayer.HoldItem.isEmpty()) return &myPlayer.HoldItem;
	if (pcursinvitem != -1) {
		if (pcursinvitem < INVITEM_INV_FIRST) return &myPlayer.InvBody[pcursinvitem];
		if (pcursinvitem <= INVITEM_INV_LAST) return &myPlayer.InvList[pcursinvitem - INVITEM_INV_FIRST];
		return &myPlayer.SpdList[pcursinvitem - INVITEM_BELT_FIRST];
	}
	if (pcursitem != -1) return &Items[pcursitem];
	return nullptr;
}

std::string DebugCmdItemInfo()
{
	const Item *pItem = DebugCmdGetItem();
	if (pItem != nullptr) {
		const Player &myPlayer = *MyPlayer;
		std::string_view netPackValidation { "N/A" };
		if (gbIsMultiplayer) {
			ItemNetPack itemPack;
			Item unpacked;
			PackNetItem(*pItem, itemPack);
			netPackValidation = UnPackNetItem(myPlayer, itemPack, unpacked) ? "Success" : "Failure";
		}
		return StrCat("Name: ", pItem->_iIName,
		    "\nIDidx: ", pItem->IDidx, " (", AllItemsList[pItem->IDidx].iName, ")",
		    "\nSeed: ", pItem->_iSeed,
		    "\nCreateInfo: ", pItem->_iCreateInfo,
		    "\nLevel: ", pItem->_iCreateInfo & CF_LEVEL,
		    "\nOnly Good: ", ((pItem->_iCreateInfo & CF_ONLYGOOD) == 0) ? "False" : "True",
		    "\nUnique Monster: ", ((pItem->_iCreateInfo & CF_UPER15) == 0) ? "False" : "True",
		    "\nDungeon Item: ", ((pItem->_iCreateInfo & CF_UPER1) == 0) ? "False" : "True",
		    "\nUnique Item: ", ((pItem->_iCreateInfo & CF_UNIQUE) == 0) ? "False" : "True",
		    "\nSmith: ", ((pItem->_iCreateInfo & CF_SMITH) == 0) ? "False" : "True",
		    "\nSmith Premium: ", ((pItem->_iCreateInfo & CF_SMITHPREMIUM) == 0) ? "False" : "True",
		    "\nBoy: ", ((pItem->_iCreateInfo & CF_BOY) == 0) ? "False" : "True",
		    "\nWitch: ", ((pItem->_iCreateInfo & CF_WITCH) == 0) ? "False" : "True",
		    "\nHealer: ", ((pItem->_iCreateInfo & CF_HEALER) == 0) ? "False" : "True",
		    "\nPregen: ", ((pItem->_iCreateInfo & CF_PREGEN) == 0) ? "False" : "True",
		    "\nNet Validation: ", netPackValidation);
	}
	return StrCat("Num items: ", ActiveItemCount);
}

std::mt19937 BetterRng;
std::string DebugSpawnItem(std::string itemName)
{
	if (ActiveItemCount >= MAXITEMS) return "No space to generate the item!";

	const int max_time = 3000;
	const int max_iter = 1000000;

	AsciiStrToLower(itemName);

	Item testItem;

	uint32_t begin = SDL_GetTicks();
	int i = 0;
	for (;; i++) {
		// using a better rng here to seed the item to prevent getting stuck repeating same values using old one
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		if (SDL_GetTicks() - begin > max_time)
			return StrCat("Item not found in ", max_time / 1000, " seconds!");

		if (i > max_iter)
			return StrCat("Item not found in ", max_iter, " tries!");

		const int8_t monsterLevel = dist(BetterRng) % CF_LEVEL + 1;
		_item_indexes idx = RndItemForMonsterLevel(monsterLevel);
		if (IsAnyOf(idx, IDI_NONE, IDI_GOLD))
			continue;

		testItem = {};
		SetupAllItems(*MyPlayer, testItem, idx, AdvanceRndSeed(), monsterLevel, 1, false, false);
		TryRandomUniqueItem(testItem, idx, monsterLevel, 1, false, false);
		SetupItem(testItem);

		std::string tmp = AsciiStrToLower(testItem._iIName);
		if (tmp.find(itemName) != std::string::npos)
			break;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	item._iIdentified = true;
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
	return StrCat("Item generated successfully - iterations: ", i);
}

std::string DebugSpawnUniqueItem(std::string itemName)
{
	if (ActiveItemCount >= MAXITEMS) return "No space to generate the item!";

	AsciiStrToLower(itemName);
	UniqueItem uniqueItem;
	bool foundUnique = false;
	int uniqueIndex = 0;
	for (const auto &item : UniqueItems) {
		const std::string tmp = AsciiStrToLower(std::string_view(item.UIName));
		if (tmp.find(itemName) != std::string::npos) {
			itemName = tmp;
			uniqueItem = item;
			foundUnique = true;
			break;
		}
		++uniqueIndex;
	}
	if (!foundUnique) return "No unique item found!";

	_item_indexes uniqueBaseIndex = IDI_GOLD;
	for (size_t j = 0; j < AllItemsList.size(); j++) {
		if (!IsItemAvailable(static_cast<int>(j)))
			continue;
		if (AllItemsList[j].iItemId == uniqueItem.UIItemId) {
			uniqueBaseIndex = static_cast<_item_indexes>(j);
			break;
		}
	}

	if (uniqueBaseIndex == IDI_GOLD) return "Base item not available!";

	auto &baseItemData = AllItemsList[static_cast<size_t>(uniqueBaseIndex)];

	Item testItem;

	int i = 0;
	for (uint32_t begin = SDL_GetTicks();; i++) {
		constexpr int max_time = 3000;
		if (SDL_GetTicks() - begin > max_time)
			return StrCat("Item not found in ", max_time / 1000, " seconds!");

		constexpr int max_iter = 1000000;
		if (i > max_iter)
			return StrCat("Item not found in ", max_iter, " tries!");

		testItem = {};
		testItem._iMiscId = baseItemData.iMiscId;
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		for (auto &flag : UniqueItemFlags)
			flag = true;
		UniqueItemFlags[uniqueIndex] = false;
		SetupAllItems(*MyPlayer, testItem, uniqueBaseIndex, testItem._iMiscId == IMISC_UNIQUE ? uniqueIndex : AdvanceRndSeed(), uniqueItem.UIMinLvl, 1, false, false);
		TryRandomUniqueItem(testItem, uniqueBaseIndex, uniqueItem.UIMinLvl, 1, false, false);
		SetupItem(testItem);
		for (auto &flag : UniqueItemFlags)
			flag = false;

		if (testItem._iMagical != ITEM_QUALITY_UNIQUE)
			continue;

		const std::string tmp = AsciiStrToLower(testItem._iIName);
		if (tmp.find(itemName) != std::string::npos)
			break;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	item._iIdentified = true;
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);

	return StrCat("Item generated successfully - iterations: ", i);
}

} // namespace

sol::table LuaDevItemsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "get", "() -> Item", "Get the currently selected item.", &DebugCmdGetItem);
	LuaSetDocFn(table, "info", "()", "Show info of currently selected item.", &DebugCmdItemInfo);
	LuaSetDocFn(table, "spawn", "(name: string)", "Attempt to generate an item.", &DebugSpawnItem);
	LuaSetDocFn(table, "spawnUnique", "(name: string)", "Attempt to generate a unique item.", &DebugSpawnUniqueItem);
	return table;
}

} // namespace devilution
#endif // _DEBUG
