#include <cstdint>

#include <gtest/gtest.h>

#include "cursor.h"
#include "engine/assets.hpp"
#include "game_mode.hpp"
#include "monstdat.h"
#include "pack.h"
#include "playerdat.hpp"
#include "utils/is_of.hpp"
#include "utils/paths.h"

namespace devilution {
namespace {

void SwapLE(ItemPack &pack)
{
	pack.iSeed = SDL_SwapLE32(pack.iSeed);
	pack.iCreateInfo = SDL_SwapLE16(pack.iCreateInfo);
	pack.idx = SDL_SwapLE16(pack.idx);
	pack.wValue = SDL_SwapLE16(pack.wValue);
	pack.dwBuff = SDL_SwapLE32(pack.dwBuff);
}

void SwapLE(PlayerPack &pack)
{
	pack.dwLowDateTime = SDL_SwapLE32(pack.dwLowDateTime);
	pack.dwHighDateTime = SDL_SwapLE32(pack.dwHighDateTime);
	pack.pExperience = SDL_SwapLE32(pack.pExperience);
	pack.pGold = SDL_SwapLE32(pack.pGold);
	pack.pHPBase = SDL_SwapLE32(pack.pHPBase);
	pack.pMaxHPBase = SDL_SwapLE32(pack.pMaxHPBase);
	pack.pManaBase = SDL_SwapLE32(pack.pManaBase);
	pack.pMaxManaBase = SDL_SwapLE32(pack.pMaxManaBase);
	pack.pMemSpells = SDL_SwapLE64(pack.pMemSpells);
	for (ItemPack &item : pack.InvBody)
		SwapLE(item);
	for (ItemPack &item : pack.InvList)
		SwapLE(item);
	for (ItemPack &item : pack.SpdList)
		SwapLE(item);
	pack.wReflections = SDL_SwapLE16(pack.wReflections);
	pack.pDiabloKillLevel = SDL_SwapLE32(pack.pDiabloKillLevel);
	pack.pDifficulty = SDL_SwapLE32(pack.pDifficulty);
	pack.pDamAcFlags = SDL_SwapLE32(pack.pDamAcFlags);
}

ItemPack SwappedLE(const ItemPack &pack)
{
	ItemPack swapped = pack;
	SwapLE(swapped);
	return swapped;
}

void ComparePackedItems(const ItemPack &item1LE, const ItemPack &item2LE)
{
	// Packs are little-endian.
	// Swap to native endianness on big-endian systems before comparison for better error messages.
	const ItemPack item1 = SwappedLE(item1LE);
	const ItemPack item2 = SwappedLE(item2LE);

	// `ItemPack` is packed, so we copy the unaligned values out before comparing them.
	// This avoids the following UBSAN error such as this one:
	// runtime error: load of misaligned address for type 'const unsigned int', which requires 4 byte alignment
	{
		const auto item1_iSeed = item1.iSeed;
		const auto item2_iSeed = item2.iSeed;
		EXPECT_EQ(item1_iSeed, item2_iSeed);
	}
	{
		const auto item1_iCreateInfo = item1.iCreateInfo;
		const auto item2_iCreateInfo = item2.iCreateInfo;
		EXPECT_EQ(item1_iCreateInfo, item2_iCreateInfo);
	}
	{
		const auto item1_idx = item1.idx;
		const auto item2_idx = item2.idx;
		EXPECT_EQ(item1_idx, item2_idx);
	}
	EXPECT_EQ(item1.bId, item2.bId);
	EXPECT_EQ(item1.bDur, item2.bDur);
	EXPECT_EQ(item1.bMDur, item2.bMDur);
	EXPECT_EQ(item1.bCh, item2.bCh);
	EXPECT_EQ(item1.bMCh, item2.bMCh);
	{
		const auto item1_wValue = item1.wValue;
		const auto item2_wValue = item2.wValue;
		EXPECT_EQ(item1_wValue, item2_wValue);
	}
	{
		const auto item1_dwBuff = item1.dwBuff;
		const auto item2_dwBuff = item2.dwBuff;
		EXPECT_EQ(item1_dwBuff, item2_dwBuff);
	}
}
typedef struct TestItemStruct {
	char _iIName[ItemNameLength];
	ItemType _itype;
	int _iClass;
	int _iCurs;
	int _iIvalue;
	int _iMinDam;
	int _iMaxDam;
	int _iAC;
	ItemSpecialEffect _iFlags;
	int _iMiscId;
	SpellID _iSpell;
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
	int8_t _iSplLvlAdd;
	int _iUid;
	int _iFMinDam;
	int _iFMaxDam;
	int _iLMinDam;
	int _iLMaxDam;
	int8_t _iPrePower;
	int8_t _iSufPower;
	int8_t _iMinStr;
	uint8_t _iMinMag;
	int8_t _iMinDex;
	int IDidx;
} TestItemStruct;

static void TestItemNameGeneration(const Item &item)
{
	bool allowIdentified = (item._iMiscId != IMISC_EAR); // Ears can't be identified. Item::getName() doesn't handle it, so don't test it.
	ASSERT_EQ(allowIdentified & item._iIdentified, item._iIdentified);

	Item testItem = item;

	testItem._iIdentified = false;
	ASSERT_STREQ(testItem.getName().str().data(), testItem._iName) << "unidentified name";
	if (allowIdentified) {
		testItem._iIdentified = true;
		ASSERT_STREQ(testItem.getName().str().data(), testItem._iIName) << "identified name";

		// Check that UpdateHellfireFlag ensures that dwBuff is updated to get the correct name
		if (item._iMagical == ITEM_QUALITY_MAGIC) {
			bool isHellfireItem = (testItem.dwBuff & CF_HELLFIRE);
			testItem.dwBuff = 0;
			UpdateHellfireFlag(testItem, testItem._iIName);

			testItem._iIdentified = true;
			ASSERT_STREQ(testItem.getName().str().data(), testItem._iIName) << "identified name with UpdateHellfireFlag";
			ASSERT_TRUE(isHellfireItem || ((item.dwBuff & CF_HELLFIRE) != CF_HELLFIRE)) << "item was wrongly converted to hellfire";
		}
	}
}

static void CompareItems(const Item &item1, const TestItemStruct &item2)
{
	ASSERT_STREQ(item1._iIName, item2._iIName);
	EXPECT_EQ(item1._itype, item2._itype);
	EXPECT_EQ(item1._iClass, item2._iClass);
	EXPECT_EQ(item1._iCurs, item2._iCurs);
	EXPECT_EQ(item1._iIvalue, item2._iIvalue);
	EXPECT_EQ(item1._iMinDam, item2._iMinDam);
	EXPECT_EQ(item1._iMaxDam, item2._iMaxDam);
	EXPECT_EQ(item1._iAC, item2._iAC);
	EXPECT_EQ(item1._iFlags, item2._iFlags);
	EXPECT_EQ(item1._iMiscId, item2._iMiscId);
	EXPECT_EQ(item1._iSpell, item2._iSpell);
	EXPECT_EQ(item1._iCharges, item2._iCharges);
	EXPECT_EQ(item1._iMaxCharges, item2._iMaxCharges);
	EXPECT_EQ(item1._iDurability, item2._iDurability);
	EXPECT_EQ(item1._iMaxDur, item2._iMaxDur);
	EXPECT_EQ(item1._iPLDam, item2._iPLDam);
	EXPECT_EQ(item1._iPLToHit, item2._iPLToHit);
	EXPECT_EQ(item1._iPLAC, item2._iPLAC);
	EXPECT_EQ(item1._iPLStr, item2._iPLStr);
	EXPECT_EQ(item1._iPLMag, item2._iPLMag);
	EXPECT_EQ(item1._iPLDex, item2._iPLDex);
	EXPECT_EQ(item1._iPLVit, item2._iPLVit);
	EXPECT_EQ(item1._iPLFR, item2._iPLFR);
	EXPECT_EQ(item1._iPLLR, item2._iPLLR);
	EXPECT_EQ(item1._iPLMR, item2._iPLMR);
	EXPECT_EQ(item1._iPLMana, item2._iPLMana);
	EXPECT_EQ(item1._iPLHP, item2._iPLHP);
	EXPECT_EQ(item1._iPLDamMod, item2._iPLDamMod);
	EXPECT_EQ(item1._iPLGetHit, item2._iPLGetHit);
	EXPECT_EQ(item1._iPLLight, item2._iPLLight);
	EXPECT_EQ(item1._iSplLvlAdd, item2._iSplLvlAdd);
	EXPECT_EQ(item1._iUid, item2._iUid);
	EXPECT_EQ(item1._iFMinDam, item2._iFMinDam);
	EXPECT_EQ(item1._iFMaxDam, item2._iFMaxDam);
	EXPECT_EQ(item1._iLMinDam, item2._iLMinDam);
	EXPECT_EQ(item1._iLMaxDam, item2._iLMaxDam);
	EXPECT_EQ(item1._iPrePower, item2._iPrePower);
	EXPECT_EQ(item1._iSufPower, item2._iSufPower);
	EXPECT_EQ(item1._iMinStr, item2._iMinStr);
	EXPECT_EQ(item1._iMinMag, item2._iMinMag);
	EXPECT_EQ(item1._iMinDex, item2._iMinDex);
	EXPECT_EQ(item1.IDidx, item2.IDidx);
}

const ItemPack PackedDiabloItems[] = {
	// clang-format off
	//     iSeed,                              iCreateInfo,  idx,  bId,  bDur,  bMDur,  bCh,  bMCh,  wValue,  dwBuff
	{ 2082213289, 25 | CF_UPER1                           ,   53,    3,    60,     60,    0,     0,       0,       0 }, // Amber Helm of harmony
	{  338833725, 24 | CF_UPER1                           ,  154,    3,     0,      0,    0,     0,       0,       0 }, // Cobalt Amulet of giants
	{  545145866, 26 | CF_UPER1                           ,  120,    3,    38,     40,    0,     0,       0,       0 }, // Brutal Sword of gore
	{ 1504248345, 26 | CF_UPER1  | CF_ONLYGOOD | CF_UNIQUE,   70,    5,   255,    255,    0,     0,       0,       0 }, // Demonspike Coat
	{ 1884424756,  6 | CF_UPER1  | CF_ONLYGOOD            ,  151,    3,     0,      0,    0,     0,       0,       0 }, // Steel Ring of the jaguar
	{ 1712759905, 28 | CF_UPER15 | CF_ONLYGOOD            ,  151,    3,     0,      0,    0,     0,       0,       0 }, // Ring of the heavens
	{  981777658, 30 | CF_UPER1                           ,  153,    2,     0,      0,    0,     0,       0,       0 }, // Ring of sorcery
	{  844854815, 26 | CF_UPER1                           ,   75,    3,   255,    255,    0,     0,       0,       0 }, // Tower Shield of the ages
	{ 1151513535, 30 | CF_UPER1                           ,   73,    2,    12,     32,    0,     0,       0,       0 }, // Sapphire Large Shield
	{  640243885,  6                                      ,   27,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Town Portal
	{  741806938,  9                                      ,   25,    0,     0,      0,    0,     0,       0,       0 }, // IDI_MANA
	{ 1456608333,  1 | CF_UPER1                           ,   79,    0,     0,      0,    0,     0,       0,       0 }, // Mana
	{  554676945, 16                                      ,   30,    0,     0,      0,    0,     0,       0,       0 }, // Full mana
	{  355389938,  0                                      ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Healing
	{  868435486, 16                                      ,   29,    0,     0,      0,    0,     0,       0,       0 }, // Full healing
	{ 1372832903,  0                                      ,    4,    0,    30,     30,    0,     0,       0,       0 }, // IDI_ROGUE
	{  896239556, 20             | CF_SMITHPREMIUM        ,   53,    3,    56,     60,    0,     0,       0,       0 }, // Jade Helm of the wolf
	{ 1286225254, 13 | CF_UPER1                           ,  151,    3,     0,      0,    0,     0,       0,       0 }, // Steel Ring of accuracy
	{  548642293, 10 | CF_UPER1                           ,   21,    0,     0,      0,    0,     0,       0,       0 }, // Blood Stone
	{  307669016, 14 | CF_UPER1                           ,  151,    3,     0,      0,    0,     0,       0,       0 }, // Ring of power
	{  204766888, 12 | CF_UPER1  | CF_ONLYGOOD            ,  154,    3,     0,      0,    0,     0,       0,       0 }, // Gold Amulet of accuracy
	{ 1642744332, 17 | CF_UPER1                           ,  122,    3,    25,     60,    0,     0,       0,       0 }, // Bastard Sword of the bat
	{ 1031508271, 12             | CF_SMITH               ,   72,    0,    14,     24,    0,     0,       0,       0 }, // Small Shield
	{ 1384620228, 18 | CF_UPER1  | CF_ONLYGOOD            ,   65,    3,    44,     80,    0,     0,       0,       0 }, // Breast Plate of giants
	{  681296530, 10 | CF_UPER1                           ,   87,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Healing
	{  109984285,  6             | CF_HEALER              ,   81,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Rejuvenation - made by Pepin
	{ 1857753366,  4 | CF_UPER1                           ,   81,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Rejuvenation
	{  965103261, 17 | CF_UPER1                           ,   82,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Full Rejuvenation
	{  430621075, 25 | CF_UPER15 | CF_ONLYGOOD            ,  141,    3,    45,     45,    0,     0,       0,       0 }, // Savage Bow of perfection
	{ 1272669062,  2 | CF_UPER1                           ,  115,    0,    10,     20,    0,     0,       0,       0 }, // Falchion
	{ 1133884051, 22 | CF_UPER1                           ,  120,    2,    18,     40,    0,     0,       0,       0 }, // Long Sword of vim
	{ 1743897351,  3 | CF_UPER1                           ,  146,    2,    10,     25,   60,    60,       0,       0 }, // Frog's Staff of Holy Bolt
	{  429107209,  0                                      ,    5,    0,    25,     25,    9,    40,       0,       0 }, // IDI_SORCERER
	{  466015738,  1 | CF_UPER1                           ,  146,    0,    18,     25,   50,    50,       0,       0 }, // Staff of Charged Bolt
	{  686949358,  1 | CF_UPER15 | CF_ONLYGOOD            ,   48,    3,    12,     15,    0,     0,       0,       0 }, // Cap of the mind armor
	{  888855755,  3 | CF_UPER15 | CF_ONLYGOOD            ,   58,    3,    30,     30,    0,     0,       0,       0 }, // Armor of protection
	{          2,  8 | CF_UPER1                | CF_UNIQUE,    8,    5,     0,      0,    0,     0,       0,       0 }, // Empyrean Band,
	{          3,  8 | CF_UPER1                | CF_UNIQUE,   10,    5,     0,      0,    0,     0,       0,       0 }, // Optic Amulet
	{          4,  4 | CF_UPER1                | CF_UNIQUE,   11,    5,     0,      0,    0,     0,       0,       0 }, // Ring of Truth
	{          5,  8 | CF_UPER1                | CF_UNIQUE,   13,    5,    15,     15,    0,     0,       0,       0 }, // Harlequin Crest
	{          6, 15                           | CF_UNIQUE,   14,    5,    60,     60,    0,     0,       0,       0 }, // Veil of Steel
	{          7, 13 | CF_UPER1                | CF_UNIQUE,   28,    5,    39,     40,    0,     0,       0,       0 }, // Arkaine's Valor
	{          8, 19 | CF_UPER1                | CF_UNIQUE,   31,    5,    42,     44,    0,     0,       0,       0 }, // Griswold's Edge
	{  557339094, 16             | CF_WITCH               ,  150,    3,    75,     75,    0,     0,       0,       0 }, // War Staff of haste
	{ 1684844665, 16             | CF_WITCH               ,  150,    3,    75,     75,   56,    56,       0,       0 }, // White Staff of Lightning
	{ 1297052552, 26             | CF_SMITHPREMIUM        ,  137,    3,    50,     50,    0,     0,       0,       0 }, // Lightning Maul
	{  981895960, 25             | CF_SMITHPREMIUM        ,  130,    3,    75,     75,    0,     0,       0,       0 }, // Ivory Great Axe of blood
	{  935416728, 22             | CF_SMITHPREMIUM        ,   52,    3,    18,     40,    0,     0,       0,       0 }, // Jade Crown of vim
	{ 1140525626,  1 | CF_UPER1                           ,  138,    3,    16,     30,    0,     0,       0,       0 }, // Short Bow of atrophy
	{ 1187758333,  2 | CF_UPER1                           ,  113,    3,    11,     16,    0,     0,       0,       0 }, // Brass Dagger of weakness
	{ 1283803700, 11 | CF_UPER1                           ,  138,    3,    16,     30,    0,     0,       0,       0 }, // Clumsy Short Bow
	{ 1317748726,  5 | CF_UPER1                           ,  114,    3,    17,     24,    0,     0,       0,       0 }, // Tin Sword of the fool
	{ 1331764286,  5 | CF_UPER1                           ,  135,    3,     6,     20,    0,     0,       0,       0 }, // Club of paralysis
	{ 1375639122, 13 | CF_UPER1                           ,  146,    3,    18,     25,   46,    46,       0,       0 }, // Dull Staff of Lightning
	{  145523894, 21 | CF_UPER1                           ,  115,    3,     6,     20,    0,     0,       0,       0 }, // Falchion of speed
	{ 1527777846,  3 | CF_UPER1                           ,  115,    3,    14,     20,    0,     0,       0,       0 }, // Bent Falchion
	{ 1655088363, 32 | CF_UPER1                           ,  146,    3,    13,     25,   98,    98,       0,       0 }, // Plentiful Staff of Firebolt
	{ 1679472538,  7 | CF_UPER1                           ,  113,    3,     6,     16,    0,     0,       0,       0 }, // Dagger of illness
	{ 1812092773,  8 | CF_UPER1                           ,   54,    3,     4,     12,    0,     0,       0,       0 }, // Cape of corruption
	{ 1965885799, 22 | CF_UPER1                           ,  119,    3,    33,     45,    0,     0,       0,       0 }, // Sabre of trouble
	{ 1970135469,  2 | CF_UPER1                           ,   48,    3,     5,     15,    0,     0,       0,       0 }, // Cap of tears
	{ 1979635474,  5 | CF_UPER1                           ,  135,    3,    14,     20,    0,     0,       0,       0 }, // Tin Club
	{ 2008721689,  2 | CF_UPER1                           ,   54,    3,     9,     12,    0,     0,       0,       0 }, // Rusted Cape of dyslexia
	{ 2082373839,  6 | CF_UPER1                           ,   48,    3,    10,     15,    0,     0,       0,       0 }, // Cap of pain
	{  278391972,  7 | CF_UPER1                           ,  119,    3,     1,      1,    0,     0,       0,       0 }, // Clumsy Sabre of fragility
	{  283130709,  4 | CF_UPER1                           ,   48,    3,     5,     15,    0,     0,       0,       0 }, // Vulnerable Cap of health
	{  308974695,  8 | CF_UPER1                           ,  113,    3,     5,     16,    0,     0,       0,       0 }, // Useless Dagger
	{  588501657, 44 | CF_UPER1                           ,  146,    3,    17,     25,   36,    36,       0,       0 }, // Bountiful Staff of Fire Wall
	{  640482348,  3 | CF_UPER1                           ,  131,    3,    22,     32,    0,     0,       0,       0 }, // Mace of frailty
	{  715324531,  2 | CF_UPER1                           ,  138,    3,    20,     30,    0,     0,       0,       0 }, // Weak Short Bow
	{  794222370,  3 | CF_UPER1                           ,  146,    3,    12,     25,    0,     0,       0,       0 }, // Short Staff of readiness
	{   49304273,  6                                      ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Healing
	{ 1015622844,  6                                      ,   29,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Full Healing
	{  376595272,  6                                      ,   25,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Mana
	{ 1354859033,  6                                      ,   30,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Full Mana
	{ 2088923858,  6                                      ,   27,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Town Portal
	{  998169832,  1 | CF_UPER1                | CF_UNIQUE,  138,    5,    30,     30,    0,     0,       0,       0 }, // The Rift Bow
	{  583181782, 30 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  124,    5,   100,    100,    0,     0,       0,       0 }, // The Grandfather
	{ 1488997713,  1 | CF_UPER1                           ,   76,    3,    17,     60,    0,     0,       0,       0 }, // Gothic Shield of thorns
	{ 2140415407, 19 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,   67,    5,    75,     75,    0,     0,       0,       0 }, // Naj's Light Plate
	{  601465140,  9 | CF_UPER1                | CF_UNIQUE,  151,    5,     0,      0,    0,     0,       0,       0 }, // Constricting Ring
	{ 1154984294, 24 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,   53,    5,    60,     60,    0,     0,       0,       0 }, // Gotterdamerung
	{  951404319, 28 | CF_UPER15 | CF_ONLYGOOD            ,  122,    3,    32,     60,    0,     0,       0,       0 }, // King's Bastard Sword
	{ 1452181783, 38             | CF_BOY                 ,  135,    3,    60,     60,    0,     0,       0,       0 }, // Champion's Club of gore
	{  846169922, 29             | CF_SMITHPREMIUM        ,  136,    3,    60,     60,    0,     0,       0,       0 }, // Master's Flail
	{   18974604, 30             | CF_SMITHPREMIUM        ,  121,    3,    60,     60,    0,     0,       0,       0 }, // Knight's Broad Sword
	{ 1535190647, 28 | CF_UPER1                           ,  115,    3,    13,     20,    0,     0,       0,       0 }, // Lord's Falchion of precision
	{  300540323, 30 | CF_UPER1                           ,  122,    3,    60,     60,    0,     0,       0,       0 }, // Soldier's Sword of vigor
	{ 1813930929, 10 | CF_UPER1                           ,  140,    3,    35,     35,    0,     0,       0,       0 }, // Fine Long Bow of the pit
	{  733908617,  1 | CF_UPER1                           ,  120,    3,    24,     40,    0,     0,       0,       0 }, // Sharp Sword of atrophy
	{ 1158010141, 63 | CF_UPER1                           ,  145,    3,    60,     60,    0,     0,       0,       0 }, // Emerald Bow of burning
	// clang-format on
};

constexpr ItemSpecialEffect EmpyreanBandSpecialEffect = ItemSpecialEffect::FastHitRecovery | ItemSpecialEffect::HalfTrapDamage;
constexpr ItemSpecialEffect GrisworldEdgeSpecialEffect = ItemSpecialEffect::FireDamage | ItemSpecialEffect::Knockback | ItemSpecialEffect::FastAttack;
constexpr ItemSpecialEffect FireArrows = ItemSpecialEffect::FireArrows | ItemSpecialEffect::FireDamage;

const TestItemStruct DiabloItems[] = {
	// clang-format off
	// _iIName,                       _itype,                 _iClass,  _iCurs,  _iIvalue,  _iMinDam,  _iMaxDam,  _iAC, _iFlags,                                 _iMiscId, _iSpell,               _iCharges,  _iMaxCharges,  _iDurability,  _iMaxDur,  _iPLDam,  _iPLToHit,  _iPLAC,  _iPLStr,  _iPLMag,  _iPLDex,  _iPLVit,  _iPLFR,  _iPLLR,  _iPLMR,  _iPLMana,  _iPLHP,  _iPLDamMod,  _iPLGetHit,  _iPLLight,  _iSplLvlAdd,  _iUid,  _iFMinDam,  _iFMaxDam,  _iLMinDam,  _iLMaxDam,  _iPrePower,  _iSufPower,  _iMinStr,  _iMinMag,  _iMinDex,  IDidx
	{ "Amber Helm of harmony",        ItemType::Helm,               2,      98,     21100,         0,         0,    11, ItemSpecialEffect::FastestHitRecovery,          0, SpellID::Null,                 0,             0,            60,        60,        0,          0,       0,        0,        0,        0,        0,      18,      18,      18,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          11,          59,        50,         0,         0,     53 },
	{ "Cobalt Amulet of giants",      ItemType::Amulet,             3,      45,     26840,         0,         0,     0, ItemSpecialEffect::None,                       26, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,       19,        0,        0,        0,       0,      46,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           9,          19,         0,         0,         0,    159 },
	{ "Brutal Long Sword of gore",    ItemType::Sword,              1,      60,     13119,         2,        10,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            38,        40,       91,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,          12,           0,          0,            0,      0,          0,          0,          0,          0,           2,          61,        30,         0,        30,    125 },
	{ "Demonspike Coat",              ItemType::HeavyArmor,         2,     151,    251175,         0,         0,   100, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,       10,        0,        0,        0,      50,       0,       0,         0,       0,           0,          -6,          0,            0,     78,          0,          0,          0,          0,          -1,          -1,        90,         0,         0,     70 },
	{ "Steel Ring of the jaguar",     ItemType::Ring,               3,      12,     10600,         0,         0,     0, ItemSpecialEffect::None,                       25, SpellID::Null,                 0,             0,             0,         0,        0,         15,       0,        0,        0,        0,        0,       0,       0,       0,         0,    1024,           0,           0,          0,            0,      0,          0,          0,          0,          0,           0,          31,         0,         0,         0,    156 },
	{ "Ring of the heavens",          ItemType::Ring,               3,      12,     37552,         0,         0,     0, ItemSpecialEffect::None,                       25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,       14,       14,       14,       14,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          27,         0,         0,         0,    156 },
	{ "Ring of sorcery",              ItemType::Ring,               3,      12,     10200,         0,         0,     0, ItemSpecialEffect::None,                       25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,       16,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          21,         0,         0,         0,    158 },
	{ "Tower Shield of the ages",     ItemType::Shield,             2,     132,      4850,         0,         0,    18, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          37,        60,         0,         0,     75 },
	{ "Sapphire Large Shield",        ItemType::Shield,             2,     147,     21000,         0,         0,     7, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            12,        32,        0,          0,       0,        0,        0,        0,        0,       0,      60,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           9,          -1,        40,         0,         0,     73 },
	{ "Scroll of Town Portal",        ItemType::Misc,               3,       1,       200,         0,         0,     0, ItemSpecialEffect::None,                       21, SpellID::TownPortal,           0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     27 },
	{ "Potion of Mana",               ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                        6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	{ "Potion of Mana",               ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                        6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     79 },
	{ "Potion of Full Mana",          ItemType::Misc,               3,       0,       150,         0,         0,     0, ItemSpecialEffect::None,                        7, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     30 },
	{ "Potion of Healing",            ItemType::Misc,               3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,                        3, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	{ "Potion of Full Healing",       ItemType::Misc,               3,      35,       150,         0,         0,     0, ItemSpecialEffect::None,                        2, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     29 },
	{ "Short Bow",                    ItemType::Bow,                1,     118,       100,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            30,        30,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      4 },
	{ "Jade Great Helm of the wolf",  ItemType::Helm,               2,      98,     22310,         0,         0,    14, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            56,        60,        0,          0,       0,        0,        0,        0,        0,      27,      27,      27,         0,    2112,           0,           0,          0,            0,      0,          0,          0,          0,          0,          11,          31,        50,         0,         0,     53 },
	{ "Steel Ring of accuracy",       ItemType::Ring,               3,      12,     13400,         0,         0,     0, ItemSpecialEffect::None,                       25, SpellID::Null,                 0,             0,             0,         0,        0,         14,       0,        0,        0,       15,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           0,          23,         0,         0,         0,    156 },
	{ "Blood Stone",                  ItemType::Misc,               5,      25,         0,         0,         0,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     21 },
	{ "Ring of power",                ItemType::Ring,               3,      12,      6400,         0,         0,     0, ItemSpecialEffect::None,                       25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,       12,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          19,         0,         0,         0,    156 },
	{ "Gold Amulet of accuracy",      ItemType::Amulet,             3,      45,     20896,         0,         0,     0, ItemSpecialEffect::None,                       26, SpellID::Null,                 0,             0,             0,         0,        0,         25,       0,        0,        0,       14,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           0,          23,         0,         0,         0,    159 },
	{ "Bastard Sword of the bat",     ItemType::Sword,              1,      57,     10500,         6,        15,     0, ItemSpecialEffect::StealMana3,                  0, SpellID::Null,                 0,             0,            25,        60,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          55,        50,         0,         0,    127 },
	{ "Small Shield",                 ItemType::Shield,             2,     105,        90,         0,         0,     3, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            14,        24,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,        25,         0,         0,     72 },
	{ "Breast Plate of giants",       ItemType::HeavyArmor,         2,     153,     23250,         0,         0,    20, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            44,        80,        0,          0,       0,       17,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          19,        40,         0,         0,     65 },
	{ "Scroll of Healing",            ItemType::Misc,               3,       1,        50,         0,         0,     0, ItemSpecialEffect::None,                       21, SpellID::Healing,              0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     91 },
	{ "Potion of Rejuvenation",       ItemType::Misc,               3,      37,       120,         0,         0,     0, ItemSpecialEffect::None,                       18, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     81 },
	{ "Potion of Rejuvenation",       ItemType::Misc,               3,      37,       120,         0,         0,     0, ItemSpecialEffect::None,                       18, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     81 },
	{ "Potion of Full Rejuvenation",  ItemType::Misc,               3,      33,       600,         0,         0,     0, ItemSpecialEffect::None,                       19, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     82 },
	{ "Savage Bow of perfection",     ItemType::Bow,                1,     133,     23438,         3,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            45,        45,      117,          0,       0,        0,        0,       22,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           2,          23,        25,         0,        40,    146 },
	{ "Falchion",                     ItemType::Sword,              1,      62,       250,         4,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            10,        20,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,        30,         0,         0,    120 },
	{ "Long Sword of vim",            ItemType::Sword,              1,      60,      4400,         2,        10,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            18,        40,        0,          0,       0,        0,        0,        0,       15,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          25,        30,         0,        30,    125 },
	{ "Frog's Staff of Holy Bolt",    ItemType::Staff,              1,     109,         1,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::HolyBolt,            60,            60,            10,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,      -384,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          34,          -1,         0,        20,         0,    151 },
	{ "Short Staff of Charged Bolt",  ItemType::Staff,              1,     109,       470,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::ChargedBolt,          9,            40,            25,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        25,         0,    166 },
	{ "Short Staff of Charged Bolt",  ItemType::Staff,              1,     109,         1,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::ChargedBolt,         50,            50,            18,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        25,         0,    151 },
	{ "Cap of the mind",              ItemType::Helm,               2,      91,      1845,         0,         0,     2, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            12,        15,        0,          0,       0,        0,        9,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          21,         0,         0,         0,     48 },
	{ "Quilted Armor of protection",  ItemType::LightArmor,         2,     129,      1200,         0,         0,     7, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            30,        30,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,          -2,          0,            0,      0,          0,          0,          0,          0,          -1,          30,         0,         0,         0,     58 },
	{ "Empyrean Band",                ItemType::Ring,               3,      18,      8000,         0,         0,     0, EmpyreanBandSpecialEffect,                     27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        2,        2,        2,        2,       0,       0,       0,         0,       0,           0,           0,          2,            0,      2,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      8 },
	{ "Optic Amulet",                 ItemType::Amulet,             3,      44,      9750,         0,         0,     0, ItemSpecialEffect::None,                       27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        5,        0,        0,       0,      20,       0,         0,       0,           0,          -1,          2,            0,      3,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     10 },
	{ "Ring of Truth",                ItemType::Ring,               3,      10,      9100,         0,         0,     0, ItemSpecialEffect::None,                       27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,      10,      10,      10,         0,     640,           0,          -1,          0,            0,      4,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     11 },
	{ "Harlequin Crest",              ItemType::Helm,               2,      81,      4000,         0,         0,    -3, ItemSpecialEffect::None,                       27, SpellID::Null,                 0,             0,            15,        15,        0,          0,       0,        2,        2,        2,        2,       0,       0,       0,       448,     448,           0,          -1,          0,            0,      5,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     13 },
	{ "Veil of Steel",                ItemType::Helm,               2,      85,     63800,         0,         0,    18, ItemSpecialEffect::None,                       27, SpellID::Null,                 0,             0,            60,        60,        0,          0,      60,       15,        0,        0,       15,      50,      50,      50,     -1920,       0,           0,           0,         -2,            0,      6,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     14 },
	{ "Arkaine's Valor",              ItemType::MediumArmor,        2,     157,     42000,         0,         0,    25, ItemSpecialEffect::FastestHitRecovery,         27, SpellID::Null,                 0,             0,            39,        40,        0,          0,       0,        0,        0,        0,       10,       0,       0,       0,         0,       0,           0,          -3,          0,            0,      7,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     28 },
	{ "Griswold's Edge",              ItemType::Sword,              1,      61,     42000,         4,        12,     0, GrisworldEdgeSpecialEffect,                    27, SpellID::Null,                 0,             0,            42,        44,        0,         25,       0,        0,        0,        0,        0,       0,       0,       0,      1280,   -1280,           0,           0,          0,            0,      8,          1,         10,          0,          0,          -1,          -1,        40,         0,         0,     31 },
	{ "War Staff of haste",           ItemType::Staff,              1,     124,     40000,         8,        16,     0, ItemSpecialEffect::FastestAttack,              23, SpellID::Null,                 0,             0,            75,        75,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          58,        30,         0,         0,    155 },
	{ "White Staff of Lightning",     ItemType::Staff,              1,     124,      7160,         8,        16,     0, ItemSpecialEffect::None,                       23, SpellID::Lightning,           56,            56,            75,        75,        0,          0,       0,        0,        0,        0,        0,       0,       0,      13,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          10,          -1,        30,        20,         0,    155 },
	{ "Lightning Maul",               ItemType::Mace,               1,     122,     11800,         6,        20,     0, ItemSpecialEffect::LightningDamage,             0, SpellID::Null,                 0,             0,            50,        50,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          2,         20,          17,          -1,        55,         0,         0,    142 },
	{ "Ivory Great Axe of blood",     ItemType::Axe,                1,     143,     31194,        12,        30,     0, ItemSpecialEffect::StealLife5,                  0, SpellID::Null,                 0,             0,            75,        75,        0,          0,       0,        0,        0,        0,        0,       0,       0,      37,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          10,          56,        80,         0,         0,    135 },
	{ "Jade Crown of vim",            ItemType::Helm,               2,      95,     19200,         0,         0,    10, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            18,        40,        0,          0,       0,        0,        0,        0,       14,      30,      30,      30,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          11,          25,         0,         0,         0,     52 },
	{ "Short Bow of atrophy",         ItemType::Bow,                1,     118,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            16,        30,        0,          0,       0,        0,        0,       -5,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          24,         0,         0,         0,    143 },
	{ "Brass Dagger of weakness",     ItemType::Sword,              1,      51,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            11,        16,        0,         -1,       0,       -5,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           1,          20,         0,         0,         0,    118 },
	{ "Clumsy Short Bow",             ItemType::Bow,                1,     118,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            16,        30,      -67,         -8,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           5,          -1,         0,         0,         0,    143 },
	{ "Tin Short Sword of the fool",  ItemType::Sword,              1,      64,         1,         2,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            17,        24,        0,         -7,       0,        0,       -9,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           1,          22,        18,         0,         0,    119 },
	{ "Club of paralysis",            ItemType::Mace,               1,      66,         1,         1,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             6,        20,        0,          0,       0,        0,        0,       -9,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          24,         0,         0,         0,    140 },
	{ "Dull Staff of Lightning",      ItemType::Staff,              1,     109,         1,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::Lightning,           46,            46,            18,        25,      -28,         -1,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           5,          -1,         0,        20,         0,    151 },
	{ "Falchion of speed",            ItemType::Sword,              1,      62,     10000,         4,         8,     0, ItemSpecialEffect::FasterAttack,                0, SpellID::Null,                 0,             0,             6,        20,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          58,        30,         0,         0,    120 },
	{ "Bent Falchion",                ItemType::Sword,              1,      62,         1,         4,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            14,        20,      -68,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           3,          -1,        30,         0,         0,    120 },
	{ "Plentiful Staff of Firebolt",  ItemType::Staff,              1,     109,      3040,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::Firebolt,            98,            98,            13,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          15,          -1,         0,        15,         0,    151 },
	{ "Dagger of illness",            ItemType::Sword,              1,      51,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             6,        16,        0,          0,       0,        0,        0,        0,       -8,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          26,         0,         0,         0,    118 },
	{ "Cape of corruption",           ItemType::LightArmor,         2,     150,         1,         0,         0,     3, ItemSpecialEffect::NoMana,                      0, SpellID::Null,                 0,             0,             4,        12,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          46,         0,         0,         0,     54 },
	{ "Sabre of trouble",             ItemType::Sword,              1,      67,         1,         1,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            33,        45,        0,          0,       0,       -6,       -6,       -6,       -6,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          28,        17,         0,         0,    124 },
	{ "Cap of tears",                 ItemType::Helm,               2,      91,         1,         0,         0,     2, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             5,        15,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           1,          0,            0,      0,          0,          0,          0,          0,          -1,          29,         0,         0,         0,     48 },
	{ "Tin Club",                     ItemType::Mace,               1,      66,         1,         1,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            14,        20,        0,         -8,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           1,          -1,         0,         0,         0,    140 },
	{ "Rusted Cape of dyslexia",      ItemType::LightArmor,         2,     150,         1,         0,         0,     2, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             9,        12,        0,          0,     -34,        0,       -4,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           7,          22,         0,         0,         0,     54 },
	{ "Cap of pain",                  ItemType::Helm,               2,      91,         1,         0,         0,     2, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            10,        15,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           2,          0,            0,      0,          0,          0,          0,          0,          -1,          29,         0,         0,         0,     48 },
	{ "Clumsy Sabre of fragility",    ItemType::Sword,              1,      67,         1,         1,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             1,         1,      -75,        -10,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           5,          36,        17,         0,         0,    124 },
	{ "Vulnerable Cap of health",     ItemType::Helm,               2,      91,       185,         0,         0,     1, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             5,        15,        0,          0,     -63,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,          -1,          0,            0,      0,          0,          0,          0,          0,           7,          30,         0,         0,         0,     48 },
	{ "Useless Dagger",               ItemType::Sword,              1,      51,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,             5,        16,     -100,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           3,          -1,         0,         0,         0,    118 },
	{ "Bountiful Staff of Fire Wall", ItemType::Staff,              1,     109,      5970,         2,         4,     0, ItemSpecialEffect::None,                       23, SpellID::FireWall,            36,            36,            17,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          15,          -1,         0,        27,         0,    151 },
	{ "Mace of frailty",              ItemType::Mace,               1,      59,         1,         1,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            22,        32,        0,          0,       0,       -7,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          20,        16,         0,         0,    136 },
	{ "Weak Short Bow",               ItemType::Bow,                1,     118,         1,         1,         4,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            20,        30,      -44,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           3,          -1,         0,         0,         0,    143 },
	{ "Short Staff of readiness",     ItemType::Staff,              1,     109,      2060,         2,         4,     0, ItemSpecialEffect::QuickAttack,                23, SpellID::Null,                 0,             0,            12,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          58,         0,         0,         0,    151 },
	{ "Potion of Healing",            ItemType::Misc,               3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,                        3, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	{ "Potion of Full Healing",       ItemType::Misc,               3,      35,       150,         0,         0,     0, ItemSpecialEffect::None,                        2, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     29 },
	{ "Potion of Mana",               ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                        6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	{ "Potion of Full Mana",          ItemType::Misc,               3,       0,       150,         0,         0,     0, ItemSpecialEffect::None,                        7, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     30 },
	{ "Scroll of Town Portal",        ItemType::Misc,               3,       1,       200,         0,         0,     0, ItemSpecialEffect::None,                       21, SpellID::TownPortal,           0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     27 },
	{ "The Rift Bow",                 ItemType::Bow,                1,     118,      1800,         1,         4,     0, ItemSpecialEffect::RandomArrowVelocity,         0, SpellID::Null,                 0,             0,            30,        30,        0,          0,       0,        0,        0,       -3,        0,       0,       0,       0,         0,       0,           2,           0,          0,            0,     10,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    143 },
	{ "The Grandfather",              ItemType::Sword,              1,     161,    119800,        10,        20,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,           100,       100,       70,         20,       0,        5,        5,        5,        5,       0,       0,       0,         0,    1280,           0,           0,          0,            0,     35,          0,          0,          0,          0,          -1,          -1,        75,         0,         0,    129 },
	{ "Gothic Shield of thorns",      ItemType::Shield,             2,     148,      5100,         0,         0,    18, ItemSpecialEffect::Thorns,                      0, SpellID::Null,                 0,             0,            17,        60,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          45,        80,         0,         0,     76 },
	{ "Naj's Light Plate",            ItemType::HeavyArmor,         2,     159,     78700,         0,         0,    50, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            75,        75,        0,          0,       0,        0,        5,        0,        0,      20,      20,      20,      1280,       0,           0,           0,          0,            1,     77,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     67 },
	{ "Constricting Ring",            ItemType::Ring,               3,      14,     62000,         0,         0,     0, ItemSpecialEffect::DrainLife,                  25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,      75,      75,      75,         0,       0,           0,           0,          0,            0,     88,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    156 },
	{ "Gotterdamerung",               ItemType::Helm,               2,      85,     54900,         0,         0,    60, ItemSpecialEffect::ZeroResistance,              0, SpellID::Null,                 0,             0,            60,        60,        0,          0,       0,       20,       20,       20,       20,       0,       0,       0,         0,       0,           0,          -4,         -4,            0,     67,          0,          0,          0,          0,          -1,          -1,        50,         0,         0,     53 },
	{ "King's Bastard Sword",         ItemType::Sword,              1,      57,     69730,         6,        15,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            32,        60,      168,         95,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          -1,        50,         0,         0,    127 },
	{ "Champion's Club of gore",      ItemType::Mace,               1,      66,     25576,         1,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            60,        60,      141,         75,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           9,           0,          0,            0,      0,          0,          0,          0,          0,           4,          61,         0,         0,         0,    140 },
	{ "Master's Flail",               ItemType::Mace,               1,     131,     27340,         2,        12,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            60,        60,      123,         44,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          -1,        30,         0,         0,    141 },
	{ "Knight's Broad Sword",         ItemType::Sword,              1,      61,     27597,         4,        12,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            60,        60,      108,         37,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          -1,        40,         0,         0,    126 },
	{ "Lord's Falchion of precision", ItemType::Sword,              1,      62,     17025,         4,         8,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            13,        20,       88,         23,       0,        0,        0,       20,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          23,        30,         0,         0,    120 },
	{ "Soldier's Sword of vigor",     ItemType::Sword,              1,      57,     31600,         6,        15,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            60,        60,       66,         19,       0,        0,        0,        0,       20,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          25,        50,         0,         0,    127 },
	{ "Fine Long Bow of the pit",     ItemType::Bow,                1,     102,      2152,         1,         6,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            35,        35,       49,         10,       0,       -2,       -2,       -2,       -2,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          28,        25,         0,        30,    145 },
	{ "Sharp Sword of atrophy",       ItemType::Sword,              1,      60,      1958,         2,        10,     0, ItemSpecialEffect::None,                        0, SpellID::Null,                 0,             0,            24,        40,       34,          4,       0,        0,        0,       -1,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          24,        30,         0,        30,    125 },
	{ "Emerald Bow of burning",       ItemType::Bow,                1,     120,    107000,         1,        14,     0, FireArrows,                                     0, SpellID::Null,                 0,             0,            60,        60,        0,          0,       0,        0,        0,        0,        0,      50,      50,      50,         0,       0,           0,           0,          0,            0,      0,          1,         16,          0,          0,          11,          42,        45,         0,        80,    150 },
	// clang-format on
};

class PackTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
	}

	static void SetUpTestSuite()
	{
		LoadSpellData();
		LoadItemData();
	}
};

TEST_F(PackTest, UnPackItem_diablo)
{
	Item id;
	ItemPack is;

	gbIsHellfire = false;
	gbIsMultiplayer = false;
	gbIsSpawn = false;

	MyPlayer->_pMaxManaBase = 125 << 6;
	MyPlayer->_pMaxHPBase = 125 << 6;

	for (size_t i = 0; i < sizeof(PackedDiabloItems) / sizeof(*PackedDiabloItems); i++) {
		const ItemPack packed = SwappedLE(PackedDiabloItems[i]);
		UnPackItem(packed, *MyPlayer, id, false);
		CompareItems(id, DiabloItems[i]);
		TestItemNameGeneration(id);

		PackItem(is, id, gbIsHellfire);
		ComparePackedItems(is, packed);
	}
}

TEST_F(PackTest, UnPackItem_diablo_unique_bug)
{
	const auto pkItemBug = SwappedLE(ItemPack { 6, 15 | CF_UPER1 | CF_UPER15 | CF_UNIQUE, 14, 5, 60, 60, 0, 0, 0, 0 }); // Veil of Steel - with morph bug
	const auto pkItem = SwappedLE(ItemPack { 6, 15 | CF_UPER15 | CF_UNIQUE, 14, 5, 60, 60, 0, 0, 0, 0 });               // Veil of Steel - fixed

	gbIsHellfire = false;
	gbIsMultiplayer = false;
	gbIsSpawn = false;

	Item id;
	UnPackItem(pkItemBug, *MyPlayer, id, false);
	ASSERT_STREQ(id._iIName, "Veil of Steel");
	ASSERT_EQ(id._itype, ItemType::Helm);
	ASSERT_EQ(id._iClass, ICLASS_ARMOR);
	ASSERT_EQ(id._iCurs, 85);
	ASSERT_EQ(id._iIvalue, 63800);
	ASSERT_EQ(id._iAC, 18);
	ASSERT_EQ(id._iMiscId, IMISC_UNIQUE);
	ASSERT_EQ(id._iPLAC, 60);
	ASSERT_EQ(id._iPLStr, 15);
	ASSERT_EQ(id._iPLVit, 15);
	ASSERT_EQ(id._iPLFR, 50);
	ASSERT_EQ(id._iPLLR, 50);
	ASSERT_EQ(id._iPLMR, 50);
	ASSERT_EQ(id._iPLMana, -1920);
	ASSERT_EQ(id._iPLLight, -2);
	ASSERT_EQ(id._iUid, 6);
	ASSERT_EQ(id.IDidx, IDI_STEELVEIL);

	ItemPack is;
	PackItem(is, id, gbIsHellfire);
	ComparePackedItems(is, pkItem);
}

const ItemPack PackedSpawnItems[] = {
	// clang-format off
	//     iSeed,  iCreateInfo,  idx,  bId,  bDur,  bMDur,  bCh,  bMCh,  wValue,  dwBuff
	{ 2060036013, 1 | CF_UPER1,  131,    0,    11,     25,   50,    50,       0,       0 }, // Short Staff of Firebolt
	{   81574809, 2 | CF_UPER1,   94,    0,     0,      0,    0,     0,       0,       0 }, // Book of Holy Bolt
	// clang-format on
};

const TestItemStruct SpawnItems[] = {
	// clang-format off
	// _iIName,                  _itype,           _iClass,  _iCurs,  _iIvalue,  _iMinDam,  _iMaxDam,  _iAC, _iFlags,                  _iMiscId, _iSpell,            _iCharges,  _iMaxCharges,  _iDurability,  _iMaxDur,  _iPLDam,  _iPLToHit,  _iPLAC,  _iPLStr,  _iPLMag,  _iPLDex,  _iPLVit,  _iPLFR,  _iPLLR,  _iPLMR,  _iPLMana,  _iPLHP,  _iPLDamMod,  _iPLGetHit,  _iPLLight,  _iSplLvlAdd,  _iUid,  _iFMinDam,  _iFMaxDam,  _iLMinDam,  _iLMaxDam,  _iPrePower,  _iSufPower,  _iMinStr,  _iMinMag,  _iMinDex,  IDidx
	{ "Short Staff of Firebolt", ItemType::Staff,        1,     109,         1,         2,         4,     0, ItemSpecialEffect::None,        23, SpellID::Firebolt,         50,            50,            11,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        15,         0,    151 },
	{ "Book of Holy Bolt",       ItemType::Misc,         3,      86,      1000,         0,         0,     0, ItemSpecialEffect::None,        24, SpellID::HolyBolt,          0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        20,         0,    114 },
	// clang-format on
};

TEST_F(PackTest, UnPackItem_spawn)
{
	Item id;
	ItemPack is;

	gbIsHellfire = false;
	gbIsMultiplayer = false;
	gbIsSpawn = true;

	MyPlayer->_pMaxManaBase = 125 << 6;
	MyPlayer->_pMaxHPBase = 125 << 6;

	for (size_t i = 0; i < sizeof(PackedSpawnItems) / sizeof(*PackedSpawnItems); i++) {
		const ItemPack packed = SwappedLE(PackedSpawnItems[i]);
		UnPackItem(packed, *MyPlayer, id, false);
		CompareItems(id, SpawnItems[i]);
		TestItemNameGeneration(id);

		PackItem(is, id, gbIsHellfire);
		ComparePackedItems(is, packed);
	}
}

const ItemPack PackedDiabloMPItems[] = {
	// clang-format off
	//     iSeed,                 iCreateInfo,  idx,  bId,  bDur,  bMDur,  bCh,  bMCh,  wValue,  dwBuff
	{  309674341, 1 | CF_UPER15 | CF_ONLYGOOD,  109,    0,     0,      0,    0,     0,       0,       0 }, // Book of Firebolt
	{ 1291471654, 6                          ,   34,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Resurrect
	{ 1580941742, 1 | CF_USEFUL              ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Healing
	{  467997257, 4 | CF_USEFUL              ,   27,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Town Portal
	{  796933756, 1 | CF_USEFUL              ,   25,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Mana
	// clang-format on
};

const TestItemStruct DiabloMPItems[] = {
	// clang-format off
	// _iIName,                _itype,          _iClass,  _iCurs,  _iIvalue,  _iMinDam,  _iMaxDam,  _iAC, _iFlags,                  _iMiscId, _iSpell,              _iCharges,  _iMaxCharges,  _iDurability,  _iMaxDur,  _iPLDam,  _iPLToHit,  _iPLAC,  _iPLStr,  _iPLMag,  _iPLDex,  _iPLVit,  _iPLFR,  _iPLLR,  _iPLMR,  _iPLMana,  _iPLHP,  _iPLDamMod,  _iPLGetHit,  _iPLLight,  _iSplLvlAdd,  _iUid,  _iFMinDam,  _iFMaxDam,  _iLMinDam,  _iLMaxDam,  _iPrePower,  _iSufPower,  _iMinStr,  _iMinMag,  _iMinDex,  IDidx
	{ "Book of Firebolt",      ItemType::Misc,        3,      87,      1000,         0,         0,     0, ItemSpecialEffect::None,        24, SpellID::Firebolt,            0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        15,         0,    114 },
	{ "Scroll of Resurrect",   ItemType::Misc,        3,       1,       250,         0,         0,     0, ItemSpecialEffect::None,        22, SpellID::Resurrect,           0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     34 },
	{ "Potion of Healing",     ItemType::Misc,        3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,         3, SpellID::Null,                0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	{ "Scroll of Town Portal", ItemType::Misc,        3,       1,       200,         0,         0,     0, ItemSpecialEffect::None,        21, SpellID::TownPortal,          0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     27 },
	{ "Potion of Mana",        ItemType::Misc,        3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,         6, SpellID::Null,                0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	// clang-format on
};

TEST_F(PackTest, UnPackItem_diablo_multiplayer)
{
	Item id;
	ItemPack is;

	gbIsHellfire = false;
	gbIsMultiplayer = true;
	gbIsSpawn = false;

	MyPlayer->_pMaxManaBase = 125 << 6;
	MyPlayer->_pMaxHPBase = 125 << 6;

	for (size_t i = 0; i < sizeof(PackedDiabloMPItems) / sizeof(*PackedDiabloMPItems); i++) {
		const ItemPack packed = SwappedLE(PackedDiabloMPItems[i]);
		UnPackItem(packed, *MyPlayer, id, false);
		CompareItems(id, DiabloMPItems[i]);
		TestItemNameGeneration(id);

		PackItem(is, id, gbIsHellfire);
		ComparePackedItems(is, packed);
	}
}

const ItemPack PackedHellfireItems[] = {
	// clang-format off
	//     iSeed,                              iCreateInfo,  idx,  bId,  bDur,  bMDur,  bCh,  bMCh,  wValue,  dwBuff
	{ 1717442367, 10 | CF_UPER1                           ,  156,    3,     0,      0,    0,     0,       0,       0 }, // Ring of stability
	{ 1268518156, 18 | CF_UPER1  | CF_ONLYGOOD            ,  157,    3,     0,      0,    0,     0,       0,       0 }, // Ring of precision
	{  132733863, 27 | CF_UPER1                           ,  157,    3,     0,      0,    0,     0,       0,       0 }, // Obsidian Ring of wizardry
	{  511953594, 27 | CF_UPER1                           ,  158,    3,     0,      0,    0,     0,       0,       0 }, // Ring of precision
	{ 1183326923, 26 | CF_UPER1  | CF_ONLYGOOD            ,  160,    3,     0,      0,    0,     0,       0,       0 }, // Amulet of titans
	{ 1863009736, 24 | CF_UPER1                           ,  160,    3,     0,      0,    0,     0,       0,       0 }, // Gold Amulet
	{ 1872860650, 30 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  135,    5,    75,     75,    0,     0,       0,       0 }, // Messerschmidt's Reaver
	{ 1584694222, 25 | CF_UPER1                           ,  142,    3,   127,    128,    0,     0,       0,       0 }, // Vicious Maul of structure
	{  669112929, 24 | CF_UPER1                           ,  119,    0,    15,     24,    0,     0,       0,       0 }, // Short Sword
	{  303108965, 24 | CF_UPER1                           ,  148,    3,    18,     50,    0,     0,       0,       0 }, // Long Battle Bow of shock
	{  575830996,  1 | CF_UPER1                           ,  143,    3,    30,     30,    0,     0,       0,       0 }, // Short Bow of magic
	{ 1488880650,  2 | CF_UPER15 | CF_ONLYGOOD            ,  152,    3,    35,     35,   22,    33,       0,       0 }, // Red Long Staff of Healing
	{ 1864450901,  7 | CF_UPER1                           ,   71,    0,     6,     16,    0,     0,       0,       0 }, // Buckler
	{   28387651,  7 | CF_UPER1                           ,   49,    0,    15,     20,    0,     0,       0,       0 }, // Skull Cap
	{ 1298183212,  1 | CF_UPER1                           ,   55,    0,     6,      6,    0,     0,       0,       0 }, // Rags
	{ 1113945523,  4 | CF_UPER1                           ,   58,    0,    30,     30,    0,     0,       0,       0 }, // Quilted Armor
	{  765757608,  4 | CF_UPER1                           ,   58,    2,    12,     30,    0,     0,       0,       0 }, // Quilted Armor of light
	{  188812770, 26 | CF_UPER1  | CF_ONLYGOOD            ,   67,    3,    75,     75,    0,     0,       0,       0 }, // Saintly Plate of the stars
	{  283577043, 22             | CF_SMITHPREMIUM        ,   67,    3,    63,     75,    0,     0,       0,       0 }, // Plate Mail of the stars
	{  123272767, 16                                      ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Healing
	{  433688373, 16                                      ,   29,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Full Healing
	{ 1213385484,  2             | CF_PREGEN              ,   25,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Mana
	{ 1405075219, 24 | CF_UPER1                           ,  110,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Golem
	{ 1478792102,  3 | CF_UPER1                           ,   92,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Search
	{ 1569255955,  6 | CF_UPER1                           ,   94,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Identify
	{ 1291205782,  5 | CF_UPER1                           ,   98,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Town Portal
	{  811925807,  4 | CF_UPER1                           ,   91,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Healing
	{ 1275007287,  1 | CF_UPER1                           ,  161,    0,     0,      0,    0,     0,       0,       0 }, // Rune of Fire
	{  561216242, 22 | CF_UPER1                           ,    0,    0,     0,      0,    0,     0,    1663,       0 }, // Gold
	{          1,  3                           | CF_UNIQUE,    7,    5,    45,     50,    0,     0,       0,       0 }, // The Undead Crown
	{          2,  6 | CF_UPER1                | CF_UNIQUE,    8,    5,     0,      0,    0,     0,       0,       0 }, // Empyrean Band
	{          4,  1 | CF_UPER1                | CF_UNIQUE,   11,    5,     0,      0,    0,     0,       0,       0 }, // Ring of Truth
	{          8,  0                           | CF_UNIQUE,   31,    5,    50,     50,    0,     0,       0,       0 }, // Griswold's Edge
	{          9, 18 | CF_UPER1  | CF_ONLYGOOD | CF_UNIQUE,   32,    5,   255,    255,    0,     0,       0,       0 }, // Bovine Plate
	{  410929431,  2 | CF_UPER1                           ,  114,    0,     0,      0,    0,     0,       0,       0 }, // Book of Healing
	{  876535546,  2 | CF_UPER1                           ,  114,    0,     0,      0,    0,     0,       0,       0 }, // Book of Charged Bolt
	{ 1009350361,  2 | CF_UPER1                           ,  114,    0,     0,      0,    0,     0,       0,       0 }, // Book of Firebolt
	{   41417651,  2 | CF_UPER1                           ,   83,    0,     0,      0,    0,     0,       0,       0 }, // Blacksmith Oil
	{  132200437,  2 | CF_UPER1                           ,   84,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Accuracy
	{  385651490,  1 | CF_UPER1                           ,   85,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Sharpness
	{ 1154514759, 34 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Permanence
	{ 2020998927, 18             | CF_SMITHPREMIUM        ,  131,    3,    23,     32,    0,     0,       0,       0 }, // Doppelganger's Axe
	{  581541889, 19             | CF_SMITHPREMIUM        ,  141,    3,    36,     36,    0,     0,       0,       0 }, // Flail of vampires
	{ 1069448901, 12 | CF_UPER1  | CF_ONLYGOOD | CF_UNIQUE,  157,    5,     0,      0,    0,     0,       0,       0 }, // Gladiator's Ring
	{ 1670063399, 20             | CF_SMITHPREMIUM        ,  155,    3,    75,     75,    0,     0,       0,       0 }, // Warrior's Staff of the moon
	{  342570085, 18             | CF_BOY                 ,   74,    3,   255,    255,    0,     0,       0,       0 }, // Kite Shield of the ages
	{ 1514523617, 18             | CF_SMITHPREMIUM        ,  139,    3,    20,     20,    0,     0,       0,       0 }, // Heavy Club of puncturing
	{  701987341, 16             | CF_WITCH               ,  114,    1,     0,      0,    0,     0,       0,       0 }, // Book of Lightning
	{  568338383,  4 | CF_UPER15 | CF_ONLYGOOD            ,  124,    3,    23,     45,    0,     0,       0,       0 }, // Jester's Sabre
	{ 1308277119,  8             | CF_SMITHPREMIUM        ,   72,    3,    24,     24,    0,     0,       0,       0 }, // Small Shield of blocking
	{          0,  0                           | CF_UNIQUE,    6,    5,    10,     10,    0,     0,       0,       0 }, // The Butcher's Cleaver
	{ 1621745295,  9             | CF_SMITHPREMIUM        ,  121,    3,    28,     28,    0,     0,       0,       0 }, // Scimitar of peril
	{  492619876,  6             | CF_SMITHPREMIUM        ,  132,    3,    12,     12,    0,     0,       0,       0 }, // Crystalline Large Axe
	{ 1859493982,  5             | CF_SMITHPREMIUM        ,   56,    3,    18,     18,    0,     0,       0,       0 }, // Red Cloak
	{ 1593032051,  2             | CF_SMITHPREMIUM        ,  136,    3,    32,     32,    0,     0,       0,       0 }, // Mace of decay
	{          4,  0                           | CF_UNIQUE,   11,    5,     0,      0,    0,     0,       0,       0 }, // Ring of Truth
	{ 1500728519,  4 | CF_UPER1                           ,   61,    3,    18,     45,    0,     0,       0,       0 }, // Red Armor of paralysis
	{  954183925,  5 | CF_UPER1                           ,  144,    3,    26,     40,    0,     0,       0,       0 }, // Bent Hunter's Bow
	{  438248055,  7 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  136,    5,    32,     32,    0,     0,       0,       0 }, // Civerb's Cudgel
	{ 1133027011,  8 | CF_UPER1  | CF_ONLYGOOD            ,  139,    3,     8,     20,    0,     0,       0,       0 }, // Deadly Spiked Club
	{  224835143, 28 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  144,    5,   255,    255,    0,     0,       0,       0 }, // Gnat Sting
	{ 1498080548, 28 | CF_UPER1                | CF_UNIQUE,  138,    5,   255,    255,    0,     0,       0,       0 }, // Thunderclap
	{ 1218409601, 24 | CF_UPER1                | CF_UNIQUE,  155,    5,    75,     75,   50,    50,       0,       0 }, // Rod of Onan
	{ 1228950066,  7 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  146,    5,   255,    255,    0,     0,       0,       0 }, // Flambeau
	{  863852923,  5 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  156,    5,     0,      0,    0,     0,       0,       0 }, // Ring of Thunder
	{   89183927, 12 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  159,    5,     0,      0,    0,     0,       0,       0 }, // Acolyte's Amulet
	{  858625694, 28 | CF_UPER1                | CF_UNIQUE,  151,    5,    25,     25,   86,    86,       0,       0 }, // The Protector
	{  127653047, 30 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,   63,    5,    55,     55,    0,     0,       0,       0 }, // Bone Chain Armor
	{ 1282740811, 34 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Permanence
	{ 1403842263, 26 | CF_UPER1  | CF_ONLYGOOD | CF_UNIQUE,   70,    5,    90,     90,    0,     0,       0,       0 }, // Demon Plate Armor
	{ 1543909415, 28 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Fortitude
	{ 1572202402,  1 | CF_UPER1                | CF_UNIQUE,  157,    5,     0,      0,    0,     0,       0,       0 }, // Ring of Regha
	{ 1572202657,  1 | CF_UPER1                           ,  156,    3,     0,      0,    0,     0,       0,       0 }, // Bronze Ring of dexterity
	{ 1642077210,  8 | CF_UPER1                           ,   84,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Accuracy
	{ 2049461998,  4 | CF_USEFUL                          ,   35,    0,     0,      0,    0,     0,       0,       0 }, // Blacksmith Oil
	{ 2054447852,  2             | CF_SMITHPREMIUM        ,  151,    3,    25,     25,    0,     0,       0,       0 }, // Spider's Staff of devastation
	{  257276810, 28 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Hardening
	{   29449848, 11 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Skill
	{  296008111, 26 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Mastery
	{  521895255, 20 | CF_UPER15 | CF_ONLYGOOD | CF_UNIQUE,  146,    5,   255,    255,    0,     0,       0,       0 }, // Blitzen
	{  580426378, 23 | CF_UPER1                           ,   86,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Death
	{  626919077,  8 | CF_UPER1                           ,   85,    0,     0,      0,    0,     0,       0,       0 }, // Oil of Sharpness
	{  979073763, 11 | CF_UPER1                           ,  119,    3,    13,     13,    0,     0,       0,       0 }, // Crystalline Sword of the leech
	{ 1294354855, 16             | CF_WITCH               ,  153,    3,    45,     45,   14,    14,       0,       0 }, // Plentiful Staff of Mana Shield
	{  695065155, 30             | CF_SMITHPREMIUM        ,  155,    3,    75,     75,    0,     0,       0,       0 }, // King's War Staff
	{ 1100844414,  2 | CF_USEFUL                          ,   25,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Mana
	{ 1944120644,  2 | CF_USEFUL                          ,   27,    0,     0,      0,    0,     0,       0,       0 }, // Scroll of Town Portal
	{  525564945,  1 | CF_USEFUL                          ,   25,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Mana
	{ 1988162817,  1 | CF_USEFUL                          ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Healing
	{ 1191207687,  1 | CF_USEFUL                          ,   24,    0,     0,      0,    0,     0,       0,       0 }, // Potion of Healing
	// clang-format on
};

constexpr ItemSpecialEffect GnatStingSpecialEffect = ItemSpecialEffect::MultipleArrows | ItemSpecialEffect::QuickAttack;
constexpr ItemSpecialEffect ThunderclapSpecialEffect = ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage;
constexpr ItemSpecialEffect ExplosiveArrows = ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows;
constexpr ItemSpecialEffect LightningArrows = ItemSpecialEffect::LightningArrows | ItemSpecialEffect::LightningDamage;

const TestItemStruct HellfireItems[] = {
	// clang-format off
	// _iIName,                         _itype,                 _iClass,  _iCurs,  _iIvalue,  _iMinDam,  _iMaxDam,  _iAC, _iFlags,                               _iMiscId, _iSpell,               _iCharges,  _iMaxCharges,  _iDurability,  _iMaxDur,  _iPLDam,  _iPLToHit,  _iPLAC,  _iPLStr,  _iPLMag,  _iPLDex,  _iPLVit,  _iPLFR,  _iPLLR,  _iPLMR,  _iPLMana,  _iPLHP,  _iPLDamMod,  _iPLGetHit,  _iPLLight,  _iSplLvlAdd,  _iUid,  _iFMinDam,  _iFMaxDam,  _iLMinDam,  _iLMaxDam,  _iPrePower,  _iSufPower,  _iMinStr,  _iMinMag,  _iMinDex,  IDidx
	{ "Ring of stability",              ItemType::Ring,               3,      12,      8000,         0,         0,     0, ItemSpecialEffect::FasterHitRecovery,        25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          59,         0,         0,         0,    156 },
	{ "Ring of precision",              ItemType::Ring,               3,      12,     10200,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,       16,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          23,         0,         0,         0,    157 },
	{ "Obsidian Ring of wizardry",      ItemType::Ring,               3,      12,     56928,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,       27,        0,        0,      37,      37,      37,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          11,          21,         0,         0,         0,    157 },
	{ "Ring of precision",              ItemType::Ring,               3,      12,     10200,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,       16,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          23,         0,         0,         0,    158 },
	{ "Amulet of titans",               ItemType::Amulet,             3,      45,     20896,         0,         0,     0, ItemSpecialEffect::None,                     26, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,       28,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          19,         0,         0,         0,    160 },
	{ "Gold Amulet",                    ItemType::Amulet,             3,      45,     13692,         0,         0,     0, ItemSpecialEffect::None,                     26, SpellID::Null,                 0,             0,             0,         0,        0,         29,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           0,          -1,         0,         0,         0,    160 },
	{ "Messerschmidt's Reaver",         ItemType::Axe,                1,     163,     58000,        12,        30,     0, ItemSpecialEffect::FireDamage,                0, SpellID::Null,                 0,             0,            75,        75,      200,          0,       0,        5,        5,        5,        5,       0,       0,       0,         0,   -3200,          15,           0,          0,            0,     44,          2,         12,          0,          0,          -1,          -1,        80,         0,         0,    135 },
	{ "Vicious Maul of structure",      ItemType::Mace,               1,     122,     10489,         6,        20,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,           127,       128,       72,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           2,          35,        55,         0,         0,    142 },
	{ "Short Sword",                    ItemType::Sword,              1,      64,       120,         2,         6,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            15,        24,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,        18,         0,         0,    119 },
	{ "Long Battle Bow of shock",       ItemType::Bow,                1,     119,      8000,         1,        10,     0, LightningArrows,                              0, SpellID::Null,                 0,             0,            18,        50,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          1,          6,          -1,          43,        30,         0,        60,    148 },
	{ "Short Bow of magic",             ItemType::Bow,                1,     118,       400,         1,         4,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            30,        30,        0,          0,       0,        0,        1,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          21,         0,         0,         0,    143 },
	{ "Red Long Staff of Healing",      ItemType::Staff,              1,     123,      1360,         4,         8,     0, ItemSpecialEffect::None,                     23, SpellID::Healing,             22,            33,            35,        35,        0,          0,       0,        0,        0,        0,        0,      10,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           8,          -1,         0,        17,         0,    152 },
	{ "Buckler",                        ItemType::Shield,             2,      83,        30,         0,         0,     5, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,             6,        16,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     71 },
	{ "Skull Cap",                      ItemType::Helm,               2,      90,        25,         0,         0,     3, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            15,        20,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     49 },
	{ "Rags",                           ItemType::LightArmor,         2,     128,         5,         0,         0,     4, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,             6,         6,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     55 },
	{ "Quilted Armor",                  ItemType::LightArmor,         2,     129,       200,         0,         0,     7, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            30,        30,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     58 },
	{ "Quilted Armor of light",         ItemType::LightArmor,         2,     129,      1150,         0,         0,    10, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            12,        30,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          2,            0,      0,          0,          0,          0,          0,          -1,          38,         0,         0,         0,     58 },
	{ "Saintly Plate of the stars",     ItemType::HeavyArmor,         2,     103,    140729,         0,         0,    46, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            75,        75,        0,          0,     121,       10,       10,       10,       10,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           6,          27,        60,         0,         0,     67 },
	{ "Plate Mail of the stars",        ItemType::HeavyArmor,         2,     103,     77800,         0,         0,    49, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            63,        75,        0,          0,       0,        8,        8,        8,        8,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          27,        60,         0,         0,     67 },
	{ "Potion of Healing",              ItemType::Misc,               3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,                      3, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	{ "Potion of Full Healing",         ItemType::Misc,               3,      35,       150,         0,         0,     0, ItemSpecialEffect::None,                      2, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     29 },
	{ "Potion of Mana",                 ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                      6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	{ "Scroll of Golem",                ItemType::Misc,               3,       1,      1100,         0,         0,     0, ItemSpecialEffect::None,                     22, SpellID::Golem,                0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        51,         0,    110 },
	{ "Scroll of Search",               ItemType::Misc,               3,       1,        50,         0,         0,     0, ItemSpecialEffect::None,                     21, SpellID::Search,               0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     92 },
	{ "Scroll of Identify",             ItemType::Misc,               3,       1,       100,         0,         0,     0, ItemSpecialEffect::None,                     21, SpellID::Identify,             0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     94 },
	{ "Scroll of Town Portal",          ItemType::Misc,               3,       1,       200,         0,         0,     0, ItemSpecialEffect::None,                     21, SpellID::TownPortal,           0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     98 },
	{ "Scroll of Healing",              ItemType::Misc,               3,       1,        50,         0,         0,     0, ItemSpecialEffect::None,                     21, SpellID::Healing,              0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     91 },
	{ "Rune of Fire",                   ItemType::Misc,               3,     193,       100,         0,         0,     0, ItemSpecialEffect::None,                     47, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    161 },
	{ "Gold",                           ItemType::Gold,               4,       5,         0,         0,         0,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      0 },
	{ "The Undead Crown",               ItemType::Helm,               2,      77,     16650,         0,         0,     8, ItemSpecialEffect::RandomStealLife,          27, SpellID::Null,                 0,             0,            45,        50,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      1,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      7 },
	{ "Empyrean Band",                  ItemType::Ring,               3,      18,      8000,         0,         0,     0, EmpyreanBandSpecialEffect,                   27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        2,        2,        2,        2,       0,       0,       0,         0,       0,           0,           0,          2,            0,      2,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      8 },
	{ "Ring of Truth",                  ItemType::Ring,               3,      10,      9100,         0,         0,     0, ItemSpecialEffect::None,                     27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,      10,      10,      10,         0,     640,           0,          -1,          0,            0,      4,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     11 },
	{ "Griswold's Edge",                ItemType::Sword,              1,      61,     42000,         4,        12,     0, GrisworldEdgeSpecialEffect,                  27, SpellID::Null,                 0,             0,            50,        50,        0,         25,       0,        0,        0,        0,        0,       0,       0,       0,      1280,   -1280,           0,           0,          0,            0,      8,          1,         10,          0,          0,          -1,          -1,        40,         0,         0,     31 },
	{ "Bovine Plate",                   ItemType::HeavyArmor,         2,     226,       400,         0,         0,   150, ItemSpecialEffect::None,                     27, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,      30,      30,      30,     -3200,       0,           0,           0,          5,           -2,      9,          0,          0,          0,          0,          -1,          -1,        50,         0,         0,     32 },
	{ "Book of Healing",                ItemType::Misc,               3,      86,      1000,         0,         0,     0, ItemSpecialEffect::None,                     24, SpellID::Healing,              0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        17,         0,    114 },
	{ "Book of Charged Bolt",           ItemType::Misc,               3,      88,      1000,         0,         0,     0, ItemSpecialEffect::None,                     24, SpellID::ChargedBolt,          0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        25,         0,    114 },
	{ "Book of Firebolt",               ItemType::Misc,               3,      87,      1000,         0,         0,     0, ItemSpecialEffect::None,                     24, SpellID::Firebolt,             0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        15,         0,    114 },
	{ "Blacksmith Oil",                 ItemType::Misc,               3,      30,       100,         0,         0,     0, ItemSpecialEffect::None,                     36, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     83 },
	{ "Oil of Accuracy",                ItemType::Misc,               3,      30,       500,         0,         0,     0, ItemSpecialEffect::None,                     31, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     84 },
	{ "Oil of Sharpness",               ItemType::Misc,               3,      30,       500,         0,         0,     0, ItemSpecialEffect::None,                     33, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     85 },
	{ "Oil of Permanence",              ItemType::Misc,               3,      30,     15000,         0,         0,     0, ItemSpecialEffect::None,                     38, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Doppelganger's Axe",             ItemType::Axe,                1,     144,      6640,         4,        12,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            23,        32,       86,         26,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          89,          -1,        22,         0,         0,    131 },
	{ "Flail of vampires",              ItemType::Mace,               1,     131,     16500,         2,        12,     0, ItemSpecialEffect::StealMana5,                0, SpellID::Null,                 0,             0,            36,        36,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          55,        30,         0,         0,    141 },
	{ "Gladiator's Ring",               ItemType::Ring,               3,     186,     10000,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,      3200,   -3200,           0,           0,          0,            0,    109,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    157 },
	{ "Warrior's Staff of the moon",    ItemType::Staff,              1,     124,     42332,         8,        16,     0, ItemSpecialEffect::None,                     23, SpellID::Null,                 0,             0,            75,        75,       54,         15,       0,        5,        5,        5,        5,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          27,        30,         0,         0,    155 },
	{ "Kite Shield of the ages",        ItemType::Shield,             2,     113,      2600,         0,         0,    10, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          37,        50,         0,         0,     74 },
	{ "Heavy Club of puncturing",       ItemType::Mace,               1,      70,      5239,         3,         6,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            20,        20,       52,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           2,          57,        18,         0,         0,    139 },
	{ "Book of Lightning",              ItemType::Misc,               3,      88,      3000,         0,         0,     0, ItemSpecialEffect::None,                     24, SpellID::Lightning,            0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,        20,         0,    114 },
	{ "Jester's Sabre",                 ItemType::Sword,              1,      67,      1710,         1,         8,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            23,        45,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          87,          -1,        17,         0,         0,    124 },
	{ "Small Shield of blocking",       ItemType::Shield,             2,     105,      4360,         0,         0,     6, ItemSpecialEffect::FastBlock,                 0, SpellID::Null,                 0,             0,            24,        24,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          60,        25,         0,         0,     72 },
	{ "The Butcher's Cleaver",          ItemType::Axe,                1,     106,      3650,         4,        24,     0, ItemSpecialEffect::None,                     27, SpellID::Null,                 0,             0,            10,        10,        0,          0,       0,       10,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,      6 },
	{ "Scimitar of peril",              ItemType::Sword,              1,      72,       700,         3,         7,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            28,        28,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          86,        23,         0,        23,    121 },
	{ "Crystalline Large Axe",          ItemType::Axe,                1,     142,      5250,         6,        16,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            12,        12,      280,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          88,          -1,        30,         0,         0,    132 },
	{ "Red Cloak",                      ItemType::LightArmor,         2,     149,       580,         0,         0,     3, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            18,        18,        0,          0,       0,        0,        0,        0,        0,      10,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           8,          -1,         0,         0,         0,     56 },
	{ "Mace of decay",                  ItemType::Mace,               1,      59,       600,         1,         8,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            32,        32,      232,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          85,        16,         0,         0,    136 },
	{ "Ring of Truth",                  ItemType::Ring,               3,      10,      9100,         0,         0,     0, ItemSpecialEffect::None,                     27, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,      10,      10,      10,         0,     640,           0,          -1,          0,            0,      4,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     11 },
	{ "Red Armor of paralysis",         ItemType::LightArmor,         2,     107,       800,         0,         0,    17, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            18,        45,        0,          0,       0,        0,        0,       -8,        0,      20,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           8,          24,        20,         0,         0,     61 },
	{ "Bent Hunter's Bow",              ItemType::Bow,                1,     102,         1,         2,         5,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            26,        40,      -69,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           3,          -1,        20,         0,        35,    144 },
	{ "Civerb's Cudgel",                ItemType::Mace,               1,      59,      2000,         1,         8,     0, ItemSpecialEffect::TripleDemonDamage,         0, SpellID::Null,                 0,             0,            32,        32,        0,          0,       0,        0,       -2,       -5,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,     47,          0,          0,          0,          0,          -1,          -1,        16,         0,         0,    136 },
	{ "Deadly Spiked Club",             ItemType::Mace,               1,      70,      1556,         3,         6,     0, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,             8,        20,       47,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           2,          -1,        18,         0,         0,    139 },
	{ "Gnat Sting",                     ItemType::Bow,                1,     210,     30000,         1,         2,     0, GnatStingSpecialEffect,                       0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,     98,          0,          0,          0,          0,          -1,          -1,        20,         0,        35,    144 },
	{ "Thunderclap",                    ItemType::Mace,               1,     205,     30000,         5,         9,     0, ThunderclapSpecialEffect,                     0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,       20,        0,        0,        0,       0,      30,       0,         0,       0,           0,           0,          2,            0,    102,          3,          6,          2,          0,          -1,          -1,        40,         0,         0,    138 },
	{ "Rod of Onan",                    ItemType::Staff,              1,     124,     44167,         8,        16,     0, ItemSpecialEffect::None,                     23, SpellID::Golem,               50,            50,            75,        75,      100,          0,       0,        5,        5,        5,        5,       0,       0,       0,         0,       0,           0,           0,          0,            0,     62,          0,          0,          0,          0,          -1,          -1,        30,         0,         0,    155 },
	{ "Flambeau",                       ItemType::Bow,                1,     209,     30000,         0,         0,     0, ExplosiveArrows,                              0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,     99,         15,         20,          0,          0,          -1,          -1,        25,         0,        40,    146 },
	{ "Ring of Thunder",                ItemType::Ring,               3,     177,      8000,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,     -30,      60,     -30,         0,       0,           0,           0,          0,            0,     96,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    156 },
	{ "Acolyte's Amulet",               ItemType::Amulet,             3,     183,     10000,         0,         0,     0, ItemSpecialEffect::None,                     26, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,     -3968,    3968,           0,           0,          0,            0,    108,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    159 },
	{ "The Protector",                  ItemType::Staff,              1,     162,     17240,         2,         4,    40, ItemSpecialEffect::Thorns,                   23, SpellID::Healing,             86,            86,            25,        25,        0,          0,       0,        0,        0,        0,        5,       0,       0,       0,         0,       0,           0,          -5,          0,            0,     59,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    151 },
	{ "Bone Chain Armor",               ItemType::MediumArmor,        2,     204,     36000,         0,         0,    40, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            55,        55,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,    106,          0,          0,          0,          0,          -1,          -1,        30,         0,         0,     63 },
	{ "Oil of Permanence",              ItemType::Misc,               3,      30,     15000,         0,         0,     0, ItemSpecialEffect::None,                     38, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Demon Plate Armor",              ItemType::HeavyArmor,         2,     225,     80000,         0,         0,    80, ItemSpecialEffect::None,                      0, SpellID::Null,                 0,             0,            90,        90,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,    107,          0,          0,          0,          0,          -1,          -1,        90,         0,         0,     70 },
	{ "Oil of Fortitude",               ItemType::Misc,               3,      30,      2500,         0,         0,     0, ItemSpecialEffect::None,                     37, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Ring of Regha",                  ItemType::Ring,               3,      11,      4175,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,       -3,       10,       -3,        0,       0,       0,      10,         0,       0,           0,           0,          1,            0,     86,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,    157 },
	{ "Bronze Ring of dexterity",       ItemType::Ring,               3,      12,      5200,         0,         0,     0, ItemSpecialEffect::None,                     25, SpellID::Null,                 0,             0,             0,         0,        0,          4,       0,        0,        0,        4,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           0,          23,         0,         0,         0,    156 },
	{ "Oil of Accuracy",                ItemType::Misc,               3,      30,       500,         0,         0,     0, ItemSpecialEffect::None,                     31, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     84 },
	{ "Blacksmith Oil",                 ItemType::Misc,               3,      30,       100,         0,         0,     0, ItemSpecialEffect::None,                     36, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     35 },
	{ "Spider's Staff of devastation",  ItemType::Staff,              1,     109,      2050,         2,         4,     0, ItemSpecialEffect::None,                     23, SpellID::Null,                 0,             0,            25,        25,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,       768,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          33,          84,         0,         0,         0,    151 },
	{ "Oil of Hardening",               ItemType::Misc,               3,      30,       500,         0,         0,     0, ItemSpecialEffect::None,                     39, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Oil of Skill",                   ItemType::Misc,               3,      30,      1500,         0,         0,     0, ItemSpecialEffect::None,                     35, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Oil of Mastery",                 ItemType::Misc,               3,      30,      2500,         0,         0,     0, ItemSpecialEffect::None,                     32, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Blitzen",                        ItemType::Bow,                1,     219,     30000,         0,         0,     0, ExplosiveArrows,                              0, SpellID::Null,                 0,             0,           255,       255,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,    101,         10,         15,          1,          0,          -1,          -1,        25,         0,        40,    146 },
	{ "Oil of Death",                   ItemType::Misc,               3,      30,      2500,         0,         0,     0, ItemSpecialEffect::None,                     34, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     86 },
	{ "Oil of Sharpness",               ItemType::Misc,               3,      30,       500,         0,         0,     0, ItemSpecialEffect::None,                     33, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     85 },
	{ "Crystalline Sword of the leech", ItemType::Sword,              1,      64,     10020,         2,         6,     0, ItemSpecialEffect::StealLife3,                0, SpellID::Null,                 0,             0,            13,        13,      232,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          88,          56,        18,         0,         0,    119 },
	{ "Plentiful Staff of Mana Shield", ItemType::Staff,              1,     166,      6360,         5,        10,     0, ItemSpecialEffect::None,                     23, SpellID::ManaShield,          14,            14,            45,        45,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          15,          -1,         0,        25,         0,    153 },
	{ "King's War Staff",               ItemType::Staff,              1,     124,     92000,         8,        16,     0, ItemSpecialEffect::None,                     23, SpellID::Null,                 0,             0,            75,        75,      175,         76,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,           4,          -1,        30,         0,         0,    155 },
	{ "Potion of Mana",                 ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                      6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	{ "Scroll of Town Portal",          ItemType::Misc,               3,       1,       200,         0,         0,     0, ItemSpecialEffect::None,                     21, SpellID::TownPortal,           0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     27 },
	{ "Potion of Mana",                 ItemType::Misc,               3,      39,        50,         0,         0,     0, ItemSpecialEffect::None,                      6, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     25 },
	{ "Potion of Healing",              ItemType::Misc,               3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,                      3, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	{ "Potion of Healing",              ItemType::Misc,               3,      32,        50,         0,         0,     0, ItemSpecialEffect::None,                      3, SpellID::Null,                 0,             0,             0,         0,        0,          0,       0,        0,        0,        0,        0,       0,       0,       0,         0,       0,           0,           0,          0,            0,      0,          0,          0,          0,          0,          -1,          -1,         0,         0,         0,     24 },
	// clang-format on
};

TEST_F(PackTest, UnPackItem_hellfire)
{
	Item id;
	ItemPack is;

	gbIsHellfire = true;
	gbIsMultiplayer = false;
	gbIsSpawn = false;

	MyPlayer->_pMaxManaBase = 125 << 6;
	MyPlayer->_pMaxHPBase = 125 << 6;

	for (size_t i = 0; i < sizeof(PackedHellfireItems) / sizeof(*PackedHellfireItems); i++) {
		const ItemPack packed = SwappedLE(PackedHellfireItems[i]);
		UnPackItem(packed, *MyPlayer, id, true);
		CompareItems(id, HellfireItems[i]);
		TestItemNameGeneration(id);

		PackItem(is, id, gbIsHellfire);
		is.dwBuff &= ~CF_HELLFIRE;
		ComparePackedItems(is, packed);
	}
}

TEST_F(PackTest, UnPackItem_diablo_strip_hellfire_items)
{
	const auto is = SwappedLE(ItemPack { 1478792102, 3 | CF_UPER1, 92, 0, 0, 0, 0, 0, 0, 0 }); // Scroll of Search
	Item id;

	gbIsHellfire = false;
	gbIsMultiplayer = false;
	gbIsSpawn = false;

	UnPackItem(is, *MyPlayer, id, true);
	TestItemNameGeneration(id);

	ASSERT_EQ(id._itype, ItemType::None);
}

TEST_F(PackTest, UnPackItem_empty)
{
	const auto is = SwappedLE(ItemPack { 0, 0, 0xFFFF, 0, 0, 0, 0, 0, 0, 0 });
	Item id;

	UnPackItem(is, *MyPlayer, id, false);
	TestItemNameGeneration(id);

	ASSERT_EQ(id._itype, ItemType::None);
}

TEST_F(PackTest, PackItem_empty)
{
	ItemPack is;
	Item id = {};

	id._itype = ItemType::None;

	PackItem(is, id, gbIsHellfire);

	// Copy the value out before comparing to avoid loading a misaligned address.
	const auto idx = is.idx;
	ASSERT_EQ(SDL_SwapLE16(idx), 0xFFFF);
	TestItemNameGeneration(id);
}

static void compareGold(const ItemPack &is, int iCurs)
{
	Item id;
	UnPackItem(is, *MyPlayer, id, false);
	ASSERT_EQ(id._iCurs, iCurs);
	ASSERT_EQ(id.IDidx, IDI_GOLD);
	// Copy the value out before comparing to avoid loading a misaligned address.
	const auto wvalue = SDL_SwapLE16(is.wValue);
	ASSERT_EQ(id._ivalue, wvalue);
	ASSERT_EQ(id._itype, ItemType::Gold);
	ASSERT_EQ(id._iClass, ICLASS_GOLD);
	TestItemNameGeneration(id);

	ItemPack is2;
	PackItem(is2, id, gbIsHellfire);
	ComparePackedItems(is, is2);
}

TEST_F(PackTest, UnPackItem_gold_small)
{
	const auto is = SwappedLE(ItemPack { 0, 0, IDI_GOLD, 0, 0, 0, 0, 0, 1000, 0 });
	compareGold(is, ICURS_GOLD_SMALL);
}

TEST_F(PackTest, UnPackItem_gold_medium)
{
	const auto is = SwappedLE(ItemPack { 0, 0, IDI_GOLD, 0, 0, 0, 0, 0, 1001, 0 });
	compareGold(is, ICURS_GOLD_MEDIUM);
}

TEST_F(PackTest, UnPackItem_gold_large)
{
	const auto is = SwappedLE(ItemPack { 0, 0, IDI_GOLD, 0, 0, 0, 0, 0, 2500, 0 });
	compareGold(is, ICURS_GOLD_LARGE);
}

TEST_F(PackTest, UnPackItem_ear)
{
	const auto is = SwappedLE(ItemPack { 1633955154, 17509, 23, 111, 103, 117, 101, 68, 19843, 0 });
	Item id;

	UnPackItem(is, *MyPlayer, id, false);
	ASSERT_STREQ(id._iName, "Ear of Dead-RogueDM");
	ASSERT_EQ(id._ivalue, 3);
	TestItemNameGeneration(id);

	ItemPack is2;
	PackItem(is2, id, gbIsHellfire);
	ComparePackedItems(is, is2);
}

class NetPackTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(2);
		MyPlayer = &Players[0];
		gbIsMultiplayer = true;
		gbIsSpawn = false;

		PlayerPack testPack {
			0, 0, -1, 9, 0, 2, 61, 24, 0, 0, "MP-Warrior", 0, 120, 25, 60, 60, 37, 0, 85670061, 3921, 13568, 13568, 3904, 3904,
			{ 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
			1610612737,
			{ { 3160997530, 798, 52, 5, 40, 40, 0, 0, 0, 0 },
			    { 1911837482, 286, 152, 3, 0, 0, 0, 0, 0, 0 },
			    { 1262914017, 286, 151, 3, 0, 0, 0, 0, 0, 0 },
			    { 3846694361, 286, 155, 3, 0, 0, 0, 0, 0, 0 },
			    { 3454746195, 2077, 122, 3, 60, 60, 0, 0, 0, 0 },
			    { 1560055601, 4117, 75, 3, 50, 50, 0, 0, 0, 0 },
			    { 3097669048, 286, 70, 3, 66, 90, 0, 0, 0, 0 } },
			{ { 423576018, 16400, 82, 0, 0, 0, 0, 0, 0, 0 },
			    { 543375803, 16400, 81, 0, 0, 0, 0, 0, 0, 0 },
			    { 1802859062, 8208, 147, 3, 35, 35, 34, 34, 0, 0 },
			    { 368922902, 0, 0, 0, 0, 0, 0, 0, 3921, 0 },
			    { 1424628865, 8208, 111, 1, 0, 0, 0, 0, 0, 0 },
			    { 1128267486, 1, 25, 0, 0, 0, 0, 0, 0, 0 },
			    { 885129636, 1, 25, 0, 0, 0, 0, 0, 0, 0 },
			    { 1954673116, 260, 93, 0, 0, 0, 0, 0, 0, 0 },
			    { 379449999, 260, 93, 0, 0, 0, 0, 0, 0, 0 },
			    { 1684106752, 24937, 23, 0, 0, 0, 0, 0, 65, 0 },
			    { 1924887070, 385, 24, 0, 0, 0, 0, 0, 0, 0 },
			    { 813588879, 194, 56, 2, 6, 18, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
			{ -3, -3, -12, -12, 0, 0, 0, 0, -5, -5, -3, -3, -12, -12, 0, 0, 0, 0, 5, -5, 3, -3, 12, -12, 0, 0, 0, 0, 0, 0, 2, 1, 6, 7, 8, 9, 10, 11, 4, 0 },
			12,
			{ { 112017676, 16, 24, 0, 0, 0, 0, 0, 0, 0 },
			    { 367533949, 16, 24, 0, 0, 0, 0, 0, 0, 0 },
			    { 778528030, 16, 24, 0, 0, 0, 0, 0, 0, 0 },
			    { 960294091, 16, 24, 0, 0, 0, 0, 0, 0, 0 },
			    { 1948311560, 16, 29, 0, 0, 0, 0, 0, 0, 0 },
			    { 639577687, 16, 29, 0, 0, 0, 0, 0, 0, 0 },
			    { 103356930, 16, 29, 0, 0, 0, 0, 0, 0, 0 },
			    { 215810455, 16, 29, 0, 0, 0, 0, 0, 0, 0 } },
			0, 0, 0, 0, 0, 0, 0, 0, 0,
			{ 0, 0 },
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			0, 0, 0, 0,
			{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
		};

		SwapLE(testPack);
		UnPackPlayer(testPack, *MyPlayer);
	}

	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();

		// The tests need spawn.mpq or diabdat.mpq
		// Please provide them so that the tests can run successfully
		ASSERT_TRUE(HaveMainData());

		gbIsHellfire = false;
		InitCursor();
		LoadSpellData();
		LoadPlayerDataFiles();
		LoadMonsterData();
		LoadItemData();
	}
};

bool TestNetPackValidation()
{
	PlayerNetPack packed;
	PackNetPlayer(packed, *MyPlayer);
	return UnPackNetPlayer(packed, Players[1]);
}

TEST_F(NetPackTest, UnPackNetPlayer_valid)
{
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_class)
{
	PlayerNetPack packed;
	PackNetPlayer(packed, *MyPlayer);
	packed.pClass = std::numeric_limits<uint8_t>::max();
	ASSERT_FALSE(UnPackNetPlayer(packed, Players[1]));
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_oob)
{
	WorldTilePosition position = MyPlayer->position.tile;

	MyPlayer->position.tile.x = MAXDUNX + 1;
	ASSERT_FALSE(TestNetPackValidation());

	MyPlayer->position.tile = position;
	MyPlayer->position.tile.y = MAXDUNY + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_plrlevel)
{
	MyPlayer->plrlevel = NUMLEVELS;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_hpBase)
{
	MyPlayer->_pHPBase = -64;
	ASSERT_FALSE(TestNetPackValidation());

	MyPlayer->_pHPBase = MyPlayer->_pMaxHPBase + 64;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_manaBase)
{
	MyPlayer->_pManaBase = MyPlayer->_pMaxManaBase + 64;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_baseStr)
{
	MyPlayer->_pBaseStr = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength) + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_baseMag)
{
	MyPlayer->_pBaseMag = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Magic) + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_baseDex)
{
	MyPlayer->_pBaseDex = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Dexterity) + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_baseVit)
{
	MyPlayer->_pBaseVit = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Vitality) + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_numInv)
{
	MyPlayer->_pNumInv = InventoryGridCells + 1;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_strength)
{
	MyPlayer->_pStrength++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_magic)
{
	MyPlayer->_pMagic++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_dexterity)
{
	MyPlayer->_pDexterity++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_vitality)
{
	MyPlayer->_pVitality++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_hitPoints)
{
	MyPlayer->_pHitPoints++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_maxHP)
{
	MyPlayer->_pMaxHP++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_mana)
{
	MyPlayer->_pMana++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_maxMana)
{
	MyPlayer->_pMaxMana++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_damageMod)
{
	MyPlayer->_pDamageMod++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_baseToBlk)
{
	PlayerNetPack packed;
	PackNetPlayer(packed, *MyPlayer);
	packed.pBaseToBlk++;
	ASSERT_FALSE(UnPackNetPlayer(packed, Players[1]));
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iMinDam)
{
	MyPlayer->_pIMinDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iMinDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iMaxDam)
{
	MyPlayer->_pIMaxDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iMaxDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iAC)
{
	MyPlayer->_pIAC++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_CHEST]._iAC++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iBonusDam)
{
	MyPlayer->_pIBonusDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iPLDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iBonusToHit)
{
	MyPlayer->_pIBonusToHit++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iPLToHit++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iBonusAC)
{
	MyPlayer->_pIBonusAC++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_CHEST]._iPLAC++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iBonusDamMod)
{
	MyPlayer->_pIBonusDamMod++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iPLDamMod++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iGetHit)
{
	MyPlayer->_pIGetHit++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_CHEST]._iPLGetHit++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iEnAc)
{
	MyPlayer->_pIEnAc++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_CHEST]._iPLEnAc++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iFMinDam)
{
	MyPlayer->_pIFMinDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iFMinDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iFMaxDam)
{
	MyPlayer->_pIFMaxDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iFMaxDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iLMinDam)
{
	MyPlayer->_pILMinDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iLMinDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_iLMaxDam)
{
	MyPlayer->_pILMaxDam++;
	ASSERT_FALSE(TestNetPackValidation());

	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_TRUE(TestNetPackValidation());

	MyPlayer->InvBody[INVLOC_HAND_LEFT]._iLMaxDam++;
	CalcPlrItemVals(*MyPlayer, false);
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_maxHPBase)
{
	MyPlayer->_pMaxHPBase++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_maxManaBase)
{
	MyPlayer->_pMaxManaBase++;
	ASSERT_FALSE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_pregenItemFlags)
{
	size_t count = 0;
	for (Item &item : MyPlayer->InvList) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		uint16_t createInfo = item._iCreateInfo;
		item._iCreateInfo |= CF_PREGEN;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;
		count++;
	}
	ASSERT_GT(count, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_usefulItemFlags)
{
	size_t count = 0;
	for (Item &item : MyPlayer->InvList) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		if ((item._iCreateInfo & CF_USEFUL) != CF_USEFUL)
			continue;
		uint16_t createInfo = item._iCreateInfo;
		item._iCreateInfo |= CF_ONLYGOOD;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;
		count++;
	}
	ASSERT_GT(count, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_townItemFlags)
{
	size_t count = 0;
	for (Item &item : MyPlayer->InvList) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		if ((item._iCreateInfo & CF_TOWN) == 0)
			continue;
		uint16_t createInfo = item._iCreateInfo;
		item._iCreateInfo |= CF_ONLYGOOD;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;
		count++;
	}
	ASSERT_GT(count, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_townItemLevel)
{
	size_t boyCount = 0;
	size_t otherCount = 0;
	for (Item &item : MyPlayer->InvBody) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		if ((item._iCreateInfo & CF_TOWN) == 0)
			continue;
		uint16_t createInfo = item._iCreateInfo;
		bool BoyItem = (item._iCreateInfo & CF_BOY) != 0;
		item._iCreateInfo &= ~CF_LEVEL;
		item._iCreateInfo |= BoyItem ? MyPlayer->getMaxCharacterLevel() + 1 : 31;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;

		size_t &count = BoyItem ? boyCount : otherCount;
		count++;
	}
	ASSERT_GT(boyCount, 0);
	ASSERT_GT(otherCount, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_uniqueMonsterItemLevel)
{
	size_t count = 0;
	for (Item &item : MyPlayer->InvList) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		if ((item._iCreateInfo & CF_USEFUL) != CF_UPER15)
			continue;
		uint16_t createInfo = item._iCreateInfo;
		item._iCreateInfo &= ~CF_LEVEL;
		item._iCreateInfo |= 31;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;
		count++;
	}
	ASSERT_GT(count, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

TEST_F(NetPackTest, UnPackNetPlayer_invalid_monsterItemLevel)
{
	size_t count = 0;
	for (Item &item : MyPlayer->InvBody) {
		if (item.isEmpty())
			continue;
		if (IsAnyOf(item.IDidx, IDI_GOLD, IDI_EAR))
			continue;
		if ((item._iCreateInfo & CF_TOWN) != 0)
			continue;
		if ((item._iCreateInfo & CF_USEFUL) == CF_UPER15)
			continue;
		uint16_t createInfo = item._iCreateInfo;
		item._iCreateInfo &= ~CF_LEVEL;
		item._iCreateInfo |= 31;
		ASSERT_FALSE(TestNetPackValidation());
		item._iCreateInfo = createInfo;
		count++;
	}
	ASSERT_GT(count, 0);
	ASSERT_TRUE(TestNetPackValidation());
}

} // namespace
} // namespace devilution
