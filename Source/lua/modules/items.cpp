#include "lua/modules/items.hpp"

#include <string_view>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "items.h"
#include "lua/metadoc.hpp"
#include "player.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

void InitItemUserType(sol::state_view &lua)
{
	// Create a new usertype for Item with no constructor
	sol::usertype<Item> itemType = lua.new_usertype<Item>(sol::no_constructor);

	// Member variables
	LuaSetDocProperty(itemType, "seed", "number", "Randomly generated identifier", &Item::_iSeed);
	LuaSetDocProperty(itemType, "createInfo", "number", "Creation flags", &Item::_iCreateInfo);
	LuaSetDocProperty(itemType, "type", "ItemType", "Item type", &Item::_itype);
	LuaSetDocProperty(itemType, "animFlag", "boolean", "Animation flag", &Item::_iAnimFlag);
	LuaSetDocProperty(itemType, "position", "Point", "Item world position", &Item::position);
	// TODO: Add AnimationInfo usertype
	// LuaSetDocProperty(itemType, "animInfo", "AnimationInfo", "Animation information", &Item::AnimInfo);
	LuaSetDocProperty(itemType, "delFlag", "boolean", "Deletion flag", &Item::_iDelFlag);
	LuaSetDocProperty(itemType, "selectionRegion", "number", "Selection region", &Item::selectionRegion);
	LuaSetDocProperty(itemType, "postDraw", "boolean", "Post-draw flag", &Item::_iPostDraw);
	LuaSetDocProperty(itemType, "identified", "boolean", "Identified flag", &Item::_iIdentified);
	LuaSetDocProperty(itemType, "magical", "number", "Item quality", &Item::_iMagical);
	LuaSetDocProperty(itemType, "name", "string", "Item name", [](const Item &i) { return std::string_view(i._iName); }, [](Item &i, std::string_view val) { CopyUtf8(i._iName, val, sizeof(i._iName)); });
	LuaSetDocProperty(itemType, "iName", "string", "Identified item name", [](const Item &i) { return std::string_view(i._iIName); }, [](Item &i, std::string_view val) { CopyUtf8(i._iIName, val, sizeof(i._iIName)); });
	LuaSetDocProperty(itemType, "loc", "ItemEquipType", "Equipment location", &Item::_iLoc);
	LuaSetDocProperty(itemType, "class", "ItemClass", "Item class", &Item::_iClass);
	LuaSetDocProperty(itemType, "curs", "number", "Cursor index", &Item::_iCurs);
	LuaSetDocProperty(itemType, "value", "number", "Item value", &Item::_ivalue);
	LuaSetDocProperty(itemType, "ivalue", "number", "Identified item value", &Item::_iIvalue);
	LuaSetDocProperty(itemType, "minDam", "number", "Minimum damage", &Item::_iMinDam);
	LuaSetDocProperty(itemType, "maxDam", "number", "Maximum damage", &Item::_iMaxDam);
	LuaSetDocProperty(itemType, "AC", "number", "Armor class", &Item::_iAC);
	LuaSetDocProperty(itemType, "flags", "ItemSpecialEffect", "Special effect flags", &Item::_iFlags);
	LuaSetDocProperty(itemType, "miscId", "ItemMiscID", "Miscellaneous ID", &Item::_iMiscId);
	LuaSetDocProperty(itemType, "spell", "SpellID", "Spell", &Item::_iSpell);
	LuaSetDocProperty(itemType, "IDidx", "ItemIndex", "Base item index", &Item::IDidx);
	LuaSetDocProperty(itemType, "charges", "number", "Number of charges", &Item::_iCharges);
	LuaSetDocProperty(itemType, "maxCharges", "number", "Maximum charges", &Item::_iMaxCharges);
	LuaSetDocProperty(itemType, "durability", "number", "Durability", &Item::_iDurability);
	LuaSetDocProperty(itemType, "maxDur", "number", "Maximum durability", &Item::_iMaxDur);
	LuaSetDocProperty(itemType, "PLDam", "number", "Damage % bonus", &Item::_iPLDam);
	LuaSetDocProperty(itemType, "PLToHit", "number", "Chance to hit bonus", &Item::_iPLToHit);
	LuaSetDocProperty(itemType, "PLAC", "number", "Armor class % bonus", &Item::_iPLAC);
	LuaSetDocProperty(itemType, "PLStr", "number", "Strength bonus", &Item::_iPLStr);
	LuaSetDocProperty(itemType, "PLMag", "number", "Magic bonus", &Item::_iPLMag);
	LuaSetDocProperty(itemType, "PLDex", "number", "Dexterity bonus", &Item::_iPLDex);
	LuaSetDocProperty(itemType, "PLVit", "number", "Vitality bonus", &Item::_iPLVit);
	LuaSetDocProperty(itemType, "PLFR", "number", "Fire resistance bonus", &Item::_iPLFR);
	LuaSetDocProperty(itemType, "PLLR", "number", "Lightning resistance bonus", &Item::_iPLLR);
	LuaSetDocProperty(itemType, "PLMR", "number", "Magic resistance bonus", &Item::_iPLMR);
	LuaSetDocProperty(itemType, "PLMana", "number", "Mana bonus", &Item::_iPLMana);
	LuaSetDocProperty(itemType, "PLHP", "number", "Life bonus", &Item::_iPLHP);
	LuaSetDocProperty(itemType, "PLDamMod", "number", "Damage modifier bonus", &Item::_iPLDamMod);
	LuaSetDocProperty(itemType, "PLGetHit", "number", "Damage from enemies bonus", &Item::_iPLGetHit);
	LuaSetDocProperty(itemType, "PLLight", "number", "Light bonus", &Item::_iPLLight);
	LuaSetDocProperty(itemType, "splLvlAdd", "number", "Spell level bonus", &Item::_iSplLvlAdd);
	LuaSetDocProperty(itemType, "request", "boolean", "Request flag", &Item::_iRequest);
	LuaSetDocProperty(itemType, "uid", "number", "Unique item ID", &Item::_iUid);
	LuaSetDocProperty(itemType, "fMinDam", "number", "Fire minimum damage", &Item::_iFMinDam);
	LuaSetDocProperty(itemType, "fMaxDam", "number", "Fire maximum damage", &Item::_iFMaxDam);
	LuaSetDocProperty(itemType, "lMinDam", "number", "Lightning minimum damage", &Item::_iLMinDam);
	LuaSetDocProperty(itemType, "lMaxDam", "number", "Lightning maximum damage", &Item::_iLMaxDam);
	LuaSetDocProperty(itemType, "PLEnAc", "number", "Damage target AC bonus", &Item::_iPLEnAc);
	LuaSetDocProperty(itemType, "prePower", "ItemEffectType", "Prefix power", &Item::_iPrePower);
	LuaSetDocProperty(itemType, "sufPower", "ItemEffectType", "Suffix power", &Item::_iSufPower);
	LuaSetDocProperty(itemType, "vAdd1", "number", "Value addition 1", &Item::_iVAdd1);
	LuaSetDocProperty(itemType, "vMult1", "number", "Value multiplier 1", &Item::_iVMult1);
	LuaSetDocProperty(itemType, "vAdd2", "number", "Value addition 2", &Item::_iVAdd2);
	LuaSetDocProperty(itemType, "vMult2", "number", "Value multiplier 2", &Item::_iVMult2);
	LuaSetDocProperty(itemType, "minStr", "number", "Minimum strength required", &Item::_iMinStr);
	LuaSetDocProperty(itemType, "minMag", "number", "Minimum magic required", &Item::_iMinMag);
	LuaSetDocProperty(itemType, "minDex", "number", "Minimum dexterity required", &Item::_iMinDex);
	LuaSetDocProperty(itemType, "statFlag", "boolean", "Equippable flag", &Item::_iStatFlag);
	LuaSetDocProperty(itemType, "damAcFlags", "ItemSpecialEffectHf", "Secondary special effect flags", &Item::_iDamAcFlags);
	LuaSetDocProperty(itemType, "buff", "number", "Secondary creation flags", &Item::dwBuff);

	// Member functions
	LuaSetDocFn(itemType, "pop", "() -> Item", "Clears this item and returns the old value", &Item::pop);
	LuaSetDocFn(itemType, "clear", "() -> void", "Resets the item", &Item::clear);
	LuaSetDocFn(itemType, "isEmpty", "() -> boolean", "Checks whether this item is empty", &Item::isEmpty);
	LuaSetDocFn(itemType, "isEquipment", "() -> boolean", "Checks if item is equipment", &Item::isEquipment);
	LuaSetDocFn(itemType, "isWeapon", "() -> boolean", "Checks if item is a weapon", &Item::isWeapon);
	LuaSetDocFn(itemType, "isArmor", "() -> boolean", "Checks if item is armor", &Item::isArmor);
	LuaSetDocFn(itemType, "isGold", "() -> boolean", "Checks if item is gold", &Item::isGold);
	LuaSetDocFn(itemType, "isHelm", "() -> boolean", "Checks if item is a helm", &Item::isHelm);
	LuaSetDocFn(itemType, "isShield", "() -> boolean", "Checks if item is a shield", &Item::isShield);
	LuaSetDocFn(itemType, "isJewelry", "() -> boolean", "Checks if item is jewelry", &Item::isJewelry);
	LuaSetDocFn(itemType, "isScroll", "() -> boolean", "Checks if item is a scroll", &Item::isScroll);
	LuaSetDocFn(itemType, "isScrollOf", "(spell: SpellID) -> boolean", "Checks if item is a scroll of a given spell", &Item::isScrollOf);
	LuaSetDocFn(itemType, "isRune", "() -> boolean", "Checks if item is a rune", &Item::isRune);
	LuaSetDocFn(itemType, "isRuneOf", "(spell: SpellID) -> boolean", "Checks if item is a rune of a given spell", &Item::isRuneOf);
	LuaSetDocFn(itemType, "isUsable", "() -> boolean", "Checks if item is usable", &Item::isUsable);
	LuaSetDocFn(itemType, "keyAttributesMatch", "(seed: number, idx: number, createInfo: number) -> boolean", "Checks if key attributes match", &Item::keyAttributesMatch);
	LuaSetDocFn(itemType, "getTextColor", "() -> UiFlags", "Gets the text color", &Item::getTextColor);
	LuaSetDocFn(itemType, "getTextColorWithStatCheck", "() -> UiFlags", "Gets the text color with stat check", &Item::getTextColorWithStatCheck);
	LuaSetDocFn(itemType, "setNewAnimation", "(showAnimation: boolean) -> void", "Sets the new animation", &Item::setNewAnimation);
	LuaSetDocFn(itemType, "updateRequiredStatsCacheForPlayer", "(player: Player) -> void", "Updates the required stats cache", &Item::updateRequiredStatsCacheForPlayer);
	LuaSetDocFn(itemType, "getName", "() -> string", "Gets the translated item name", &Item::getName);
}

