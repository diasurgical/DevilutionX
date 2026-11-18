/**
 * @file shrines.cpp
 *
 * Implementation of the shrine effects referenced by shrines.tsv.
 */
#include "shrines.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <tables/shrinedat.hpp>

#include "automap.h"
#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "inv_iterators.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "missiles.h"
#include "msg.h"
#include "objects.h"
#include "player.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "tables/spelldat.h"
#include "utils/is_of.hpp"
#include "utils/language.h"

namespace devilution {

namespace {

/**
 * Maps from shrine_id to shrine name.
 * Not referenced in code: shrine names are loaded from shrines.tsv and translated at runtime.
 * This array only exists so that the names are picked up for translation extraction.
 */
[[maybe_unused]] const char *const ShrineNames[] = {
	// TRANSLATORS: Shrine Name Block
	N_("Mysterious"),
	N_("Hidden"),
	N_("Gloomy"),
	N_("Weird"),
	N_("Magical"),
	N_("Stone"),
	N_("Religious"),
	N_("Enchanted"),
	N_("Thaumaturgic"),
	N_("Fascinating"),
	N_("Cryptic"),
	N_("Magical"),
	N_("Eldritch"),
	N_("Eerie"),
	N_("Divine"),
	N_("Holy"),
	N_("Sacred"),
	N_("Spiritual"),
	N_("Spooky"),
	N_("Abandoned"),
	N_("Creepy"),
	N_("Quiet"),
	N_("Secluded"),
	N_("Ornate"),
	N_("Glimmering"),
	N_("Tainted"),
	N_("Oily"),
	N_("Glowing"),
	N_("Mendicant's"),
	N_("Sparkling"),
	N_("Town"),
	N_("Shimmering"),
	N_("Solar"),
	// TRANSLATORS: Shrine Name Block end
	N_("Murphy's"),
};

int ClampU8(int v)
{
	return std::clamp(v, 0, 255);
}

int FractionOrPercentReduction(int v, int reduction)
{
	return (reduction < 0) ? (v / (-reduction)) : (v * reduction / 100);
}

int ValueFromInterval(DiabloGenerator &rng, const ShrineEffect &shrineEffect)
{
	const int v1 = shrineEffect.param1;
	const int v2 = shrineEffect.param2;
	if (v1 == v2) {
		return v1;
	}
	const int min = std::min(v1, v2);
	const int max = std::max(v1, v2);
	return rng.generateRnd(1 + max - min) + min;
}

} // namespace

/**
 * @brief Modifies a random attribute using first value and the rest with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Value to change a random attribute
 * @param shrineEffect.param2 Value to change the rest of the attributes
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRandomAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int randomAttributeChange = shrineEffect.param1;
	const int otherAttributesChange = shrineEffect.param2;
	Player &player = *MyPlayer;

	const int r = rng.generateRnd((randomAttributeChange != otherAttributesChange) ? 4 : -1);
	const int v1 = r == 0 ? randomAttributeChange : otherAttributesChange;
	const int v2 = r == 1 ? randomAttributeChange : otherAttributesChange;
	const int v3 = r == 2 ? randomAttributeChange : otherAttributesChange;
	const int v4 = r == 3 ? randomAttributeChange : otherAttributesChange;

	if (v1 != 0) {
		ModifyPlrStr(player, v1);
	}
	if (v2 != 0) {
		ModifyPlrMag(player, v2);
	}
	if (v3 != 0) {
		ModifyPlrDex(player, v3);
	}
	if (v4 != 0) {
		ModifyPlrVit(player, v4);
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Modifies the durability of a random item using first value and the rest of items with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Durability change to a random item
 * @param shrineEffect.param2 Durability change for the rest of the items
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRandomItemDurabilityChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int otherItemsDurabilityChange = shrineEffect.param2;
	const int randomItemDurabilityChange = shrineEffect.param1 - otherItemsDurabilityChange;
	Player &player = *MyPlayer;

	int cnt = 0;
	for (const auto &item : player.InvBody) {
		if (!item.isEmpty())
			cnt++;
	}
	if (cnt > 0) {
		for (auto &item : player.InvBody) {
			if (!item.isEmpty()
			    && item._iMaxDur != DUR_INDESTRUCTIBLE
			    && item._iMaxDur != 0) {
				item._iDurability += otherItemsDurabilityChange;
				item._iMaxDur += otherItemsDurabilityChange;
				if (item._iDurability > item._iMaxDur)
					item._iDurability = item._iMaxDur;
				if (item._iDurability <= 0)
					item._iDurability = 1;
				if (item._iMaxDur <= 0)
					item._iMaxDur = 1;
				// TODO: Check if here it should clamp the max durability so the item doesn't become indestructible
			}
		}
		while (true) {
			cnt = 0;
			for (auto &item : player.InvBody) {
				if (!item.isEmpty() && item._iMaxDur != DUR_INDESTRUCTIBLE && item._iMaxDur != 0) {
					cnt++;
				}
			}
			if (cnt == 0)
				break;
			const int r = rng.generateRnd(NUM_INVLOC);
			if (player.InvBody[r].isEmpty() || player.InvBody[r]._iMaxDur == DUR_INDESTRUCTIBLE || player.InvBody[r]._iMaxDur == 0)
				continue;

			player.InvBody[r]._iDurability += randomItemDurabilityChange;
			player.InvBody[r]._iMaxDur += randomItemDurabilityChange;
			if (player.InvBody[r]._iDurability > player.InvBody[r]._iMaxDur)
				player.InvBody[r]._iDurability = player.InvBody[r]._iMaxDur;
			if (player.InvBody[r]._iDurability <= 0)
				player.InvBody[r]._iDurability = 1;
			if (player.InvBody[r]._iMaxDur <= 0)
				player.InvBody[r]._iMaxDur = 1;
			// TODO: Check if here it should clamp the max durability so the item doesn't become indestructible
			break;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief For all items in the player's inventory, modifies the armor class of all armor and the max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Change to items armor class
 * @param shrineEffect.param2 Change to items max damage
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectArmorClassMaxDamageChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int itemsAcChange = shrineEffect.param1;
	const int itemsMaxDamChange = shrineEffect.param2;
	Player &player = *MyPlayer;

	for (Item &item : PlayerItemsRange(player)) {
		switch (item._itype) {
		case ItemType::Sword:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff: {
			const int minDam = static_cast<int>(item._iMinDam);
			const int maxDam = static_cast<int>(item._iMaxDam);
			const int newMax = std::max(maxDam + itemsMaxDamChange, minDam);
			item._iMaxDam = static_cast<uint8_t>(ClampU8(newMax));
		} break;
		case ItemType::Shield:
		case ItemType::Helm:
		case ItemType::LightArmor:
		case ItemType::MediumArmor:
		case ItemType::HeavyArmor:
			item._iAC = ClampU8(item._iAC + itemsAcChange);
			break;
		default:
			break;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief For all items in the player's inventory, modifies the min and max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Change to items max damage
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectMinMaxDamageChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	auto isWeapon = [](const Item &item) {
		switch (item._itype) {
		case ItemType::Sword:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff:
			return true;
		default:
			return false;
		}
	};

	const int itemsMinDamChange = shrineEffect.param1;
	const int itemsMaxDamChange = shrineEffect.param2;

	auto bumpDam = [&](Item &item) {
		const int minDam = static_cast<int>(item._iMinDam);
		const int maxDam = static_cast<int>(item._iMaxDam);
		const int minDamChanged = ClampU8(minDam + itemsMinDamChange);
		const int maxDamChanged = ClampU8(maxDam + itemsMaxDamChange);

		item._iMinDam = std::max(minDamChanged, 1);
		item._iMaxDam = std::max(maxDamChanged, minDamChanged);
	};

	Player &player = *MyPlayer;

	Item &left = player.InvBody[INVLOC_HAND_LEFT];
	if (!left.isEmpty() && left._itype != ItemType::Shield)
		bumpDam(left);

	Item &right = player.InvBody[INVLOC_HAND_RIGHT];
	if (!right.isEmpty() && right._itype != ItemType::Shield)
		bumpDam(right);

	for (Item &item : InventoryPlayerItemsRange { player }) {
		if (isWeapon(item))
			bumpDam(item);
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Player operating shrine casts a spell at his position
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine to use as the source position of the spell
 * @param shrineEffect.spellId Spell that the player will cast
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectPlayerCastSpellFromPlayer(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const SpellID spellId = shrineEffect.spellId;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineEffect.param1;
		const int spellAdd = shrineEffect.param2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    operatePlayer.position.tile,
			    operatePlayer.position.tile,
			    operatePlayer._pdir,
			    spellData.sMissiles[i],
			    TARGET_MONSTERS,
			    operatePlayer,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Recharges all staves in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRechargesStaves(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (Item &item : PlayerItemsRange { *MyPlayer }) {
		if (item._itype == ItemType::Staff)
			item._iCharges = item._iMaxCharges;
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Repairs all items in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRepairsItems(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (Item &item : PlayerItemsRange { *MyPlayer }) {
		item._iDurability = item._iMaxDur;
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Modifies the level of a random spell using first value and the rest of spell levels with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Level change to a random spell
 * @param shrineEffect.param2 Level change for the rest of the spells
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRandomSpellLevelChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int effectParam1 = shrineEffect.param1;
	const int effectParam2 = shrineEffect.param2;

	Player &player = *MyPlayer;

	int cnt = 0;
	uint64_t spell = 1;
	const uint64_t spells = player._pMemSpells;
	for (uint16_t j = 0; j < SpellsData.size(); j++) {
		if ((spell & spells) != 0)
			cnt++;
		spell *= 2;
	}
	if (cnt > 1) {
		int spellToReduce;
		do {
			spellToReduce = rng.generateRnd(static_cast<int32_t>(SpellsData.size())) + 1;
		} while ((player._pMemSpells & GetSpellBitmask(static_cast<SpellID>(spellToReduce))) == 0);

		spell = 1;
		for (auto j = static_cast<uint8_t>(SpellID::Firebolt); j < SpellsData.size(); j++) {
			if ((player._pMemSpells & spell) != 0 && player._pSplLvl[j] < MaxSpellLevel && j != spellToReduce) {
				const auto newSpellLevel = static_cast<uint8_t>(std::clamp(player._pSplLvl[j] + effectParam1, 0, static_cast<int>(MaxSpellLevel)));
				player._pSplLvl[j] = newSpellLevel;
				NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, j, newSpellLevel);
			}
			spell *= 2;
		}

		if (player._pSplLvl[spellToReduce] > 0) {
			const auto newSpellLevel = static_cast<uint8_t>(std::clamp(player._pSplLvl[spellToReduce] + effectParam2, 0, static_cast<int>(MaxSpellLevel)));
			player._pSplLvl[spellToReduce] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, spellToReduce, newSpellLevel);
		}

		for (Item &item : InventoryPlayerItemsRange { player }) {
			item.updateRequiredStatsCacheForPlayer(player);
		}
		if (IsStashOpen) {
			Stash.RefreshItemStatFlags();
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Closes and refills all opened chests on current level
 * @warning Probably doesn't work in multiplayer
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRefillsChests(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &object = Objects[ActiveObjects[j]];
		if (object.IsChest() && !object.canInteractWith()) {
			object._oRndSeed = rng.advanceRndSeed();
			object.selectionRegion = SelectionRegion::Bottom;
			object._oAnimFrame -= 2;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes a specific spell level
 * @param player The player that activated the shrine
 * @param shrineEffect.param1 Min amount of levels to increase the spell
 * @param shrineEffect.param2 Max amount of levels to increase the spell
 * @param shrineEffect.spellId Spell to increase level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectChangeSpellLevel(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int spellLevelChange = ValueFromInterval(rng, shrineEffect);
	const SpellID spellId = shrineEffect.spellId;

	Player &player = *MyPlayer;

	player._pMemSpells |= GetSpellBitmask(spellId);

	const uint8_t curSpellLevel = player._pSplLvl[static_cast<int8_t>(spellId)];
	if (curSpellLevel < MaxSpellLevel) {
		const uint8_t newSpellLevel = std::min(static_cast<uint8_t>(curSpellLevel + spellLevelChange), MaxSpellLevel);
		player._pSplLvl[static_cast<int8_t>(spellId)] = newSpellLevel;
		NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellId), newSpellLevel);
	}

	for (Item &item : InventoryPlayerItemsRange { player }) {
		item.updateRequiredStatsCacheForPlayer(player);
	}
	if (IsStashOpen) {
		Stash.RefreshItemStatFlags();
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Reduces base max mana
 * @param player The player that activated the shrine
 * @param shrineEffect.param1 Percent of base max mana to decrease or a fraction if negative
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectManaBaseReduction(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;

	int maxBase = player._pMaxManaBase.raw();

	if (maxBase < 0) {
		// Fix bugged state; do not turn this into a "negative penalty" mana boost.
		player._pMaxManaBase = Fixed26_6::fromInt(0);
		maxBase = 0;
	}

	const int manaReduction = shrineEffect.param1;
	if (manaReduction != 0 && manaReduction < 100) {
		const Fixed26_6 penalty = Fixed26_6::fromRaw(FractionOrPercentReduction(maxBase, manaReduction)); // manaReduction % of max base mana (>= 0)

		player._pMaxManaBase -= penalty; // will remain >= 0
		player._pManaBase -= penalty;    // may go negative, allowed
		player._pMaxMana -= penalty;     // may go negative, allowed
		player._pMana -= penalty;        // may go negative, allowed
	}

	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Restores life and/or mana
 * @param player The player that activated the shrine
 * @param shrineEffect.param1 If greater than 0, restores life
 * @param shrineEffect.param2 If greater than 0, restores mana
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRestoreLifeAndMana(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;

	if (shrineEffect.param1 > 0) {
		player._pHitPoints = player._pMaxHP;
		player._pHPBase = player._pMaxHPBase;
	}
	if (shrineEffect.param2 > 0) {
		player._pMana = player._pMaxMana;
		player._pManaBase = player._pMaxManaBase;
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief All potions in the player's inventory are turn to Rejuvenation.
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectTransformPotionsToRejuvenation(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (Item &item : InventoryAndBeltPlayerItemsRange { *MyPlayer }) {
		if (item._itype != ItemType::Misc) {
			continue;
		}
		if (IsAnyOf(item._iMiscId, IMISC_HEAL, IMISC_MANA)) {
			// Reinitializing the item zeroes out the seed, we save and restore here to avoid triggering false
			// positives on duplicated item checks (e.g. when picking up the item).
			auto seed = item._iSeed;
			InitializeItem(item, ItemMiscIdIdx(IMISC_REJUV));
			item._iSeed = seed;
			item._iStatFlag = true;
			continue;
		}
		if (IsAnyOf(item._iMiscId, IMISC_FULLHEAL, IMISC_FULLMANA)) {
			// As above.
			auto seed = item._iSeed;
			InitializeItem(item, ItemMiscIdIdx(IMISC_FULLREJUV));
			item._iSeed = seed;
			item._iStatFlag = true;
			continue;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes the magic attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Min amount to change the magic attribute
 * @param shrineEffect.param2 Max amount to change the magic attribute
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectMagicAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int change = ValueFromInterval(rng, shrineEffect);
	ModifyPlrMag(*MyPlayer, change);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Creates mana or rejuvenation potions at the shrine position
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The map tile where the potions will be spawned
 * @param shrineEffect.param1 The minimum dungeon level that will spawn rejuvenation potions
 * @param shrineEffect.param2 The amount of potions as an absolute value. Full potions if positive, normal potions if negative
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectCreateManaOrRejuvPotions(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int rejuvenationMinDungeonLevel = shrineEffect.param1;
	const int numPotions = std::abs(shrineEffect.param2);
	const bool fullPotions = shrineEffect.param2 > 0;

	item_misc_id potionMiscId;
	if (currlevel < rejuvenationMinDungeonLevel) {
		potionMiscId = fullPotions ? IMISC_FULLMANA : IMISC_MANA;
	} else {
		potionMiscId = fullPotions ? IMISC_FULLREJUV : IMISC_REJUV;
	}
	for (int i = 0; i < numPotions; i++) {
		CreateTypeItem(spawnPosition, false, ItemType::Misc, potionMiscId, false, false, true);
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Creates life or rejuvenation potions at the shrine position
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The map tile where the potions will be spawned
 * @param shrineEffect.param1 The minimum dungeon level that will spawn rejuvenation potions
 * @param shrineEffect.param2 The amount of potions as an absolute value. Full potions if positive, normal potions if negative
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectCreateLifeOrRejuvPotions(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int rejuvenationMinDungeonLevel = shrineEffect.param1;
	const int numPotions = std::abs(shrineEffect.param2);
	const bool fullPotions = shrineEffect.param2 > 0;

	item_misc_id potionMiscId;
	if (currlevel < rejuvenationMinDungeonLevel) {
		potionMiscId = fullPotions ? IMISC_FULLHEAL : IMISC_HEAL;
	} else {
		potionMiscId = fullPotions ? IMISC_FULLREJUV : IMISC_REJUV;
	}
	for (int i = 0; i < numPotions; i++) {
		CreateTypeItem(spawnPosition, false, ItemType::Misc, potionMiscId, false, false, true);
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Player operating shrine casts a spell at his position with 0,0 coordinates as destiny
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine to use as the source position of the spell
 * @param shrineEffect.spellId Spell that the player will cast
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectPlayerCastSpellFromPlayerTowardZero(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const SpellID spellId = shrineEffect.spellId;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineEffect.param1;
		const int spellAdd = shrineEffect.param2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    operatePlayer.position.tile,
			    { 0, 0 },
			    Direction::South,
			    spellData.sMissiles[i],
			    TARGET_MONSTERS,
			    operatePlayer,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Fill empty slots in inventory with gold based in the dungeon level
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 First multiplier of gold per dungeon level
 * @param shrineEffect.param2 Second multiplier of gold per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectFillEmptySlotsGold(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int goldMultiplier1 = shrineEffect.param1;
	const int goldMultiplier2 = shrineEffect.param2;

	for (int8_t &itemIndex : player.InvGrid) {
		if (itemIndex == 0) {
			Item &goldItem = player.InvList[player._pNumInv];
			MakeGoldStack(goldItem, (goldMultiplier1 * leveltype) + rng.generateRnd(goldMultiplier2 * leveltype));
			player._pNumInv++;
			itemIndex = player._pNumInv;

			player._pGold += goldItem._ivalue;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes the dexterity attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Min amount to change the dexterity attribute
 * @param shrineEffect.param2 Max amount to change the dexterity attribute
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectDexterityAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int change = ValueFromInterval(rng, shrineEffect);
	ModifyPlrDex(*MyPlayer, change);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes the strength attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Min amount to change the strength attribute
 * @param shrineEffect.param2 Max amount to change the strength attribute
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectStrengthAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int change = ValueFromInterval(rng, shrineEffect);
	ModifyPlrStr(*MyPlayer, change);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes the vitality attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Min amount to change the vitality attribute
 * @param shrineEffect.param2 Max amount to change the vitality attribute
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectVitalityAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int change = ValueFromInterval(rng, shrineEffect);
	ModifyPlrVit(*MyPlayer, change);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Reveals all the current level minimap
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectRevealMap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Identifies all items in the player’s inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect Unused
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectIdentifyItems(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	for (Item &item : PlayerItemsRange { *MyPlayer }) {
		if (item._iMagical != ITEM_QUALITY_NORMAL && !item._iIdentified) {
			item._iIdentified = true;
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Oily shrines change the players preferred attribute(s).
 * If the class has more than one preferred attribute, the change will be split between them, with excess to the first one
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Min amount to change the preferred attribute(s)
 * @param shrineEffect.param2 Max amount to change the preferred attribute(s)
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectPreferredAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int change = ValueFromInterval(rng, shrineEffect);
	const int half = change / 2;

	switch (player._pClass) {
	case HeroClass::Warrior:
		ModifyPlrStr(player, change);
		break;
	case HeroClass::Rogue:
		ModifyPlrDex(player, change);
		break;
	case HeroClass::Sorcerer:
		ModifyPlrMag(player, change);
		break;
	case HeroClass::Barbarian:
		ModifyPlrVit(player, change);
		break;
	case HeroClass::Monk:
		ModifyPlrStr(player, change - half);
		ModifyPlrDex(player, half);
		break;
	case HeroClass::Bard:
		ModifyPlrDex(player, change - half);
		ModifyPlrMag(player, half);
		break;
	default:
		break;
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Triggers a magic trap with the FireWall missile targeting players
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The origin of the FireWall missile
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */

ShrineMessage ShrineEffectCastFirewallTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int spellMultiplier = shrineEffect.param1;
	const int spellAdd = shrineEffect.param2;

