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
	SetDocumented(itemType, "seed", "number", "Randomly generated identifier", &Item::_iSeed);
	SetDocumented(itemType, "createInfo", "number", "Creation flags", &Item::_iCreateInfo);
	SetDocumented(itemType, "type", "ItemType", "Item type", &Item::_itype);
	SetDocumented(itemType, "animFlag", "boolean", "Animation flag", &Item::_iAnimFlag);
	SetDocumented(itemType, "position", "Point", "Item world position", &Item::position);
	SetDocumented(itemType, "animInfo", "AnimationInfo", "Animation information", &Item::AnimInfo);
	SetDocumented(itemType, "delFlag", "boolean", "Deletion flag", &Item::_iDelFlag);
	SetDocumented(itemType, "selectionRegion", "number", "Selection region", [](Item &i) { return static_cast<int>(i.selectionRegion); }, [](Item &i, int val) { i.selectionRegion = static_cast<SelectionRegion>(val); });
	SetDocumented(itemType, "postDraw", "boolean", "Post-draw flag", &Item::_iPostDraw);
	SetDocumented(itemType, "identified", "boolean", "Identified flag", &Item::_iIdentified);
	SetDocumented(itemType, "magical", "number", "Item quality", [](Item &i) { return static_cast<int>(i._iMagical); }, [](Item &i, int val) { i._iMagical = static_cast<item_quality>(val); });
	SetDocumented(itemType, "name", "string", "Item name", &Item::_iName);
	SetDocumented(itemType, "iName", "string", "Identified item name", &Item::_iIName);
	SetDocumented(itemType, "loc", "ItemEquipType", "Equipment location", [](Item &i) { return i._iLoc; }, [](Item &i, int val) { i._iLoc = static_cast<item_equip_type>(val); });
	SetDocumented(itemType, "class", "ItemClass", "Item class", [](Item &i) { return i._iClass; }, [](Item &i, int val) { i._iClass = static_cast<item_class>(val); });
	SetDocumented(itemType, "curs", "number", "Cursor index", &Item::_iCurs);
	SetDocumented(itemType, "value", "number", "Item value", &Item::_ivalue);
	SetDocumented(itemType, "ivalue", "number", "Identified item value", &Item::_iIvalue);
	SetDocumented(itemType, "minDam", "number", "Minimum damage", &Item::_iMinDam);
	SetDocumented(itemType, "maxDam", "number", "Maximum damage", &Item::_iMaxDam);
	SetDocumented(itemType, "AC", "number", "Armor class", &Item::_iAC);
	SetDocumented(itemType, "flags", "ItemSpecialEffect", "Special effect flags", [](Item &i) { return static_cast<int>(i._iFlags); }, [](Item &i, int val) { i._iFlags = static_cast<ItemSpecialEffect>(val); });
	SetDocumented(itemType, "miscId", "ItemMiscId", "Miscellaneous ID", [](Item &i) { return i._iMiscId; }, [](Item &i, int val) { i._iMiscId = static_cast<item_misc_id>(val); });
	SetDocumented(itemType, "spell", "SpellID", "Spell", [](Item &i) { return i._iSpell; }, [](Item &i, int val) { i._iSpell = static_cast<SpellID>(val); });
	SetDocumented(itemType, "IDidx", "ItemIndex", "Base item index", [](Item &i) { return i.IDidx; }, [](Item &i, int val) { i.IDidx = static_cast<_item_indexes>(val); });
	SetDocumented(itemType, "charges", "number", "Number of charges", &Item::_iCharges);
	SetDocumented(itemType, "maxCharges", "number", "Maximum charges", &Item::_iMaxCharges);
	SetDocumented(itemType, "durability", "number", "Durability", &Item::_iDurability);
	SetDocumented(itemType, "maxDur", "number", "Maximum durability", &Item::_iMaxDur);
	SetDocumented(itemType, "PLDam", "number", "Damage % bonus", &Item::_iPLDam);
	SetDocumented(itemType, "PLToHit", "number", "Chance to hit bonus", &Item::_iPLToHit);
	SetDocumented(itemType, "PLAC", "number", "Armor class % bonus", &Item::_iPLAC);
	SetDocumented(itemType, "PLStr", "number", "Strength bonus", &Item::_iPLStr);
	SetDocumented(itemType, "PLMag", "number", "Magic bonus", &Item::_iPLMag);
	SetDocumented(itemType, "PLDex", "number", "Dexterity bonus", &Item::_iPLDex);
	SetDocumented(itemType, "PLVit", "number", "Vitality bonus", &Item::_iPLVit);
	SetDocumented(itemType, "PLFR", "number", "Fire resistance bonus", &Item::_iPLFR);
	SetDocumented(itemType, "PLLR", "number", "Lightning resistance bonus", &Item::_iPLLR);
	SetDocumented(itemType, "PLMR", "number", "Magic resistance bonus", &Item::_iPLMR);
	SetDocumented(itemType, "PLMana", "number", "Mana bonus", &Item::_iPLMana);
	SetDocumented(itemType, "PLHP", "number", "Life bonus", &Item::_iPLHP);
	SetDocumented(itemType, "PLDamMod", "number", "Damage modifier bonus", &Item::_iPLDamMod);
	SetDocumented(itemType, "PLGetHit", "number", "Damage from enemies bonus", &Item::_iPLGetHit);
	SetDocumented(itemType, "PLLight", "number", "Light bonus", &Item::_iPLLight);
	SetDocumented(itemType, "splLvlAdd", "number", "Spell level bonus", &Item::_iSplLvlAdd);
	SetDocumented(itemType, "request", "boolean", "Request flag", &Item::_iRequest);
	SetDocumented(itemType, "uid", "number", "Unique item ID", &Item::_iUid);
	SetDocumented(itemType, "fMinDam", "number", "Fire minimum damage", &Item::_iFMinDam);
	SetDocumented(itemType, "fMaxDam", "number", "Fire maximum damage", &Item::_iFMaxDam);
	SetDocumented(itemType, "lMinDam", "number", "Lightning minimum damage", &Item::_iLMinDam);
	SetDocumented(itemType, "lMaxDam", "number", "Lightning maximum damage", &Item::_iLMaxDam);
	SetDocumented(itemType, "PLEnAc", "number", "Damage target AC bonus", &Item::_iPLEnAc);
	SetDocumented(itemType, "prePower", "ItemEffect", "Prefix power", [](Item &i) { return i._iPrePower; }, [](Item &i, int val) { i._iPrePower = static_cast<item_effect_type>(val); });
	SetDocumented(itemType, "sufPower", "ItemEffect", "Suffix power", [](Item &i) { return i._iSufPower; }, [](Item &i, int val) { i._iSufPower = static_cast<item_effect_type>(val); });
	SetDocumented(itemType, "vAdd1", "number", "Value addition 1", &Item::_iVAdd1);
	SetDocumented(itemType, "vMult1", "number", "Value multiplier 1", &Item::_iVMult1);
	SetDocumented(itemType, "vAdd2", "number", "Value addition 2", &Item::_iVAdd2);
	SetDocumented(itemType, "vMult2", "number", "Value multiplier 2", &Item::_iVMult2);
	SetDocumented(itemType, "minStr", "number", "Minimum strength required", &Item::_iMinStr);
	SetDocumented(itemType, "minMag", "number", "Minimum magic required", &Item::_iMinMag);
	SetDocumented(itemType, "minDex", "number", "Minimum dexterity required", &Item::_iMinDex);
	SetDocumented(itemType, "statFlag", "boolean", "Equippable flag", &Item::_iStatFlag);
	SetDocumented(itemType, "damAcFlags", "ItemSpecialEffectHf", "Secondary special effect flags", [](Item &i) { return i._iDamAcFlags; }, [](Item &i, int val) { i._iDamAcFlags = static_cast<ItemSpecialEffectHf>(val); });
	SetDocumented(itemType, "buff", "number", "Secondary creation flags", &Item::dwBuff);

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
