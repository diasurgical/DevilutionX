#include "lua/modules/monsters.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>

#include <sol/sol.hpp>

#include "cursor.h"
#include "data/file.hpp"
#include "engine/lighting_defs.hpp"
#include "engine/point.hpp"
#include "game_mode.hpp"
#include "levels/dun_tile_data.hpp"
#include "lua/metadoc.hpp"
#include "monster.h"
#include "player.h"
#include "tables/monstdat.h"
#include "utils/language.h"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

void AddMonsterDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LoadMonstDatFromFile(dataFile, path, true);
}

void AddUniqueMonsterDataFromTsv(const std::string_view path)
{
	DataFile dataFile = DataFile::loadOrDie(path);
	LoadUniqueMonstDatFromFile(dataFile, path);
}

_difficulty CurrentDifficulty()
{
	return static_cast<_difficulty>(sgGameInitInfo.nDifficulty);
}

void InitPointUserType(sol::state_view &lua)
{
	sol::usertype<Point> pointType = lua.new_usertype<Point>(sol::no_constructor);
	pointType["x"] = &Point::x;
	pointType["y"] = &Point::y;
	pointType["new"] = [](int x, int y) -> Point { return { x, y }; };
}

void InitMonsterUserType(sol::state_view &lua)
{
	sol::usertype<Monster> monsterType = lua.new_usertype<Monster>(sol::no_constructor);
	LuaSetDocReadonlyProperty(monsterType, "position", "Point",
	    "Monster's current position (readonly)",
	    [](const Monster &monster) {
		    return Point { monster.position.tile };
	    });
	LuaSetDocReadonlyProperty(monsterType, "id", "integer",
	    "Monster's unique ID (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(reinterpret_cast<uintptr_t>(&monster));
	    });
	LuaSetDocReadonlyProperty(monsterType, "typeId", "integer",
	    "Monster type ID, an index into the monster data (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.type().type);
	    });
	LuaSetDocReadonlyProperty(monsterType, "name", "string",
	    "Monster's display name (readonly)",
	    [](const Monster &monster) {
		    return std::string(monster.name());
	    });
	LuaSetDocReadonlyProperty(monsterType, "health", "integer",
	    "Current hit points (readonly)",
	    [](const Monster &monster) {
		    return monster.hitPoints.whole();
	    });
	LuaSetDocReadonlyProperty(monsterType, "maxHealth", "integer",
	    "Maximum hit points (readonly)",
	    [](const Monster &monster) {
		    return monster.maxHitPoints.whole();
	    });
	LuaSetDocFn(monsterType, "setHitPoints", "(hp: integer)",
	    "Set the current hit points",
	    [](Monster &monster, int hp) {
		    monster.hitPoints = Fixed26_6::fromInt(hp);
	    });
	LuaSetDocFn(monsterType, "setMaxHitPoints", "(hp: integer)",
	    "Set the maximum hit points",
	    [](Monster &monster, int hp) {
		    monster.maxHitPoints = Fixed26_6::fromInt(hp);
	    });
	LuaSetDocReadonlyProperty(monsterType, "isUnique", "boolean",
	    "Whether this is a unique monster (readonly)",
	    [](const Monster &monster) {
		    return monster.isUnique();
	    });
	LuaSetDocReadonlyProperty(monsterType, "uniqueType", "integer",
	    "Index into the unique monster data, or -1 for a regular monster (readonly)",
	    [](const Monster &monster) {
		    if (monster.uniqueType == UniqueMonsterType::None) return -1;
		    return static_cast<int>(monster.uniqueType);
	    });
	LuaSetDocReadonlyProperty(monsterType, "isQuestMonster", "boolean",
	    "Whether this monster is a unique monster or Diablo (readonly)",
	    [](const Monster &monster) {
		    return monster.isUnique() || monster.type().type == MT_DIABLO;
	    });
	LuaSetDocReadonlyProperty(monsterType, "level", "integer",
	    "Monster level at the current difficulty (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.level(CurrentDifficulty()));
	    });
	LuaSetDocReadonlyProperty(monsterType, "minDamage", "integer",
	    "Minimum melee damage (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.minDamage);
	    });
	LuaSetDocReadonlyProperty(monsterType, "maxDamage", "integer",
	    "Maximum melee damage (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.maxDamage);
	    });
	LuaSetDocReadonlyProperty(monsterType, "armorClass", "integer",
	    "Armor class (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.armorClass);
	    });
	LuaSetDocReadonlyProperty(monsterType, "toHit", "integer",
	    "Chance to hit at the current difficulty (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.toHit(CurrentDifficulty()));
	    });
	LuaSetDocReadonlyProperty(monsterType, "resistance", "integer",
	    "Resistance and immunity flags, see monsters.Resistance (readonly)",
	    [](const Monster &monster) {
		    return static_cast<int>(monster.resistance);
	    });
	LuaSetDocFn(monsterType, "setMinDamage", "(value: integer)",
	    "Set the minimum melee damage (clamped to 0-255)",
	    [](Monster &monster, int value) {
		    monster.minDamage = static_cast<uint8_t>(std::clamp(value, 0, 255));
	    });
	LuaSetDocFn(monsterType, "setMaxDamage", "(value: integer)",
	    "Set the maximum melee damage (clamped to 0-255)",
	    [](Monster &monster, int value) {
		    monster.maxDamage = static_cast<uint8_t>(std::clamp(value, 0, 255));
	    });
	LuaSetDocFn(monsterType, "setArmorClass", "(value: integer)",
	    "Set the armor class (clamped to 0-255)",
	    [](Monster &monster, int value) {
		    monster.armorClass = static_cast<uint8_t>(std::clamp(value, 0, 255));
	    });
	LuaSetDocFn(monsterType, "setResistance", "(flags: integer)",
	    "Set the resistance and immunity flags, see monsters.Resistance",
	    [](Monster &monster, int flags) {
		    monster.resistance = static_cast<uint16_t>(flags);
	    });
	LuaSetDocReadonlyProperty(monsterType, "isLit", "boolean",
	    "Whether the monster's tile is lit by any light source (readonly)",
	    [](const Monster &monster) {
		    return dLight[monster.position.tile.x][monster.position.tile.y] < LightsMax;
	    });
	LuaSetDocReadonlyProperty(monsterType, "hasNoLife", "boolean",
	    "Whether the monster is at 0 hit points (readonly)",
	    [](const Monster &monster) {
		    return monster.hasNoLife();
	    });
	LuaSetDocReadonlyProperty(monsterType, "isActive", "boolean",
	    "Whether the monster is awake; monster AI does not run until it is (readonly)",
	    [](const Monster &monster) {
		    return monster.activeForTicks != 0;
	    });
	LuaSetDocReadonlyProperty(monsterType, "monsterClass", "string",
	    "\"Animal\", \"Demon\" or \"Undead\" (readonly)",
	    [](const Monster &monster) -> std::string {
		    switch (monster.data().monsterClass) {
		    case MonsterClass::Animal: return "Animal";
		    case MonsterClass::Demon: return "Demon";
		    case MonsterClass::Undead: return "Undead";
		    }
		    return "Animal";
	    });
	LuaSetDocReadonlyProperty(monsterType, "isHidden", "boolean",
	    "Whether the monster is faded out, e.g. a cloaked Sneak (readonly)",
	    [](const Monster &monster) {
		    return (monster.flags & MFLAG_HIDDEN) != 0;
	    });
	LuaSetDocReadonlyProperty(monsterType, "hasRangedAttack", "boolean",
	    "Whether the monster type's AI uses a ranged attack (readonly)",
	    [](const Monster &monster) {
		    switch (monster.data().ai) {
		    case MonsterAIID::SkeletonRanged:
		    case MonsterAIID::GoatRanged:
		    case MonsterAIID::Magma:
		    case MonsterAIID::Gargoyle:
		    case MonsterAIID::Succubus:
		    case MonsterAIID::Storm:
		    case MonsterAIID::Acid:
		    case MonsterAIID::AcidUnique:
		    case MonsterAIID::Diablo:
		    case MonsterAIID::LazarusSuccubus:
		    case MonsterAIID::FireBat:
		    case MonsterAIID::Torchant:
		    case MonsterAIID::Lich:
		    case MonsterAIID::ArchLich:
		    case MonsterAIID::Psychorb:
		    case MonsterAIID::Necromorb:
		    case MonsterAIID::BoneDemon:
		    case MonsterAIID::Counselor:
		    case MonsterAIID::Mega:
			    return true;
		    default:
			    return false;
		    }
	    });
	LuaSetDocFn(monsterType, "distanceTo", "(player: Player) -> integer",
	    "Distance in tiles to the player, counting diagonal steps as one",
	    [](const Monster &monster, const Player &player) {
		    return monster.position.tile.WalkingDistance(player.position.tile);
	    });
	LuaSetDocFn(monsterType, "hasLineOfSightTo", "(other: Monster) -> boolean",
	    "Whether a missile could travel from this monster to the other monster",
	    [](const Monster &monster, const Monster &other) {
		    return LineClearMovingMissile(monster.position.tile, other.position.tile);
	    });
	LuaSetDocFn(monsterType, "hasLineOfSightToPlayer", "(player: Player) -> boolean",
	    "Whether a missile could travel from this monster to the player",
	    [](const Monster &monster, const Player &player) {
		    return LineClearMovingMissile(monster.position.tile, player.position.tile);
	    });
}