void RegisterItemTypeEnum(sol::state_view &lua)
{
	lua.new_enum<ItemType>("ItemType",
	    {
	        { "Misc", ItemType::Misc },
	        { "Sword", ItemType::Sword },
	        { "Axe", ItemType::Axe },
	        { "Bow", ItemType::Bow },
	        { "Mace", ItemType::Mace },
	        { "Shield", ItemType::Shield },
	        { "LightArmor", ItemType::LightArmor },
	        { "Helm", ItemType::Helm },
	        { "MediumArmor", ItemType::MediumArmor },
	        { "HeavyArmor", ItemType::HeavyArmor },
	        { "Staff", ItemType::Staff },
	        { "Gold", ItemType::Gold },
	        { "Ring", ItemType::Ring },
	        { "Amulet", ItemType::Amulet },
	        { "None", ItemType::None },
	    });
}

void RegisterItemEquipTypeEnum(sol::state_view &lua)
{
	lua.new_enum<item_equip_type>("ItemEquipType",
	    {
	        { "None", ILOC_NONE },
	        { "OneHand", ILOC_ONEHAND },
	        { "TwoHand", ILOC_TWOHAND },
	        { "Armor", ILOC_ARMOR },
	        { "Helm", ILOC_HELM },
	        { "Ring", ILOC_RING },
	        { "Amulet", ILOC_AMULET },
	        { "Unequipable", ILOC_UNEQUIPABLE },
	        { "Belt", ILOC_BELT },
	        { "Invalid", ILOC_INVALID },
	    });
}

