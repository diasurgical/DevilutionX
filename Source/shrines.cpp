#include <ctime>

#include "shrines.hpp"
#include <engine/backbuffer_state.hpp>
#include <qol/stash.h>
#include <tables/shrinedat.hpp>

#include "automap.h"
#include "inv_iterators.hpp"
#include "missiles.h"
#include "objects.h"
#include "spells.h"
#include "stores.h"

namespace devilution {

namespace {

/** Maps from shrine_id to shrine name. */
const char *const ShrineNames[] = {
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

/**
 * @brief Fully restores life and/or mana if configured in shrineOperateFlags and returns true if it was done
 * @param player Player who should have it's life/mana restored
 * @param shrineData Data of the shrine that was activated
 * @return true if life or mana was restored so a call to RedrawEverything() is needed, false otherwise
 */
bool CheckRestoreLifeMana(Player &player, const ShrineData &shrineData)
{
	bool redraw = false;
	if (HasAnyOf(shrineData.shrineOperateFlags, ShrineOperateFlag::RestoreLife)) {
		player._pHitPoints = player._pMaxHP;
		player._pHPBase = player._pMaxHPBase;
		redraw = true;
	}
	if (HasAnyOf(shrineData.shrineOperateFlags, ShrineOperateFlag::RestoreMana)) {
		player._pMana = player._pMaxMana;
		player._pManaBase = player._pMaxManaBase;
		redraw = true;
	}
	return redraw;
}

mienemy_type SpellTarget(const ShrineData &shrineData)
{
	const bool targetPlayers = HasAnyOf(shrineData.shrineOperateFlags, ShrineOperateFlag::SpellTargetsPlayers);
	const bool targetMonsters = HasAnyOf(shrineData.shrineOperateFlags, ShrineOperateFlag::SpellTargetsMonsters);
	return targetMonsters ? (targetPlayers ? TARGET_BOTH : TARGET_MONSTERS) : (targetPlayers ? TARGET_PLAYERS : TARGET_BOTH);
}

} // namespace

int OperateShrineMysterious(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int randomAttributeChange = shrineData.powerParam1;
	const int otherAttributesChange = shrineData.powerParam2;

	ModifyPlrStr(player, otherAttributesChange);
	ModifyPlrMag(player, otherAttributesChange);
	ModifyPlrDex(player, otherAttributesChange);
	ModifyPlrVit(player, otherAttributesChange);

	switch (static_cast<CharacterAttribute>(rng.generateRnd(4))) {
	case CharacterAttribute::Strength:
		ModifyPlrStr(player, randomAttributeChange - otherAttributesChange);
		break;
	case CharacterAttribute::Magic:
		ModifyPlrMag(player, randomAttributeChange - otherAttributesChange);
		break;
	case CharacterAttribute::Dexterity:
		ModifyPlrDex(player, randomAttributeChange - otherAttributesChange);
		break;
	case CharacterAttribute::Vitality:
		ModifyPlrVit(player, randomAttributeChange - otherAttributesChange);
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Modifies the durability of a random item using first value and the rest of items with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Durability change to a random item
 * @param shrineData.powerParam2 Durability change for the rest of the items
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineHidden(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int randomItemDurabilityChange = shrineData.powerParam1;
	const int otherItemsDurabilityChange = shrineData.powerParam2;

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

			player.InvBody[r]._iDurability += (randomItemDurabilityChange - otherItemsDurabilityChange);
			player.InvBody[r]._iMaxDur += (randomItemDurabilityChange - otherItemsDurabilityChange);
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

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief For all items in the player's inventory, modifies the armor class of all armor and the max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Change to items armor class
 * @param shrineData.powerParam2 Change to items max damage
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGloomy(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int itemsDurabilityChange = shrineData.powerParam1;
	const int itemsMaxDamChange = shrineData.powerParam2;

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
			item._iAC = ClampU8(item._iAC + itemsDurabilityChange);
			break;
		default:
			break;
		}
	}

	CalcPlrInv(player, true);
	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief For all items in the player's inventory, modifies the min and max damage of all weapons
 * @warning Probably doesn't work in multiplayer
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Change to items min damage
 * @param shrineData.powerParam2 Change to items max damage
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineWeird(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
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

	const int itemsMinDamChange = shrineData.powerParam1;
	const int itemsMaxDamChange = shrineData.powerParam2;

	auto bumpDam = [&](Item &item) {
		const int minDam = static_cast<int>(item._iMinDam);
		const int maxDam = static_cast<int>(item._iMaxDam);
		const int minDamChanged = ClampU8(minDam + itemsMinDamChange);
		const int maxDamChanged = ClampU8(maxDam + itemsMaxDamChange);

		item._iMinDam = ClampU8(std::max(minDamChanged, 1));
		item._iMaxDam = ClampU8(std::max(maxDamChanged, minDamChanged));
	};

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

	CalcPlrInv(player, true);
	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Casts a spell at the player position, destiny player position and player's direction
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Spell level multiplier for dungeon level
 * @param shrineData.powerParam2 Spell level add to dungeon level
 * @param shrineData.spell Spell to cast
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCastSpellPlayerPosition(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	const SpellID spellId = shrineData.spell;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineData.powerParam1;
		const int spellAdd = shrineData.powerParam2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    player.position.tile,
			    player.position.tile,
			    player._pdir,
			    spellData.sMissiles[i],
			    SpellTarget(shrineData),
			    player,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}

	if (&player != MyPlayer)
		return 0;

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Recharges all staves in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineStone(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._itype == ItemType::Staff)
			item._iCharges = item._iMaxCharges;
	}

	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Repairs all items in the player's inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineReligious(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		item._iDurability = item._iMaxDur;
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Modifies the level of a random spell using first value and the rest of spell levels with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Level change to a random spell
 * @param shrineData.powerParam2 Level change for the rest of the spells
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEnchanted(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int randomSpellLevelChange = shrineData.powerParam1;
	const int otherSpellLevelChange = shrineData.powerParam2;

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
				const auto newSpellLevel = static_cast<uint8_t>(std::clamp(0, player._pSplLvl[j] + randomSpellLevelChange, static_cast<int>(MaxSpellLevel)));
				player._pSplLvl[j] = newSpellLevel;
				NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, j, newSpellLevel);
			}
			spell *= 2;
		}

