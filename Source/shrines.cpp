/**
 * @file shrines.cpp
 *
 * Implementation of the shrine effects referenced by shrines.tsv.
 */
#include "shrines.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>

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

} // namespace

/**
 * @brief Modifies a random attribute using first value and the rest with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Value to change a random attribute
 * @param powerParam2 Value to change the rest of the attributes
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMysterious(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrStr(player, powerParam2);
	ModifyPlrMag(player, powerParam2);
	ModifyPlrDex(player, powerParam2);
	ModifyPlrVit(player, powerParam2);

	switch (static_cast<CharacterAttribute>(rng.generateRnd(4))) {
	case CharacterAttribute::Strength:
		ModifyPlrStr(player, powerParam1 - powerParam2);
		break;
	case CharacterAttribute::Magic:
		ModifyPlrMag(player, powerParam1 - powerParam2);
		break;
	case CharacterAttribute::Dexterity:
		ModifyPlrDex(player, powerParam1 - powerParam2);
		break;
	case CharacterAttribute::Vitality:
		ModifyPlrVit(player, powerParam1 - powerParam2);
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief Modifies the durability of a random item using first value and the rest of items with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Durability change to a random item
 * @param powerParam2 Durability change for the rest of the items
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineHidden(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

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
				item._iDurability += powerParam2;
				item._iMaxDur += powerParam2;
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

			player.InvBody[r]._iDurability += (powerParam1 - powerParam2);
			player.InvBody[r]._iMaxDur += (powerParam1 - powerParam2);
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
	return 1;
}

/**
 * @brief For all items in the player's inventory, modifies the armor class of all armor and the max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Change to items armor class
 * @param powerParam2 Change to items max damage
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGloomy(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange(player)) {
		switch (item._itype) {
		case ItemType::Sword:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff: {
			const int minDam = static_cast<int>(item._iMinDam);
			const int maxDam = static_cast<int>(item._iMaxDam);
			const int newMax = std::max(maxDam + powerParam2, minDam);
			item._iMaxDam = static_cast<uint8_t>(ClampU8(newMax));
		} break;
		case ItemType::Shield:
		case ItemType::Helm:
		case ItemType::LightArmor:
		case ItemType::MediumArmor:
		case ItemType::HeavyArmor:
			item._iAC = ClampU8(item._iAC + powerParam1);
			break;
		default:
			break;
		}
	}

	CalcPlrInv(player, true);
	return 1;
}

/**
 * @brief For all items in the player's inventory, modifies the max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Change to items max damage
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineWeird(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

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

	auto bumpMaxDam = [&](Item &item) {
		const int minDam = static_cast<int>(item._iMinDam);
		const int maxDam = static_cast<int>(item._iMaxDam);
		item._iMaxDam = ClampU8(std::max(maxDam + powerParam1, minDam));
	};

	Item &left = player.InvBody[INVLOC_HAND_LEFT];
	if (!left.isEmpty() && left._itype != ItemType::Shield)
		bumpMaxDam(left);

	Item &right = player.InvBody[INVLOC_HAND_RIGHT];
	if (!right.isEmpty() && right._itype != ItemType::Shield)
		bumpMaxDam(right);

	for (Item &item : InventoryPlayerItemsRange { player }) {
		if (isWeapon(item))
			bumpMaxDam(item);
	}

	CalcPlrInv(player, true);
	return 1;
}

/**
 * @brief Casts a mana shield
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Spell level multiplier for dungeon level
 * @param powerParam2 Spell level add to dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMagical(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	AddMissile(
	    player.position.tile,
	    player.position.tile,
	    player._pdir,
	    MissileID::ManaShield,
	    TARGET_MONSTERS,
	    player,
	    0,
	    powerParam1 * leveltype + powerParam2);
	return (&player == MyPlayer) ? 1 : 0;
}

/**
 * @brief Recharges all staves in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineStone(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._itype == ItemType::Staff)
			item._iCharges = item._iMaxCharges;
	}

	CalcPlrInv(player, true);

	RedrawEverything();
	return 1;
}

/**
 * @brief Repairs all items in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineReligious(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		item._iDurability = item._iMaxDur;
	}
	return 1;
}

/**
 * @brief Modifies the level of a random spell using first value and the rest of spell levels with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Level change to a random spell
 * @param powerParam2 Level change for the rest of the spells
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEnchanted(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

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
				const auto newSpellLevel = static_cast<uint8_t>(std::clamp(player._pSplLvl[j] + powerParam1, 0, static_cast<int>(MaxSpellLevel)));
				player._pSplLvl[j] = newSpellLevel;
				NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, j, newSpellLevel);
			}
			spell *= 2;
		}

		if (player._pSplLvl[spellToReduce] > 0) {
			const auto newSpellLevel = static_cast<uint8_t>(std::clamp(player._pSplLvl[spellToReduce] + powerParam2, 0, static_cast<int>(MaxSpellLevel)));
			player._pSplLvl[spellToReduce] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, spellToReduce, newSpellLevel);
		}

		// TODO: Always true, the function returns early for other players. Drop the check (see #8799).
		if (&player == MyPlayer) {
			for (Item &item : InventoryPlayerItemsRange { player }) {
				item.updateRequiredStatsCacheForPlayer(player);
			}
			if (IsStashOpen) {
				Stash.RefreshItemStatFlags();
			}
		}
	}
	return 1;
}

/**
 * @brief Closes and refills all opened chests on current level
 * @warning Probably doesn't work in multiplayer
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineThaumaturgic(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &object = Objects[ActiveObjects[j]];
		if (object.IsChest() && !object.canInteractWith()) {
			object._oRndSeed = rng.advanceRndSeed();
			object.selectionRegion = SelectionRegion::Bottom;
			object._oAnimFrame -= 2;
		}
	}
	return (&player == MyPlayer) ? 1 : 0;
}

/**
 * @brief Increases a specific spell level and reduces max mana (or magic if configured)
 * @param player The player that activated the shrine
 * @param spellId Spell to increase level
 * @param powerParam1 Amount of levels to increase the spell
 * @param powerParam2 Percent of max mana to permanently reduce (positive values) or magic attribute to reduce (negative values)
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCostOfWisdom(Player &player, SpellID spellId, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	player._pMemSpells |= GetSpellBitmask(spellId);

	const uint8_t curSpellLevel = player._pSplLvl[static_cast<int8_t>(spellId)];
	if (curSpellLevel < MaxSpellLevel) {
		const uint8_t newSpellLevel = std::min(static_cast<uint8_t>(curSpellLevel + powerParam1), MaxSpellLevel);
		player._pSplLvl[static_cast<int8_t>(spellId)] = newSpellLevel;
		NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellId), newSpellLevel);
	}

	// TODO: Always true, the function returns early for other players. Drop the check (see #8799).
	if (&player == MyPlayer) {
		for (Item &item : InventoryPlayerItemsRange { player }) {
			item.updateRequiredStatsCacheForPlayer(player);
		}
		if (IsStashOpen) {
			Stash.RefreshItemStatFlags();
		}
	}

	int maxBase = player._pMaxManaBase.raw();

	if (maxBase < 0) {
		// Fix bugged state; do not turn this into a "negative penalty" mana boost.
		player._pMaxManaBase = Fixed26_6::fromInt(0);
		maxBase = 0;
	}

	if (powerParam2 > 0) {
		const Fixed26_6 penalty = Fixed26_6::fromRaw(maxBase * powerParam2 / 100); // powerParam2 % of max base mana (>= 0)

		player._pMaxManaBase -= penalty; // will remain >= 0
		player._pManaBase -= penalty;    // may go negative, allowed
		player._pMaxMana -= penalty;     // may go negative, allowed
		player._pMana -= penalty;        // may go negative, allowed
	} else if (powerParam2 < 0) {
		// This could be classifield as new functionality, but also it will be requested a player penalty that is not permanently crippling
		ModifyPlrMag(player, powerParam2);
	}
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases Firebolt spell level and reduces max mana (or magic if configured)
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount of levels to increase the Firebolt spell
 * @param powerParam2 Percent of max mana to permanently reduce (positive values) or magic attribute to reduce (negative values)
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineFascinating(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	return OperateShrineCostOfWisdom(player, SpellID::Firebolt, powerParam1, powerParam2);
}

/**
 * @brief Casts a nova spell and refills your mana
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Spell level multiplier for dungeon level
 * @param powerParam2 Spell level add to dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCryptic(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	AddMissile(
	    player.position.tile,
	    player.position.tile,
	    player._pdir,
	    MissileID::Nova,
	    TARGET_MONSTERS,
	    player,
	    0,
	    powerParam1 * leveltype + powerParam2);

	if (&player != MyPlayer)
		return 0;

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;

	RedrawEverything();
	// TODO: Always 1, the function returns early for other players. Simplify (see #8799).
	return (&player == MyPlayer) ? 1 : 0;
}

/**
 * @brief All potions in the player's inventory are turn to Rejuvenation.
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEldritch(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : InventoryAndBeltPlayerItemsRange { player }) {
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

	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the magic attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount to increase the magic attribute
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEerie(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrMag(player, powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief Fully restores HP and Mana of the active player and spawns potions
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The map tile where the potions will be spawned
 * @param powerParam1 The minimum dungeon level that will spawn rejuvenation potions
 * @param powerParam2 The amoun of potions
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineDivine(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	if (currlevel < powerParam1) {
		for (int i = 0; i < powerParam2; i++) {
			item_misc_id imisc = ((i & 1) == 0) ? IMISC_FULLMANA : IMISC_FULLHEAL;
			CreateTypeItem(spawnPosition, false, ItemType::Misc, imisc, false, false, true);
		}
	} else {
		for (int i = 0; i < powerParam2; i++) {
			CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLREJUV, false, false, true);
		}
	}

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;
	player._pHitPoints = player._pMaxHP;
	player._pHPBase = player._pMaxHPBase;

	RedrawEverything();
	return 1;
}

/**
 * @brief Casts a phasing spell
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Spell level multiplier for dungeon level
 * @param powerParam2 Spell level add to dungeon level
 */
