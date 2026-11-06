#include "lua/modules/player.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include <magic_enum/magic_enum.hpp>
#include <sol/sol.hpp>

#include "cursor.h"
#include "data/file.hpp"
#include "effects.h"
#include "engine/backbuffer_state.hpp"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "lua/metadoc.hpp"
#include "msg.h"
#include "player.h"
#include "spells.h"
#include "tables/playerdat.hpp"

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
		    const auto itemIndex = static_cast<_item_indexes>(itemId);
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
		    const auto targetId = static_cast<_item_indexes>(itemId);
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
	    [](Player &player) { return player._pMana.whole(); });
	LuaSetDocReadonlyProperty(playerType, "maxMana", "number",
	    "Maximum mana (readonly)",
	    [](Player &player) { return player._pMaxMana.whole(); });
	LuaSetDocReadonlyProperty(playerType, "health", "integer",
	    "Current hit points (readonly)",
	    [](const Player &player) { return player._pHitPoints.whole(); });
	LuaSetDocReadonlyProperty(playerType, "maxHealth", "integer",
	    "Maximum hit points (readonly)",
	    [](const Player &player) { return player._pMaxHP.whole(); });
	LuaSetDocReadonlyProperty(playerType, "lightRadius", "integer",
	    "Light radius in tiles (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pLightRad); });
	LuaSetDocReadonlyProperty(playerType, "isMoving", "boolean",
	    "True while the player is walking (readonly)",
	    [](const Player &player) { return player.isWalking(); });
	LuaSetDocReadonlyProperty(playerType, "strength", "integer",
	    "Base strength (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pBaseStr); });
	LuaSetDocReadonlyProperty(playerType, "magic", "integer",
	    "Base magic (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pBaseMag); });
	LuaSetDocReadonlyProperty(playerType, "magicCurrent", "integer",
	    "Magic including equipment bonuses, as shown on the character sheet (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pMagic); });
	LuaSetDocReadonlyProperty(playerType, "dexterity", "integer",
	    "Base dexterity (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pBaseDex); });
	LuaSetDocReadonlyProperty(playerType, "vitality", "integer",
	    "Base vitality (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pBaseVit); });
	LuaSetDocProperty(playerType, "statPoints", "integer",
	    "Unspent stat points. The hero file stores this as one byte, so values above 255 do not survive a save.",
	    &Player::_pStatPts);
	LuaSetDocReadonlyProperty(playerType, "armorClass", "integer",
	    "Armor class as shown on the character sheet (readonly)",
	    [](const Player &player) { return player.GetArmor() + (player.getCharacterLevel() * 2); });
	LuaSetDocReadonlyProperty(playerType, "toHit", "integer",
	    "To-hit percentage as shown on the character sheet: ranged with a bow equipped, else melee (readonly)",
	    [](const Player &player) {
		    return player.UsesRangedWeapon() ? player.GetRangedToHit() : player.GetMeleeToHit();
	    });
	LuaSetDocReadonlyProperty(playerType, "minDamage", "integer",
	    "Minimum damage as shown on the character sheet (readonly)",
	    [](const Player &player) {
		    int damageMod = player._pIBonusDamMod;
		    damageMod += (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && player._pClass != HeroClass::Rogue) ? player._pDamageMod / 2 : player._pDamageMod;
		    return player._pIMinDam + (player._pIBonusDam * player._pIMinDam / 100) + damageMod;
	    });
	LuaSetDocReadonlyProperty(playerType, "maxDamage", "integer",
	    "Maximum damage as shown on the character sheet (readonly)",
	    [](const Player &player) {
		    int damageMod = player._pIBonusDamMod;
		    damageMod += (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && player._pClass != HeroClass::Rogue) ? player._pDamageMod / 2 : player._pDamageMod;
		    return player._pIMaxDam + (player._pIBonusDam * player._pIMaxDam / 100) + damageMod;
	    });
	LuaSetDocReadonlyProperty(playerType, "className", "string",
	    "Class name (readonly)",
	    [](const Player &player) -> std::string { return std::string(player.getPlayerData().className); });
	LuaSetDocReadonlyProperty(playerType, "friendlyMode", "boolean",
	    "Whether the player is in friendly mode; false means hostile toward other players (readonly)",
	    [](const Player &player) { return player.friendlyMode; });
	LuaSetDocReadonlyProperty(playerType, "isHoldingShield", "boolean",
	    "Whether a shield is equipped (readonly)",
	    [](const Player &player) { return player.isHoldingItem(ItemType::Shield); });
	LuaSetDocReadonlyProperty(playerType, "weaponGraphic", "integer",
	    "PlayerWeaponGraphic of the equipped weapon combination (Unarmed=0, UnarmedShield=1, Sword=2, SwordShield=3, Bow=4, Axe=5, Mace=6, MaceShield=7, Staff=8) (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pgfxnum & 0xF); });
	LuaSetDocReadonlyProperty(playerType, "readySpellId", "integer",
	    "SpellID of the readied spell (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pRSpell); });
	LuaSetDocReadonlyProperty(playerType, "readySpellType", "integer",
	    "SpellType of the readied spell: 0 Skill, 1 Spell, 2 Scroll, 3 Charges, 4 Invalid (readonly)",
	    [](const Player &player) { return static_cast<int>(player._pRSplType); });
	LuaSetDocFn(playerType, "heldItem", "() -> Item|nil",
	    "The item held on the cursor, or nil if the hand is empty",
	    [](Player &player) -> Item * {
		    if (player.HoldItem.isEmpty())
			    return nullptr;
		    return &player.HoldItem;
	    });
	LuaSetDocFn(playerType, "iterateInventory", "(callback: function)",
	    "Call callback(item) for each item in the inventory and belt",
	    [](Player &player, const sol::function &callback) {
		    for (int i = 0; i < player._pNumInv; i++) {
			    if (!player.InvList[i].isEmpty())
				    callback(&player.InvList[i]);
		    }
		    for (int i = 0; i < MaxBeltItems; i++) {
			    if (!player.SpdList[i].isEmpty())
				    callback(&player.SpdList[i]);
		    }
	    });
	LuaSetDocFn(playerType, "say", "(speechId: integer)",
	    "Play the player's voice line for a player.HeroSpeech value",
	    [](const Player &player, int speechId) {
		    player.Say(static_cast<HeroSpeech>(speechId));
	    });
	LuaSetDocFn(playerType, "enterHealOtherMode", "()",
	    "Switch the cursor to Heal Other targeting",
	    [](const Player & /*player*/) {
		    NewCursor(CURSOR_HEALOTHER);
	    });
	LuaSetDocFn(playerType, "modifyStat", "(name: string, amount: integer)",
	    "Add to a base stat (\"Strength\", \"Magic\", \"Dexterity\" or \"Vitality\") and recalculate the player",
	    [](Player &player, std::string_view name, int amount) {
		    if (name == "Strength")
			    ModifyPlrStr(player, amount);
		    else if (name == "Magic")
			    ModifyPlrMag(player, amount);
		    else if (name == "Dexterity")
			    ModifyPlrDex(player, amount);
		    else if (name == "Vitality")
			    ModifyPlrVit(player, amount);
		    CheckStats(player);
		    CalcPlrInv(player, true);
	    });
	LuaSetDocFn(playerType, "addSkill", "(spellId: integer)",
	    "Add a skill to the player's skill list",
	    [](Player &player, int spellId) {
		    player._pAblSpells |= GetSpellBitmask(static_cast<SpellID>(spellId));
	    });
	LuaSetDocFn(playerType, "classBaseStats", "() -> integer, integer, integer, integer",
	    "The class's starting base stats as strength, magic, dexterity, vitality",
	    [](const Player &player) -> std::tuple<int, int, int, int> {
		    const ClassAttributes &attr = player.getClassAttributes();
		    return { attr.baseStr, attr.baseMag, attr.baseDex, attr.baseVit };
	    });
	LuaSetDocFn(playerType, "resetStats", "(str: integer, mag: integer, dex: integer, vit: integer)",
	    "Set the base stats, refund the difference as stat points and recalculate base life and mana",
	    [](Player &player, int str, int mag, int dex, int vit) {
		    const int refund = (player._pBaseStr - str) + (player._pBaseMag - mag)
		        + (player._pBaseDex - dex) + (player._pBaseVit - vit);

		    player._pBaseStr = str;
		    player._pStrength = str;
		    player._pBaseMag = mag;
		    player._pMagic = mag;
		    player._pBaseDex = dex;
		    player._pDexterity = dex;
		    player._pBaseVit = vit;
		    player._pVitality = vit;

		    if (refund > 0) player._pStatPts += refund;

		    const Fixed26_6 baseLife = player.calculateBaseLife();
		    player._pMaxHPBase = baseLife;
		    player._pHPBase = std::min(player._pHPBase, baseLife);

		    const Fixed26_6 baseMana = player.calculateBaseMana();
		    player._pMaxManaBase = baseMana;
		    player._pManaBase = std::min(player._pManaBase, baseMana);

		    CheckStats(player);
		    CalcPlrInv(player, true);

		    if (&player == MyPlayer) {
			    NetSendCmdParam1(false, CMD_SETSTR, player._pBaseStr);
			    NetSendCmdParam1(false, CMD_SETMAG, player._pBaseMag);
			    NetSendCmdParam1(false, CMD_SETDEX, player._pBaseDex);
			    NetSendCmdParam1(false, CMD_SETVIT, player._pBaseVit);
		    }
	    });
}

void AddClassDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LoadClassDatFromFile(dataFile, path);
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
	LuaSetDocFn(table, "get", "(index: integer) -> Player|nil",
	    "The player in the given 0-based player slot, or nil if the slot is out of range or not active",
	    [](int index) -> Player * {
		    if (index < 0 || static_cast<size_t>(index) >= Players.size())
			    return nullptr;
		    Player &player = Players[index];
		    if (!player.plractive)
			    return nullptr;
		    return &player;
	    });
	LuaSetDocFn(table, "walk_to", "(x: integer, y: integer)",
	    "Walk to the given coordinates",
	    [](int x, int y) {
		    NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, Point { x, y });
	    });
	LuaSetDocFn(table, "addClassDataFromTsv", "(path: string)",
	    "Add the classes in a classdat-format TSV file. Call inside a PlayerDataLoaded handler. Each class's data is read "
	    "from its folderName folder, like the base classes.",
	    AddClassDataFromTsv);

	sol::table heroSpeech = lua.create_table();
	for (const HeroSpeech speech : magic_enum::enum_values<HeroSpeech>()) {
		heroSpeech[std::string(magic_enum::enum_name(speech))] = static_cast<int>(speech);
	}
	table["HeroSpeech"] = heroSpeech;

	return table;
}

} // namespace devilution