void RegisterItemClassEnum(sol::state_view &lua)
{
	lua.new_enum<item_class>("ItemClass",
	    {
	        { "None", ICLASS_NONE },
	        { "Weapon", ICLASS_WEAPON },
	        { "Armor", ICLASS_ARMOR },
	        { "Misc", ICLASS_MISC },
	        { "Gold", ICLASS_GOLD },
	        { "Quest", ICLASS_QUEST },
	    });
}

void RegisterItemSpecialEffectEnum(sol::state_view &lua)
{
	lua.new_enum<ItemSpecialEffect>("ItemSpecialEffect",
	    {
	        { "None", ItemSpecialEffect::None },
	        { "RandomStealLife", ItemSpecialEffect::RandomStealLife },
	        { "RandomArrowVelocity", ItemSpecialEffect::RandomArrowVelocity },
	        { "FireArrows", ItemSpecialEffect::FireArrows },
	        { "FireDamage", ItemSpecialEffect::FireDamage },
	        { "LightningDamage", ItemSpecialEffect::LightningDamage },
	        { "DrainLife", ItemSpecialEffect::DrainLife },
	        { "MultipleArrows", ItemSpecialEffect::MultipleArrows },
	        { "Knockback", ItemSpecialEffect::Knockback },
	        { "StealMana3", ItemSpecialEffect::StealMana3 },
	        { "StealMana5", ItemSpecialEffect::StealMana5 },
	        { "StealLife3", ItemSpecialEffect::StealLife3 },
	        { "StealLife5", ItemSpecialEffect::StealLife5 },
	        { "QuickAttack", ItemSpecialEffect::QuickAttack },
	        { "FastAttack", ItemSpecialEffect::FastAttack },
	        { "FasterAttack", ItemSpecialEffect::FasterAttack },
	        { "FastestAttack", ItemSpecialEffect::FastestAttack },
	        { "FastHitRecovery", ItemSpecialEffect::FastHitRecovery },
	        { "FasterHitRecovery", ItemSpecialEffect::FasterHitRecovery },
	        { "FastestHitRecovery", ItemSpecialEffect::FastestHitRecovery },
	        { "FastBlock", ItemSpecialEffect::FastBlock },
	        { "LightningArrows", ItemSpecialEffect::LightningArrows },
	        { "Thorns", ItemSpecialEffect::Thorns },
	        { "NoMana", ItemSpecialEffect::NoMana },
	        { "HalfTrapDamage", ItemSpecialEffect::HalfTrapDamage },
	        { "TripleDemonDamage", ItemSpecialEffect::TripleDemonDamage },
	        { "ZeroResistance", ItemSpecialEffect::ZeroResistance },
	    });
}

