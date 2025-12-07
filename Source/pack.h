/**
 * @file pack.h
 *
 * Interface of functions for minifying player data structure.
 */
#pragma once

#include <cstdint>

#include "inv.h"
#include "items.h"
#include "msg.h"
#include "player.h"

namespace devilution {

#define MAX_SPELLS 52

#pragma pack(push, 1)
struct ItemPack {
	uint32_t iSeed;
	uint16_t iCreateInfo;
	uint16_t idx;
	uint8_t bId;
	uint8_t bDur;
	uint8_t bMDur;
	uint8_t bCh;
	uint8_t bMCh;
	uint16_t wValue;
	uint32_t dwBuff;
};

struct PlayerPack {
	uint32_t dwLowDateTime;
	uint32_t dwHighDateTime;
	int8_t destAction;
	int8_t destParam1;
	int8_t destParam2;
	uint8_t plrlevel;
	uint8_t px;
	uint8_t py;
	uint8_t targx;
	uint8_t targy;
	char pName[PlayerNameLength];
	uint8_t pClass;
	uint8_t pBaseStr;
	uint8_t pBaseMag;
	uint8_t pBaseDex;
	uint8_t pBaseVit;
	uint8_t pLevel;
	uint8_t pStatPts;
	uint32_t pExperience;
	int32_t pGold;
	int32_t pHPBase;
	int32_t pMaxHPBase;
	int32_t pManaBase;
	int32_t pMaxManaBase;
	uint8_t pSplLvl[37]; // Should be MAX_SPELLS but set to 37 to make save games compatible
	uint64_t pMemSpells;
	ItemPack InvBody[NUM_INVLOC];
	ItemPack InvList[InventoryGridCells];
	int8_t InvGrid[InventoryGridCells];
	uint8_t _pNumInv;
	ItemPack SpdList[MaxBeltItems];
	int8_t pTownWarps;
	int8_t pDungMsgs;
	int8_t pLvlLoad;
	uint8_t pBattleNet;
	uint8_t pManaShield;
	uint8_t pDungMsgs2;
	/** The format the character is in, 0: Diablo, 1: Hellfire */
	int8_t bIsHellfire;
	uint8_t reserved; // For future use
	uint16_t wReflections;
	uint8_t reserved2[2]; // For future use
	uint8_t pSplLvl2[10]; // Hellfire spells
	int16_t wReserved8;   // For future use
	uint32_t pDiabloKillLevel;
	uint32_t pDifficulty;
	uint32_t pDamAcFlags;  // `ItemSpecialEffectHf` is 1 byte but this is 4 bytes.
	uint8_t reserved3[20]; // For future use
};

union ItemNetPack {
	TItemDef def;
	TItem item;
	TEar ear;
};

struct PlayerNetPack {
	uint8_t plrlevel;
	uint8_t px;
	uint8_t py;
	char pName[PlayerNameLength];
	uint8_t pClass;
	uint8_t pBaseStr;
	uint8_t pBaseMag;
	uint8_t pBaseDex;
	uint8_t pBaseVit;
	int8_t pLevel;
	uint8_t pStatPts;
	uint32_t pExperience;
	int32_t pHPBase;
	int32_t pMaxHPBase;
	int32_t pManaBase;
	int32_t pMaxManaBase;
	uint8_t pSplLvl[MAX_SPELLS];
	uint64_t pMemSpells;
	ItemNetPack InvBody[NUM_INVLOC];
	ItemNetPack InvList[InventoryGridCells];
	int8_t InvGrid[InventoryGridCells];
	uint8_t _pNumInv;
	ItemNetPack SpdList[MaxBeltItems];
	uint8_t pManaShield;
	uint16_t wReflections;
	uint8_t pDiabloKillLevel;
	uint8_t friendlyMode;
	uint8_t isOnSetLevel;

	// For validation
	int32_t pStrength;
	int32_t pMagic;
	int32_t pDexterity;
	int32_t pVitality;
	int32_t pHitPoints;
	int32_t pMaxHP;
	int32_t pMana;
	int32_t pMaxMana;
	int32_t pDamageMod;
	int32_t pBaseToBlk;
	int32_t pIMinDam;
	int32_t pIMaxDam;
	int32_t pIAC;
	int32_t pIBonusDam;
	int32_t pIBonusToHit;
	int32_t pIBonusAC;
	int32_t pIBonusDamMod;
	int32_t pIGetHit;
	int32_t pIEnAc;
	int32_t pIFMinDam;
	int32_t pIFMaxDam;
	int32_t pILMinDam;
	int32_t pILMaxDam;
};
#pragma pack(pop)

bool RecreateHellfireSpellBook(const Player &player, const TItem &packedItem, Item *item = nullptr);
void PackPlayer(PlayerPack &pPack, const Player &player);
void UnPackPlayer(const PlayerPack &pPack, Player &player);
void PackNetPlayer(PlayerNetPack &packed, const Player &player);
bool UnPackNetPlayer(const PlayerNetPack &packed, Player &player);

/**
 * @brief Save the attributes needed to recreate this item into an ItemPack struct
 *
 * @param packedItem The destination packed struct
 * @param item The source item
 * @param isHellfire Whether the item is from Hellfire or not
 */
void PackItem(ItemPack &packedItem, const Item &item, bool isHellfire);

/**
 * Expand a ItemPack in to a Item
 *
 * @param packedItem The source packed item
 * @param item The destination item
 * @param isHellfire Whether the item is from Hellfire or not
 */
void UnPackItem(const ItemPack &packedItem, const Player &player, Item &item, bool isHellfire);

/**
 * @brief Save the attributes needed to recreate this item into an ItemNetPack struct
 * @param item The source item
 * @param packedItem The destination packed struct
 */
void PackNetItem(const Item &item, ItemNetPack &packedItem);

/**
 * @brief Expand a ItemPack in to a Item
 * @param player The player holding the item
 * @param packedItem The source packed item
 * @param item The destination item
 * @return True if the item is valid
 */
bool UnPackNetItem(const Player &player, const ItemNetPack &packedItem, Item &item);

} // namespace devilution