std::tuple<int, int> GetTypeHpRange(int typeId)
{
	if (typeId < 0 || typeId >= static_cast<int>(MonstersData.size()))
		return { 0, 0 };
	// Same values as the monster info box (PrintMonstHistory).
	int minHP = MonstersData[typeId].hitPointsMinimum;
	int maxHP = MonstersData[typeId].hitPointsMaximum;
	if (!gbIsMultiplayer) {
		minHP /= 2;
		maxHP /= 2;
	}
	minHP = std::max(minHP, 1);
	maxHP = std::max(maxHP, 1);

	int hpBonusNightmare = 100;
	int hpBonusHell = 200;
	if (gbIsHellfire) {
		hpBonusNightmare = (!gbIsMultiplayer ? 50 : 100);
		hpBonusHell = (!gbIsMultiplayer ? 100 : 200);
	}
	if (sgGameInitInfo.nDifficulty == DIFF_NIGHTMARE) {
		minHP = 3 * minHP + hpBonusNightmare;
		maxHP = 3 * maxHP + hpBonusNightmare;
	} else if (sgGameInitInfo.nDifficulty == DIFF_HELL) {
		minHP = 4 * minHP + hpBonusHell;
		maxHP = 4 * maxHP + hpBonusHell;
	}
	return { minHP, maxHP };
}