void RegisterItemMiscIDEnum(sol::state_view &lua)
{
	lua.new_enum<item_misc_id>("ItemMiscID",
	    {
	        { "None", IMISC_NONE },
	        { "FullHeal", IMISC_FULLHEAL },
	        { "Heal", IMISC_HEAL },
	        { "Mana", IMISC_MANA },
	        { "FullMana", IMISC_FULLMANA },
	        { "ElixirStr", IMISC_ELIXSTR },
	        { "ElixirMag", IMISC_ELIXMAG },
	        { "ElixirDex", IMISC_ELIXDEX },
	        { "ElixirVit", IMISC_ELIXVIT },
	        { "Rejuv", IMISC_REJUV },
	        { "FullRejuv", IMISC_FULLREJUV },
	        { "Scroll", IMISC_SCROLL },
	        { "ScrollT", IMISC_SCROLLT },
	        { "Staff", IMISC_STAFF },
	        { "Book", IMISC_BOOK },
	        { "Ring", IMISC_RING },
	        { "Amulet", IMISC_AMULET },
	        { "Unique", IMISC_UNIQUE },
	    });
}

void RegisterSpellIDEnum(sol::state_view &lua)
{
	lua.new_enum<SpellID>("SpellID",
	    {
	        { "Null", SpellID::Null },
	        { "Firebolt", SpellID::Firebolt },
	        { "Healing", SpellID::Healing },
	        { "Lightning", SpellID::Lightning },
	        { "Flash", SpellID::Flash },
	        { "Identify", SpellID::Identify },
	        { "FireWall", SpellID::FireWall },
	        { "TownPortal", SpellID::TownPortal },
	        { "StoneCurse", SpellID::StoneCurse },
	        { "Infravision", SpellID::Infravision },
	        { "Phasing", SpellID::Phasing },
	        { "ManaShield", SpellID::ManaShield },
	        { "Fireball", SpellID::Fireball },
	        { "Guardian", SpellID::Guardian },
	        { "ChainLightning", SpellID::ChainLightning },
	        { "FlameWave", SpellID::FlameWave },
	        { "DoomSerpents", SpellID::DoomSerpents },
	        { "BloodRitual", SpellID::BloodRitual },
	        { "Nova", SpellID::Nova },
	        { "Invisibility", SpellID::Invisibility },
	        { "Inferno", SpellID::Inferno },
	        { "Golem", SpellID::Golem },
	        { "Rage", SpellID::Rage },
	        { "Teleport", SpellID::Teleport },
	        { "Apocalypse", SpellID::Apocalypse },
	        { "Etherealize", SpellID::Etherealize },
	        { "ItemRepair", SpellID::ItemRepair },
	        { "StaffRecharge", SpellID::StaffRecharge },
	        { "TrapDisarm", SpellID::TrapDisarm },
	        { "Elemental", SpellID::Elemental },
	        { "ChargedBolt", SpellID::ChargedBolt },
	        { "HolyBolt", SpellID::HolyBolt },
	        { "Resurrect", SpellID::Resurrect },
	        { "Telekinesis", SpellID::Telekinesis },
	        { "HealOther", SpellID::HealOther },
	        { "BloodStar", SpellID::BloodStar },
	        { "BoneSpirit", SpellID::BoneSpirit },
	        { "Mana", SpellID::Mana },
	        { "Magi", SpellID::Magi },
	        { "Jester", SpellID::Jester },
	        { "LightningWall", SpellID::LightningWall },
	        { "Immolation", SpellID::Immolation },
	        { "Warp", SpellID::Warp },
	        { "Reflect", SpellID::Reflect },
	        { "Berserk", SpellID::Berserk },
	        { "RingOfFire", SpellID::RingOfFire },
	        { "Search", SpellID::Search },
	        { "RuneOfFire", SpellID::RuneOfFire },
	        { "RuneOfLight", SpellID::RuneOfLight },
	        { "RuneOfNova", SpellID::RuneOfNova },
	        { "RuneOfImmolation", SpellID::RuneOfImmolation },
	        { "RuneOfStone", SpellID::RuneOfStone },
	        { "Invalid", SpellID::Invalid },
	    });
}

