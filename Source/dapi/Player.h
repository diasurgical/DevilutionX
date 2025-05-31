#pragma once
#include "Item.h"
#include <map>

namespace DAPI {
const int NUM_INVLOC = 7;
const int MAXINV = 40;
const int MAXSPD = 8;

struct PlayerData {
	int _pmode;
	int pnum;
	int plrlevel;
	int _px;
	int _py;
	int _pfutx;
	int _pfuty;
	int _pdir;

	int _pRSpell;
	char _pRSplType;

	char _pSplLvl[64];
	uint64_t _pMemSpells;
	uint64_t _pAblSpells;
	uint64_t _pScrlSpells;

	char _pName[32];
	char _pClass;

	int _pStrength;
	int _pBaseStr;
	int _pMagic;
	int _pBaseMag;
	int _pDexterity;
	int _pBaseDex;
	int _pVitality;
	int _pBaseVit;

	int _pStatPts;

	int _pDamageMod;

	int _pHitPoints;
	int _pMaxHP;
	int _pMana;
	int _pMaxMana;
	char _pLevel;
	int _pExperience;

	char _pArmorClass;

	char _pMagResist;
	char _pFireResist;
	char _pLightResist;

	int _pGold;

	std::map<int, int> InvBody;
	int InvList[MAXINV];
	int InvGrid[MAXINV];
	std::map<int, int> SpdList;
	int HoldItem;

	int _pIMinDam;
	int _pIMaxDam;
	int _pIBonusDam;
	int _pIAC;
	int _pIBonusToHit;
	int _pIBonusAC;
	int _pIBonusDamMod;
	bool pManaShield;
};
} // namespace DAPI
