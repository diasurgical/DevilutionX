#pragma once

#include "../items.h"

namespace DAPI {
struct ItemData {
	bool compare(devilution::Item &item)
	{
		return item._iSeed == _iSeed && item._iCreateInfo == _iCreateInfo && item.IDidx == IDidx;
	}

	int ID;

	uint32_t _iSeed;
	int _iCreateInfo;
	int _itype;
	int _ix;
	int _iy;

	uint32_t _iIdentified;
	char _iMagical;
	char _iName[64];
	char _iIName[64];
	char _iClass;
	int _iCurs;
	int _ivalue;
	int _iMinDam;
	int _iMaxDam;
	int _iAC;
	int _iFlags;
	int _iMiscId;
	int _iSpell;

	int _iCharges;
	int _iMaxCharges;

	int _iDurability;
	int _iMaxDur;

	int _iPLDam;
	int _iPLToHit;
	int _iPLAC;
	int _iPLStr;
	int _iPLMag;
	int _iPLDex;
	int _iPLVit;
	int _iPLFR;
	int _iPLLR;
	int _iPLMR;
	int _iPLMana;
	int _iPLHP;
	int _iPLDamMod;
	int _iPLGetHit;
	int _iPLLight;
	char _iSplLvlAdd;

	int _iFMinDam;
	int _iFMaxDam;
	int _iLMinDam;
	int _iLMaxDam;

	char _iPrePower;
	char _iSufPower;

	char _iMinStr;
	char _iMinMag;
	char _iMinDex;
	uint32_t _iStatFlag;
	int IDidx;
};
} // namespace DAPI