void RegisterItemIndexEnum(sol::state_view &lua)
{
	lua.new_enum<_item_indexes>("ItemIndex",
	    {
	        { "Gold", IDI_GOLD },
	        { "Warrior", IDI_WARRIOR },
	        { "WarriorShield", IDI_WARRSHLD },
	        { "WarriorClub", IDI_WARRCLUB },
	        { "Rogue", IDI_ROGUE },
	        { "Sorcerer", IDI_SORCERER },
	        { "Cleaver", IDI_CLEAVER },
	        { "FirstQuest", IDI_FIRSTQUEST },
	        { "SkeletonKingCrown", IDI_SKCROWN },
	        { "InfravisionRing", IDI_INFRARING },
	        { "Rock", IDI_ROCK },
	        { "OpticAmulet", IDI_OPTAMULET },
	        { "TruthRing", IDI_TRING },
	        { "Banner", IDI_BANNER },
	        { "HarlequinCrest", IDI_HARCREST },
	        { "SteelVeil", IDI_STEELVEIL },
	        { "GoldenElixir", IDI_GLDNELIX },
	        { "Anvil", IDI_ANVIL },
	        { "Mushroom", IDI_MUSHROOM },
	        { "Brain", IDI_BRAIN },
	        { "FungalTome", IDI_FUNGALTM },
	        { "SpectralElixir", IDI_SPECELIX },
	        { "BloodStone", IDI_BLDSTONE },
	        { "MapOfDoom", IDI_MAPOFDOOM },
	        { "LastQuest", IDI_LASTQUEST },
	        { "Ear", IDI_EAR },
	        { "Heal", IDI_HEAL },
	        { "Mana", IDI_MANA },
	        { "Identify", IDI_IDENTIFY },
	        { "Portal", IDI_PORTAL },
	        { "ArmorOfValor", IDI_ARMOFVAL },
	        { "FullHeal", IDI_FULLHEAL },
	        { "FullMana", IDI_FULLMANA },
	        { "Griswold", IDI_GRISWOLD },
	        { "Lightforge", IDI_LGTFORGE },
	        { "LazarusStaff", IDI_LAZSTAFF },
	        { "Resurrect", IDI_RESURRECT },
	        { "Oil", IDI_OIL },
	        { "ShortStaff", IDI_SHORTSTAFF },
	        { "BardSword", IDI_BARDSWORD },
	        { "BardDagger", IDI_BARDDAGGER },
	        { "RuneBomb", IDI_RUNEBOMB },
	        { "Theodore", IDI_THEODORE },
	        { "Auric", IDI_AURIC },
	        { "Note1", IDI_NOTE1 },
	        { "Note2", IDI_NOTE2 },
	        { "Note3", IDI_NOTE3 },
	        { "FullNote", IDI_FULLNOTE },
	        { "BrownSuit", IDI_BROWNSUIT },
	        { "GreySuit", IDI_GREYSUIT },
	        { "Book1", IDI_BOOK1 },
	        { "Book2", IDI_BOOK2 },
	        { "Book3", IDI_BOOK3 },
	        { "Book4", IDI_BOOK4 },
	        { "Barbarian", IDI_BARBARIAN },
	        { "ShortBattleBow", IDI_SHORT_BATTLE_BOW },
	        { "RuneOfStone", IDI_RUNEOFSTONE },
	        { "SorcererDiablo", IDI_SORCERER_DIABLO },
	        { "ArenaPotion", IDI_ARENAPOT },
	        { "Last", IDI_LAST },
	        { "None", IDI_NONE },
	    });
}

