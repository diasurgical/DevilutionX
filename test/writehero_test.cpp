#include "player_test.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>
#include <picosha2.h>

#include "codec.h"
#include "cursor.h"
#include "engine/assets.hpp"
#include "game_mode.hpp"
#include "init.hpp"
#include "loadsave.h"
#include "menu.h"
#include "pack.h"
#include "pfile.h"
#include "spells.h"
#include "tables/playerdat.hpp"
#include "utils/endian_swap.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"

namespace devilution {

uint32_t gSaveNumber = 0;

namespace {

constexpr int SpellDatVanilla[] = {
	0, 1, 1, 4, 5, -1, 3, 3, 6, -1, 7, 6, 8, 9,
	8, 9, -1, -1, -1, -1, 3, 11, -1, 14, -1, -1,
	-1, -1, -1, 8, 1, 1, -1, 2, 1, 14, 9
};

void SwapLE(ItemPack &pack)
{
	pack.iSeed = Swap32LE(pack.iSeed);
	pack.iCreateInfo = Swap16LE(pack.iCreateInfo);
	pack.idx = Swap16LE(pack.idx);
	pack.wValue = Swap16LE(pack.wValue);
	pack.dwBuff = Swap32LE(pack.dwBuff);
}

void SwapLE(PlayerPack &player)
{
	player.dwLowDateTime = Swap32LE(player.dwLowDateTime);
	player.dwHighDateTime = Swap32LE(player.dwHighDateTime);
	player.pExperience = Swap32LE(player.pExperience);
	player.pGold = Swap32LE(player.pGold);
	player.pHPBase = Swap32LE(player.pHPBase);
	player.pMaxHPBase = Swap32LE(player.pMaxHPBase);
	player.pManaBase = Swap32LE(player.pManaBase);
	player.pMaxManaBase = Swap32LE(player.pMaxManaBase);
	player.pMemSpells = Swap64LE(player.pMemSpells);
	for (ItemPack &item : player.InvBody) {
		SwapLE(item);
	}
	for (ItemPack &item : player.InvList) {
		SwapLE(item);
	}
	for (ItemPack &item : player.SpdList) {
		SwapLE(item);
	}
	player.wReflections = Swap16LE(player.wReflections);
	player.pDiabloKillLevel = Swap32LE(player.pDiabloKillLevel);
	player.pDifficulty = Swap32LE(player.pDifficulty);
	player.pDamAcFlags = Swap32LE(player.pDamAcFlags);
}

void PackItemUnique(ItemPack *id, int idx)
{
	id->idx = idx;
	id->iCreateInfo = 0x2DE;
	id->bId = 1 + 2 * ITEM_QUALITY_UNIQUE;
	id->bDur = 40;
	id->bMDur = 40;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x1C0C44B0;
}

void PackItemStaff(ItemPack *id)
{
	id->idx = 150;
	id->iCreateInfo = 0x2010;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 75;
	id->bMDur = 75;
	id->bCh = 12;
	id->bMCh = 12;
	id->iSeed = 0x2A15243F;
}

void PackItemBow(ItemPack *id)
{
	id->idx = 145;
	id->iCreateInfo = 0x0814;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 60;
	id->bMDur = 60;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x449D8992;
}

void PackItemSword(ItemPack *id)
{
	id->idx = 122;
	id->iCreateInfo = 0x081E;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 60;
	id->bMDur = 60;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x680FAC02;
}

void PackItemRing1(ItemPack *id)
{
	id->idx = 153;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x5B41AFA8;
}

void PackItemRing2(ItemPack *id)
{
	id->idx = 153;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x1E41FEFC;
}

void PackItemAmulet(ItemPack *id)
{
	id->idx = 155;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x70A0383A;
}

void PackItemArmor(ItemPack *id)
{
	id->idx = 70;
	id->iCreateInfo = 0xDE;
	id->bId = 1 + 2 * ITEM_QUALITY_MAGIC;
	id->bDur = 90;
	id->bMDur = 90;
	id->bCh = 0;
	id->bMCh = 0;
	id->iSeed = 0x63AAC49B;
}

void PackItemFullRejuv(ItemPack *id, int i)
{
	const uint32_t seeds[] = { 0x7C253335, 0x3EEFBFF8, 0x76AFB1A9, 0x38EB45FE, 0x1154E197, 0x5964B644, 0x76B58BEB, 0x002A6E5A };
	id->idx = ItemMiscIdIdx(IMISC_FULLREJUV);
	id->iSeed = seeds[i];
	id->iCreateInfo = 0;
	id->bId = 2 * ITEM_QUALITY_NORMAL;
	id->bDur = 0;
	id->bMDur = 0;
	id->bCh = 0;
	id->bMCh = 0;
}

int PrepareInvSlot(PlayerPack *pPack, int pos, int size, int start = 0)
{
	static char ret = 0;
	if (start)
		ret = 0;
	++ret;
	if (size == 0) {
		pPack->InvGrid[pos] = ret;
	} else if (size == 1) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 20] = -ret;
	} else if (size == 2) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos + 1] = -ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 10 + 1] = -ret;
		pPack->InvGrid[pos - 20] = -ret;
		pPack->InvGrid[pos - 20 + 1] = -ret;
	} else if (size == 3) {
		pPack->InvGrid[pos] = ret;
		pPack->InvGrid[pos + 1] = -ret;
		pPack->InvGrid[pos - 10] = -ret;
		pPack->InvGrid[pos - 10 + 1] = -ret;
	} else {
		abort();
	}
	return ret - 1;
}

