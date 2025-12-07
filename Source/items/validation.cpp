/**
 * @file items/validation.cpp
 *
 * Implementation of functions for validation of player and item data.
 */

#include "items/validation.h"

#include <cstdint>

#include "items.h"
#include "monstdat.h"
#include "msg.h"
#include "player.h"
#include "spells.h"
#include "utils/endian_swap.hpp"
#include "utils/is_of.hpp"

namespace devilution {

namespace {

bool hasMultipleFlags(uint16_t flags)
{
	return (flags & (flags - 1)) > 0;
}

} // namespace

bool IsCreationFlagComboValid(uint16_t iCreateInfo)
{
	iCreateInfo = iCreateInfo & ~CF_LEVEL;
	const bool isTownItem = (iCreateInfo & CF_TOWN) != 0;
	const bool isPregenItem = (iCreateInfo & CF_PREGEN) != 0;
	const bool isUsefulItem = (iCreateInfo & CF_USEFUL) == CF_USEFUL;

	if (isPregenItem) {
		// Pregen flags are discarded when an item is picked up, therefore impossible to have in the inventory
		return false;
	}
	if (isUsefulItem && (iCreateInfo & ~CF_USEFUL) != 0)
		return false;
	if (isTownItem && hasMultipleFlags(iCreateInfo)) {
		// Items from town can only have 1 towner flag
		return false;
	}
	return true;
}

bool IsTownItemValid(uint16_t iCreateInfo, const Player &player)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;
	const bool isBoyItem = (iCreateInfo & CF_BOY) != 0;
	const uint8_t maxTownItemLevel = 30;

	// Wirt items in multiplayer are equal to the level of the player, therefore they cannot exceed the max character level
	if (isBoyItem && level <= player.getMaxCharacterLevel())
		return true;

	return level <= maxTownItemLevel;
}

bool IsShopPriceValid(const Item &item)
{
	const int boyPriceLimit = MaxBoyValue;
	if (!gbIsHellfire && (item._iCreateInfo & CF_BOY) != 0 && item._iIvalue > boyPriceLimit)
		return false;

	const int premiumPriceLimit = MaxVendorValue;
	if (!gbIsHellfire && (item._iCreateInfo & CF_SMITHPREMIUM) != 0 && item._iIvalue > premiumPriceLimit)
		return false;

	const uint16_t smithOrWitch = CF_SMITH | CF_WITCH;
	const int smithAndWitchPriceLimit = gbIsHellfire ? MaxVendorValueHf : MaxVendorValue;
	if ((item._iCreateInfo & smithOrWitch) != 0 && item._iIvalue > smithAndWitchPriceLimit)
		return false;

	return true;
}

bool IsUniqueMonsterItemValid(uint16_t iCreateInfo, uint32_t dwBuff)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;

	// Check all unique monster levels to see if they match the item level
	for (const UniqueMonsterData &uniqueMonsterData : UniqueMonstersData) {
		if (IsAnyOf(uniqueMonsterData.mtype, MT_DEFILER, MT_NAKRUL, MT_HORKDMN)) {
			// These monsters don't use their mlvl for item generation
			continue;
		}

		const auto &uniqueMonsterLevel = static_cast<uint8_t>(MonstersData[uniqueMonsterData.mtype].level);

		if (level == uniqueMonsterLevel) {
			// If the ilvl matches the mlvl, we confirm the item is legitimate
			return true;
		}
	}

	return false;
}

bool IsDungeonItemValid(uint16_t iCreateInfo, uint32_t dwBuff)
{
	const uint8_t level = iCreateInfo & CF_LEVEL;
	const bool isHellfireItem = (dwBuff & CF_HELLFIRE) != 0;

	// Check all monster levels to see if they match the item level
	int type = -1;
	for (const MonsterData &monsterData : MonstersData) {
		type++;
		auto monsterLevel = static_cast<uint8_t>(monsterData.level);

		if (type != MT_DIABLO && monsterData.availability == MonsterAvailability::Never) {
			// Skip monsters that are unable to appear in the game
			continue;
		}

		if (type == MT_DIABLO && isHellfireItem) {
			// Adjust The Dark Lord's mlvl if the item is a Hellfire item to match the Diablo mlvl
			monsterLevel += 15;
		}

		if (level == monsterLevel) {
			// If the ilvl matches the mlvl, we confirm the item is legitimate
			return true;
		}
	}

	if (isHellfireItem) {
		uint8_t hellfireMaxDungeonLevel = 24;

		// Hellfire adjusts the currlevel minus 7 in dungeon levels 20-24 for generating items
		hellfireMaxDungeonLevel -= 7;
		return level <= (hellfireMaxDungeonLevel * 2);
	}

	uint8_t diabloMaxDungeonLevel = 16;

	// Diablo doesn't have containers that drop items in dungeon level 16, therefore we decrement by 1
	diabloMaxDungeonLevel--;
	return level <= (diabloMaxDungeonLevel * 2);
}

bool IsHellfireSpellBookValid(const Item &spellBook)
{
	// Hellfire uses the spell book level when generating items via CreateSpellBook()
	int spellBookLevel = GetSpellBookLevel(spellBook._iSpell);

	// CreateSpellBook() adds 1 to the spell level for ilvl
	spellBookLevel++;

	if (spellBookLevel >= 1 && (spellBook._iCreateInfo & CF_LEVEL) == spellBookLevel * 2) {
		// The ilvl matches the result for a spell book drop, so we confirm the item is legitimate
		return true;
	}

	return IsDungeonItemValid(spellBook._iCreateInfo, spellBook.dwBuff);
}

bool IsItemValid(const Player &player, const Item &item)
{
	if (!gbIsMultiplayer)
		return true;

	if (item.IDidx == IDI_EAR)
		return true;
	if (item.IDidx != IDI_GOLD && !IsCreationFlagComboValid(item._iCreateInfo))
		return false;
	if ((item._iCreateInfo & CF_TOWN) != 0)
		return IsTownItemValid(item._iCreateInfo, player) && IsShopPriceValid(item);
	if ((item._iCreateInfo & CF_USEFUL) == CF_UPER15)
		return IsUniqueMonsterItemValid(item._iCreateInfo, item.dwBuff);
	if ((item.dwBuff & CF_HELLFIRE) != 0 && AllItemsList[item.IDidx].iMiscId == IMISC_BOOK)
		return IsHellfireSpellBookValid(item);

	return IsDungeonItemValid(item._iCreateInfo, item.dwBuff);
}

bool IsItemDeltaValid(const TCmdPItem &itemDelta)
{
	if (itemDelta.bCmd == CMD_INVALID)
		return true;
	if (IsNoneOf(itemDelta.bCmd, TCmdPItem::FloorItem, TCmdPItem::PickedUpItem, TCmdPItem::DroppedItem))
		return false;
	if (!InDungeonBounds({ itemDelta.x, itemDelta.y }))
		return false;
	const _item_indexes idx = static_cast<_item_indexes>(Swap16LE(itemDelta.def.wIndx));
	if (idx == IDI_EAR)
		return true;
	if (!IsItemAvailable(idx))
		return false;
	Item item = {};
	RecreateItem(*MyPlayer, itemDelta.item, item);
	return IsItemValid(*MyPlayer, item);
}

} // namespace devilution