void RegisterItemEffectTypeEnum(sol::state_view &lua)
{
	lua.new_enum<item_effect_type>("ItemEffectType",
	    {
	        { "ToHit", IPL_TOHIT },
	        { "ToHitCurse", IPL_TOHIT_CURSE },
	        { "DamagePercent", IPL_DAMP },
	        { "DamagePercentCurse", IPL_DAMP_CURSE },
	        { "ToHitDamagePercent", IPL_TOHIT_DAMP },
	        { "ToHitDamagePercentCurse", IPL_TOHIT_DAMP_CURSE },
	        { "ArmorClassPercent", IPL_ACP },
	        { "ArmorClassPercentCurse", IPL_ACP_CURSE },
	        { "FireResist", IPL_FIRERES },
	        { "LightningResist", IPL_LIGHTRES },
	        { "MagicResist", IPL_MAGICRES },
	        { "AllResist", IPL_ALLRES },
	        { "SpellLevelAdd", IPL_SPLLVLADD },
	        { "Charges", IPL_CHARGES },
	        { "FireDamage", IPL_FIREDAM },
	        { "LightningDamage", IPL_LIGHTDAM },
	        { "Strength", IPL_STR },
	        { "StrengthCurse", IPL_STR_CURSE },
	        { "Magic", IPL_MAG },
	        { "MagicCurse", IPL_MAG_CURSE },
	        { "Dexterity", IPL_DEX },
	        { "DexterityCurse", IPL_DEX_CURSE },
	        { "Vitality", IPL_VIT },
	        { "VitalityCurse", IPL_VIT_CURSE },
	        { "Attributes", IPL_ATTRIBS },
	        { "AttributesCurse", IPL_ATTRIBS_CURSE },
	        { "GetHitCurse", IPL_GETHIT_CURSE },
	        { "GetHit", IPL_GETHIT },
	        { "Life", IPL_LIFE },
	        { "LifeCurse", IPL_LIFE_CURSE },
	        { "Mana", IPL_MANA },
	        { "ManaCurse", IPL_MANA_CURSE },
	        { "Durability", IPL_DUR },
	        { "DurabilityCurse", IPL_DUR_CURSE },
	        { "Indestructible", IPL_INDESTRUCTIBLE },
	        { "Light", IPL_LIGHT },
	        { "LightCurse", IPL_LIGHT_CURSE },
	        { "MultipleArrows", IPL_MULT_ARROWS },
	        { "FireArrows", IPL_FIRE_ARROWS },
	        { "LightningArrows", IPL_LIGHT_ARROWS },
	        { "Thorns", IPL_THORNS },
	        { "NoMana", IPL_NOMANA },
	        { "Fireball", IPL_FIREBALL },
	        { "AbsorbHalfTrap", IPL_ABSHALFTRAP },
	        { "Knockback", IPL_KNOCKBACK },
	        { "StealMana", IPL_STEALMANA },
	        { "StealLife", IPL_STEALLIFE },
	        { "TargetArmorClass", IPL_TARGAC },
	        { "FastAttack", IPL_FASTATTACK },
	        { "FastRecover", IPL_FASTRECOVER },
	        { "FastBlock", IPL_FASTBLOCK },
	        { "DamageModifier", IPL_DAMMOD },
	        { "RandomArrowVelocity", IPL_RNDARROWVEL },
	        { "SetDamage", IPL_SETDAM },
	        { "SetDurability", IPL_SETDUR },
	        { "NoMinimumStrength", IPL_NOMINSTR },
	        { "Spell", IPL_SPELL },
	        { "OneHand", IPL_ONEHAND },
	        { "3xDamageVsDemons", IPL_3XDAMVDEM },
	        { "AllResistZero", IPL_ALLRESZERO },
	        { "DrainLife", IPL_DRAINLIFE },
	        { "RandomStealLife", IPL_RNDSTEALLIFE },
	        { "SetArmorClass", IPL_SETAC },
	        { "AddArmorClassLife", IPL_ADDACLIFE },
	        { "AddManaArmorClass", IPL_ADDMANAAC },
	        { "ArmorClassCurse", IPL_AC_CURSE },
	        { "LastDiablo", IPL_LASTDIABLO },
	        { "FireResistCurse", IPL_FIRERES_CURSE },
	        { "LightResistCurse", IPL_LIGHTRES_CURSE },
	        { "MagicResistCurse", IPL_MAGICRES_CURSE },
	        { "Devastation", IPL_DEVASTATION },
	        { "Decay", IPL_DECAY },
	        { "Peril", IPL_PERIL },
	        { "Jesters", IPL_JESTERS },
	        { "Crystalline", IPL_CRYSTALLINE },
	        { "Doppelganger", IPL_DOPPELGANGER },
	        { "ArmorClassDemon", IPL_ACDEMON },
	        { "ArmorClassUndead", IPL_ACUNDEAD },
	        { "ManaToLife", IPL_MANATOLIFE },
	        { "LifeToMana", IPL_LIFETOMANA },
	        { "Invalid", IPL_INVALID },
	    });
}