	AddMissile(
	    spawnPosition,
	    operatePlayer.position.tile,
	    operatePlayer._pdir,
	    MissileID::FireWall,
	    TARGET_PLAYERS,
	    -1,
	    (spellMultiplier * currlevel) + spellAdd,
	    0);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Increase magic attribute but reduces total experience
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Maximum magic gained, and the percent of experience taken
 * @param shrineEffect.param2 Experience needed for each point of magic gained
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectExperienceToMagicAttribute(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int magicAttributeChange = shrineEffect.param1;
	const int expConsumedPerAttribute = shrineEffect.param2;

	// Add one point of Magic for every expConsumedPerAttribute experience the player has, up to magicAttributeChange points
	ModifyPlrMag(player, static_cast<int>(std::min<uint32_t>(player._pExperience / expConsumedPerAttribute, magicAttributeChange)));

	// Take magicAttributeChange % of the player's experience to offset the bonus. Players with no more experience
	// than the full bonus costs (magicAttributeChange * expConsumedPerAttribute) lose all of it.
	if (player._pExperience > static_cast<uint64_t>(magicAttributeChange) * expConsumedPerAttribute)
		player._pExperience = static_cast<uint32_t>(player._pExperience * ((100.0 - magicAttributeChange) / 100.0));
	else
		player._pExperience = 0;
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Takes a percent of the player's gold and converts it to experience
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 % of gold consumed
 * @param shrineEffect.param2 Experience gained for each gold unit
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectGoldToExperience(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int expPercentGoldLoss = shrineEffect.param1;
	const int expPerGoldUnit = shrineEffect.param2;

	const int gold = player._pGold * expPercentGoldLoss / 100;
	player.addExperience(gold * expPerGoldUnit);
	TakePlrsMoney(gold);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Grants experience to the player based on the current dungeon level
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineEffect.param1 Exp multiplier per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectExperiencePerDungeonLevel(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	MyPlayer->addExperience(ValueFromInterval(rng, shrineEffect) * currlevel);
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Triggers a magic trap with the FlashBottom missile targeting players
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The origin of the FlashBottom missile
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectCastFlashBottomTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const int spellMultiplier = shrineEffect.param1;
	const int spellAdd = shrineEffect.param2;

	AddMissile(
	    spawnPosition,
	    operatePlayer.position.tile,
	    operatePlayer._pdir,
	    MissileID::FlashBottom,
	    TARGET_PLAYERS,
	    -1,
	    (spellMultiplier * currlevel) + spellAdd,
	    0);

	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Triggers a magic trap with the FlashBottom missile targeting the player
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The trap results in creating a FlashBottom at the shrine location
 * @param shrineEffect.spellId Spell that will be casted as the trap
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectCastSpellTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const SpellID spellId = shrineEffect.spellId;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineEffect.param1;
		const int spellAdd = shrineEffect.param2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {

			AddMissile(
			    spawnPosition,
			    operatePlayer.position.tile,
			    operatePlayer._pdir,
			    MissileID::FlashBottom,
			    TARGET_PLAYERS,
			    -1,
			    (spellMultiplier * currlevel) + spellAdd,
			    0);
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Player operating shrine casts a spell at the shrine position
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine to use as the source position of the spell
 * @param shrineEffect.spellId Spell that the player will cast
 * @param shrineEffect.param1 Damage multiplier per dungeon level
 * @param shrineEffect.param2 Damage add per dungeon level
 * @return ShrineMessage::ShrineMessageDefault
 */
ShrineMessage ShrineEffectPlayerCastSpellFromShrine(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	const SpellID spellId = shrineEffect.spellId;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineEffect.param1;
		const int spellAdd = shrineEffect.param2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    spawnPosition,
			    operatePlayer.position.tile,
			    operatePlayer._pdir,
			    spellData.sMissiles[i],
			    TARGET_MONSTERS,
			    operatePlayer,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

/**
 * @brief Changes an attribute based on the current hour
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param shrineEffect.param1 Min amount to change the hour based attribute
 * @param shrineEffect.param2 Max amount to change the hour based attribute
 * * @return ShrineMessage::ShrineMessage1 - ShrineMessage::ShrineMessage4 (depending on the hour) if the player is the active player
 */
ShrineMessage ShrineEffectLocalHourAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int change = ValueFromInterval(rng, shrineEffect);

	const time_t timeResult = time(nullptr);
	const std::tm *localtimeResult = localtime(&timeResult);
	const int hour = localtimeResult != nullptr ? localtimeResult->tm_hour : 20;
	if (hour >= 20 || hour < 4) {
		ModifyPlrVit(player, change);
		return ShrineMessage::ShrineMessage4;
	} else if (hour >= 18) {
		ModifyPlrMag(player, change);
		return ShrineMessage::ShrineMessage3;
	} else if (hour >= 12) {
		ModifyPlrStr(player, change);
		return ShrineMessage::ShrineMessage2;
	} else /* 4:00 to 11:59 */ {
		ModifyPlrDex(player, change);
		return ShrineMessage::ShrineMessage1;
	}
}

/**
 * @brief Decreses an item durability or if none selected, takes away a fraction of gold
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param shrineEffect.param1 % of durability to decrease
 * @param shrineEffect.param2 Fraction of gold to take away if no item is selected
 * @return ShrineMessage::ShrineMessage1 - ShrineMessage::ShrineMessage4 (depending on the hour) if the player is the active player
 */
ShrineMessage ShrineEffectBreakItemOrTakeGold(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect)
{
	Player &player = *MyPlayer;
	const int durabilityPercentDecrease = shrineEffect.param1;
	const int goldDecrease = shrineEffect.param2;

	bool broke = false;
	if (durabilityPercentDecrease != 0 && durabilityPercentDecrease < 100) {
		for (auto &item : player.InvBody) {
			if (!item.isEmpty() && rng.flipCoin(3)) {
				if (item._iDurability != DUR_INDESTRUCTIBLE) {
					if (item._iDurability > 0) {
						item._iDurability = FractionOrPercentReduction(item._iDurability, durabilityPercentDecrease);
						broke = true;
						break;
					}
				}
			}
		}
	}
	if (!broke) {
		if (goldDecrease != 0 && goldDecrease < 100) {
			TakePlrsMoney(FractionOrPercentReduction(player._pGold, goldDecrease));
		}
	}
	return ShrineMessage::ShrineMessageDefault;
}

ShrineMessage OperateShrine(DiabloGenerator &rng, Player &operatePlayer, const Point shrinePosition, const ShrineData &shrineData)
{
	ShrineMessage shrineMessage = ShrineMessage::ShrineMessageDefault;
	for (size_t i = 0; i < MAX_SHRINE_EFFECTS; ++i) {
		ShrineEffect shrineEffect = shrineData.shrineEffect[i];
		if (shrineEffect.typeFn == nullptr) {
			continue;
		}
		if (&operatePlayer == MyPlayer) {
			if (shrineEffect.affectsPlayer == ShrineAffectsPlayer::Others) {
				continue;
			}
		} else {
			if (shrineEffect.affectsPlayer == ShrineAffectsPlayer::Activator) {
				continue;
			}
		}
		const ShrineMessage lastShrineMessage = shrineEffect.typeFn(rng, operatePlayer, shrinePosition, shrineEffect);
		if (lastShrineMessage != ShrineMessage::ShrineMessageDefault) {
			shrineMessage = lastShrineMessage;
		}
	}
	if (shrineMessage != ShrineMessage::ShrineMessageDefault) {
		return shrineMessage;
	}
	return (&operatePlayer == MyPlayer) ? ShrineMessage::ShrineMessage1 : ShrineMessage::ShrineMessage5;
}

} // namespace devilution