		if (player._pSplLvl[spellToReduce] > 0) {
			const auto newSpellLevel = static_cast<uint8_t>(std::clamp(0, player._pSplLvl[spellToReduce] + otherSpellLevelChange, static_cast<int>(MaxSpellLevel)));
			player._pSplLvl[spellToReduce] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, spellToReduce, newSpellLevel);
		}

		if (&player == MyPlayer) {
			for (Item &item : InventoryPlayerItemsRange { player }) {
				item.updateRequiredStatsCacheForPlayer(player);
			}
			if (IsStashOpen) {
				Stash.RefreshItemStatFlags();
			}
		}
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Closes and refills all opened chests on current level
 * @warning Probably doesn't work in multiplayer
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineThaumaturgic(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &object = Objects[ActiveObjects[j]];
		if (object.IsChest() && !object.canInteractWith()) {
			object._oRndSeed = rng.advanceRndSeed();
			object.selectionRegion = SelectionRegion::Bottom;
			object._oAnimFrame -= 2;
		}
	}

	if (&player != MyPlayer)
		return 0;

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Increases a specific spell level and reduces max mana (or magic if configured)
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Amount of levels to increase the spell
 * @param shrineData.powerParam2 Percent of max mana to permanently reduce (positive values) or magic attribute to reduce (negative values)
 * @param shrineData.spell Spell that will change its value
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCostOfWisdom(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int spellLevelChange = shrineData.powerParam1;
	const int wisdomCost = shrineData.powerParam2;
	const SpellID spellId = shrineData.spell;

	player._pMemSpells |= GetSpellBitmask(spellId);
	const uint8_t curSpellLevel = player._pSplLvl[static_cast<int8_t>(spellId)];
	if (curSpellLevel < MaxSpellLevel) {
		const uint8_t newSpellLevel = std::min(static_cast<uint8_t>(curSpellLevel + spellLevelChange), MaxSpellLevel);
		player._pSplLvl[static_cast<int8_t>(spellId)] = newSpellLevel;
		NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellId), newSpellLevel);
	}

	// TODO: Haven't changed this section from previous code but I think this if is always true
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

	if (wisdomCost > 0) {
		const Fixed26_6 penalty = Fixed26_6::fromRaw(maxBase * wisdomCost / 100); // wisdomCost % of max base mana (>= 0)

		player._pMaxManaBase -= penalty; // will remain >= 0
		player._pManaBase -= penalty;    // may go negative, allowed
		player._pMaxMana -= penalty;     // may go negative, allowed
		player._pMana -= penalty;        // may go negative, allowed
	} else if (wisdomCost < 0) {
		// This could be classifield as new functionality, but also it will be requested a player penalty that is not permanently crippling
		ModifyPlrMag(player, wisdomCost);
	}

	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief All potions in the player's inventory are turn to Rejuvenation.
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEldritch(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
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

	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the magic attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Amount to increase the magic attribute
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineEerie(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrMag(player, shrineData.powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Fully restores HP and Mana of the active player and spawns potions
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The map tile where the potions will be spawned
 * @param shrineData.powerParam1 The minimum dungeon level that will spawn rejuvenation potions
 * @param shrineData.powerParam2 The amoun of potions
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineDivine(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int minDungeonLevelRejuvenation = shrineData.powerParam1;
	const int numPotions = shrineData.powerParam2;

	if (currlevel < minDungeonLevelRejuvenation) {
		for (int i = 0; i < numPotions; i++) {
			const item_misc_id imisc = ((i & 1) == 0) ? IMISC_FULLMANA : IMISC_FULLHEAL;
			CreateTypeItem(spawnPosition, false, ItemType::Misc, imisc, false, false, true);
		}
	} else {
		for (int i = 0; i < numPotions; i++) {
			CreateTypeItem(spawnPosition, false, ItemType::Misc, IMISC_FULLREJUV, false, false, true);
		}
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Casts a spell at the operating player position, destiny (0,0) direction south
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Spell level multiplier for dungeon level
 * @param shrineData.powerParam2 Spell level add to dungeon level
 * @param shrineData.spell Spell to cast
 */
int OperateShrineCastSpellDestinyZero(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	const SpellID spellId = shrineData.spell;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineData.powerParam1;
		const int spellAdd = shrineData.powerParam2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    player.position.tile,
			    { 0, 0 },
			    Direction::South,
			    spellData.sMissiles[i],
			    SpellTarget(shrineData), player,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}

	if (&player != MyPlayer)
		return 0;

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Fill empty slots in inventory with gold based in the dungeon level
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 First multiplier of gold per dungeon level
 * @param shrineData.powerParam2 Second multiplier of gold per dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSpiritual(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int goldMultiplier1 = shrineData.powerParam1;
	const int goldMultiplier2 = shrineData.powerParam2;

	for (int8_t &itemIndex : player.InvGrid) {
		if (itemIndex == 0) {
			Item &goldItem = player.InvList[player._pNumInv];
			MakeGoldStack(goldItem, (goldMultiplier1 * leveltype) + rng.generateRnd(goldMultiplier2 * leveltype));
			player._pNumInv++;
			itemIndex = player._pNumInv;

			player._pGold += goldItem._ivalue;
		}
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief No effect for the player who activated the shrine, the other players gets its life and mana fully restored
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 2 (blessed message) otherwise
 */
int OperateShrineSpooky(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player == MyPlayer) {
		return 1;
	}

	if (CheckRestoreLifeMana(*MyPlayer, shrineData)) {
		RedrawEverything();
	}
	return 2;
}

/**
 * @brief Increases the dexterity attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Amount to increase the dexterity attribute
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineAbandoned(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrDex(player, shrineData.powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the strength attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Amount to increase the strength attribute
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCreepy(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrStr(player, shrineData.powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases the vitality attribute
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Amount to increase the vitality attribute
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineQuiet(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	ModifyPlrVit(player, shrineData.powerParam1);
	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Reveals all the current level minimap
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSecluded(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);
	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Identifies all items in the player’s inventory
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGlimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	for (Item &item : PlayerItemsRange { player }) {
		if (item._iMagical != ITEM_QUALITY_NORMAL && !item._iIdentified) {
			item._iIdentified = true;
		}
	}

	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief No effect for the player who activated the shrine, the other players gets a random attribute modified using first value and the rest with the second value
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Value to change a random attribute
 * @param shrineData.powerParam2 Value to change the rest of the attributes
 * @return 1 (normal message) if the player is the active player, 2 (cursed message) otherwise
 */
int OperateShrineTainted(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player == MyPlayer) {
		return 1;
	}

	const int randomAttributeChange = shrineData.powerParam1;
	const int otherAttributesChange = shrineData.powerParam2;

	const int r = rng.generateRnd(4);

	const int v1 = r == 0 ? randomAttributeChange : otherAttributesChange;
	const int v2 = r == 1 ? randomAttributeChange : otherAttributesChange;
	const int v3 = r == 2 ? randomAttributeChange : otherAttributesChange;
	const int v4 = r == 3 ? randomAttributeChange : otherAttributesChange;

	Player &myPlayer = *MyPlayer;

	ModifyPlrStr(myPlayer, v1);
	ModifyPlrMag(myPlayer, v2);
	ModifyPlrDex(myPlayer, v3);
	ModifyPlrVit(myPlayer, v4);

	CheckStats(myPlayer);
	CalcPlrInv(myPlayer, true);
	CheckRestoreLifeMana(myPlayer, shrineData);
	RedrawEverything();
	return 2;
}

/**
 * @brief Oily shrines increase the players primary stat(s), but spawn a
 *        firewall near the shrine that will spread towards the player
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 Value to change primary stats
 * @param shrineData.powerParam2 Value of how much will be given to the second primary stat, substracted from the first
 * @param shrineData.spell Spell to cast at the shrine location
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineOily(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int totalAttributesChange = shrineData.powerParam1;
	const int secondaryAttributeChange = shrineData.powerParam2;

	switch (player._pClass) {
	case HeroClass::Warrior:
		ModifyPlrStr(player, totalAttributesChange);
		break;
	case HeroClass::Rogue:
		ModifyPlrDex(player, totalAttributesChange);
		break;
	case HeroClass::Sorcerer:
		ModifyPlrMag(player, totalAttributesChange);
		break;
	case HeroClass::Barbarian:
		ModifyPlrVit(player, totalAttributesChange);
		break;
	case HeroClass::Monk:
		ModifyPlrStr(player, totalAttributesChange - secondaryAttributeChange);
		ModifyPlrDex(player, secondaryAttributeChange);
		break;
	case HeroClass::Bard:
		ModifyPlrDex(player, totalAttributesChange - secondaryAttributeChange);
		ModifyPlrMag(player, secondaryAttributeChange);
		break;
	default:
		break;
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FireWall,
	    SpellTarget(shrineData),
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
 * @param shrineData.powerParam1 Value to increase magic attribute
 * @param shrineData.powerParam2 Fraction of experience lost
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineGlowing(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int magicAttributesChange = shrineData.powerParam1;
	const int experienceLossFraction = shrineData.powerParam2;

	// Add [0 - magicAttributesChange] points to Magic (for each magic attribute point reduce (100 / experienceLossFraction)% of the players XP)
	ModifyPlrMag(player, static_cast<int>(std::min<uint32_t>(player._pExperience / experienceLossFraction, magicAttributesChange)));

	// Take magicAttributesChange % of the players experience to offset the bonus, unless they're very low level in which case take all their experience.
	if (player._pExperience > static_cast<uint32_t>(magicAttributesChange * 1000000 / experienceLossFraction))
		player._pExperience = static_cast<uint32_t>(player._pExperience * ((100.0 - magicAttributesChange) * 10.0 / experienceLossFraction));
	else
		player._pExperience = 0;

	CheckStats(player);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Takes a percent of the player's gold and converts it to experience
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition Unused
 * @param shrineData.powerParam1 % of gold consumed
 * @param shrineData.powerParam2 Experience gained for each gold unit
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMendicant(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int goldLossPercent = shrineData.powerParam1;
	const int experienceGainedPerGold = shrineData.powerParam2;

	const int gold = player._pGold * goldLossPercent / 100;
	player.addExperience(gold * experienceGainedPerGold);
	TakePlrsMoney(gold);

	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Grants experience to the player based on the current dungeon level while also triggering a magic trap
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The trap results in casting flash from this location targeting the player
 * @param shrineData.powerParam1 Exp multiplier per dungeon level
 * @param shrineData.powerParam2 Flash spell damage multiplier per dungeon level
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSparkling(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int experiencePerLevel = shrineData.powerParam1;
	const int spellDamageMultiplier = shrineData.powerParam2;

	player.addExperience(experiencePerLevel * currlevel);

	AddMissile(
	    spawnPosition,
	    player.position.tile,
	    player._pdir,
	    MissileID::FlashBottom,
	    SpellTarget(shrineData),
	    -1,
	    (spellDamageMultiplier * currlevel) + 2,
	    0);

	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Casts a spell at the shrine position, destiny player position and player's direction
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine, the portal will be placed on the side closest to the player
 * @param shrineData.powerParam1 Spell level multiplier for dungeon level
 * @param shrineData.powerParam2 Spell level add to dungeon level
 * @param shrineData.spell Spell to cast
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineCastSpellShrinePosition(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const SpellID spellId = shrineData.spell;

	if (spellId != SpellID::Null) {

		const int spellMultiplier = shrineData.powerParam1;
		const int spellAdd = shrineData.powerParam2;
		const SpellData &spellData = GetSpellData(spellId);

		for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
			AddMissile(
			    spawnPosition,
			    player.position.tile,
			    player._pdir,
			    spellData.sMissiles[i],
			    SpellTarget(shrineData),
			    player,
			    0,
			    (spellMultiplier * leveltype) + spellAdd);
		}
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

/**
 * @brief Replenishes the mana to maximum of the player who activated it .
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine, the portal will be placed on the side closest to the player
 * @return 1 (normal message) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineShimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return 1;
}

/**
 * @brief Increases an attribute based on the current hour
 * @param rng Unused
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param shrineData.powerParam1 Amount to increase the attribute
 * @return 1 - 4 (depending ont the hour) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineSolar(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int attributeChange = shrineData.powerParam1;

	const time_t timeResult = time(nullptr);
	const std::tm *localtimeResult = localtime(&timeResult);
	const int hour = localtimeResult != nullptr ? localtimeResult->tm_hour : 20;
	int messageIndex;
	if (hour >= 20 || hour < 4) {
		messageIndex = 4;
		ModifyPlrVit(player, attributeChange);
	} else if (hour >= 18) {
		messageIndex = 3;
		ModifyPlrMag(player, attributeChange);
	} else if (hour >= 12) {
		messageIndex = 2;
		ModifyPlrStr(player, attributeChange);
	} else /* 4:00 to 11:59 */ {
		messageIndex = 1;
		ModifyPlrDex(player, attributeChange);
	}

	CheckStats(player);
	CalcPlrInv(player, true);
	CheckRestoreLifeMana(player, shrineData);
	RedrawEverything();
	return messageIndex;
}

/**
 * @brief Decreses an item durability or if none selected, takes away a fraction of gold
 * @param rng random number generator
 * @param player The player that activated the shrine
 * @param spawnPosition The position of the shrine
 * @param shrineData.powerParam1 % of durability to decrease
 * @param shrineData.powerParam2 Fraction of gold to take away if no item is selected
 * @return 1 - 4 (depending ont the hour) if the player is the active player, 0 (no message) otherwise
 */
int OperateShrineMurphys(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData)
{
	if (&player != MyPlayer)
		return 0;

	const int durabilityPercentLoss = shrineData.powerParam1;
	const int goldLossFraction = shrineData.powerParam2;

	bool broke = false;
	for (auto &item : player.InvBody) {
		if (!item.isEmpty() && rng.flipCoin(3)) {
			if (item._iDurability != DUR_INDESTRUCTIBLE) {
				if (item._iDurability > 0) {
					item._iDurability = item._iDurability * durabilityPercentLoss / 100;
					broke = true;
					break;
				}
			}
		}
	}
	if (!broke) {
		TakePlrsMoney((goldLossFraction > 1) ? player._pGold / goldLossFraction : player._pGold);
	}

	if (CheckRestoreLifeMana(player, shrineData)) {
		RedrawEverything();
	}
	return 1;
}

} // namespace devilution