void RegisterItemSpecialEffectHfEnum(sol::state_view &lua)
{
	lua.new_enum<ItemSpecialEffectHf>("ItemSpecialEffectHf",
	    {
	        { "None", ItemSpecialEffectHf::None },
	        { "Devastation", ItemSpecialEffectHf::Devastation },
	        { "Decay", ItemSpecialEffectHf::Decay },
	        { "Peril", ItemSpecialEffectHf::Peril },
	        { "Jesters", ItemSpecialEffectHf::Jesters },
	        { "Doppelganger", ItemSpecialEffectHf::Doppelganger },
	        { "ACAgainstDemons", ItemSpecialEffectHf::ACAgainstDemons },
	        { "ACAgainstUndead", ItemSpecialEffectHf::ACAgainstUndead },
	    });
}

void AddUniqueItemPower(UniqueItem &uniqueItem, const std::optional<std::string_view> power, const std::optional<int> power_value1, const std::optional<int> power_value2)
{
	if (!power.has_value()) {
		return;
	}

	ItemPower &itemPower = uniqueItem.powers[uniqueItem.UINumPL++];

	const auto powerResult = ParseItemEffectType(power.value());
	if (!powerResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Item Failed"), fmt::format(fmt::runtime(_("Failed to parse power \"{}\": {}")), power.value(), powerResult.error()));
	}

	itemPower.type = powerResult.value();

	if (power_value1.has_value()) {
		itemPower.param1 = power_value1.value();
	}

	if (power_value2.has_value()) {
		itemPower.param2 = power_value2.value();
	}
}