void PackPlayerTest(PlayerPack *pPack)
{
	memset(pPack, 0, 0x4F2);
	pPack->destAction = -1;
	pPack->destParam1 = 0;
	pPack->destParam2 = 0;
	pPack->plrlevel = 0;
	pPack->pExperience = 1583495809;
	pPack->pLevel = 50;
	pPack->px = 75;
	pPack->py = 68;
	pPack->targx = 75;
	pPack->targy = 68;
	pPack->pGold = 0;
	pPack->pStatPts = 0;
	pPack->pDiabloKillLevel = 3;
	for (auto i = 0; i < 40; i++)
		pPack->InvList[i].idx = -1;
	for (auto i = 0; i < 7; i++)
		pPack->InvBody[i].idx = -1;
	for (auto i = 0; i < MaxBeltItems; i++)
		PackItemFullRejuv(pPack->SpdList + i, i);
	for (auto i = 1; i < 37; i++) {
		if (SpellDatVanilla[i] != -1) {
			pPack->pMemSpells |= 1ULL << (i - 1);
			pPack->pSplLvl[i] = 15;
		}
	}
	for (auto i = 0; i < 7; i++)
		pPack->InvBody[i].idx = -1;
	strcpy(pPack->pName, "TestPlayer");
	pPack->pClass = static_cast<uint8_t>(HeroClass::Rogue);
	pPack->pBaseStr = 20 + 35;
	pPack->pBaseMag = 15 + 55;
	pPack->pBaseDex = 30 + 220;
	pPack->pBaseVit = 20 + 60;
	pPack->pHPBase = ((20 + 10) << 6) + ((20 + 10) << 5) + 48 * 128 + (60 << 6);
	pPack->pMaxHPBase = pPack->pHPBase;
	pPack->pManaBase = (15 << 6) + (15 << 5) + 48 * 128 + (55 << 6);
	pPack->pMaxManaBase = pPack->pManaBase;

	PackItemUnique(pPack->InvBody + INVLOC_HEAD, 52);
	PackItemRing1(pPack->InvBody + INVLOC_RING_LEFT);
	PackItemRing2(pPack->InvBody + INVLOC_RING_RIGHT);
	PackItemAmulet(pPack->InvBody + INVLOC_AMULET);
	PackItemArmor(pPack->InvBody + INVLOC_CHEST);
	PackItemBow(pPack->InvBody + INVLOC_HAND_LEFT);

	PackItemStaff(pPack->InvList + PrepareInvSlot(pPack, 28, 2, 1));
	PackItemSword(pPack->InvList + PrepareInvSlot(pPack, 20, 1));

	pPack->_pNumInv = 2;

	SwapLE(*pPack);
}