int OperateShrineHoly(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	AddMissile(player.position.tile, { 0, 0 }, Direction::South, MissileID::Phasing, TARGET_MONSTERS, player, 0, powerParam1 * leveltype + powerParam2);
	return (&player == MyPlayer) ? 1 : 0;
}

/**
 * @brief Increases Charged Bolt spell level and reduces max mana (or magic if configured)
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount of levels to increase the Charged Bolt spell
 * @param powerParam2 Percent of max mana to permanently reduce (positive values) or magic attribute to reduce (negative values)
 */
int OperateShrineSacred(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	return OperateShrineCostOfWisdom(player, SpellID::ChargedBolt, powerParam1, powerParam2);
}

/**
 * @brief Fill empty slots in inventory with gold based in the dungeon level
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 First multiplier of gold per dungeon level
 * @param powerParam2 Second multiplier of gold per dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSpiritual(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (int8_t &itemIndex : player.InvGrid) {
		if (itemIndex == 0) {
			Item &goldItem = player.InvList[player._pNumInv];
			MakeGoldStack(goldItem, (powerParam1 * leveltype) + rng.generateRnd(powerParam2 * leveltype));
			player._pNumInv++;
			itemIndex = player._pNumInv;

			player._pGold += goldItem._ivalue;
		}
	}
	return 1;
}

/**
 * @brief No effect for the player who activated the shrine, the other players gets its life and mana fully restored
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 2 (blessed message) otherwise
 */
