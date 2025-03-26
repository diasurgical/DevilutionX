#include "lua/modules/player.hpp"

#include <optional>

#include <sol/sol.hpp>

#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "player.h"

namespace devilution {

namespace {

void InitPlayerUserType(sol::state_view &lua)
{
	sol::usertype<Player> playerType = lua.new_usertype<Player>(sol::no_constructor);

	// Member variables
	SetDocumented(playerType, "name", "string", "Player's name", [](const Player &p) { return std::string(p._pName); }, [](Player &p, const std::string &val) { strncpy(p._pName, val.c_str(), PlayerNameLength); });
	SetDocumented(playerType, "invBody", "Item[]", "Equipped items", [](const Player &p) { return sol::as_table(std::vector<Item>(std::begin(p.InvBody), std::end(p.InvBody))); }, [](Player &p, const std::vector<Item> &items) { std::copy_n(items.begin(), std::min(items.size(), size_t(NUM_INVLOC)), std::begin(p.InvBody)); });
	SetDocumented(playerType, "invList", "Item[]", "Inventory items", [](const Player &p) { return sol::as_table(std::vector<Item>(std::begin(p.InvList), std::end(p.InvList))); }, [](Player &p, const std::vector<Item> &items) { std::copy_n(items.begin(), std::min(items.size(), size_t(InventoryGridCells)), std::begin(p.InvList)); });
	SetDocumented(playerType, "spdList", "Item[]", "Belt items", [](const Player &p) { return sol::as_table(std::vector<Item>(std::begin(p.SpdList), std::end(p.SpdList))); }, [](Player &p, const std::vector<Item> &items) { std::copy_n(items.begin(), std::min(items.size(), size_t(MaxBeltItems)), std::begin(p.SpdList)); });
	SetDocumented(playerType, "holdItem", "Item", "Item currently being held", [](const Player &p) { return p.HoldItem; }, [](Player &p, const Item &item) { p.HoldItem = item; });
	SetDocumented(playerType, "lightId", "integer", "Light ID", [](const Player &p) { return p.lightId; }, [](Player &p, int val) { p.lightId = val; });
	SetDocumented(playerType, "numInv", "integer", "Number of items in inventory", [](const Player &p) { return p._pNumInv; }, [](Player &p, int val) { p._pNumInv = val; });
	SetDocumented(playerType, "strength", "integer", "Current strength", [](const Player &p) { return p._pStrength; }, [](Player &p, int val) { p._pStrength = val; });
	SetDocumented(playerType, "baseStr", "integer", "Base strength", [](const Player &p) { return p._pBaseStr; }, [](Player &p, int val) { p._pBaseStr = val; });
	SetDocumented(playerType, "magic", "integer", "Current magic", [](const Player &p) { return p._pMagic; }, [](Player &p, int val) { p._pMagic = val; });
	SetDocumented(playerType, "baseMag", "integer", "Base magic", [](const Player &p) { return p._pBaseMag; }, [](Player &p, int val) { p._pBaseMag = val; });
	SetDocumented(playerType, "dexterity", "integer", "Current dexterity", [](const Player &p) { return p._pDexterity; }, [](Player &p, int val) { p._pDexterity = val; });
	SetDocumented(playerType, "baseDex", "integer", "Base dexterity", [](const Player &p) { return p._pBaseDex; }, [](Player &p, int val) { p._pBaseDex = val; });
	SetDocumented(playerType, "vitality", "integer", "Current vitality", [](const Player &p) { return p._pVitality; }, [](Player &p, int val) { p._pVitality = val; });
	SetDocumented(playerType, "baseVit", "integer", "Base vitality", [](const Player &p) { return p._pBaseVit; }, [](Player &p, int val) { p._pBaseVit = val; });
	SetDocumented(playerType, "statPts", "integer", "Unallocated stat points", [](const Player &p) { return p._pStatPts; }, [](Player &p, int val) { p._pStatPts = val; });
	SetDocumented(playerType, "damageMod", "integer", "Damage modifier", [](const Player &p) { return p._pDamageMod; }, [](Player &p, int val) { p._pDamageMod = val; });
	SetDocumented(playerType, "hpBase", "integer", "Base HP value", [](const Player &p) { return p._pHPBase; }, [](Player &p, int val) { p._pHPBase = val; });
	SetDocumented(playerType, "maxHpBase", "integer", "Base max HP value", [](const Player &p) { return p._pMaxHPBase; }, [](Player &p, int val) { p._pMaxHPBase = val; });
	SetDocumented(playerType, "hitPoints", "integer", "Current HP", [](const Player &p) { return p._pHitPoints; }, [](Player &p, int val) { p._pHitPoints = val; });
	SetDocumented(playerType, "maxHp", "integer", "Max HP", [](const Player &p) { return p._pMaxHP; }, [](Player &p, int val) { p._pMaxHP = val; });
	SetDocumented(playerType, "hpPercent", "integer", "HP percent (0-80)", [](const Player &p) { return p._pHPPer; }, [](Player &p, int val) { p._pHPPer = val; });
	SetDocumented(playerType, "manaBase", "integer", "Base mana value", [](const Player &p) { return p._pManaBase; }, [](Player &p, int val) { p._pManaBase = val; });
	SetDocumented(playerType, "maxManaBase", "integer", "Base max mana", [](const Player &p) { return p._pMaxManaBase; }, [](Player &p, int val) { p._pMaxManaBase = val; });
	SetDocumented(playerType, "mana", "integer", "Current mana", [](const Player &p) { return p._pMana; }, [](Player &p, int val) { p._pMana = val; });
	SetDocumented(playerType, "maxMana", "integer", "Max mana", [](const Player &p) { return p._pMaxMana; }, [](Player &p, int val) { p._pMaxMana = val; });
	SetDocumented(playerType, "manaPercent", "integer", "Mana percent (0-80)", [](const Player &p) { return p._pManaPer; }, [](Player &p, int val) { p._pManaPer = val; });
	SetDocumented(playerType, "iMinDam", "integer", "Min item damage", [](const Player &p) { return p._pIMinDam; }, [](Player &p, int val) { p._pIMinDam = val; });
	SetDocumented(playerType, "iMaxDam", "integer", "Max item damage", [](const Player &p) { return p._pIMaxDam; }, [](Player &p, int val) { p._pIMaxDam = val; });
	SetDocumented(playerType, "iAC", "integer", "Item armor class", [](const Player &p) { return p._pIAC; }, [](Player &p, int val) { p._pIAC = val; });
	SetDocumented(playerType, "iBonusDam", "integer", "Item bonus damage", [](const Player &p) { return p._pIBonusDam; }, [](Player &p, int val) { p._pIBonusDam = val; });
	SetDocumented(playerType, "iBonusToHit", "integer", "Item bonus to hit", [](const Player &p) { return p._pIBonusToHit; }, [](Player &p, int val) { p._pIBonusToHit = val; });
	SetDocumented(playerType, "iBonusAC", "integer", "Item bonus AC", [](const Player &p) { return p._pIBonusAC; }, [](Player &p, int val) { p._pIBonusAC = val; });
	SetDocumented(playerType, "iBonusDamMod", "integer", "Item damage bonus modifier", [](const Player &p) { return p._pIBonusDamMod; }, [](Player &p, int val) { p._pIBonusDamMod = val; });
	SetDocumented(playerType, "iGetHit", "integer", "Item get-hit penalty", [](const Player &p) { return p._pIGetHit; }, [](Player &p, int val) { p._pIGetHit = val; });
	SetDocumented(playerType, "iEnAc", "integer", "Item enhanced AC", [](const Player &p) { return p._pIEnAc; }, [](Player &p, int val) { p._pIEnAc = val; });
	SetDocumented(playerType, "iFMinDam", "integer", "Fire min damage", [](const Player &p) { return p._pIFMinDam; }, [](Player &p, int val) { p._pIFMinDam = val; });
	SetDocumented(playerType, "iFMaxDam", "integer", "Fire max damage", [](const Player &p) { return p._pIFMaxDam; }, [](Player &p, int val) { p._pIFMaxDam = val; });
	SetDocumented(playerType, "iLMinDam", "integer", "Lightning min damage", [](const Player &p) { return p._pILMinDam; }, [](Player &p, int val) { p._pILMinDam = val; });
	SetDocumented(playerType, "iLMaxDam", "integer", "Lightning max damage", [](const Player &p) { return p._pILMaxDam; }, [](Player &p, int val) { p._pILMaxDam = val; });
	SetDocumented(playerType, "experience", "integer", "Current experience", [](const Player &p) { return p._pExperience; }, [](Player &p, uint32_t val) { p._pExperience = val; });
	SetDocumented(playerType, "mode", "PlayerMode", "Current player mode", [](const Player &p) { return p._pmode; }, [](Player &p, PLR_MODE val) { p._pmode = val; });
	SetDocumented(playerType, "walkpath", "integer[]", "Pathfinding data (internal)", [](const Player &p) { return sol::as_table(std::vector<int8_t>(std::begin(p.walkpath), std::end(p.walkpath))); }, [](Player &p, const std::vector<int8_t> &path) { std::copy_n(path.begin(), std::min(path.size(), size_t(MaxPathLengthPlayer)), std::begin(p.walkpath)); });
	SetDocumented(playerType, "plractive", "boolean", "Whether this player slot is in use", [](const Player &p) { return p.plractive; }, [](Player &p, bool val) { p.plractive = val; });
	// TODO: Add ActionID new_enum
	// SetDocumented(playerType, "destAction", "ActionID", "Pending action ID", [](const Player &p) { return p.destAction; }, [](Player &p, action_id val) { p.destAction = val; });
	SetDocumented(playerType, "destParam1", "integer", "Pending action parameter 1", [](const Player &p) { return p.destParam1; }, [](Player &p, int val) { p.destParam1 = val; });
	SetDocumented(playerType, "destParam2", "integer", "Pending action parameter 2", [](const Player &p) { return p.destParam2; }, [](Player &p, int val) { p.destParam2 = val; });
	SetDocumented(playerType, "destParam3", "integer", "Pending action parameter 3", [](const Player &p) { return p.destParam3; }, [](Player &p, int val) { p.destParam3 = val; });
	SetDocumented(playerType, "destParam4", "integer", "Pending action parameter 4", [](const Player &p) { return p.destParam4; }, [](Player &p, int val) { p.destParam4 = val; });
	SetDocumented(playerType, "gold", "integer", "Amount of gold carried", [](const Player &p) { return p._pGold; }, [](Player &p, int val) { p._pGold = val; });
	// TODO: Add AnimationInfo usertype
	// SetDocumented(playerType, "animInfo", "AnimationInfo", "Current animation info", [](const Player &p) { return p.AnimInfo; }, [](Player &p, const AnimationInfo &val) { p.AnimInfo = val; });
	// TODO: Add OptionalClxSprite usertype
	// SetDocumented(playerType, "previewCelSprite", "OptionalClxSprite", "Temporary preview sprite", [](const Player &p) { return p.previewCelSprite; }, [](Player &p, const OptionalClxSprite &val) { p.previewCelSprite = val; });
	SetDocumented(playerType, "progressToNextGameTickWhenPreviewWasSet", "integer", "Animation tick progress when preview was set", [](const Player &p) { return p.progressToNextGameTickWhenPreviewWasSet; }, [](Player &p, int8_t val) { p.progressToNextGameTickWhenPreviewWasSet = val; });
	SetDocumented(playerType, "iFlags", "ItemSpecialEffect", "Item special effects (bitmask)", [](const Player &p) { return p._pIFlags; }, [](Player &p, ItemSpecialEffect val) { p._pIFlags = val; });
	// TODO: Add PlayerAnimationData usertype
	// TODO: Add PlayerGraphic new_enum
	// SetDocumented(playerType, "animationData", "PlayerAnimationData[]", "Per-graphic animation frame data", [](const Player &p) { return p.AnimationData; }, [](Player &p, const std::array<PlayerAnimationData, enum_size<player_graphic>::value> &val) { p.AnimationData = val; });
	SetDocumented(playerType, "nFrames", "integer", "Total number of animation frames", [](const Player &p) { return p._pNFrames; }, [](Player &p, int8_t val) { p._pNFrames = val; });
	SetDocumented(playerType, "wFrames", "integer", "Walk animation frames", [](const Player &p) { return p._pWFrames; }, [](Player &p, int8_t val) { p._pWFrames = val; });
	SetDocumented(playerType, "aFrames", "integer", "Attack animation frames", [](const Player &p) { return p._pAFrames; }, [](Player &p, int8_t val) { p._pAFrames = val; });
	SetDocumented(playerType, "aFNum", "integer", "Final attack frame", [](const Player &p) { return p._pAFNum; }, [](Player &p, int8_t val) { p._pAFNum = val; });
	SetDocumented(playerType, "sFrames", "integer", "Spell animation frames", [](const Player &p) { return p._pSFrames; }, [](Player &p, int8_t val) { p._pSFrames = val; });
	SetDocumented(playerType, "sFNum", "integer", "Final spell frame", [](const Player &p) { return p._pSFNum; }, [](Player &p, int8_t val) { p._pSFNum = val; });
	SetDocumented(playerType, "hFrames", "integer", "Hit animation frames", [](const Player &p) { return p._pHFrames; }, [](Player &p, int8_t val) { p._pHFrames = val; });
	SetDocumented(playerType, "dFrames", "integer", "Death animation frames", [](const Player &p) { return p._pDFrames; }, [](Player &p, int8_t val) { p._pDFrames = val; });
	SetDocumented(playerType, "bFrames", "integer", "Block animation frames", [](const Player &p) { return p._pBFrames; }, [](Player &p, int8_t val) { p._pBFrames = val; });
	SetDocumented(playerType, "invGrid", "integer[]", "Inventory grid status", [](const Player &p) { return sol::as_table(std::vector<int8_t>(std::begin(p.InvGrid), std::end(p.InvGrid))); }, [](Player &p, const std::vector<int8_t> &grid) { std::copy_n(grid.begin(), std::min(grid.size(), size_t(InventoryGridCells)), std::begin(p.InvGrid)); });
	SetDocumented(playerType, "plrLevel", "integer", "Current dungeon level", [](const Player &p) { return p.plrlevel; }, [](Player &p, uint8_t val) { p.plrlevel = val; });
	SetDocumented(playerType, "plrIsOnSetLevel", "boolean", "Is on a set level?", [](const Player &p) { return p.plrIsOnSetLevel; }, [](Player &p, bool val) { p.plrIsOnSetLevel = val; });
	// TODO: Add ActorPosition usertype
	// SetDocumented(playerType, "position", "ActorPosition", "Player position", [](const Player &p) { return p.position; }, [](Player &p, const ActorPosition &val) { p.position = val; });
	SetDocumented(playerType, "dir", "Direction", "Direction faced", [](const Player &p) { return p._pdir; }, [](Player &p, Direction val) { p._pdir = val; });
	// TODO: Add HeroClass new_enum
	// SetDocumented(playerType, "class", "HeroClass", "Hero class", [](const Player &p) { return p._pClass; }, [](Player &p, HeroClass val) { p._pClass = val; });
	SetDocumented(playerType, "gfxNum", "integer", "Bitmask indicating what variant of the sprite the player is using", [](const Player &p) { return p._pgfxnum; }, [](Player &p, uint8_t val) { p._pgfxnum = val; });
	SetDocumented(playerType, "splLvlAdd", "integer", "Bonus spell levels from equipment", [](const Player &p) { return p._pISplLvlAdd; }, [](Player &p, int8_t val) { p._pISplLvlAdd = val; });
	SetDocumented(playerType, "friendlyMode", "boolean", "Whether the player is in friendly mode (non-PvP)", [](const Player &p) { return p.friendlyMode; }, [](Player &p, bool val) { p.friendlyMode = val; });
	// TODO: Add SpellCastInfo new_enum
	// SetDocumented(playerType, "queuedSpell", "SpellCastInfo", "The next queued spell", [](const Player &p) { return p.queuedSpell; }, [](Player &p, const SpellCastInfo &val) { p.queuedSpell = val; });
	// SetDocumented(playerType, "executedSpell", "SpellCastInfo", "The spell that is currently being cast", [](const Player &p) { return p.executedSpell; }, [](Player &p, const SpellCastInfo &val) { p.executedSpell = val; });
	SetDocumented(playerType, "inventorySpell", "SpellID", "Spell selected in inventory (e.g., teleport)", [](const Player &p) { return p.inventorySpell; }, [](Player &p, SpellID val) { p.inventorySpell = val; });
	SetDocumented(playerType, "spellFrom", "integer", "Inventory slot of scroll used for teleport", [](const Player &p) { return p.spellFrom; }, [](Player &p, int8_t val) { p.spellFrom = val; });
	SetDocumented(playerType, "rSpell", "SpellID", "Spell currently readied for casting", [](const Player &p) { return p._pRSpell; }, [](Player &p, SpellID val) { p._pRSpell = val; });
	// TODO: Add SpellType new_enum
	// SetDocumented(playerType, "rSplType", "SpellType", "Spell type currently readied", [](const Player &p) { return p._pRSplType; }, [](Player &p, SpellType val) { p._pRSplType = val; });
	SetDocumented(playerType, "sBkSpell", "SpellID", "Spell currently selected in spellbook", [](const Player &p) { return p._pSBkSpell; }, [](Player &p, SpellID val) { p._pSBkSpell = val; });
	SetDocumented(playerType, "splLvl", "integer[64]", "Spell level array", [](const Player &p) { return sol::as_table(std::vector<uint8_t>(std::begin(p._pSplLvl), std::end(p._pSplLvl))); }, [](Player &p, const std::vector<uint8_t> &vals) { std::copy_n(vals.begin(), std::min(vals.size(), size_t(64)), std::begin(p._pSplLvl)); });
	SetDocumented(playerType, "iSpells", "integer", "Bitmask of spells available via staves", [](const Player &p) { return p._pISpells; }, [](Player &p, uint64_t val) { p._pISpells = val; });
	SetDocumented(playerType, "memSpells", "integer", "Bitmask of learned spells", [](const Player &p) { return p._pMemSpells; }, [](Player &p, uint64_t val) { p._pMemSpells = val; });
	SetDocumented(playerType, "ablSpells", "integer", "Bitmask of abilities", [](const Player &p) { return p._pAblSpells; }, [](Player &p, uint64_t val) { p._pAblSpells = val; });
	SetDocumented(playerType, "scrlSpells", "integer", "Bitmask of spells available via scrolls", [](const Player &p) { return p._pScrlSpells; }, [](Player &p, uint64_t val) { p._pScrlSpells = val; });
	// TODO: Add SpellFlag new_enum
	// SetDocumented(playerType, "spellFlags", "SpellFlag", "Flags affecting spell behavior", [](const Player &p) { return p._pSpellFlags; }, [](Player &p, SpellFlag val) { p._pSpellFlags = val; });
	SetDocumented(playerType, "splHotKey", "SpellID[4]", "Readied spell hotkeys", [](const Player &p) { return sol::as_table(std::vector<SpellID>(std::begin(p._pSplHotKey), std::end(p._pSplHotKey))); }, [](Player &p, const std::vector<SpellID> &vals) { std::copy_n(vals.begin(), std::min(vals.size(), size_t(NumHotkeys)), std::begin(p._pSplHotKey)); });
	// SetDocumented(playerType, "splTHotKey", "SpellType[4]", "Spell type per hotkey", [](const Player &p) { return sol::as_table(std::vector<SpellType>(std::begin(p._pSplTHotKey), std::end(p._pSplTHotKey))); }, [](Player &p, const std::vector<SpellType> &vals) { std::copy_n(vals.begin(), std::min(vals.size(), size_t(NumHotkeys)), std::begin(p._pSplTHotKey)); });
	SetDocumented(playerType, "blockFlag", "boolean", "Whether blocking is active", [](const Player &p) { return p._pBlockFlag; }, [](Player &p, bool val) { p._pBlockFlag = val; });
	SetDocumented(playerType, "invincible", "boolean", "Whether the player is invincible", [](const Player &p) { return p._pInvincible; }, [](Player &p, bool val) { p._pInvincible = val; });
	SetDocumented(playerType, "lightRad", "integer", "Player's light radius", [](const Player &p) { return p._pLightRad; }, [](Player &p, int8_t val) { p._pLightRad = val; });
	SetDocumented(playerType, "lvlChanging", "boolean", "True when player is transitioning levels", [](const Player &p) { return p._pLvlChanging; }, [](Player &p, bool val) { p._pLvlChanging = val; });
	SetDocumented(playerType, "armorClass", "integer", "Player's armor class", [](const Player &p) { return p._pArmorClass; }, [](Player &p, int8_t val) { p._pArmorClass = val; });
	SetDocumented(playerType, "magResist", "integer", "Player's magic resistance", [](const Player &p) { return p._pMagResist; }, [](Player &p, int8_t val) { p._pMagResist = val; });
	SetDocumented(playerType, "fireResist", "integer", "Player's fire resistance", [](const Player &p) { return p._pFireResist; }, [](Player &p, int8_t val) { p._pFireResist = val; });
	SetDocumented(playerType, "lghtResist", "integer", "Player's lightning resistance", [](const Player &p) { return p._pLghtResist; }, [](Player &p, int8_t val) { p._pLghtResist = val; });
	SetDocumented(playerType, "infraFlag", "boolean", "Whether the player has infra-vision", [](const Player &p) { return p._pInfraFlag; }, [](Player &p, bool val) { p._pInfraFlag = val; });
	// TODO: Add Direction new_enum
	SetDocumented(playerType, "tempDirection", "Direction", "Casting direction or final move direction", [](const Player &p) { return p.tempDirection; }, [](Player &p, Direction val) { p.tempDirection = val; });
	SetDocumented(playerType, "lvlVisited", "boolean[NUMLEVELS]", "Array tracking visited levels", [](const Player &p) { return sol::as_table(std::vector<bool>(std::begin(p._pLvlVisited), std::end(p._pLvlVisited))); }, [](Player &p, const std::vector<bool> &vals) { std::copy_n(vals.begin(), std::min(vals.size(), size_t(NUMLEVELS)), std::begin(p._pLvlVisited)); });
	SetDocumented(playerType, "sLvlVisited", "boolean[NUMLEVELS]", "Array tracking visited set levels", [](const Player &p) { return sol::as_table(std::vector<bool>(std::begin(p._pSLvlVisited), std::end(p._pSLvlVisited))); }, [](Player &p, const std::vector<bool> &vals) { std::copy_n(vals.begin(), std::min(vals.size(), size_t(NUMLEVELS)), std::begin(p._pSLvlVisited)); });
	SetDocumented(playerType, "oilType", "ItemMiscID", "Oil type applied to equipped weapon", [](const Player &p) { return p._pOilType; }, [](Player &p, item_misc_id val) { p._pOilType = val; });
	SetDocumented(playerType, "townWarps", "integer", "Bitmask of available town warps", [](const Player &p) { return p.pTownWarps; }, [](Player &p, uint8_t val) { p.pTownWarps = val; });
	SetDocumented(playerType, "dungMsgs", "integer", "Message state for dungeon events", [](const Player &p) { return p.pDungMsgs; }, [](Player &p, uint8_t val) { p.pDungMsgs = val; });
	SetDocumented(playerType, "lvlLoad", "integer", "Level load state", [](const Player &p) { return p.pLvlLoad; }, [](Player &p, uint8_t val) { p.pLvlLoad = val; });
	SetDocumented(playerType, "manaShield", "boolean", "Whether Mana Shield is active", [](const Player &p) { return p.pManaShield; }, [](Player &p, bool val) { p.pManaShield = val; });
	SetDocumented(playerType, "dungMsgs2", "integer", "Secondary dungeon message state", [](const Player &p) { return p.pDungMsgs2; }, [](Player &p, uint8_t val) { p.pDungMsgs2 = val; });
	SetDocumented(playerType, "originalCathedral", "boolean", "Is original cathedral flag set", [](const Player &p) { return p.pOriginalCathedral; }, [](Player &p, bool val) { p.pOriginalCathedral = val; });
	SetDocumented(playerType, "diabloKillLevel", "integer", "Level at which Diablo was killed", [](const Player &p) { return p.pDiabloKillLevel; }, [](Player &p, uint8_t val) { p.pDiabloKillLevel = val; });
	SetDocumented(playerType, "reflections", "integer", "Bitmask of active reflection effects", [](const Player &p) { return p.wReflections; }, [](Player &p, uint16_t val) { p.wReflections = val; });
	SetDocumented(playerType, "damAcFlags", "ItemSpecialEffectHf", "Bitmask of damage/armor bonuses (Hellfire)", [](const Player &p) { return p.pDamAcFlags; }, [](Player &p, ItemSpecialEffectHf val) { p.pDamAcFlags = val; });

	// Member functions
	SetDocumented(playerType, "getName", "(): string", "Returns the player's name", &Player::name);
	SetDocumented(playerType, "getClassAttributes", "(): ClassAttributes", "Returns base stats/bonuses for this player's class", &Player::getClassAttributes);
	SetDocumented(playerType, "getPlayerCombatData", "(): PlayerCombatData", "Returns combat attributes for this player's class", &Player::getPlayerCombatData);
	SetDocumented(playerType, "getPlayerData", "(): PlayerData", "Returns metadata for this player's class", &Player::getPlayerData);
	SetDocumented(playerType, "getClassName", "(): string", "Gets the localized name of the character's class", &Player::getClassName);
	SetDocumented(playerType, "getBaseToBlock", "(): integer", "Returns base block chance for this player's class", &Player::getBaseToBlock);
	SetDocumented(playerType, "calcScrolls", "()", "Recomputes known scrolls based on inventory", &Player::CalcScrolls);
	SetDocumented(playerType, "canUseItem", "(item: Item): boolean", "Returns whether the player can use the given item", &Player::CanUseItem);
	SetDocumented(playerType, "canCleave", "(): boolean", "Returns whether the player's class qualifies for cleaving attacks", &Player::CanCleave);
	SetDocumented(playerType, "isEquipped", "(itemType: ItemType, isTwoHanded: boolean = false): boolean", "Checks if the player has a given item type equipped", &Player::isEquipped);
	SetDocumented(playerType, "removeInvItem", "(index: integer, calcScrolls: boolean = true)", "Removes item from inventory", &Player::RemoveInvItem);
	SetDocumented(playerType, "getId", "(): integer", "Returns the player’s network ID", &Player::getId);
	SetDocumented(playerType, "removeSpdBarItem", "(index: integer)", "Removes item from speed bar", &Player::RemoveSpdBarItem);
	SetDocumented(playerType, "getBaseAttributeValue", "(attribute: CharacterAttribute): integer", "Gets base value for a stat", &Player::GetBaseAttributeValue);
	SetDocumented(playerType, "getCurrentAttributeValue", "(attribute: CharacterAttribute): integer", "Gets current value for a stat", &Player::GetCurrentAttributeValue);
	SetDocumented(playerType, "getMaximumAttributeValue", "(attribute: CharacterAttribute): integer", "Gets max allowable value for a stat", &Player::GetMaximumAttributeValue);
	SetDocumented(playerType, "getTargetPosition", "(): Point", "Gets the target tile position (for movement or casting)", &Player::GetTargetPosition);
	SetDocumented(playerType, "getPositionPathIndex", "(position: Point): integer", "Gets index of given position in walkpath", &Player::GetPositionPathIndex);
	SetDocumented(playerType, "say", "(speechId: HeroSpeech)", "Says a speech line", sol::resolve<void(HeroSpeech) const>(&Player::Say));
	SetDocumented(playerType, "sayWithDelay", "(speechId: HeroSpeech, delay: integer)", "Says a speech line after a delay", sol::resolve<void(HeroSpeech, int) const>(&Player::Say));
	SetDocumented(playerType, "saySpecific", "(speechId: HeroSpeech)", "Says a specific speech line with no variation", &Player::SaySpecific);
	SetDocumented(playerType, "stop", "()", "Cancels any queued actions", &Player::Stop);
	SetDocumented(playerType, "isWalking", "(): boolean", "Returns whether the player is currently walking", &Player::isWalking);
	SetDocumented(playerType, "getItemLocation", "(item: Item): item_equip_type", "Gets the logical equipment location for the item", &Player::GetItemLocation);
	SetDocumented(playerType, "getArmor", "(): integer", "Returns total armor value", &Player::GetArmor);
	SetDocumented(playerType, "getMeleeToHit", "(): integer", "Returns melee hit chance", &Player::GetMeleeToHit);
	SetDocumented(playerType, "getMeleePiercingToHit", "(): integer", "Returns melee hit chance including armor piercing", &Player::GetMeleePiercingToHit);
	SetDocumented(playerType, "getRangedToHit", "(): integer", "Returns ranged hit chance", &Player::GetRangedToHit);
	SetDocumented(playerType, "getRangedPiercingToHit", "(): integer", "Returns ranged hit chance including armor piercing", &Player::GetRangedPiercingToHit);
	SetDocumented(playerType, "getMagicToHit", "(): integer", "Returns magic hit chance", &Player::GetMagicToHit);
	SetDocumented(playerType, "getBlockChance", "(useLevel: boolean = true): integer", "Returns block chance", &Player::GetBlockChance);
	SetDocumented(playerType, "getManaShieldDamageReduction", "(): integer", "Returns damage reduction factor from Mana Shield", &Player::GetManaShieldDamageReduction);
	SetDocumented(playerType, "getSpellLevel", "(spell: SpellID): integer", "Returns effective level of the given spell", &Player::GetSpellLevel);
	SetDocumented(playerType, "calculateArmorPierce", "(monsterArmor: integer, isMelee: boolean): integer", "Returns monster armor after applying piercing effects", &Player::CalculateArmorPierce);
	SetDocumented(playerType, "updateHitPointPercentage", "(): integer", "Recalculates and returns current HP percentage", &Player::UpdateHitPointPercentage);
	SetDocumented(playerType, "updateManaPercentage", "(): integer", "Recalculates and returns current mana percentage", &Player::UpdateManaPercentage);
	SetDocumented(playerType, "restorePartialLife", "()", "Restores a portion of the player's HP", &Player::RestorePartialLife);
	SetDocumented(playerType, "restoreFullLife", "()", "Fully restores HP", &Player::RestoreFullLife);
	SetDocumented(playerType, "restorePartialMana", "()", "Restores a portion of the player's mana", &Player::RestorePartialMana);
	SetDocumented(playerType, "restoreFullMana", "()", "Fully restores mana", &Player::RestoreFullMana);
	SetDocumented(playerType, "readySpellFromEquipment", "(bodyLocation: inv_body_loc, forceSpell: boolean)", "Sets spell from equipment slot as active", &Player::ReadySpellFromEquipment);
	SetDocumented(playerType, "usesRangedWeapon", "(): boolean", "Returns true if player is using a ranged weapon", &Player::UsesRangedWeapon);
	SetDocumented(playerType, "canChangeAction", "(): boolean", "Returns true if player can cancel current action", &Player::CanChangeAction);
	SetDocumented(playerType, "getGraphic", "(): player_graphic", "Returns current player graphic state", &Player::getGraphic);
	SetDocumented(playerType, "getSpriteWidth", "(): integer", "Returns sprite width", &Player::getSpriteWidth);
	SetDocumented(playerType, "getAnimationFramesAndTicksPerFrame", "(graphics: player_graphic): (numberOfFrames: integer, ticksPerFrame: integer)", "Returns animation frame count and ticks per frame", &Player::getAnimationFramesAndTicksPerFrame);
	SetDocumented(playerType, "currentSprite", "(): ClxSprite", "Returns the sprite currently being displayed", &Player::currentSprite);
	SetDocumented(playerType, "getRenderingOffset", "(sprite: ClxSprite): Displacement", "Returns rendering offset for the given sprite", &Player::getRenderingOffset);
	SetDocumented(playerType, "updatePreviewCelSprite", "(cmdId: _cmd_id, point: Point, wParam1: integer, wParam2: integer)", "Updates preview sprite for upcoming action", &Player::UpdatePreviewCelSprite);
	SetDocumented(playerType, "getCharacterLevel", "(): integer", "Returns current character level", &Player::getCharacterLevel);
	SetDocumented(playerType, "setCharacterLevel", "(level: integer)", "Sets the character level", &Player::setCharacterLevel);
	SetDocumented(playerType, "getMaxCharacterLevel", "(): integer", "Returns maximum possible character level", &Player::getMaxCharacterLevel);
	SetDocumented(playerType, "isMaxCharacterLevel", "(): boolean", "Returns true if player is at max level", &Player::isMaxCharacterLevel);
	SetDocumented(playerType, "addExperience", "(experience: integer)", "Adds experience to the player (auto-adjusted for multiplayer scaling)", sol::resolve<void(uint32_t)>(&Player::addExperience));
	SetDocumented(playerType, "addExperienceScaled", "(experience: integer, monsterLevel: integer)", "Adds experience adjusted by monster level and multiplayer scaling", sol::resolve<void(uint32_t, int)>(&Player::addExperience));
	SetDocumented(playerType, "getNextExperienceThreshold", "(): integer", "Returns experience required for next level", &Player::getNextExperienceThreshold);
	SetDocumented(playerType, "isOnActiveLevel", "(): boolean", "Checks if player is on the same level as the local player", &Player::isOnActiveLevel);
	SetDocumented(playerType, "isOnLevel", "(level: integer): boolean", "Checks if player is on the specified dungeon level", sol::resolve<bool(uint8_t) const>(&Player::isOnLevel));
	SetDocumented(playerType, "isOnSetLevel", "(level: _setlevels): boolean", "Checks if player is on the specified set level", sol::resolve<bool(_setlevels) const>(&Player::isOnLevel));
	SetDocumented(playerType, "isOnArenaLevel", "(): boolean", "Returns true if player is on an arena level", &Player::isOnArenaLevel);
	SetDocumented(playerType, "setLevel", "(level: integer)", "Sets the player's current dungeon level", sol::resolve<void(uint8_t)>(&Player::setLevel));
	SetDocumented(playerType, "setSetLevel", "(level: _setlevels)", "Sets the player's current set level", sol::resolve<void(_setlevels)>(&Player::setLevel));
	SetDocumented(playerType, "calculateBaseLife", "(): integer", "Calculates starting HP based on vitality and class", &Player::calculateBaseLife);
	SetDocumented(playerType, "calculateBaseMana", "(): integer", "Calculates starting Mana based on magic and class", &Player::calculateBaseMana);
	SetDocumented(playerType, "occupyTile", "(position: Point, isMoving: boolean)", "Marks a tile as occupied by the player", &Player::occupyTile);
	SetDocumented(playerType, "isLevelOwnedByLocalClient", "(): boolean", "Returns true if player level is owned by local client", &Player::isLevelOwnedByLocalClient);
	SetDocumented(playerType, "isHoldingItem", "(itemType: ItemType): boolean", "Checks if the player is holding a usable item of the given type", &Player::isHoldingItem);
}

void RegisterCharacterAttributeEnum(sol::state_view &lua)
{
	lua.new_enum<CharacterAttribute>("CharacterAttribute",
	    {
	        { "Strength", CharacterAttribute::Strength },
	        { "Magic", CharacterAttribute::Magic },
	        { "Dexterity", CharacterAttribute::Dexterity },
	        { "Vitality", CharacterAttribute::Vitality },
	    });
}

void RegisterInvBodyLocEnum(sol::state_view &lua)
{
	lua.new_enum<inv_body_loc>("InvBodyLoc",
	    {
	        { "Head", INVLOC_HEAD },
	        { "RingLeft", INVLOC_RING_LEFT },
	        { "RingRight", INVLOC_RING_RIGHT },
	        { "Amulet", INVLOC_AMULET },
	        { "HandLeft", INVLOC_HAND_LEFT },
	        { "HandRight", INVLOC_HAND_RIGHT },
	        { "Chest", INVLOC_CHEST },
	    });
}

void RegisterPlayerGraphicEnum(sol::state_view &lua)
{
	lua.new_enum<player_graphic>("PlayerGraphic",
	    {
	        { "Stand", player_graphic::Stand },
	        { "Walk", player_graphic::Walk },
	        { "Attack", player_graphic::Attack },
	        { "Hit", player_graphic::Hit },
	        { "Lightning", player_graphic::Lightning },
	        { "Fire", player_graphic::Fire },
	        { "Magic", player_graphic::Magic },
	        { "Death", player_graphic::Death },
	        { "Block", player_graphic::Block },
	    });
}

void RegisterPlayerWeaponGraphicEnum(sol::state_view &lua)
{
	lua.new_enum<PlayerWeaponGraphic>("PlayerWeaponGraphic",
	    {
	        { "Unarmed", PlayerWeaponGraphic::Unarmed },
	        { "UnarmedShield", PlayerWeaponGraphic::UnarmedShield },
	        { "Sword", PlayerWeaponGraphic::Sword },
	        { "SwordShield", PlayerWeaponGraphic::SwordShield },
	        { "Bow", PlayerWeaponGraphic::Bow },
	        { "Axe", PlayerWeaponGraphic::Axe },
	        { "Mace", PlayerWeaponGraphic::Mace },
	        { "MaceShield", PlayerWeaponGraphic::MaceShield },
	        { "Staff", PlayerWeaponGraphic::Staff },
	    });
}

void RegisterPlrModeEnum(sol::state_view &lua)
{
	lua.new_enum<PLR_MODE>("PlayerMode",
	    {
	        { "Stand", PM_STAND },
	        { "WalkNorth", PM_WALK_NORTHWARDS },
	        { "WalkSouth", PM_WALK_SOUTHWARDS },
	        { "WalkSideways", PM_WALK_SIDEWAYS },
	        { "Attack", PM_ATTACK },
	        { "RangedAttack", PM_RATTACK },
	        { "Block", PM_BLOCK },
	        { "GotHit", PM_GOTHIT },
	        { "Death", PM_DEATH },
	        { "Spell", PM_SPELL },
	        { "NewLevel", PM_NEWLVL },
	        { "Quit", PM_QUIT },
	    });
}

void RegisterActionIdEnum(sol::state_view &lua)
{
	lua.new_enum<action_id>("ActionID",
	    {
	        { "AutoWalk", ACTION_WALK },
	        { "None", ACTION_NONE },
	        { "Attack", ACTION_ATTACK },
	        { "RangedAttack", ACTION_RATTACK },
	        { "Spell", ACTION_SPELL },
	        { "Operate", ACTION_OPERATE },
	        { "Disarm", ACTION_DISARM },
	        { "PickupToHand", ACTION_PICKUPITEM },
	        { "PickupToInventory", ACTION_PICKUPAITEM },
	        { "Talk", ACTION_TALK },
	        { "OperateTelekinesis", ACTION_OPERATETK },
	        { "AttackMonster", ACTION_ATTACKMON },
	        { "AttackPlayer", ACTION_ATTACKPLR },
	        { "RangedAttackMonster", ACTION_RATTACKMON },
	        { "RangedAttackPlayer", ACTION_RATTACKPLR },
	        { "SpellMonster", ACTION_SPELLMON },
	        { "SpellPlayer", ACTION_SPELLPLR },
	        { "SpellWall", ACTION_SPELLWALL },
	    });
}

void RegisterSpellFlagEnum(sol::state_view &lua)
{
	lua.new_enum<SpellFlag>("SpellFlag",
	    {
	        { "None", SpellFlag::None },
	        { "Etherealize", SpellFlag::Etherealize },
	        { "RageActive", SpellFlag::RageActive },
	        { "RageCooldown", SpellFlag::RageCooldown },
	    });
}

void RegisterDeathReasonEnum(sol::state_view &lua)
{
	lua.new_enum<DeathReason>("DeathReason",
	    {
	        { "MonsterOrTrap", DeathReason::MonsterOrTrap },
	        { "Player", DeathReason::Player },
	        { "Unknown", DeathReason::Unknown },
	    });
}

} // namespace

sol::table LuaPlayerModule(sol::state_view &lua)
{
	InitPlayerUserType(lua);
	RegisterCharacterAttributeEnum(lua);
	RegisterInvBodyLocEnum(lua);
	RegisterPlayerGraphicEnum(lua);
	RegisterPlayerWeaponGraphicEnum(lua);
	RegisterPlrModeEnum(lua);
	RegisterActionIdEnum(lua);
	RegisterSpellFlagEnum(lua);
	RegisterDeathReasonEnum(lua);

	sol::table table = lua.create_table();
	SetDocumented(table, "self", "()",
	    "The current player",
	    []() {
		    return MyPlayer;
	    });
	SetDocumented(table, "walk_to", "(x: integer, y: integer)",
	    "Walk to the given coordinates",
	    [](int x, int y) {
		    NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, Point { x, y });
	    });
	return table;
}

} // namespace devilution
