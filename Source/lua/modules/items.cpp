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
	// TODO: Add AnimationInfo usertype
	// SetDocumented(itemType, "animInfo", "AnimationInfo", "Animation information", [](const Item &i) { return i.AnimInfo; }, [](Item &i, AnimationInfo val) { i.AnimInfo = val; });
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
	SetDocumented(itemType, "miscId", "ItemMiscID", "Miscellaneous ID", [](const Item &i) { return i._iMiscId; }, [](Item &i, int val) { i._iMiscId = static_cast<item_misc_id>(val); });
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
	SetDocumented(itemType, "prePower", "ItemEffectType", "Prefix power", [](const Item &i) { return i._iPrePower; }, [](Item &i, int val) { i._iPrePower = static_cast<item_effect_type>(val); });
	SetDocumented(itemType, "sufPower", "ItemEffectType", "Suffix power", [](const Item &i) { return i._iSufPower; }, [](Item &i, int val) { i._iSufPower = static_cast<item_effect_type>(val); });
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

	return table;
}

} // namespace devilution