void AppendLE32(std::vector<std::byte> &buffer, int32_t value)
{
	const uint32_t rawValue = static_cast<uint32_t>(value);
	buffer.push_back(static_cast<std::byte>(rawValue & 0xFF));
	buffer.push_back(static_cast<std::byte>((rawValue >> 8) & 0xFF));
	buffer.push_back(static_cast<std::byte>((rawValue >> 16) & 0xFF));
	buffer.push_back(static_cast<std::byte>((rawValue >> 24) & 0xFF));
}

void AppendU8(std::vector<std::byte> &buffer, uint8_t value)
{
	buffer.push_back(static_cast<std::byte>(value));
}

void WriteEncodedArchiveEntry(const std::string &savePath, const char *entryName, std::vector<std::byte> decodedData)
{
	std::vector<std::byte> encodedData(codec_get_encoded_len(decodedData.size()));
	std::copy(decodedData.begin(), decodedData.end(), encodedData.begin());
	codec_encode(encodedData.data(), decodedData.size(), encodedData.size(), pfile_get_password());

	std::string savePathCopy = savePath;
	SaveWriter saveWriter(std::move(savePathCopy));
	ASSERT_TRUE(saveWriter.WriteFile(entryName, encodedData.data(), encodedData.size()));
}

void WriteLegacyHotkeys(
    const std::string &savePath,
    const std::array<SpellID, 4> &hotkeySpells,
    const std::array<SpellType, 4> &hotkeyTypes,
    SpellID selectedSpell,
    SpellType selectedSpellType)
{
	std::vector<std::byte> decodedData;
	decodedData.reserve(4 * sizeof(int32_t) + 4 * sizeof(uint8_t) + sizeof(int32_t) + sizeof(uint8_t));

	for (SpellID spellId : hotkeySpells) {
		AppendLE32(decodedData, static_cast<int32_t>(spellId));
	}
	for (SpellType spellType : hotkeyTypes) {
		AppendU8(decodedData, static_cast<uint8_t>(spellType));
	}

	AppendLE32(decodedData, static_cast<int32_t>(selectedSpell));
	AppendU8(decodedData, static_cast<uint8_t>(selectedSpellType));

	WriteEncodedArchiveEntry(savePath, "hotkeys", std::move(decodedData));
}