int OperateShrineSpooky(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player == MyPlayer) {
		return 1;
	}

	Player &myPlayer = *MyPlayer;

	myPlayer._pHitPoints = myPlayer._pMaxHP;
	myPlayer._pHPBase = myPlayer._pMaxHPBase;
	myPlayer._pMana = myPlayer._pMaxMana;
	myPlayer._pManaBase = myPlayer._pMaxManaBase;

	RedrawEverything();
	return 2;
}

/**
 * @brief Increases the dexterity attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount to increase the dexterity attribute
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineAbandoned(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrDex(player, powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the strength attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount to increase the strength attribute
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCreepy(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrStr(player, powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the vitality attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount to increase the vitality attribute
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineQuiet(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrVit(player, powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief Reveals all the current level minimap
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSecluded(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);
	return 1;
}

/**
 * @brief Increases Holy Bolt spell level and reduces max mana (or magic if configured)
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Amount of levels to increase the Holy Bolt spell
 * @param powerParam2 Percent of max mana to permanently reduce (positive values) or magic attribute to reduce (negative values)
 */
int OperateShrineOrnate(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	return OperateShrineCostOfWisdom(player, SpellID::HolyBolt, powerParam1, powerParam2);
}

/**
 * @brief Identifies all items in the player’s inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGlimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._iMagical != ITEM_QUALITY_NORMAL && !item._iIdentified) {
			item._iIdentified = true;
		}
	}

	CalcPlrInv(player, true);
	RedrawEverything();
	return 1;
}

/**
 * @brief No effect for the player who activated the shrine, the other players gets a random attribute modified using first value and the rest with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Value to change a random attribute
 * @param powerParam2 Value to change the rest of the attributes
 * @return 1 (normal message) if the player is the active player, 2 (cursed message) otherwise
 */