void AddUniqueItemData(const std::string_view name, const int32_t mappingId, const std::string_view cursorGraphic, const std::string_view uniqueBaseItem, const int8_t minLevel, const int value, const std::optional<std::string_view> power0, const std::optional<int> power0_value1, const std::optional<int> power0_value2, const std::optional<std::string_view> power1, const std::optional<int> power1_value1, const std::optional<int> power1_value2, const std::optional<std::string_view> power2, const std::optional<int> power2_value1, const std::optional<int> power2_value2, const std::optional<std::string_view> power3, const std::optional<int> power3_value1, const std::optional<int> power3_value2, const std::optional<std::string_view> power4, const std::optional<int> power4_value1, const std::optional<int> power4_value2, const std::optional<std::string_view> power5, const std::optional<int> power5_value1, const std::optional<int> power5_value2)
{
	UniqueItem uniqueItem;

	uniqueItem.UIName = name;
	uniqueItem.mappingId = mappingId;

	const auto cursorGraphicResult = ParseItemCursorGraphic(cursorGraphic);
	if (!cursorGraphicResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Item Failed"), fmt::format(fmt::runtime(_("Failed to parse cursor graphic \"{}\": {}")), cursorGraphic, cursorGraphicResult.error()));
	}

	uniqueItem.UICurs = cursorGraphicResult.value();

	const auto uniqueBaseItemResult = ParseUniqueBaseItem(uniqueBaseItem);
	if (!uniqueBaseItemResult.has_value()) {
		DisplayFatalErrorAndExit(_("Adding Unique Item Failed"), fmt::format(fmt::runtime(_("Failed to parse unique base item \"{}\": {}")), uniqueBaseItem, uniqueBaseItemResult.error()));
	}

	uniqueItem.UIItemId = uniqueBaseItemResult.value();

	uniqueItem.UIMinLvl = minLevel;
	uniqueItem.UIValue = value;

	uniqueItem.UINumPL = 0;
	AddUniqueItemPower(uniqueItem, power0, power0_value1, power0_value2);
	AddUniqueItemPower(uniqueItem, power1, power1_value1, power1_value2);
	AddUniqueItemPower(uniqueItem, power2, power2_value1, power2_value2);
	AddUniqueItemPower(uniqueItem, power3, power3_value1, power3_value2);
	AddUniqueItemPower(uniqueItem, power4, power4_value1, power4_value2);
	AddUniqueItemPower(uniqueItem, power5, power5_value1, power5_value2);

	const auto [it, inserted] = UniqueItemMappingIdsToIndices.emplace(mappingId, static_cast<int32_t>(UniqueItems.size()));
	if (!inserted) {
		DisplayFatalErrorAndExit(_("Adding Unique Item Failed"), fmt::format(fmt::runtime(_("A unique item already exists for mapping ID {}.")), mappingId));
	}

	UniqueItems.push_back(std::move(uniqueItem));
}

} // namespace

sol::table LuaItemModule(sol::state_view &lua)
{
	InitItemUserType(lua);
	RegisterItemTypeEnum(lua);
	RegisterItemEquipTypeEnum(lua);
	RegisterItemClassEnum(lua);
	RegisterItemSpecialEffectEnum(lua);
	RegisterItemMiscIDEnum(lua);
	RegisterSpellIDEnum(lua);
	RegisterItemIndexEnum(lua);
	RegisterItemEffectTypeEnum(lua);
	RegisterItemSpecialEffectHfEnum(lua);

	sol::table table = lua.create_table();

	LuaSetDocFn(table, "addUniqueItemData", "(name: string, mappingId: number, cursorGraphic: string, uniqueBaseItem: string, minLevel: number, value: number, power0: string = nil, power0_value1: number = nil, power0_value2: number = nil, power1: string = nil, power1_value1: number = nil, power1_value2: number = nil, power2: string = nil, power2_value1: number = nil, power2_value2: number = nil, power3: string = nil, power3_value1: number = nil, power3_value2: number = nil, power4: string = nil, power4_value1: number = nil, power4_value2: number = nil, power5: string = nil, power5_value1: number = nil, power5_value2: number = nil)", AddUniqueItemData);

	return table;
}

} // namespace devilution