void AssertPlayer(Player &player)
{
	ASSERT_EQ(CountU8(player._pSplLvl, 64), 23);
	ASSERT_EQ(Count8(player.InvGrid, InventoryGridCells), 9);
	ASSERT_EQ(CountItems(player.InvBody, NUM_INVLOC), 6);
	ASSERT_EQ(CountItems(player.InvList, InventoryGridCells), 2);
	ASSERT_EQ(CountItems(player.SpdList, MaxBeltItems), 8);
	ASSERT_EQ(CountItems(&player.HoldItem, 1), 0);

	ASSERT_EQ(player.position.tile.x, 75);
	ASSERT_EQ(player.position.tile.y, 68);
	ASSERT_EQ(player.position.future.x, 75);
	ASSERT_EQ(player.position.future.y, 68);
	ASSERT_EQ(player.plrlevel, 0);
	ASSERT_EQ(player.destAction, -1);
	ASSERT_STREQ(player._pName, "TestPlayer");
	ASSERT_EQ(player._pClass, HeroClass::Rogue);
	ASSERT_EQ(player._pBaseStr, 55);
	ASSERT_EQ(player._pStrength, 124);
	ASSERT_EQ(player._pBaseMag, 70);
	ASSERT_EQ(player._pMagic, 80);
	ASSERT_EQ(player._pBaseDex, 250);
	ASSERT_EQ(player._pDexterity, 281);
	ASSERT_EQ(player._pBaseVit, 80);
	ASSERT_EQ(player._pVitality, 90);
	ASSERT_EQ(player.getCharacterLevel(), 50);
	ASSERT_EQ(player._pStatPts, 0);
	ASSERT_EQ(player._pExperience, 1583495809);
	ASSERT_EQ(player._pGold, 0);
	ASSERT_EQ(player._pMaxHPBase, 12864);
	ASSERT_EQ(player._pHPBase, 12864);
	ASSERT_EQ(player.getBaseToBlock(), 20);
	ASSERT_EQ(player._pMaxManaBase, 11104);
	ASSERT_EQ(player._pManaBase, 11104);
	ASSERT_EQ(player._pMemSpells, 66309357295);
	ASSERT_EQ(player._pNumInv, 2);
	ASSERT_EQ(player.wReflections, 0);
	ASSERT_EQ(player.pTownWarps, 0);
	ASSERT_EQ(player.pDungMsgs, 0);
	ASSERT_EQ(player.pDungMsgs2, 0);
	ASSERT_EQ(player.pLvlLoad, 0);
	ASSERT_EQ(player.pDiabloKillLevel, 3);
	ASSERT_EQ(player.pManaShield, 0);
	ASSERT_EQ(player.pDamAcFlags, ItemSpecialEffectHf::None);

	ASSERT_EQ(player._pmode, 0);
	ASSERT_EQ(Count8(player.walkpath, MaxPathLengthPlayer), MaxPathLengthPlayer);
	ASSERT_EQ(player._pgfxnum, 36);
	ASSERT_EQ(player.AnimInfo.ticksPerFrame, 4);
	ASSERT_EQ(player.AnimInfo.tickCounterOfCurrentFrame, 1);
	ASSERT_EQ(player.AnimInfo.numberOfFrames, 20);
	ASSERT_EQ(player.AnimInfo.currentFrame, 0);
	ASSERT_EQ(player.queuedSpell.spellId, SpellID::Invalid);
	ASSERT_EQ(player.queuedSpell.spellType, SpellType::Invalid);
	ASSERT_EQ(player.queuedSpell.spellFrom, 0);
	ASSERT_EQ(player.inventorySpell, SpellID::Null);
	ASSERT_EQ(player._pRSpell, SpellID::Invalid);
	ASSERT_EQ(player._pRSplType, SpellType::Invalid);
	ASSERT_EQ(player._pSBkSpell, SpellID::Invalid);
	ASSERT_EQ(player._pAblSpells, 134217728);
	ASSERT_EQ(player._pScrlSpells, 0);
	ASSERT_EQ(player._pSpellFlags, SpellFlag::None);
	ASSERT_TRUE(player.UsesRangedWeapon());
	ASSERT_EQ(player._pBlockFlag, 0);
	ASSERT_EQ(player._pLightRad, 11);
	ASSERT_EQ(player._pDamageMod, 101);
	ASSERT_EQ(player._pHitPoints, 16640);
	ASSERT_EQ(player._pMaxHP, 16640);
	ASSERT_EQ(player._pMana, 14624);
	ASSERT_EQ(player._pMaxMana, 14624);
	ASSERT_EQ(player.getNextExperienceThreshold(), 1583495809);
	ASSERT_EQ(player._pMagResist, 75);
	ASSERT_EQ(player._pFireResist, 16);
	ASSERT_EQ(player._pLghtResist, 75);
	ASSERT_EQ(CountBool(player._pLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(CountBool(player._pSLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(player._pNFrames, 20);
	ASSERT_EQ(player._pWFrames, 8);
	ASSERT_EQ(player._pAFrames, 0);
	ASSERT_EQ(player._pAFNum, 0);
	ASSERT_EQ(player._pSFrames, 16);
	ASSERT_EQ(player._pSFNum, 12);
	ASSERT_EQ(player._pHFrames, 0);
	ASSERT_EQ(player._pDFrames, 20);
	ASSERT_EQ(player._pBFrames, 0);
	ASSERT_EQ(player._pIMinDam, 1);
	ASSERT_EQ(player._pIMaxDam, 14);
	ASSERT_EQ(player._pIAC, 115);
	ASSERT_EQ(player._pIBonusDam, 0);
	ASSERT_EQ(player._pIBonusToHit, 0);
	ASSERT_EQ(player._pIBonusAC, 0);
	ASSERT_EQ(player._pIBonusDamMod, 0);
	ASSERT_EQ(player._pISpells, 0);
	ASSERT_EQ(player._pIFlags, ItemSpecialEffect::None);
	ASSERT_EQ(player._pIGetHit, 0);
	ASSERT_EQ(player._pISplLvlAdd, 0);
	ASSERT_EQ(player._pIEnAc, 0);
	ASSERT_EQ(player._pIFMinDam, 0);
	ASSERT_EQ(player._pIFMaxDam, 0);
	ASSERT_EQ(player._pILMinDam, 0);
	ASSERT_EQ(player._pILMaxDam, 0);
	ASSERT_EQ(player.pOriginalCathedral, 0);
}

TEST(Writehero, pfile_write_hero)
{
	LoadCoreArchives();
	LoadGameArchives();

	// The tests need spawn.mpq or diabdat.mpq
	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);
	PlayerPack pks;
	PackPlayerTest(&pks);
	UnPackPlayer(pks, *MyPlayer);
	AssertPlayer(Players[0]);
	pfile_write_hero();

	uintmax_t fileSize;
	ASSERT_TRUE(GetFileSize(savePath.c_str(), &fileSize));
	const size_t size = static_cast<size_t>(fileSize);
	FILE *f = OpenFile(savePath.c_str(), "rb");
	ASSERT_TRUE(f != nullptr);
	const std::unique_ptr<char[]> data { new char[size] };
	ASSERT_EQ(std::fread(data.get(), size, 1, f), 1);
	std::fclose(f);

	std::vector<unsigned char> s(picosha2::k_digest_size);
	picosha2::hash256(data.get(), data.get() + size, s.begin(), s.end());
	EXPECT_EQ(picosha2::bytes_to_hex_string(s.begin(), s.end()),
	    "a79367caae6192d54703168d82e0316aa289b2a33251255fad8abe34889c1d3a");
}

TEST(Writehero, pfile_read_player_from_save_clears_invalid_spell_selections)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());

	gbVanilla = false;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);

	Player &player = *MyPlayer;
	player._pMemSpells = GetSpellBitmask(SpellID::Healing);
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 1;
	player._pMemSpells |= GetSpellBitmask(SpellID::Apocalypse);
	player._pSplHotKey[0] = SpellID::Apocalypse;
	player._pSplTHotKey[0] = SpellType::Spell;
	player._pSplHotKey[1] = SpellID::Healing;
	player._pSplTHotKey[1] = SpellType::Spell;
	player._pRSpell = SpellID::Apocalypse;
	player._pRSplType = SpellType::Spell;

	pfile_write_hero();

	player._pSplHotKey[0] = SpellID::Invalid;
	player._pSplTHotKey[0] = SpellType::Invalid;
	player._pSplHotKey[1] = SpellID::Invalid;
	player._pSplTHotKey[1] = SpellType::Invalid;
	player._pRSpell = SpellID::Invalid;
	player._pRSplType = SpellType::Invalid;

	pfile_read_player_from_save(info.saveNumber, player);

	EXPECT_EQ(player._pSplHotKey[0], SpellID::Invalid);
	EXPECT_EQ(player._pSplTHotKey[0], SpellType::Invalid);
	EXPECT_EQ(player._pSplHotKey[1], SpellID::Healing);
	EXPECT_EQ(player._pSplTHotKey[1], SpellType::Spell);
	EXPECT_EQ(player._pRSpell, SpellID::Invalid);
	EXPECT_EQ(player._pRSplType, SpellType::Invalid);
}

