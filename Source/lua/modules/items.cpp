#include "lua/modules/items.hpp"

#include <optional>

#include <sol/sol.hpp>

#include "items.h"
#include "lua/metadoc.hpp"
#include "player.h"

namespace devilution {

namespace {

void InitItemUserType(sol::state_view &lua)
{
	// Create a new usertype for Item with no constructor
	sol::usertype<Item> itemType = lua.new_usertype<Item>(sol::no_constructor);

	// Member variables
	SetDocumented(itemType, "seed", "number", "Randomly generated identifier", [](const Item &i) { return i._iSeed; }, [](Item &i, unsigned int val) { i._iSeed = val; });
	SetDocumented(itemType, "createInfo", "number", "Creation flags", [](const Item &i) { return i._iCreateInfo; }, [](Item &i, unsigned int val) { i._iCreateInfo = val; });
	SetDocumented(itemType, "type", "ItemType", "Item type", [](const Item &i) { return i._itype; }, [](Item &i, int val) { i._itype = static_cast<ItemType>(val); });
	SetDocumented(itemType, "animFlag", "boolean", "Animation flag", [](const Item &i) { return i._iAnimFlag; }, [](Item &i, bool val) { i._iAnimFlag = val; });
	SetDocumented(itemType, "position", "Point", "Item world position", [](const Item &i) { return i.position; }, [](Item &i, Point val) { i.position = val; });
	SetDocumented(itemType, "animInfo", "AnimationInfo", "Animation information", [](const Item &i) { return i.AnimInfo; }, [](Item &i, AnimationInfo val) { i.AnimInfo = val; });
	SetDocumented(itemType, "delFlag", "boolean", "Deletion flag", [](const Item &i) { return i._iDelFlag; }, [](Item &i, bool val) { i._iDelFlag = val; });
	SetDocumented(itemType, "selectionRegion", "number", "Selection region", [](const Item &i) { return static_cast<int>(i.selectionRegion); }, [](Item &i, int val) { i.selectionRegion = static_cast<SelectionRegion>(val); });
	SetDocumented(itemType, "postDraw", "boolean", "Post-draw flag", [](const Item &i) { return i._iPostDraw; }, [](Item &i, bool val) { i._iPostDraw = val; });
	SetDocumented(itemType, "identified", "boolean", "Identified flag", [](const Item &i) { return i._iIdentified; }, [](Item &i, bool val) { i._iIdentified = val; });
	SetDocumented(itemType, "magical", "number", "Item quality", [](const Item &i) { return static_cast<int>(i._iMagical); }, [](Item &i, int val) { i._iMagical = static_cast<item_quality>(val); });
	SetDocumented(itemType, "name", "string", "Item name", [](const Item &i) { return std::string(i._iName); }, [](Item &i, const std::string &val) {
        std::strncpy(i._iName, val.c_str(), sizeof(i._iName));
        i._iName[sizeof(i._iName) - 1] = '\0'; });
	SetDocumented(itemType, "iName", "string", "Identified item name", [](const Item &i) { return std::string(i._iIName); }, [](Item &i, const std::string &val) {
        std::strncpy(i._iIName, val.c_str(), sizeof(i._iIName));
        i._iIName[sizeof(i._iIName) - 1] = '\0'; });
	SetDocumented(itemType, "loc", "ItemEquipType", "Equipment location", [](const Item &i) { return i._iLoc; }, [](Item &i, int val) { i._iLoc = static_cast<item_equip_type>(val); });
	SetDocumented(itemType, "class", "ItemClass", "Item class", [](const Item &i) { return i._iClass; }, [](Item &i, int val) { i._iClass = static_cast<item_class>(val); });
	SetDocumented(itemType, "curs", "number", "Cursor index", [](const Item &i) { return i._iCurs; }, [](Item &i, int val) { i._iCurs = val; });
	SetDocumented(itemType, "value", "number", "Item value", [](const Item &i) { return i._ivalue; }, [](Item &i, int val) { i._ivalue = val; });
	SetDocumented(itemType, "ivalue", "number", "Identified item value", [](const Item &i) { return i._iIvalue; }, [](Item &i, int val) { i._iIvalue = val; });
	SetDocumented(itemType, "minDam", "number", "Minimum damage", [](const Item &i) { return i._iMinDam; }, [](Item &i, int val) { i._iMinDam = val; });
	SetDocumented(itemType, "maxDam", "number", "Maximum damage", [](const Item &i) { return i._iMaxDam; }, [](Item &i, int val) { i._iMaxDam = val; });
	SetDocumented(itemType, "AC", "number", "Armor class", [](const Item &i) { return i._iAC; }, [](Item &i, int val) { i._iAC = val; });
	SetDocumented(itemType, "flags", "ItemSpecialEffect", "Special effect flags", [](const Item &i) { return static_cast<int>(i._iFlags); }, [](Item &i, int val) { i._iFlags = static_cast<ItemSpecialEffect>(val); });
	SetDocumented(itemType, "miscId", "ItemMiscId", "Miscellaneous ID", [](const Item &i) { return i._iMiscId; }, [](Item &i, int val) { i._iMiscId = static_cast<item_misc_id>(val); });
	SetDocumented(itemType, "spell", "SpellID", "Spell", [](const Item &i) { return i._iSpell; }, [](Item &i, int val) { i._iSpell = static_cast<SpellID>(val); });
	SetDocumented(itemType, "IDidx", "ItemIndex", "Base item index", [](const Item &i) { return i.IDidx; }, [](Item &i, int val) { i.IDidx = static_cast<_item_indexes>(val); });
	SetDocumented(itemType, "charges", "number", "Number of charges", [](const Item &i) { return i._iCharges; }, [](Item &i, int val) { i._iCharges = val; });
	SetDocumented(itemType, "maxCharges", "number", "Maximum charges", [](const Item &i) { return i._iMaxCharges; }, [](Item &i, int val) { i._iMaxCharges = val; });
	SetDocumented(itemType, "durability", "number", "Durability", [](const Item &i) { return i._iDurability; }, [](Item &i, int val) { i._iDurability = val; });
	SetDocumented(itemType, "maxDur", "number", "Maximum durability", [](const Item &i) { return i._iMaxDur; }, [](Item &i, int val) { i._iMaxDur = val; });
	SetDocumented(itemType, "PLDam", "number", "Damage % bonus", [](const Item &i) { return i._iPLDam; }, [](Item &i, int val) { i._iPLDam = val; });
	SetDocumented(itemType, "PLToHit", "number", "Chance to hit bonus", [](const Item &i) { return i._iPLToHit; }, [](Item &i, int val) { i._iPLToHit = val; });
	SetDocumented(itemType, "PLAC", "number", "Armor class % bonus", [](const Item &i) { return i._iPLAC; }, [](Item &i, int val) { i._iPLAC = val; });
	SetDocumented(itemType, "PLStr", "number", "Strength bonus", [](const Item &i) { return i._iPLStr; }, [](Item &i, int val) { i._iPLStr = val; });
	SetDocumented(itemType, "PLMag", "number", "Magic bonus", [](const Item &i) { return i._iPLMag; }, [](Item &i, int val) { i._iPLMag = val; });
	SetDocumented(itemType, "PLDex", "number", "Dexterity bonus", [](const Item &i) { return i._iPLDex; }, [](Item &i, int val) { i._iPLDex = val; });
	SetDocumented(itemType, "PLVit", "number", "Vitality bonus", [](const Item &i) { return i._iPLVit; }, [](Item &i, int val) { i._iPLVit = val; });
	SetDocumented(itemType, "PLFR", "number", "Fire resistance bonus", [](const Item &i) { return i._iPLFR; }, [](Item &i, int val) { i._iPLFR = val; });
	SetDocumented(itemType, "PLLR", "number", "Lightning resistance bonus", [](const Item &i) { return i._iPLLR; }, [](Item &i, int val) { i._iPLLR = val; });
	SetDocumented(itemType, "PLMR", "number", "Magic resistance bonus", [](const Item &i) { return i._iPLMR; }, [](Item &i, int val) { i._iPLMR = val; });
	SetDocumented(itemType, "PLMana", "number", "Mana bonus", [](const Item &i) { return i._iPLMana; }, [](Item &i, int val) { i._iPLMana = val; });
	SetDocumented(itemType, "PLHP", "number", "Life bonus", [](const Item &i) { return i._iPLHP; }, [](Item &i, int val) { i._iPLHP = val; });
	SetDocumented(itemType, "PLDamMod", "number", "Damage modifier bonus", [](const Item &i) { return i._iPLDamMod; }, [](Item &i, int val) { i._iPLDamMod = val; });
	SetDocumented(itemType, "PLGetHit", "number", "Damage from enemies bonus", [](const Item &i) { return i._iPLGetHit; }, [](Item &i, int val) { i._iPLGetHit = val; });
	SetDocumented(itemType, "PLLight", "number", "Light bonus", [](const Item &i) { return i._iPLLight; }, [](Item &i, int val) { i._iPLLight = val; });
	SetDocumented(itemType, "splLvlAdd", "number", "Spell level bonus", [](const Item &i) { return i._iSplLvlAdd; }, [](Item &i, int val) { i._iSplLvlAdd = val; });
	SetDocumented(itemType, "request", "boolean", "Request flag", [](const Item &i) { return i._iRequest; }, [](Item &i, bool val) { i._iRequest = val; });
	SetDocumented(itemType, "uid", "number", "Unique item ID", [](const Item &i) { return i._iUid; }, [](Item &i, int val) { i._iUid = val; });
	SetDocumented(itemType, "fMinDam", "number", "Fire minimum damage", [](const Item &i) { return i._iFMinDam; }, [](Item &i, int val) { i._iFMinDam = val; });
	SetDocumented(itemType, "fMaxDam", "number", "Fire maximum damage", [](const Item &i) { return i._iFMaxDam; }, [](Item &i, int val) { i._iFMaxDam = val; });
	SetDocumented(itemType, "lMinDam", "number", "Lightning minimum damage", [](const Item &i) { return i._iLMinDam; }, [](Item &i, int val) { i._iLMinDam = val; });
	SetDocumented(itemType, "lMaxDam", "number", "Lightning maximum damage", [](const Item &i) { return i._iLMaxDam; }, [](Item &i, int val) { i._iLMaxDam = val; });
	SetDocumented(itemType, "PLEnAc", "number", "Damage target AC bonus", [](const Item &i) { return i._iPLEnAc; }, [](Item &i, int val) { i._iPLEnAc = val; });
	SetDocumented(itemType, "prePower", "ItemEffect", "Prefix power", [](const Item &i) { return i._iPrePower; }, [](Item &i, int val) { i._iPrePower = static_cast<item_effect_type>(val); });
	SetDocumented(itemType, "sufPower", "ItemEffect", "Suffix power", [](const Item &i) { return i._iSufPower; }, [](Item &i, int val) { i._iSufPower = static_cast<item_effect_type>(val); });
	SetDocumented(itemType, "vAdd1", "number", "Value addition 1", [](const Item &i) { return i._iVAdd1; }, [](Item &i, int val) { i._iVAdd1 = val; });
	SetDocumented(itemType, "vMult1", "number", "Value multiplier 1", [](const Item &i) { return i._iVMult1; }, [](Item &i, int val) { i._iVMult1 = val; });
	SetDocumented(itemType, "vAdd2", "number", "Value addition 2", [](const Item &i) { return i._iVAdd2; }, [](Item &i, int val) { i._iVAdd2 = val; });
	SetDocumented(itemType, "vMult2", "number", "Value multiplier 2", [](const Item &i) { return i._iVMult2; }, [](Item &i, int val) { i._iVMult2 = val; });
	SetDocumented(itemType, "minStr", "number", "Minimum strength required", [](const Item &i) { return i._iMinStr; }, [](Item &i, int val) { i._iMinStr = val; });
	SetDocumented(itemType, "minMag", "number", "Minimum magic required", [](const Item &i) { return i._iMinMag; }, [](Item &i, int val) { i._iMinMag = val; });
	SetDocumented(itemType, "minDex", "number", "Minimum dexterity required", [](const Item &i) { return i._iMinDex; }, [](Item &i, int val) { i._iMinDex = val; });
	SetDocumented(itemType, "statFlag", "boolean", "Equippable flag", [](const Item &i) { return i._iStatFlag; }, [](Item &i, bool val) { i._iStatFlag = val; });
	SetDocumented(itemType, "damAcFlags", "ItemSpecialEffectHf", "Secondary special effect flags", [](const Item &i) { return i._iDamAcFlags; }, [](Item &i, int val) { i._iDamAcFlags = static_cast<ItemSpecialEffectHf>(val); });
	SetDocumented(itemType, "buff", "number", "Secondary creation flags", [](const Item &i) { return i.dwBuff; }, [](Item &i, int val) { i.dwBuff = val; });

	// Member functions
	SetDocumented(itemType, "pop", "() -> Item", "Clears this item and returns the old value", &Item::pop);
	SetDocumented(itemType, "clear", "() -> void", "Resets the item", &Item::clear);
	SetDocumented(itemType, "isEmpty", "() -> boolean", "Checks whether this item is empty", &Item::isEmpty);
	SetDocumented(itemType, "isEquipment", "() -> boolean", "Checks if item is equipment", &Item::isEquipment);
	SetDocumented(itemType, "isWeapon", "() -> boolean", "Checks if item is a weapon", &Item::isWeapon);
	SetDocumented(itemType, "isArmor", "() -> boolean", "Checks if item is armor", &Item::isArmor);
	SetDocumented(itemType, "isGold", "() -> boolean", "Checks if item is gold", &Item::isGold);
	SetDocumented(itemType, "isHelm", "() -> boolean", "Checks if item is a helm", &Item::isHelm);
	SetDocumented(itemType, "isShield", "() -> boolean", "Checks if item is a shield", &Item::isShield);
	SetDocumented(itemType, "isJewelry", "() -> boolean", "Checks if item is jewelry", &Item::isJewelry);
	SetDocumented(itemType, "isScroll", "() -> boolean", "Checks if item is a scroll", &Item::isScroll);
	SetDocumented(itemType, "isScrollOf", "(spell: SpellID) -> boolean", "Checks if item is a scroll of a given spell", &Item::isScrollOf);
	SetDocumented(itemType, "isRune", "() -> boolean", "Checks if item is a rune", &Item::isRune);
	SetDocumented(itemType, "isRuneOf", "(spell: SpellID) -> boolean", "Checks if item is a rune of a given spell", &Item::isRuneOf);
	SetDocumented(itemType, "isUsable", "() -> boolean", "Checks if item is usable", &Item::isUsable);
	SetDocumented(itemType, "keyAttributesMatch", "(seed: number, idx: number, createInfo: number) -> boolean", "Checks if key attributes match", &Item::keyAttributesMatch);
	SetDocumented(itemType, "getTextColor", "() -> UiFlags", "Gets the text color", &Item::getTextColor);
	SetDocumented(itemType, "getTextColorWithStatCheck", "() -> UiFlags", "Gets the text color with stat check", &Item::getTextColorWithStatCheck);
	SetDocumented(itemType, "setNewAnimation", "(showAnimation: boolean) -> void", "Sets the new animation", &Item::setNewAnimation);
	SetDocumented(itemType, "updateRequiredStatsCacheForPlayer", "(player: Player) -> void", "Updates the required stats cache", &Item::updateRequiredStatsCacheForPlayer);
	SetDocumented(itemType, "getName", "() -> string", "Gets the translated item name", &Item::getName);
}

void RegisterItemEnums(sol::state_view &lua)
{
	lua.new_enum("ItemEquipType",
	    "None", ILOC_NONE,
	    "OneHand", ILOC_ONEHAND,
	    "TwoHand", ILOC_TWOHAND,
	    "Armor", ILOC_ARMOR,
	    "Helm", ILOC_HELM,
	    "Ring", ILOC_RING,
	    "Amulet", ILOC_AMULET,
	    "Unequipable", ILOC_UNEQUIPABLE,
	    "Belt", ILOC_BELT,
	    "Invalid", ILOC_INVALID);

	lua.new_enum("ItemClass",
	    "None", ICLASS_NONE,
	    "Weapon", ICLASS_WEAPON,
	    "Armor", ICLASS_ARMOR,
	    "Misc", ICLASS_MISC,
	    "Gold", ICLASS_GOLD,
	    "Quest", ICLASS_QUEST);

	lua.new_enum("ItemMiscId",
	    "None", IMISC_NONE,
	    "FullHeal", IMISC_FULLHEAL,
	    "Heal", IMISC_HEAL,
	    "Mana", IMISC_MANA,
	    "FullMana", IMISC_FULLMANA,
	    "ElixirStr", IMISC_ELIXSTR,
	    "ElixirMag", IMISC_ELIXMAG,
	    "ElixirDex", IMISC_ELIXDEX,
	    "ElixirVit", IMISC_ELIXVIT,
	    "Rejuv", IMISC_REJUV,
	    "FullRejuv", IMISC_FULLREJUV,
	    "Scroll", IMISC_SCROLL,
	    "ScrollT", IMISC_SCROLLT,
	    "Staff", IMISC_STAFF,
	    "Book", IMISC_BOOK,
	    "Ring", IMISC_RING,
	    "Amulet", IMISC_AMULET,
	    "Unique", IMISC_UNIQUE);

	lua.new_enum("SpellID",
	    "Null", SpellID::Null,
	    "Firebolt", SpellID::Firebolt,
	    "Healing", SpellID::Healing,
	    "Lightning", SpellID::Lightning,
	    "Flash", SpellID::Flash,
	    "Identify", SpellID::Identify,
	    "FireWall", SpellID::FireWall,
	    "TownPortal", SpellID::TownPortal,
	    "StoneCurse", SpellID::StoneCurse,
	    "Infravision", SpellID::Infravision,
	    "Phasing", SpellID::Phasing,
	    "ManaShield", SpellID::ManaShield,
	    "Fireball", SpellID::Fireball,
	    "Guardian", SpellID::Guardian,
	    "ChainLightning", SpellID::ChainLightning,
	    "FlameWave", SpellID::FlameWave,
	    "DoomSerpents", SpellID::DoomSerpents,
	    "BloodRitual", SpellID::BloodRitual,
	    "Nova", SpellID::Nova,
	    "Invisibility", SpellID::Invisibility,
	    "Inferno", SpellID::Inferno,
	    "Golem", SpellID::Golem,
	    "Rage", SpellID::Rage,
	    "Teleport", SpellID::Teleport,
	    "Apocalypse", SpellID::Apocalypse,
	    "Etherealize", SpellID::Etherealize,
	    "ItemRepair", SpellID::ItemRepair,
	    "StaffRecharge", SpellID::StaffRecharge,
	    "TrapDisarm", SpellID::TrapDisarm,
	    "Elemental", SpellID::Elemental,
	    "ChargedBolt", SpellID::ChargedBolt,
	    "HolyBolt", SpellID::HolyBolt,
	    "Resurrect", SpellID::Resurrect,
	    "Telekinesis", SpellID::Telekinesis,
	    "HealOther", SpellID::HealOther,
	    "BloodStar", SpellID::BloodStar,
	    "BoneSpirit", SpellID::BoneSpirit,
	    "Mana", SpellID::Mana,
	    "Magi", SpellID::Magi,
	    "Jester", SpellID::Jester,
	    "LightningWall", SpellID::LightningWall,
	    "Immolation", SpellID::Immolation,
	    "Warp", SpellID::Warp,
	    "Reflect", SpellID::Reflect,
	    "Berserk", SpellID::Berserk,
	    "RingOfFire", SpellID::RingOfFire,
	    "Search", SpellID::Search,
	    "RuneOfFire", SpellID::RuneOfFire,
	    "RuneOfLight", SpellID::RuneOfLight,
	    "RuneOfNova", SpellID::RuneOfNova,
	    "RuneOfImmolation", SpellID::RuneOfImmolation,
	    "RuneOfStone", SpellID::RuneOfStone,
	    "Invalid", SpellID::Invalid);
}

} // namespace

sol::table LuaItemModule(sol::state_view &lua)
{
	InitItemUserType(lua);
	RegisterItemEnums(lua);

	sol::table table = lua.create_table();

	return table;
}

} // namespace devilution