sol::table GetTypeResistances(int typeId, sol::this_state state)
{
	sol::state_view lua(state);
	sol::table result = lua.create_table();
	if (typeId < 0 || typeId >= static_cast<int>(MonstersData.size()))
		return result;
	const int res = (sgGameInitInfo.nDifficulty != DIFF_HELL) ? MonstersData[typeId].resistance : MonstersData[typeId].resistanceHell;
	result["resistMagic"] = (res & RESIST_MAGIC) != 0;
	result["resistFire"] = (res & RESIST_FIRE) != 0;
	result["resistLightning"] = (res & RESIST_LIGHTNING) != 0;
	result["immuneMagic"] = (res & IMMUNE_MAGIC) != 0;
	result["immuneFire"] = (res & IMMUNE_FIRE) != 0;
	result["immuneLightning"] = (res & IMMUNE_LIGHTNING) != 0;
	return result;
}

} // namespace

sol::table LuaMonstersModule(sol::state_view &lua)
{
	InitPointUserType(lua);
	InitMonsterUserType(lua);
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "addMonsterDataFromTsv", "(path: string)", AddMonsterDataFromTsv);
	LuaSetDocFn(table, "addUniqueMonsterDataFromTsv", "(path: string)", AddUniqueMonsterDataFromTsv);
	LuaSetDocFn(table, "getNameByTypeId", "(typeId: integer) -> string|nil",
	    "Display name of a monster type, or nil if the type ID is out of range",
	    [](int typeId) -> sol::optional<std::string> {
		    if (typeId < 0 || typeId >= static_cast<int>(MonstersData.size()))
			    return sol::nullopt;
		    return std::string(pgettext("monster", MonstersData[typeId].name));
	    });
	LuaSetDocFn(table, "getTypeKillCount", "(typeId: integer) -> integer",
	    "Number of monsters of this type the player has killed",
	    [](int typeId) {
		    if (typeId < 0 || typeId >= static_cast<int>(NUM_MAX_MTYPES))
			    return 0;
		    return MonsterKillCounts[typeId];
	    });
	LuaSetDocFn(table, "getTypeHpRange", "(typeId: integer) -> integer, integer",
	    "Minimum and maximum hit points of a monster type at the current difficulty, as shown in the monster info box",
	    GetTypeHpRange);
	LuaSetDocFn(table, "getTypeResistances", "(typeId: integer) -> table",
	    "Resistances of a monster type at the current difficulty: resistMagic, resistFire, resistLightning, "
	    "immuneMagic, immuneFire, immuneLightning (booleans)",
	    GetTypeResistances);
	LuaSetDocFn(table, "getHovered", "() -> Monster|nil",
	    "The monster under the cursor, or nil",
	    []() -> Monster * {
		    if (pcursmonst == -1) return nullptr;
		    return &Monsters[pcursmonst];
	    });

	sol::table resistance = lua.create_table();
	resistance["ResistMagic"] = static_cast<int>(RESIST_MAGIC);
	resistance["ResistFire"] = static_cast<int>(RESIST_FIRE);
	resistance["ResistLightning"] = static_cast<int>(RESIST_LIGHTNING);
	resistance["ImmuneMagic"] = static_cast<int>(IMMUNE_MAGIC);
	resistance["ImmuneFire"] = static_cast<int>(IMMUNE_FIRE);
	resistance["ImmuneLightning"] = static_cast<int>(IMMUNE_LIGHTNING);
	resistance["ImmuneAcid"] = static_cast<int>(IMMUNE_ACID);
	table["Resistance"] = resistance;

	return table;
}

} // namespace devilution