TEST(Writehero, LoadHotkeysWithoutFileSanitizesAndNormalizesSpellState)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());

	gbVanilla = true;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);

	Player &player = *MyPlayer;
	player._pMemSpells = GetSpellBitmask(SpellID::Healing);
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 1;

	player._pRSpell = SpellID::Apocalypse;
	player._pRSplType = SpellType::Spell;
	player.queuedSpell.spellId = SpellID::Healing;
	player.queuedSpell.spellType = SpellType::Spell;
	player.queuedSpell.spellFrom = INVITEM_BELT_FIRST;
	player.queuedSpell.spellLevel = 7;
	player.executedSpell.spellId = SpellID::Healing;
	player.executedSpell.spellType = SpellType::Scroll;
	player.executedSpell.spellFrom = INVITEM_INV_FIRST;
	player.executedSpell.spellLevel = 3;
	player.spellFrom = INVITEM_INV_FIRST;

	LoadHotkeys(info.saveNumber, player);

	EXPECT_EQ(player._pRSpell, SpellID::Invalid);
	EXPECT_EQ(player._pRSplType, SpellType::Invalid);
	EXPECT_EQ(player.queuedSpell.spellId, SpellID::Invalid);
	EXPECT_EQ(player.queuedSpell.spellType, SpellType::Invalid);
	EXPECT_EQ(player.queuedSpell.spellFrom, 0);
	EXPECT_EQ(player.queuedSpell.spellLevel, 0);
	EXPECT_EQ(player.executedSpell.spellId, SpellID::Invalid);
	EXPECT_EQ(player.executedSpell.spellType, SpellType::Invalid);
	EXPECT_EQ(player.executedSpell.spellFrom, 0);
	EXPECT_EQ(player.executedSpell.spellLevel, 0);
	EXPECT_EQ(player.spellFrom, 0);
}