int OperateShrineTainted(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player == MyPlayer) {
		return 1;
	}

	const int r = rng.generateRnd(4);

	const int v1 = r == 0 ? powerParam1 : powerParam2;
	const int v2 = r == 1 ? powerParam1 : powerParam2;
	const int v3 = r == 2 ? powerParam1 : powerParam2;
	const int v4 = r == 3 ? powerParam1 : powerParam2;

	Player &myPlayer = *MyPlayer;

	ModifyPlrStr(myPlayer, v1);
	ModifyPlrMag(myPlayer, v2);
	ModifyPlrDex(myPlayer, v3);
	ModifyPlrVit(myPlayer, v4);

	CheckStats(myPlayer);
	CalcPlrInv(myPlayer, true);
	RedrawEverything();
	return 2;
}

/**
 * @brief Oily shrines increase the players primary stat(s), but spawn a
 *        firewall near the shrine that will spread towards the player
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Value to change primary stats
 * @param powerParam2 Value of how much will be given to the second primary stat, substracted from the first
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineOily(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	switch (player._pClass) {
	case HeroClass::Warrior:
		ModifyPlrStr(player, powerParam1);
		break;
	case HeroClass::Rogue:
		ModifyPlrDex(player, powerParam1);
		break;
	case HeroClass::Sorcerer:
		ModifyPlrMag(player, powerParam1);
		break;
	case HeroClass::Barbarian:
		ModifyPlrVit(player, powerParam1);
		break;
	case HeroClass::Monk:
		ModifyPlrStr(player, powerParam1 - powerParam2);
		ModifyPlrDex(player, powerParam2);
		break;
	case HeroClass::Bard:
		ModifyPlrDex(player, powerParam1 - powerParam2);
		ModifyPlrMag(player, powerParam2);
		break;
	default:
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FireWall,
	    TARGET_PLAYERS,
	    -1,
	    (2 * currlevel) + 2,
	    0);
	return 1;
}

/**
 * @brief Increase magic attribute but reduces total experience
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 Maximum magic gained, and the percent of experience taken
 * @param powerParam2 Experience needed for each point of magic gained
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGlowing(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	// Add one point of Magic for every powerParam2 experience the player has, up to powerParam1 points
	ModifyPlrMag(player, static_cast<int>(std::min<uint32_t>(player._pExperience / powerParam2, powerParam1)));

	// Take powerParam1 % of the player's experience to offset the bonus. Players with no more experience
	// than the full bonus costs (powerParam1 * powerParam2) lose all of it.
	if (player._pExperience > static_cast<uint64_t>(powerParam1) * powerParam2)
		player._pExperience = static_cast<uint32_t>(player._pExperience * ((100.0 - powerParam1) / 100.0));
	else
		player._pExperience = 0;

	CheckStats(player);
	RedrawEverything();
	return 1;
}

/**
 * @brief Takes a percent of the player's gold and converts it to experience
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param powerParam1 % of gold consumed
 * @param powerParam2 Experience gained for each gold unit
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMendicant(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	const int gold = player._pGold * powerParam1 / 100;
	player.addExperience(gold * powerParam2);
	TakePlrsMoney(gold);

	RedrawEverything();
	return 1;
}

/**
 * @brief Grants experience to the player based on the current dungeon level while also triggering a magic trap
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The trap results in casting flash from this location targeting the player
 * @param powerParam1 Exp multiplier per dungeon level
 * @param powerParam2 Flash spell damage multiplier per dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSparkling(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	player.addExperience(powerParam1 * currlevel);

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FlashBottom,
	    TARGET_PLAYERS,
	    -1,
	    (powerParam2 * currlevel) + 2,
	    0);

	RedrawEverything();
	return 1;
}

/**
 * @brief Spawns a town portal near the active player
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine, the portal will be placed on the side closest to the player
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineTown(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::TownPortal,
	    TARGET_MONSTERS,
	    player,
	    0,
	    0);
	return 1;
}

/**
 * @brief Replenishes the mana to maximum of the player who activated it .
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine, the portal will be placed on the side closest to the player
 * @param powerParam1 Unused
 * @param powerParam2 Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineShimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	player._pMana = player._pMaxMana;
	player._pManaBase = player._pMaxManaBase;

	RedrawEverything();
	return 1;
}

/**
 * @brief Increases an attribute based on the current hour
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param powerParam1 Amount to increase the attribute
 * @param powerParam2 Unused
 * @return 1 - 4 (depending ont the hour) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSolar(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	const time_t timeResult = time(nullptr);
	const std::tm *localtimeResult = localtime(&timeResult);
	const int hour = localtimeResult != nullptr ? localtimeResult->tm_hour : 20;
	int messageIndex;
	if (hour >= 20 || hour < 4) {
		messageIndex = 4;
		ModifyPlrVit(player, powerParam1);
	} else if (hour >= 18) {
		messageIndex = 3;
		ModifyPlrMag(player, powerParam1);
	} else if (hour >= 12) {
		messageIndex = 2;
		ModifyPlrStr(player, powerParam1);
	} else /* 4:00 to 11:59 */ {
		messageIndex = 1;
		ModifyPlrDex(player, powerParam1);
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	RedrawEverything();
	return messageIndex;
}

/**
 * @brief Decreses an item durability or if none selected, takes away a fraction of gold
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param powerParam1 % of durability to decrease
 * @param powerParam2 Fraction of gold to take away if no item is selected
 * @return 1 - 4 (depending on the hour) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMurphys(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2)
{
	if (&player != MyPlayer)
		return 0;

	bool broke = false;
	for (auto &item : player.InvBody) {
		if (!item.isEmpty() && rng.flipCoin(3)) {
			if (item._iDurability != DUR_INDESTRUCTIBLE) {
				if (item._iDurability > 0) {
					item._iDurability = item._iDurability * powerParam1 / 100;
					broke = true;
					break;
				}
			}
		}
	}
	if (!broke) {
		TakePlrsMoney((powerParam2 > 1) ? player._pGold / powerParam2 : player._pGold);
	}
	return 1;
}

} // namespace devilution