TEST(Writehero, LoadHotkeysLegacyFormatSanitizesInvalidSelections)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());

	gbVanilla = false;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);

	Player &player = *MyPlayer;
	player._pMemSpells = GetSpellBitmask(SpellID::Healing);
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 1;

	WriteLegacyHotkeys(
	    savePath,
	    { SpellID::Apocalypse, SpellID::Healing, SpellID::Invalid, SpellID::Invalid },
	    { SpellType::Spell, SpellType::Spell, SpellType::Invalid, SpellType::Invalid },
	    SpellID::Apocalypse,
	    SpellType::Spell);

	player.queuedSpell.spellFrom = INVITEM_BELT_FIRST;
	player.queuedSpell.spellLevel = 9;
	player.executedSpell.spellFrom = INVITEM_INV_FIRST;
	player.executedSpell.spellLevel = 4;
	player.spellFrom = INVITEM_INV_FIRST;

	LoadHotkeys(info.saveNumber, player);

	EXPECT_EQ(player._pSplHotKey[0], SpellID::Invalid);
	EXPECT_EQ(player._pSplTHotKey[0], SpellType::Invalid);
	EXPECT_EQ(player._pSplHotKey[1], SpellID::Healing);
	EXPECT_EQ(player._pSplTHotKey[1], SpellType::Spell);
	EXPECT_EQ(player._pRSpell, SpellID::Invalid);
	EXPECT_EQ(player._pRSplType, SpellType::Invalid);
	EXPECT_EQ(player.queuedSpell.spellId, SpellID::Invalid);
	EXPECT_EQ(player.queuedSpell.spellType, SpellType::Invalid);
	EXPECT_EQ(player.queuedSpell.spellFrom, 0);
	EXPECT_EQ(player.queuedSpell.spellLevel, 0);
	EXPECT_EQ(player.executedSpell.spellId, SpellID::Invalid);
	EXPECT_EQ(player.executedSpell.spellType, SpellType::Invalid);
	EXPECT_EQ(player.executedSpell.spellFrom, 0);
	EXPECT_EQ(player.executedSpell.spellLevel, 0);
	EXPECT_EQ(player.spellFrom, 0);
}

TEST(Writehero, LoadHotkeysLegacyFormatPreservesValidScrollSelection)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());

	gbVanilla = false;
	gbIsHellfire = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	gbIsHellfireSaveGame = false;
	leveltype = DTYPE_TOWN;
	giNumberOfLevels = 17;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);

	Player &player = *MyPlayer;
	player._pNumInv = 1;
	InitializeItem(player.InvList[0], ItemMiscIdIdx(IMISC_SCROLL));
	player.InvList[0]._iSpell = SpellID::Healing;
	player.InvList[0]._iStatFlag = true;
	player._pScrlSpells = GetSpellBitmask(SpellID::Healing);
	ASSERT_TRUE((player._pScrlSpells & GetSpellBitmask(SpellID::Healing)) != 0);
	ASSERT_TRUE(IsPlayerSpellSelectionValid(player, SpellID::Healing, SpellType::Scroll));

	WriteLegacyHotkeys(
	    savePath,
	    { SpellID::Healing, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid },
	    { SpellType::Scroll, SpellType::Invalid, SpellType::Invalid, SpellType::Invalid },
	    SpellID::Healing,
	    SpellType::Scroll);

	LoadHotkeys(info.saveNumber, player);

	EXPECT_EQ(player._pRSpell, SpellID::Healing);
	EXPECT_EQ(player._pRSplType, SpellType::Scroll);
	EXPECT_EQ(player.queuedSpell.spellId, SpellID::Healing);
	EXPECT_EQ(player.queuedSpell.spellType, SpellType::Scroll);
	EXPECT_EQ(player.queuedSpell.spellFrom, 0);
	leveltype = DTYPE_CATHEDRAL;
	EXPECT_TRUE(CanUseScroll(player, SpellID::Healing));

	player.InvList[0].clear();
	player._pNumInv = 0;
	player._pScrlSpells = 0;

	EXPECT_FALSE(CanUseScroll(player, SpellID::Healing));
}

TEST(Writehero, DiabloRewritePersistsSanitizedSpellSelectionsFromHellfireSave)
{
	LoadCoreArchives();
	LoadGameArchives();

	if (!HaveMainData()) {
		GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
	}

	const std::string savePath = paths::BasePath() + "multi_0.sv";
	const std::string hellfireSavePath = paths::BasePath() + "multi_0.hsv";
	paths::SetPrefPath(paths::BasePath());
	RemoveFile(savePath.c_str());
	RemoveFile(hellfireSavePath.c_str());

	gbVanilla = false;
	gbIsSpawn = false;
	gbIsMultiplayer = true;
	leveltype = DTYPE_TOWN;
	currlevel = 0;
	ViewPosition = {};
	giNumberOfLevels = 25;
	gbIsHellfire = true;
	gbIsHellfireSaveGame = true;

	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];

	LoadSpellData();
	LoadPlayerDataFiles();
	LoadMonsterData();
	LoadItemData();
	_uiheroinfo info {};
	info.heroclass = HeroClass::Rogue;
	pfile_ui_save_create(&info);
	gSaveNumber = info.saveNumber;

	Player &player = *MyPlayer;
	player._pMemSpells = GetSpellBitmask(SpellID::Healing) | GetSpellBitmask(SpellID::Apocalypse);
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 1;
	player._pSplLvl[static_cast<size_t>(SpellID::Apocalypse)] = 1;
	player._pSplHotKey[0] = SpellID::Apocalypse;
	player._pSplTHotKey[0] = SpellType::Spell;
	player._pSplHotKey[1] = SpellID::Healing;
	player._pSplTHotKey[1] = SpellType::Spell;
	player._pRSpell = SpellID::Apocalypse;
	player._pRSplType = SpellType::Spell;

	pfile_write_hero(/*writeGameData=*/true);
	RenameFile(hellfireSavePath.c_str(), savePath.c_str());

	gbIsHellfire = false;
	gbIsHellfireSaveGame = false;
	giNumberOfLevels = 17;

	pfile_read_player_from_save(info.saveNumber, player);
	pfile_write_hero();

	player._pSplHotKey[0] = SpellID::Apocalypse;
	player._pSplTHotKey[0] = SpellType::Spell;
	player._pSplHotKey[1] = SpellID::Invalid;
	player._pSplTHotKey[1] = SpellType::Invalid;
	player._pRSpell = SpellID::Apocalypse;
	player._pRSplType = SpellType::Spell;

	LoadHotkeys();

	EXPECT_EQ(player._pSplHotKey[0], SpellID::Invalid);
	EXPECT_EQ(player._pSplTHotKey[0], SpellType::Invalid);
	EXPECT_EQ(player._pSplHotKey[1], SpellID::Healing);
	EXPECT_EQ(player._pSplTHotKey[1], SpellType::Spell);
	EXPECT_EQ(player._pRSpell, SpellID::Invalid);
	EXPECT_EQ(player._pRSplType, SpellType::Invalid);
}

} // namespace
} // namespace devilution
