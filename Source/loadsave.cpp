/**
 * @file loadsave.cpp
 *
 * Implementation of save game functionality.
 */
#include "loadsave.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>

#include <SDL.h>
#include <ankerl/unordered_dense.h>
#include <expected.hpp>
#include <fmt/core.h>

#include "automap.h"
#include "codec.h"
#include "control.h"
#include "cursor.h"
#include "dead.h"
#include "doom.h"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "game_mode.hpp"
#include "inv.h"
#include "levels/dun_tile.hpp"
#include "lighting.h"
#include "menu.h"
#include "missiles.h"
#include "monster.h"
#include "mpq/mpq_common.hpp"
#include "pfile.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "stores.h"
#include "utils/algorithm/container.hpp"
#include "utils/endian_read.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/status_macros.hpp"

namespace devilution {

bool gbIsHellfireSaveGame;
uint8_t giNumberOfLevels;

namespace {

constexpr size_t MaxMissilesForSaveGame = 125;
constexpr size_t PlayerWalkPathSizeForSaveGame = 25;

uint8_t giNumberQuests;
uint8_t giNumberOfSmithPremiumItems;

template <class T>
T SwapLE(T in)
{
	switch (sizeof(T)) {
	case 2:
		return SDL_SwapLE16(in);
	case 4:
		return SDL_SwapLE32(in);
	case 8:
		return SDL_SwapLE64(in);
	default:
		return in;
	}
}

template <class T>
T SwapBE(T in)
{
	switch (sizeof(T)) {
	case 2:
		return SDL_SwapBE16(in);
	case 4:
		return SDL_SwapBE32(in);
	case 8:
		return static_cast<T>(SDL_SwapBE64(in));
	default:
		return in;
	}
}

void TerminateUtf8(char *str, size_t maxLength)
{
	std::string_view inStr { str, maxLength };
	std::string_view truncStr = TruncateUtf8(inStr, maxLength - 1);
	size_t utf8Length = truncStr.size();
	str[utf8Length] = '\0';
}

class LoadHelper {
	std::unique_ptr<std::byte[]> m_buffer_;
	size_t m_cur_ = 0;
	size_t m_size_;

	template <class T>
	T Next()
	{
		const auto size = sizeof(T);
		if (!IsValid(size))
			return 0;

		T value;
		memcpy(&value, &m_buffer_[m_cur_], size);
		m_cur_ += size;

		return value;
	}

public:
	LoadHelper(std::optional<SaveReader> archive, const char *szFileName)
	{
		if (archive)
			m_buffer_ = ReadArchive(*archive, szFileName, &m_size_);
		else
			m_buffer_ = nullptr;
	}

	bool IsValid(size_t size = 1)
	{
		return m_buffer_ != nullptr
		    && m_size_ >= (m_cur_ + size);
	}

	template <typename T>
	constexpr void Skip(size_t count = 1)
	{
		Skip(sizeof(T) * count);
	}

	void Skip(size_t size)
	{
		m_cur_ += size;
	}

	void NextBytes(void *bytes, size_t size)
	{
		if (!IsValid(size))
			return;

		memcpy(bytes, &m_buffer_[m_cur_], size);
		m_cur_ += size;
	}

	template <class T>
	T NextLE()
	{
		return SwapLE(Next<T>());
	}

	template <class T>
	T NextBE()
	{
		return SwapBE(Next<T>());
	}

	template <class TSource, class TDesired>
	TDesired NextLENarrow(TSource modifier = 0)
	{
		static_assert(sizeof(TSource) > sizeof(TDesired), "Can only narrow to a smaller type");
		TSource value = SwapLE(Next<TSource>()) + modifier;
		return static_cast<TDesired>(std::clamp<TSource>(value, std::numeric_limits<TDesired>::min(), std::numeric_limits<TDesired>::max()));
	}

	bool NextBool8()
	{
		return Next<uint8_t>() != 0;
	}

	bool NextBool32()
	{
		return Next<uint32_t>() != 0;
	}
};

class SaveHelper {
	SaveWriter &m_mpqWriter;
	const char *m_szFileName_;
	std::unique_ptr<std::byte[]> m_buffer_;
	size_t m_cur_ = 0;
	size_t m_capacity_;

public:
	SaveHelper(SaveWriter &mpqWriter, const char *szFileName, size_t bufferLen)
	    : m_mpqWriter(mpqWriter)
	    , m_szFileName_(szFileName)
	    , m_buffer_(new std::byte[codec_get_encoded_len(bufferLen)])
	    , m_capacity_(bufferLen)
	{
	}

	bool IsValid(size_t len = 1)
	{
		return m_buffer_ != nullptr
		    && m_capacity_ >= (m_cur_ + len);
	}

	template <typename T>
	constexpr void Skip(size_t count = 1)
	{
		Skip(sizeof(T) * count);
	}

	void Skip(size_t len)
	{
		std::memset(&m_buffer_[m_cur_], 0, len);
		m_cur_ += len;
	}

	void WriteBytes(const void *bytes, size_t len)
	{
		if (!IsValid(len))
			return;

		memcpy(&m_buffer_[m_cur_], bytes, len);
		m_cur_ += len;
	}

	template <class T>
	void WriteLE(T value)
	{
		value = SwapLE(value);
		WriteBytes(&value, sizeof(value));
	}

	template <class T>
	void WriteBE(T value)
	{
		value = SwapBE(value);
		WriteBytes(&value, sizeof(value));
	}

	~SaveHelper()
	{
		const auto encodedLen = codec_get_encoded_len(m_cur_);
		const char *const password = pfile_get_password();
		codec_encode(m_buffer_.get(), m_cur_, encodedLen, password);
		m_mpqWriter.WriteFile(m_szFileName_, m_buffer_.get(), encodedLen);
	}
};

struct MonsterConversionData {
	int8_t monsterLevel;
	uint16_t experience;
	uint8_t toHit;
	uint8_t toHitSpecial;
};

struct LevelConversionData {
	MonsterConversionData monsterConversionData[MaxMonsters];
};

void LoadItemData(LoadHelper &file, Item &item)
{
	item._iSeed = file.NextLE<uint32_t>();
	item._iCreateInfo = file.NextLE<uint16_t>();
	file.Skip(2); // Alignment
	item._itype = static_cast<ItemType>(file.NextLE<uint32_t>());
	item.position.x = file.NextLE<int32_t>();
	item.position.y = file.NextLE<int32_t>();
	item._iAnimFlag = file.NextBool32();
	file.Skip(4); // Skip pointer _iAnimData
	item.AnimInfo = {};
	item.AnimInfo.numberOfFrames = file.NextLENarrow<int32_t, int8_t>();
	item.AnimInfo.currentFrame = file.NextLENarrow<int32_t, int8_t>(-1);
	file.Skip(8); // Skip _iAnimWidth and _iAnimWidth2
	file.Skip(4); // Unused since 1.02
	item.selectionRegion = static_cast<SelectionRegion>(file.NextLE<uint8_t>());
	file.Skip(3); // Alignment
	item._iPostDraw = file.NextBool32();
	item._iIdentified = file.NextBool32();
	item._iMagical = static_cast<item_quality>(file.NextLE<int8_t>());
	file.NextBytes(item._iName, ItemNameLength);
	TerminateUtf8(item._iName, ItemNameLength);
	file.NextBytes(item._iIName, ItemNameLength);
	TerminateUtf8(item._iIName, ItemNameLength);
	item._iLoc = static_cast<item_equip_type>(file.NextLE<int8_t>());
	item._iClass = static_cast<item_class>(file.NextLE<uint8_t>());
	file.Skip(1); // Alignment
	item._iCurs = file.NextLE<int32_t>();
	item._ivalue = file.NextLE<int32_t>();
	item._iIvalue = file.NextLE<int32_t>();
	item._iMinDam = file.NextLE<int32_t>();
	item._iMaxDam = file.NextLE<int32_t>();
	item._iAC = file.NextLE<int32_t>();
	item._iFlags = static_cast<ItemSpecialEffect>(file.NextLE<uint32_t>());
	item._iMiscId = static_cast<item_misc_id>(file.NextLE<int32_t>());
	item._iSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	item._iCharges = file.NextLE<int32_t>();
	item._iMaxCharges = file.NextLE<int32_t>();
	item._iDurability = file.NextLE<int32_t>();
	item._iMaxDur = file.NextLE<int32_t>();
	item._iPLDam = file.NextLE<int32_t>();
	item._iPLToHit = file.NextLE<int32_t>();
	item._iPLAC = file.NextLE<int32_t>();
	item._iPLStr = file.NextLE<int32_t>();
	item._iPLMag = file.NextLE<int32_t>();
	item._iPLDex = file.NextLE<int32_t>();
	item._iPLVit = file.NextLE<int32_t>();
	item._iPLFR = file.NextLE<int32_t>();
	item._iPLLR = file.NextLE<int32_t>();
	item._iPLMR = file.NextLE<int32_t>();
	item._iPLMana = file.NextLE<int32_t>();
	item._iPLHP = file.NextLE<int32_t>();
	item._iPLDamMod = file.NextLE<int32_t>();
	item._iPLGetHit = file.NextLE<int32_t>();
	item._iPLLight = file.NextLE<int32_t>();
	item._iSplLvlAdd = file.NextLE<int8_t>();
	item._iRequest = file.NextBool8();
	file.Skip(2); // Alignment
	item._iUid = file.NextLE<int32_t>();
	item._iFMinDam = file.NextLE<int32_t>();
	item._iFMaxDam = file.NextLE<int32_t>();
	item._iLMinDam = file.NextLE<int32_t>();
	item._iLMaxDam = file.NextLE<int32_t>();
	item._iPLEnAc = file.NextLE<int32_t>();
	item._iPrePower = static_cast<item_effect_type>(file.NextLE<int8_t>());
	item._iSufPower = static_cast<item_effect_type>(file.NextLE<int8_t>());
	file.Skip(2); // Alignment
	item._iVAdd1 = file.NextLE<int32_t>();
	item._iVMult1 = file.NextLE<int32_t>();
	item._iVAdd2 = file.NextLE<int32_t>();
	item._iVMult2 = file.NextLE<int32_t>();
	item._iMinStr = file.NextLE<int8_t>();
	item._iMinMag = file.NextLE<uint8_t>();
	item._iMinDex = file.NextLE<int8_t>();
	file.Skip(1); // Alignment
	item._iStatFlag = file.NextBool32();
	item.IDidx = static_cast<_item_indexes>(file.NextLE<int32_t>());
	if (gbIsSpawn) {
		item.IDidx = RemapItemIdxFromSpawn(item.IDidx);
	}
	if (!gbIsHellfireSaveGame) {
		item.IDidx = RemapItemIdxFromDiablo(item.IDidx);
	}
	item.dwBuff = file.NextLE<uint32_t>();
	if (gbIsHellfireSaveGame)
		item._iDamAcFlags = static_cast<ItemSpecialEffectHf>(file.NextLE<uint32_t>());
	else
		item._iDamAcFlags = ItemSpecialEffectHf::None;
	UpdateHellfireFlag(item, item._iIName);
}

void LoadAndValidateItemData(LoadHelper &file, Item &item)
{
	LoadItemData(file, item);
	RemoveInvalidItem(item);
}

void LoadPlayer(LoadHelper &file, Player &player)
{
	player._pmode = static_cast<PLR_MODE>(file.NextLE<int32_t>());

	for (size_t i = 0; i < PlayerWalkPathSizeForSaveGame; ++i) {
		player.walkpath[i] = file.NextLE<int8_t>();
	}
	player.walkpath[PlayerWalkPathSizeForSaveGame] = WALK_NONE;

	player.plractive = file.NextBool8();
	file.Skip(2); // Alignment
	player.destAction = static_cast<action_id>(file.NextLE<int32_t>());
	player.destParam1 = file.NextLE<int32_t>();
	player.destParam2 = file.NextLE<int32_t>();
	player.destParam3 = file.NextLE<int32_t>();
	player.destParam4 = file.NextLE<int32_t>();
	player.setLevel(file.NextLE<uint32_t>());
	player.position.tile.x = file.NextLE<int32_t>();
	player.position.tile.y = file.NextLE<int32_t>();
	player.position.future.x = file.NextLE<int32_t>();
	player.position.future.y = file.NextLE<int32_t>();
	file.Skip<uint32_t>(2); // Skip _ptargx and _ptargy
	player.position.last.x = file.NextLE<int32_t>();
	player.position.last.y = file.NextLE<int32_t>();
	player.position.old.x = file.NextLE<int32_t>();
	player.position.old.y = file.NextLE<int32_t>();
	file.Skip<int32_t>(4); // Skip offset and velocity
	player._pdir = static_cast<Direction>(file.NextLE<int32_t>());
	file.Skip(4); // Unused
	player._pgfxnum = file.NextLENarrow<uint32_t, uint8_t>();
	file.Skip<uint32_t>(); // Skip pointer pData
	player.AnimInfo = {};
	player.AnimInfo.ticksPerFrame = file.NextLENarrow<int32_t, int8_t>(1);
	player.AnimInfo.tickCounterOfCurrentFrame = file.NextLENarrow<int32_t, int8_t>();
	player.AnimInfo.numberOfFrames = file.NextLENarrow<int32_t, int8_t>();
	player.AnimInfo.currentFrame = file.NextLENarrow<int32_t, int8_t>(-1);
	file.Skip<uint32_t>(3); // Skip _pAnimWidth, _pAnimWidth2, _peflag
	player.lightId = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // _pvid

	player.queuedSpell.spellId = static_cast<SpellID>(file.NextLE<int32_t>());
	player.queuedSpell.spellType = static_cast<SpellType>(file.NextLE<int8_t>());
	auto spellFrom = file.NextLE<int8_t>();
	if (!IsValidSpellFrom(spellFrom))
		spellFrom = 0;
	player.spellFrom = spellFrom;
	player.queuedSpell.spellFrom = spellFrom;
	file.Skip(2); // Alignment
	player.inventorySpell = static_cast<SpellID>(file.NextLE<int32_t>());
	file.Skip<int8_t>(); // Skip _pTSplType
	file.Skip(3);        // Alignment
	player._pRSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	player._pRSplType = static_cast<SpellType>(file.NextLE<int8_t>());
	file.Skip(3); // Alignment
	player._pSBkSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	file.Skip<int8_t>(); // Skip _pSBkSplType

	// Only read spell levels for learnable spells
	for (int i = 0; i < static_cast<int>(SpellID::LAST); i++) {
		auto spl = static_cast<SpellID>(i);
		if (GetSpellData(spl).sBookLvl != -1)
			player._pSplLvl[i] = file.NextLE<uint8_t>();
		else
			file.Skip<uint8_t>();
	}
	// Skip indices that are unused
	for (int i = static_cast<int>(SpellID::LAST); i < 64; i++)
		file.Skip<uint8_t>();
	// These spells are unavailable in Diablo as learnable spells
	if (!gbIsHellfire) {
		player._pSplLvl[static_cast<uint8_t>(SpellID::Apocalypse)] = 0;
		player._pSplLvl[static_cast<uint8_t>(SpellID::Nova)] = 0;
	}

	file.Skip(7); // Alignment
	player._pMemSpells = file.NextLE<uint64_t>();
	player._pAblSpells = file.NextLE<uint64_t>();
	player._pScrlSpells = file.NextLE<uint64_t>();
	player._pSpellFlags = static_cast<SpellFlag>(file.NextLE<uint8_t>());
	file.Skip(3); // Alignment

	// Extra hotkeys: to keep single player save compatibility, read only 4 hotkeys here, rely on LoadHotkeys for the rest
	for (size_t i = 0; i < 4; i++) {
		player._pSplHotKey[i] = static_cast<SpellID>(file.NextLE<int32_t>());
	}
	for (size_t i = 0; i < 4; i++) {
		player._pSplTHotKey[i] = static_cast<SpellType>(file.NextLE<uint8_t>());
	}

	file.Skip<int32_t>(); // Skip _pwtype
	player._pBlockFlag = file.NextBool8();
	player._pInvincible = file.NextBool8();
	player._pLightRad = file.NextLE<int8_t>();
	player._pLvlChanging = file.NextBool8();

	file.NextBytes(player._pName, PlayerNameLength);
	TerminateUtf8(player._pName, PlayerNameLength);
	player._pClass = static_cast<HeroClass>(file.NextLE<int8_t>());
	file.Skip(3); // Alignment
	player._pStrength = file.NextLE<int32_t>();
	player._pBaseStr = file.NextLE<int32_t>();
	player._pMagic = file.NextLE<int32_t>();
	player._pBaseMag = file.NextLE<int32_t>();
	player._pDexterity = file.NextLE<int32_t>();
	player._pBaseDex = file.NextLE<int32_t>();
	player._pVitality = file.NextLE<int32_t>();
	player._pBaseVit = file.NextLE<int32_t>();
	player._pStatPts = file.NextLE<int32_t>();
	player._pDamageMod = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // Skip _pBaseToBlk - always a copy of PlayerData.blockBonus
	player._pHPBase = file.NextLE<int32_t>();
	player._pMaxHPBase = file.NextLE<int32_t>();
	player._pHitPoints = file.NextLE<int32_t>();
	player._pMaxHP = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // Skip _pHPPer - always derived from hp and maxHP.
	player._pManaBase = file.NextLE<int32_t>();
	player._pMaxManaBase = file.NextLE<int32_t>();
	player._pMana = file.NextLE<int32_t>();
	player._pMaxMana = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // Skip _pManaPer - always derived from mana and maxMana
	player.setCharacterLevel(file.NextLE<uint8_t>());
	file.Skip<uint8_t>(); // Skip _pMaxLevel - unused
	file.Skip(2);         // Alignment
	player._pExperience = file.NextLE<uint32_t>();
	file.Skip<uint32_t>(); // Skip _pMaxExp - unused
	file.Skip<uint32_t>(); // Skip _pNextExper, we retrieve it when needed based on _pLevel
	player._pArmorClass = file.NextLE<int8_t>();
	player._pMagResist = file.NextLE<int8_t>();
	player._pFireResist = file.NextLE<int8_t>();
	player._pLghtResist = file.NextLE<int8_t>();
	player._pGold = file.NextLE<int32_t>();
	player._pInfraFlag = file.NextBool32();

	int32_t tempPositionX = file.NextLE<int32_t>();
	int32_t tempPositionY = file.NextLE<int32_t>();
	if (player._pmode == PM_WALK_NORTHWARDS) {
		// These values are saved as offsets to remain consistent with old savefiles
		tempPositionX += player.position.tile.x;
		tempPositionY += player.position.tile.y;
	}
	player.position.temp.x = static_cast<WorldTileCoord>(tempPositionX);
	player.position.temp.y = static_cast<WorldTileCoord>(tempPositionY);

	player.tempDirection = static_cast<Direction>(file.NextLE<int32_t>());
	player.queuedSpell.spellLevel = file.NextLE<int32_t>();
	file.Skip<uint32_t>(); // skip _pVar5, was used for storing position of a tile which should have its HorizontalMovingPlayer flag removed after walking
	file.Skip<int32_t>(2); // skip offset2;
	file.Skip<uint32_t>(); // Skip actionFrame

	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		player._pLvlVisited[i] = file.NextBool8();

	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		player._pSLvlVisited[i] = file.NextBool8();

	file.Skip(2);           // Alignment
	file.Skip<uint32_t>();  // skip _pGFXLoad
	file.Skip<uint32_t>(8); // Skip pointers _pNAnim
	player._pNFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pNWidth
	file.Skip<uint32_t>(8); // Skip pointers _pWAnim
	player._pWFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pWWidth
	file.Skip<uint32_t>(8); // Skip pointers _pAAnim
	player._pAFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pAWidth
	player._pAFNum = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(8); // Skip pointers _pLAnim
	file.Skip<uint32_t>(8); // Skip pointers _pFAnim
	file.Skip<uint32_t>(8); // Skip pointers _pTAnim
	player._pSFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pSWidth
	player._pSFNum = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(8); // Skip pointers _pHAnim
	player._pHFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pHWidth
	file.Skip<uint32_t>(8); // Skip pointers _pDAnim
	player._pDFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pDWidth
	file.Skip<uint32_t>(8); // Skip pointers _pBAnim
	player._pBFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pBWidth

	for (Item &item : player.InvBody)
		LoadAndValidateItemData(file, item);

	for (Item &item : player.InvList)
		LoadAndValidateItemData(file, item);

	player._pNumInv = file.NextLE<int32_t>();

	for (int8_t &cell : player.InvGrid)
		cell = file.NextLE<int8_t>();

	for (Item &item : player.SpdList)
		LoadAndValidateItemData(file, item);

	LoadAndValidateItemData(file, player.HoldItem);

	player._pIMinDam = file.NextLE<int32_t>();
	player._pIMaxDam = file.NextLE<int32_t>();
	player._pIAC = file.NextLE<int32_t>();
	player._pIBonusDam = file.NextLE<int32_t>();
	player._pIBonusToHit = file.NextLE<int32_t>();
	player._pIBonusAC = file.NextLE<int32_t>();
	player._pIBonusDamMod = file.NextLE<int32_t>();
	file.Skip(4); // Alignment

	player._pISpells = file.NextLE<uint64_t>();
	player._pIFlags = static_cast<ItemSpecialEffect>(file.NextLE<int32_t>());
	player._pIGetHit = file.NextLE<int32_t>();
	player._pISplLvlAdd = file.NextLE<int8_t>();
	file.Skip(1);         // Unused
	file.Skip(2);         // Alignment
	file.Skip<int32_t>(); // _pISplDur
	player._pIEnAc = file.NextLE<int32_t>();
	player._pIFMinDam = file.NextLE<int32_t>();
	player._pIFMaxDam = file.NextLE<int32_t>();
	player._pILMinDam = file.NextLE<int32_t>();
	player._pILMaxDam = file.NextLE<int32_t>();
	player._pOilType = static_cast<item_misc_id>(file.NextLE<int32_t>());
	player.pTownWarps = file.NextLE<uint8_t>();
	player.pDungMsgs = file.NextLE<uint8_t>();
	player.pLvlLoad = file.NextLE<uint8_t>();

	if (gbIsHellfireSaveGame) {
		player.pDungMsgs2 = file.NextLE<uint8_t>();
	} else {
		player.pDungMsgs2 = 0;
		file.Skip(1); // pBattleNet
	}
	player.pManaShield = file.NextBool8();
	if (gbIsHellfireSaveGame) {
		player.pOriginalCathedral = file.NextBool8();
	} else {
		file.Skip(1);
		player.pOriginalCathedral = true;
	}
	file.Skip(2); // Available bytes
	player.wReflections = file.NextLE<uint16_t>();
	file.Skip(14); // Available bytes

	player.pDiabloKillLevel = file.NextLE<uint32_t>();
	sgGameInitInfo.nDifficulty = static_cast<_difficulty>(file.NextLE<uint32_t>());
	player.pDamAcFlags = static_cast<ItemSpecialEffectHf>(file.NextLE<uint32_t>());
	file.Skip(20); // Available bytes
	CalcPlrInv(player, false);

	player.executedSpell = player.queuedSpell; // Ensures backwards compatibility

	// Omit pointer _pNData
	// Omit pointer _pWData
	// Omit pointer _pAData
	// Omit pointer _pLData
	// Omit pointer _pFData
	// Omit pointer  _pTData
	// Omit pointer _pHData
	// Omit pointer _pDData
	// Omit pointer _pBData
	// Omit pointer pReserved

	// Ensure plrIsOnSetLevel and plrlevel is correctly initialized, because in vanilla sometimes plrlevel is not updated to setlvlnum
	if (setlevel)
		player.setLevel(setlvlnum);
	else
		player.setLevel(currlevel);
}

bool gbSkipSync = false;

void LoadMonster(LoadHelper *file, Monster &monster, MonsterConversionData *monsterConversionData = nullptr)
{
	monster.levelType = file->NextLE<int32_t>();
	monster.mode = static_cast<MonsterMode>(file->NextLE<int32_t>());
	monster.goal = static_cast<MonsterGoal>(file->NextLE<uint8_t>());
	file->Skip(3); // Alignment
	monster.goalVar1 = file->NextLENarrow<int32_t, int16_t>();
	monster.goalVar2 = file->NextLENarrow<int32_t, int8_t>();
	monster.goalVar3 = file->NextLENarrow<int32_t, int8_t>();
	file->Skip(4); // Unused
	monster.pathCount = file->NextLE<uint8_t>();
	file->Skip(3); // Alignment
	monster.position.tile.x = file->NextLE<int32_t>();
	monster.position.tile.y = file->NextLE<int32_t>();
	monster.position.future.x = file->NextLE<int32_t>();
	monster.position.future.y = file->NextLE<int32_t>();
	monster.position.old.x = file->NextLE<int32_t>();
	monster.position.old.y = file->NextLE<int32_t>();
	file->Skip<int32_t>(4); // Skip offset and velocity
	monster.direction = static_cast<Direction>(file->NextLE<int32_t>());
	monster.enemy = file->NextLE<int32_t>();
	monster.enemyPosition.x = file->NextLE<uint8_t>();
	monster.enemyPosition.y = file->NextLE<uint8_t>();
	file->Skip(2); // Unused

	file->Skip(4); // Skip pointer _mAnimData
	monster.animInfo = {};
	monster.animInfo.ticksPerFrame = file->NextLENarrow<int32_t, int8_t>();
	// Ensure that we can increase the tickCounterOfCurrentFrame at least once without overflow (needed for backwards compatibility for sitting gargoyles)
	monster.animInfo.tickCounterOfCurrentFrame = file->NextLENarrow<int32_t, int8_t>(1) - 1;
	monster.animInfo.numberOfFrames = file->NextLENarrow<int32_t, int8_t>();
	monster.animInfo.currentFrame = file->NextLENarrow<int32_t, int8_t>(-1);
	file->Skip(4); // Skip _meflag
	monster.isInvalid = file->NextBool32();
	monster.var1 = file->NextLENarrow<int32_t, int16_t>();
	monster.var2 = file->NextLENarrow<int32_t, int16_t>();
	monster.var3 = file->NextLENarrow<int32_t, int8_t>();
	monster.position.temp.x = file->NextLENarrow<int32_t, WorldTileCoord>();
	monster.position.temp.y = file->NextLENarrow<int32_t, WorldTileCoord>();
	file->Skip<int32_t>(2); // skip offset2;
	file->Skip(4);          // Skip actionFrame
	monster.maxHitPoints = file->NextLE<int32_t>();
	monster.hitPoints = file->NextLE<int32_t>();

	monster.ai = static_cast<MonsterAIID>(file->NextLE<uint8_t>());
	monster.intelligence = file->NextLE<uint8_t>();
	file->Skip(2); // Alignment
	monster.flags = file->NextLE<uint32_t>();
	monster.activeForTicks = file->NextLE<uint8_t>();
	file->Skip(3); // Alignment
	file->Skip(4); // Unused
	monster.position.last.x = file->NextLE<int32_t>();
	monster.position.last.y = file->NextLE<int32_t>();
	monster.rndItemSeed = file->NextLE<uint32_t>();
	monster.aiSeed = file->NextLE<uint32_t>();
	file->Skip(4); // Unused

	monster.uniqueType = static_cast<UniqueMonsterType>(file->NextLE<uint8_t>() - 1);
	monster.uniqTrans = file->NextLE<uint8_t>();
	monster.corpseId = file->NextLE<int8_t>();

	monster.whoHit = file->NextLE<int8_t>();
	if (monsterConversionData != nullptr)
		monsterConversionData->monsterLevel = file->NextLE<int8_t>();
	else
		file->Skip(1); // Skip level - now calculated on the fly
	file->Skip(1);     // Alignment
	if (monsterConversionData != nullptr)
		monsterConversionData->experience = file->NextLE<uint16_t>();
	else
		file->Skip(2); // Skip exp - now calculated from monstdat when the monster dies

	if (monsterConversionData != nullptr)
		monsterConversionData->toHit = file->NextLE<uint8_t>();
	else if (monster.isPlayerMinion()) // Don't skip for golems
		monster.golemToHit = file->NextLE<uint8_t>();
	else
		file->Skip(1); // Skip toHit - now calculated on the fly
	monster.minDamage = file->NextLE<uint8_t>();
	monster.maxDamage = file->NextLE<uint8_t>();
	if (monsterConversionData != nullptr)
		monsterConversionData->toHitSpecial = file->NextLE<uint8_t>();
	else
		file->Skip(1); // Skip toHitSpecial - now calculated on the fly
	monster.minDamageSpecial = file->NextLE<uint8_t>();
	monster.maxDamageSpecial = file->NextLE<uint8_t>();
	monster.armorClass = file->NextLE<uint8_t>();
	file->Skip(1); // Alignment
	monster.resistance = file->NextLE<uint16_t>();
	file->Skip(2); // Alignment

	monster.talkMsg = static_cast<_speech_id>(file->NextLE<int32_t>());
	if (monster.talkMsg == TEXT_KING1) // Fix original bad mapping of NONE for monsters
		monster.talkMsg = TEXT_NONE;
	monster.leader = file->NextLE<uint8_t>();
	if (monster.leader == 0)
		monster.leader = Monster::NoLeader; // Golems shouldn't be leaders of other monsters
	monster.leaderRelation = static_cast<LeaderRelation>(file->NextLE<uint8_t>());
	monster.packSize = file->NextLE<uint8_t>();
	monster.lightId = file->NextLE<int8_t>();
	if (monster.lightId == 0)
		monster.lightId = NO_LIGHT; // Correct incorrect values in old saves

	// Omit pointer name;

	if (monster.mode == MonsterMode::Petrified)
		monster.animInfo.isPetrified = true;
}

/**
 * @brief Recalculate the pack size of monster group that may have underflown
 */
void SyncPackSize(Monster &leader)
{
	if (!leader.isUnique())
		return;
	if (leader.ai != MonsterAIID::Scavenger)
		return;

	leader.packSize = 0;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &minion = Monsters[ActiveMonsters[i]];
		if (minion.leaderRelation == LeaderRelation::Leashed && minion.getLeader() == &leader)
			leader.packSize++;
	}
}

void LoadMissile(LoadHelper *file)
{
	Missile missile = {};
	missile._mitype = static_cast<MissileID>(file->NextLE<int32_t>());
	missile.position.tile.x = file->NextLE<int32_t>();
	missile.position.tile.y = file->NextLE<int32_t>();
	missile.position.offset.deltaX = file->NextLE<int32_t>();
	missile.position.offset.deltaY = file->NextLE<int32_t>();
	missile.position.velocity.deltaX = file->NextLE<int32_t>();
	missile.position.velocity.deltaY = file->NextLE<int32_t>();
	missile.position.start.x = file->NextLE<int32_t>();
	missile.position.start.y = file->NextLE<int32_t>();
	missile.position.traveled.deltaX = file->NextLE<int32_t>();
	missile.position.traveled.deltaY = file->NextLE<int32_t>();
	missile._mimfnum = file->NextLE<int32_t>();
	missile._mispllvl = file->NextLE<int32_t>();
	missile._miDelFlag = file->NextBool32();
	missile._miAnimType = static_cast<MissileGraphicID>(file->NextLE<uint8_t>());
	file->Skip(3); // Alignment
	missile._miAnimFlags = static_cast<MissileGraphicsFlags>(file->NextLE<int32_t>());
	file->Skip(4); // Skip pointer _miAnimData
	missile._miAnimDelay = file->NextLE<int32_t>();
	missile._miAnimLen = file->NextLE<int32_t>();
	missile._miAnimWidth = file->NextLE<int32_t>();
	missile._miAnimWidth2 = file->NextLE<int32_t>();
	missile._miAnimCnt = file->NextLE<int32_t>();
	missile._miAnimAdd = file->NextLE<int32_t>();
	missile._miAnimFrame = file->NextLE<int32_t>();
	missile._miDrawFlag = file->NextBool32();
	missile._miLightFlag = file->NextBool32();
	missile._miPreFlag = file->NextBool32();
	missile._miUniqTrans = file->NextLE<uint32_t>();
	missile.duration = file->NextLE<int32_t>();
	missile._misource = file->NextLE<int32_t>();
	missile._micaster = static_cast<mienemy_type>(file->NextLE<int32_t>());
	missile._midam = file->NextLE<int32_t>();
	missile._miHitFlag = file->NextBool32();
	missile._midist = file->NextLE<int32_t>();
	missile._mlid = file->NextLE<int32_t>();
	missile._mirnd = file->NextLE<int32_t>();
	missile.var1 = file->NextLE<int32_t>();
	missile.var2 = file->NextLE<int32_t>();
	missile.var3 = file->NextLE<int32_t>();
	missile.var4 = file->NextLE<int32_t>();
	missile.var5 = file->NextLE<int32_t>();
	missile.var6 = file->NextLE<int32_t>();
	missile.var7 = file->NextLE<int32_t>();
	missile.limitReached = file->NextBool32();
	missile.lastCollisionTargetHash = 0;
	if (Missiles.size() < Missiles.max_size()) {
		Missiles.push_back(missile);
	}
}

_object_id ConvertFromHellfireObject(_object_id type)
{
	if (leveltype == DTYPE_NEST) {
		switch (type) {
		case OBJ_BARREL:
			return OBJ_POD;
		case OBJ_BARRELEX:
			return OBJ_PODEX;
		default:
			break;
		}
	}

	if (leveltype == DTYPE_CRYPT) {
		switch (type) {
		case OBJ_BARREL:
			return OBJ_URN;
		case OBJ_BARRELEX:
			return OBJ_URNEX;
		case OBJ_STORYBOOK:
			return OBJ_L5BOOKS;
		case OBJ_STORYCANDLE:
			return OBJ_L5CANDLE;
		case OBJ_L1LDOOR:
			return OBJ_L5LDOOR;
		case OBJ_L1RDOOR:
			return OBJ_L5RDOOR;
		case OBJ_LEVER:
			return OBJ_L5LEVER;
		case OBJ_SARC:
			return OBJ_L5SARC;
		default:
			break;
		}
	}

	return type;
}

void LoadObject(LoadHelper &file, Object &object)
{
	object._otype = ConvertFromHellfireObject(static_cast<_object_id>(file.NextLE<int32_t>()));
	object.position.x = file.NextLE<int32_t>();
	object.position.y = file.NextLE<int32_t>();
	object.applyLighting = file.NextBool32();
	object._oAnimFlag = file.NextBool32();
	file.Skip(4); // Skip pointer _oAnimData
	object._oAnimDelay = file.NextLE<int32_t>();
	object._oAnimCnt = file.NextLE<int32_t>();
	object._oAnimLen = file.NextLE<uint32_t>();
	object._oAnimFrame = file.NextLE<uint32_t>();
	object._oAnimWidth = static_cast<uint16_t>(file.NextLE<int32_t>());
	file.Skip(4); // Skip _oAnimWidth2
	object._oDelFlag = file.NextBool32();
	object._oBreak = file.NextLE<int8_t>();
	file.Skip(3); // Alignment
	object._oSolidFlag = file.NextBool32();
	object._oMissFlag = file.NextBool32();

	object.selectionRegion = static_cast<SelectionRegion>(file.NextLE<int8_t>());
	file.Skip(3); // Alignment
	object._oPreFlag = file.NextBool32();
	object._oTrapFlag = file.NextBool32();
	object._oDoorFlag = file.NextBool32();
	object._olid = file.NextLE<int32_t>();
	object._oRndSeed = file.NextLE<uint32_t>();
	object._oVar1 = file.NextLE<int32_t>();
	object._oVar2 = file.NextLE<int32_t>();
	object._oVar3 = file.NextLE<int32_t>();
	object._oVar4 = file.NextLE<int32_t>();
	object._oVar5 = file.NextLE<int32_t>();
	object._oVar6 = file.NextLE<uint32_t>();
	object.bookMessage = static_cast<_speech_id>(file.NextLE<int32_t>());
	object._oVar8 = file.NextLE<int32_t>();
}

void LoadItem(LoadHelper &file, Item &item)
{
	LoadAndValidateItemData(file, item);
	GetItemFrm(item);
}

void LoadPremium(LoadHelper &file, int i)
{
	LoadAndValidateItemData(file, PremiumItems[i]);
}

void LoadQuest(LoadHelper *file, int i)
{
	auto &quest = Quests[i];

	quest._qlevel = file->NextLE<uint8_t>();
	file->Skip<uint8_t>(); // _qtype, identical to _qidx
	quest._qactive = static_cast<quest_state>(file->NextLE<uint8_t>());
	quest._qlvltype = static_cast<dungeon_type>(file->NextLE<uint8_t>());
	quest.position.x = file->NextLE<int32_t>();
	quest.position.y = file->NextLE<int32_t>();
	quest._qslvl = static_cast<_setlevels>(file->NextLE<uint8_t>());
	quest._qidx = static_cast<quest_id>(file->NextLE<uint8_t>());
	if (gbIsHellfireSaveGame) {
		file->Skip(2); // Alignment
		quest._qmsg = static_cast<_speech_id>(file->NextLE<int32_t>());
	} else {
		quest._qmsg = static_cast<_speech_id>(file->NextLE<uint8_t>());
	}
	quest._qvar1 = file->NextLE<uint8_t>();
	quest._qvar2 = file->NextLE<uint8_t>();
	file->Skip(2); // Alignment
	if (!gbIsHellfireSaveGame)
		file->Skip(1); // Alignment
	quest._qlog = file->NextBool32();

	ReturnLvlPosition.x = file->NextBE<int32_t>();
	ReturnLvlPosition.y = file->NextBE<int32_t>();
	ReturnLevel = file->NextBE<int32_t>();
	ReturnLevelType = static_cast<dungeon_type>(file->NextBE<int32_t>());
	file->Skip(sizeof(int32_t)); // Skip DoomQuestState
}

void LoadLighting(LoadHelper *file, Light *pLight)
{
	pLight->position.tile.x = file->NextLE<int32_t>();
	pLight->position.tile.y = file->NextLE<int32_t>();
	pLight->radius = file->NextLE<int32_t>();
	file->Skip<int32_t>(); // _lid
	pLight->isInvalid = file->NextBool32();
	pLight->hasChanged = file->NextBool32();
	file->Skip(4); // Unused
	pLight->position.old.x = file->NextLE<int32_t>();
	pLight->position.old.y = file->NextLE<int32_t>();
	pLight->oldRadius = file->NextLE<int32_t>();
	pLight->position.offset.deltaX = file->NextLE<int32_t>();
	pLight->position.offset.deltaY = file->NextLE<int32_t>();
	file->Skip<uint32_t>(); // _lflags
}

void LoadPortal(LoadHelper *file, int i)
{
	Portal *pPortal = &Portals[i];

	pPortal->open = file->NextBool32();
	pPortal->position.x = file->NextLE<int32_t>();
	pPortal->position.y = file->NextLE<int32_t>();
	pPortal->level = file->NextLE<int32_t>();
	pPortal->ltype = static_cast<dungeon_type>(file->NextLE<int32_t>());
	pPortal->setlvl = file->NextBool32();
	if (!pPortal->setlvl)
		pPortal->ltype = GetLevelType(pPortal->level);
}

void GetLevelNames(std::string_view prefix, char *out)
{
	char suf;
	uint8_t num;
	if (setlevel) {
		suf = 's';
		num = static_cast<uint8_t>(setlvlnum);
	} else {
		suf = 'l';
		num = currlevel;
	}
	*fmt::format_to(out, "{}{}{:02d}", prefix, suf, num) = '\0';
}

void GetTempLevelNames(char *szTemp)
{
	return GetLevelNames("temp", szTemp);
}

void GetPermLevelNames(char *szPerm)
{
	return GetLevelNames("perm", szPerm);
}

bool LevelFileExists(SaveWriter &archive)
{
	char szName[MaxMpqPathSize];

	GetTempLevelNames(szName);
	if (archive.HasFile(szName))
		return true;

	GetPermLevelNames(szName);
	return archive.HasFile(szName);
}

void LoadMatchingItems(LoadHelper &file, const Player &player, const int n, Item *pItem)
{
	Item heroItem;

	for (int i = 0; i < n; i++) {
		Item &unpackedItem = pItem[i];
		LoadItemData(file, heroItem);
		if (unpackedItem.isEmpty() || heroItem.isEmpty())
			continue;
		if (unpackedItem._iSeed != heroItem._iSeed)
			continue;
		if (heroItem.IDidx == IDI_EAR)
			continue;
		if (gbIsMultiplayer) {
			// Ensure that the unpacked item was regenerated using the appropriate
			// game's item generation logic before attempting to use it for validation
			if ((heroItem.dwBuff & CF_HELLFIRE) != (unpackedItem.dwBuff & CF_HELLFIRE)) {
				unpackedItem = {};
				RecreateItem(player, unpackedItem, heroItem.IDidx, heroItem._iCreateInfo, heroItem._iSeed, heroItem._ivalue, heroItem.dwBuff);
				unpackedItem._iIdentified = heroItem._iIdentified;
				unpackedItem._iMaxDur = heroItem._iMaxDur;
				unpackedItem._iDurability = ClampDurability(unpackedItem, heroItem._iDurability);
				unpackedItem._iMaxCharges = std::clamp<int>(heroItem._iMaxCharges, 0, unpackedItem._iMaxCharges);
				unpackedItem._iCharges = std::clamp<int>(heroItem._iCharges, 0, unpackedItem._iMaxCharges);
			}
			if (gbIsHellfire) {
				unpackedItem._iPLToHit = ClampToHit(unpackedItem, heroItem._iPLToHit); // Oil of Accuracy
				unpackedItem._iMaxDam = ClampMaxDam(unpackedItem, heroItem._iMaxDam);  // Oil of Sharpness
			}
		} else {
			unpackedItem = heroItem;
		}
	}
}

/**
 * @brief Loads items on the current dungeon floor
 * @param file interface to the save file
 * @param savedItemCount how many items to read from the save file
 */
void LoadDroppedItems(LoadHelper &file, size_t savedItemCount)
{
	// Skip loading ActiveItems and AvailableItems, the indices are initialised below based on the number of valid items
	file.Skip<uint8_t>(MAXITEMS * 2);

	// Reset ActiveItems, the Items array will be populated from the start
	std::iota(ActiveItems, ActiveItems + MAXITEMS, uint8_t { 0 });
	ActiveItemCount = 0;
	// Clear dItem so we can populate valid drop locations
	memset(dItem, 0, sizeof(dItem));

	for (size_t i = 0; i < savedItemCount; i++) {
		Item &item = Items[ActiveItemCount];
		LoadItem(file, item);

		if (!item.isEmpty()) {
			// Loaded a valid item
			ActiveItemCount++;
			// populate its location in the lookup table with the offset in the Items array + 1 (so 0 can be used for "no item")
			dItem[item.position.x][item.position.y] = ActiveItemCount;
		}
	}
}

int getHellfireLevelType(int type)
{
	if (type == DTYPE_CRYPT)
		return DTYPE_CATHEDRAL;

	if (type == DTYPE_NEST)
		return DTYPE_CAVES;

	return type;
}

void SaveItem(SaveHelper &file, const Item &item)
{
	auto idx = item.IDidx;
	if (!gbIsHellfire)
		idx = RemapItemIdxToDiablo(idx);
	if (gbIsSpawn)
		idx = RemapItemIdxToSpawn(idx);
	ItemType iType = item._itype;
	if (idx == -1) {
		idx = _item_indexes::IDI_GOLD;
		iType = ItemType::None;
	}

	file.WriteLE<uint32_t>(item._iSeed);
	file.WriteLE<int16_t>(item._iCreateInfo);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(static_cast<int32_t>(iType));
	file.WriteLE<int32_t>(item.position.x);
	file.WriteLE<int32_t>(item.position.y);
	file.WriteLE<uint32_t>(item._iAnimFlag ? 1 : 0);
	file.Skip(4); // Skip pointer _iAnimData
	file.WriteLE<int32_t>(item.AnimInfo.numberOfFrames);
	file.WriteLE<int32_t>(item.AnimInfo.currentFrame + 1);
	// write _iAnimWidth for vanilla compatibility
	file.WriteLE<int32_t>(ItemAnimWidth);
	// write _iAnimWidth2 for vanilla compatibility
	file.WriteLE<int32_t>(CalculateSpriteTileCenterX(ItemAnimWidth));
	file.Skip<uint32_t>(); // _delFlag, unused since 1.02
	file.WriteLE<uint8_t>(static_cast<uint8_t>(item.selectionRegion));
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(item._iPostDraw ? 1 : 0);
	file.WriteLE<uint32_t>(item._iIdentified ? 1 : 0);
	file.WriteLE<int8_t>(item._iMagical);
	file.WriteBytes(item._iName, ItemNameLength);
	file.WriteBytes(item._iIName, ItemNameLength);
	file.WriteLE<int8_t>(item._iLoc);
	file.WriteLE<uint8_t>(item._iClass);
	file.Skip(1); // Alignment
	file.WriteLE<int32_t>(item._iCurs);
	file.WriteLE<int32_t>(item._ivalue);
	file.WriteLE<int32_t>(item._iIvalue);
	file.WriteLE<int32_t>(item._iMinDam);
	file.WriteLE<int32_t>(item._iMaxDam);
	file.WriteLE<int32_t>(item._iAC);
	file.WriteLE<uint32_t>(static_cast<uint32_t>(item._iFlags));
	file.WriteLE<int32_t>(item._iMiscId);
	file.WriteLE<int32_t>(static_cast<int8_t>(item._iSpell));
	file.WriteLE<int32_t>(item._iCharges);
	file.WriteLE<int32_t>(item._iMaxCharges);
	file.WriteLE<int32_t>(item._iDurability);
	file.WriteLE<int32_t>(item._iMaxDur);
	file.WriteLE<int32_t>(item._iPLDam);
	file.WriteLE<int32_t>(item._iPLToHit);
	file.WriteLE<int32_t>(item._iPLAC);
	file.WriteLE<int32_t>(item._iPLStr);
	file.WriteLE<int32_t>(item._iPLMag);
	file.WriteLE<int32_t>(item._iPLDex);
	file.WriteLE<int32_t>(item._iPLVit);
	file.WriteLE<int32_t>(item._iPLFR);
	file.WriteLE<int32_t>(item._iPLLR);
	file.WriteLE<int32_t>(item._iPLMR);
	file.WriteLE<int32_t>(item._iPLMana);
	file.WriteLE<int32_t>(item._iPLHP);
	file.WriteLE<int32_t>(item._iPLDamMod);
	file.WriteLE<int32_t>(item._iPLGetHit);
	file.WriteLE<int32_t>(item._iPLLight);
	file.WriteLE<int8_t>(item._iSplLvlAdd);
	file.WriteLE<int8_t>(item._iRequest ? 1 : 0);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(item._iUid);
	file.WriteLE<int32_t>(item._iFMinDam);
	file.WriteLE<int32_t>(item._iFMaxDam);
	file.WriteLE<int32_t>(item._iLMinDam);
	file.WriteLE<int32_t>(item._iLMaxDam);
	file.WriteLE<int32_t>(item._iPLEnAc);
	file.WriteLE<int8_t>(item._iPrePower);
	file.WriteLE<int8_t>(item._iSufPower);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(item._iVAdd1);
	file.WriteLE<int32_t>(item._iVMult1);
	file.WriteLE<int32_t>(item._iVAdd2);
	file.WriteLE<int32_t>(item._iVMult2);
	file.WriteLE<int8_t>(item._iMinStr);
	file.WriteLE<uint8_t>(item._iMinMag);
	file.WriteLE<int8_t>(item._iMinDex);
	file.Skip(1); // Alignment
	file.WriteLE<uint32_t>(item._iStatFlag ? 1 : 0);
	file.WriteLE<int32_t>(idx);
	file.WriteLE<uint32_t>(item.dwBuff);
	if (gbIsHellfire)
		file.WriteLE<uint32_t>(static_cast<uint32_t>(item._iDamAcFlags));
}

void SavePlayer(SaveHelper &file, const Player &player)
{
	file.WriteLE<int32_t>(player._pmode);
	for (size_t i = 0; i < PlayerWalkPathSizeForSaveGame; ++i) {
		file.WriteLE<int8_t>(player.walkpath[i]);
	}
	file.WriteLE<uint8_t>(player.plractive ? 1 : 0);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(player.destAction);
	file.WriteLE<int32_t>(player.destParam1);
	file.WriteLE<int32_t>(player.destParam2);
	file.WriteLE<int32_t>(static_cast<int32_t>(player.destParam3));
	file.WriteLE<int32_t>(player.destParam4);
	file.WriteLE<uint32_t>(player.plrlevel);
	file.WriteLE<int32_t>(player.position.tile.x);
	file.WriteLE<int32_t>(player.position.tile.y);
	file.WriteLE<int32_t>(player.position.future.x);
	file.WriteLE<int32_t>(player.position.future.y);

	// For backwards compatibility
	const Point target = player.GetTargetPosition();
	file.WriteLE<int32_t>(target.x);
	file.WriteLE<int32_t>(target.y);

	file.WriteLE<int32_t>(player.position.last.x);
	file.WriteLE<int32_t>(player.position.last.y);
	file.WriteLE<int32_t>(player.position.old.x);
	file.WriteLE<int32_t>(player.position.old.y);
	DisplacementOf<int16_t> offset = {};
	DisplacementOf<int16_t> offset2 = {};
	DisplacementOf<int16_t> velocity = {};
	if (player.isWalking()) {
		offset = player.position.CalculateWalkingOffset(player._pdir, player.AnimInfo);
		offset2 = player.position.CalculateWalkingOffsetShifted8(player._pdir, player.AnimInfo);
		velocity = player.position.GetWalkingVelocityShifted8(player._pdir, player.AnimInfo);
	}
	file.WriteLE<int32_t>(offset.deltaX);
	file.WriteLE<int32_t>(offset.deltaY);
	file.WriteLE<int32_t>(velocity.deltaX);
	file.WriteLE<int32_t>(velocity.deltaY);
	file.WriteLE<int32_t>(static_cast<int32_t>(player._pdir));
	file.Skip(4); // Unused
	file.WriteLE<uint32_t>(player._pgfxnum);
	file.Skip(4); // Skip pointer _pAnimData
	file.WriteLE<int32_t>(std::max(0, player.AnimInfo.ticksPerFrame - 1));
	file.WriteLE<int32_t>(player.AnimInfo.tickCounterOfCurrentFrame);
	file.WriteLE<int32_t>(player.AnimInfo.numberOfFrames);
	file.WriteLE<int32_t>(player.AnimInfo.currentFrame + 1);
	// write _pAnimWidth for vanilla compatibility
	const int animWidth = player.getSpriteWidth();
	file.WriteLE<int32_t>(animWidth);
	// write _pAnimWidth2 for vanilla compatibility
	file.WriteLE<int32_t>(CalculateSpriteTileCenterX(animWidth));
	file.Skip<uint32_t>(); // Skip _peflag
	file.WriteLE<int32_t>(player.lightId);
	file.WriteLE<int32_t>(1); // _pvid

	file.WriteLE<int32_t>(static_cast<int8_t>(player.queuedSpell.spellId));
	file.WriteLE<int8_t>(static_cast<int8_t>(player.queuedSpell.spellType));
	file.WriteLE<int8_t>(player.queuedSpell.spellFrom);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player.inventorySpell));
	file.Skip<int8_t>(); // Skip _pTSplType
	file.Skip(3);        // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pRSpell));
	file.WriteLE<int8_t>(static_cast<uint8_t>(player._pRSplType));
	file.Skip(3); // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pSBkSpell));
	file.Skip<int8_t>(); // Skip _pSBkSplType

	for (uint8_t spellLevel : player._pSplLvl)
		file.WriteLE<uint8_t>(spellLevel);

	file.Skip(7); // Alignment
	file.WriteLE<uint64_t>(player._pMemSpells);
	file.WriteLE<uint64_t>(player._pAblSpells);
	file.WriteLE<uint64_t>(player._pScrlSpells);
	file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pSpellFlags));
	file.Skip(3); // Alignment

	// Extra hotkeys: to keep single player save compatibility, write only 4 hotkeys here, rely on SaveHotkeys for the rest
	for (size_t i = 0; i < 4; i++) {
		file.WriteLE<int32_t>(static_cast<int8_t>(player._pSplHotKey[i]));
	}
	for (size_t i = 0; i < 4; i++) {
		file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pSplTHotKey[i]));
	}

	file.WriteLE<int32_t>(player.UsesRangedWeapon() ? 1 : 0);
	file.WriteLE<uint8_t>(player._pBlockFlag ? 1 : 0);
	file.WriteLE<uint8_t>(player._pInvincible ? 1 : 0);
	file.WriteLE<int8_t>(player._pLightRad);
	file.WriteLE<uint8_t>(player._pLvlChanging ? 1 : 0);

	file.WriteBytes(player._pName, PlayerNameLength);
	file.WriteLE<int8_t>(static_cast<int8_t>(player._pClass));
	file.Skip(3); // Alignment
	file.WriteLE<int32_t>(player._pStrength);
	file.WriteLE<int32_t>(player._pBaseStr);
	file.WriteLE<int32_t>(player._pMagic);
	file.WriteLE<int32_t>(player._pBaseMag);
	file.WriteLE<int32_t>(player._pDexterity);
	file.WriteLE<int32_t>(player._pBaseDex);
	file.WriteLE<int32_t>(player._pVitality);
	file.WriteLE<int32_t>(player._pBaseVit);
	file.WriteLE<int32_t>(player._pStatPts);
	file.WriteLE<int32_t>(player._pDamageMod);

	file.WriteLE<int32_t>(player.getBaseToBlock()); // set _pBaseToBlk for backwards compatibility
	file.WriteLE<int32_t>(player._pHPBase);
	file.WriteLE<int32_t>(player._pMaxHPBase);
	file.WriteLE<int32_t>(player._pHitPoints);
	file.WriteLE<int32_t>(player._pMaxHP);
	file.Skip<int32_t>(); // Skip _pHPPer
	file.WriteLE<int32_t>(player._pManaBase);
	file.WriteLE<int32_t>(player._pMaxManaBase);
	file.WriteLE<int32_t>(player._pMana);
	file.WriteLE<int32_t>(player._pMaxMana);
	file.Skip<int32_t>(); // Skip _pManaPer
	file.WriteLE<uint8_t>(player.getCharacterLevel());
	file.Skip<uint8_t>(); // skip _pMaxLevel, this value is uninitialised in most cases in Diablo/Hellfire so there's no point setting it.
	file.Skip(2);         // Alignment
	file.WriteLE<uint32_t>(player._pExperience);
	file.Skip<uint32_t>();                                       // Skip _pMaxExp
	file.WriteLE<uint32_t>(player.getNextExperienceThreshold()); // set _pNextExper for backwards compatibility
	file.WriteLE<int8_t>(player._pArmorClass);
	file.WriteLE<int8_t>(player._pMagResist);
	file.WriteLE<int8_t>(player._pFireResist);
	file.WriteLE<int8_t>(player._pLghtResist);
	file.WriteLE<int32_t>(player._pGold);
	file.WriteLE<uint32_t>(player._pInfraFlag ? 1 : 0);

	int32_t tempPositionX = player.position.temp.x;
	int32_t tempPositionY = player.position.temp.y;
	if (player._pmode == PM_WALK_NORTHWARDS) {
		// For backwards compatibility, save this as an offset
		tempPositionX -= player.position.tile.x;
		tempPositionY -= player.position.tile.y;
	}
	file.WriteLE<int32_t>(tempPositionX);
	file.WriteLE<int32_t>(tempPositionY);

	file.WriteLE<int32_t>(static_cast<int32_t>(player.tempDirection));
	file.WriteLE<int32_t>(player.queuedSpell.spellLevel);
	file.Skip<int32_t>(); // skip _pVar5, was used for storing position of a tile which should have its HorizontalMovingPlayer flag removed after walking
	file.WriteLE<int32_t>(offset2.deltaX);
	file.WriteLE<int32_t>(offset2.deltaY);
	file.Skip<int32_t>(); // Skip _pVar8
	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		file.WriteLE<uint8_t>(player._pLvlVisited[i] ? 1 : 0);
	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		file.WriteLE<uint8_t>(player._pSLvlVisited[i] ? 1 : 0); // only 10 used

	file.Skip(2); // Alignment

	file.Skip<int32_t>();   // Skip _pGFXLoad
	file.Skip<uint32_t>(8); // Skip pointers _pNAnim
	file.WriteLE<int32_t>(player._pNFrames);
	file.Skip<uint32_t>();  // Skip _pNWidth
	file.Skip<uint32_t>(8); // Skip pointers _pWAnim
	file.WriteLE<int32_t>(player._pWFrames);
	file.Skip<uint32_t>();  // Skip _pWWidth
	file.Skip<uint32_t>(8); // Skip pointers _pAAnim
	file.WriteLE<int32_t>(player._pAFrames);
	file.Skip<uint32_t>(); // Skip _pAWidth
	file.WriteLE<int32_t>(player._pAFNum);
	file.Skip<uint32_t>(8); // Skip pointers _pLAnim
	file.Skip<uint32_t>(8); // Skip pointers _pFAnim
	file.Skip<uint32_t>(8); // Skip pointers _pTAnim
	file.WriteLE<int32_t>(player._pSFrames);
	file.Skip<uint32_t>(); // Skip _pSWidth
	file.WriteLE<int32_t>(player._pSFNum);
	file.Skip<uint32_t>(8); // Skip pointers _pHAnim
	file.WriteLE<int32_t>(player._pHFrames);
	file.Skip<uint32_t>();  // Skip _pHWidth
	file.Skip<uint32_t>(8); // Skip pointers _pDAnim
	file.WriteLE<int32_t>(player._pDFrames);
	file.Skip<uint32_t>();  // Skip _pDWidth
	file.Skip<uint32_t>(8); // Skip pointers _pBAnim
	file.WriteLE<int32_t>(player._pBFrames);
	file.Skip<uint32_t>(); // Skip _pBWidth

	for (const Item &item : player.InvBody)
		SaveItem(file, item);

	for (const Item &item : player.InvList)
		SaveItem(file, item);

	file.WriteLE<int32_t>(player._pNumInv);

	for (int8_t cell : player.InvGrid)
		file.WriteLE<int8_t>(cell);

	for (const Item &item : player.SpdList)
		SaveItem(file, item);

	SaveItem(file, player.HoldItem);

	file.WriteLE<int32_t>(player._pIMinDam);
	file.WriteLE<int32_t>(player._pIMaxDam);
	file.WriteLE<int32_t>(player._pIAC);
	file.WriteLE<int32_t>(player._pIBonusDam);
	file.WriteLE<int32_t>(player._pIBonusToHit);
	file.WriteLE<int32_t>(player._pIBonusAC);
	file.WriteLE<int32_t>(player._pIBonusDamMod);
	file.Skip(4); // Alignment

	file.WriteLE<uint64_t>(player._pISpells);
	file.WriteLE<int32_t>(static_cast<int32_t>(player._pIFlags));
	file.WriteLE<int32_t>(player._pIGetHit);

	file.WriteLE<int8_t>(player._pISplLvlAdd);
	file.Skip<uint8_t>(); // Skip _pISplCost
	file.Skip(2);         // Alignment
	file.Skip<int32_t>(); // _pISplDur
	file.WriteLE<int32_t>(player._pIEnAc);
	file.WriteLE<int32_t>(player._pIFMinDam);
	file.WriteLE<int32_t>(player._pIFMaxDam);
	file.WriteLE<int32_t>(player._pILMinDam);
	file.WriteLE<int32_t>(player._pILMaxDam);
	file.WriteLE<int32_t>(player._pOilType);
	file.WriteLE<uint8_t>(player.pTownWarps);
	file.WriteLE<uint8_t>(player.pDungMsgs);
	file.WriteLE<uint8_t>(player.pLvlLoad);
	if (gbIsHellfire)
		file.WriteLE<uint8_t>(player.pDungMsgs2);
	else
		file.WriteLE<uint8_t>(0);
	file.WriteLE<uint8_t>(player.pManaShield ? 1 : 0);
	file.WriteLE<uint8_t>(player.pOriginalCathedral ? 1 : 0);
	file.Skip(2); // Available bytes
	file.WriteLE<uint16_t>(player.wReflections);
	file.Skip(14); // Available bytes

	file.WriteLE<uint32_t>(player.pDiabloKillLevel);
	file.WriteLE<uint32_t>(sgGameInitInfo.nDifficulty);
	file.WriteLE<uint32_t>(static_cast<uint32_t>(player.pDamAcFlags));
	file.Skip(20); // Available bytes

	// Omit pointer _pNData
	// Omit pointer _pWData
	// Omit pointer _pAData
	// Omit pointer _pLData
	// Omit pointer _pFData
	// Omit pointer  _pTData
	// Omit pointer _pHData
	// Omit pointer _pDData
	// Omit pointer _pBData
	// Omit pointer pReserved
}

void SaveMonster(SaveHelper *file, Monster &monster, MonsterConversionData *monsterConversionData = nullptr)
{
	file->WriteLE<int32_t>(monster.levelType);
	file->WriteLE<int32_t>(static_cast<int>(monster.mode));
	file->WriteLE<uint8_t>(static_cast<uint8_t>(monster.goal));
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(monster.goalVar1);
	file->WriteLE<int32_t>(monster.goalVar2);
	file->WriteLE<int32_t>(monster.goalVar3);
	file->Skip(4); // Unused
	file->WriteLE<uint8_t>(monster.pathCount);
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(monster.position.tile.x);
	file->WriteLE<int32_t>(monster.position.tile.y);
	file->WriteLE<int32_t>(monster.position.future.x);
	file->WriteLE<int32_t>(monster.position.future.y);
	file->WriteLE<int32_t>(monster.position.old.x);
	file->WriteLE<int32_t>(monster.position.old.y);
	DisplacementOf<int16_t> offset = {};
	DisplacementOf<int16_t> offset2 = {};
	DisplacementOf<int16_t> velocity = {};
	if (monster.isWalking()) {
		offset = monster.position.CalculateWalkingOffset(monster.direction, monster.animInfo);
		offset2 = monster.position.CalculateWalkingOffsetShifted4(monster.direction, monster.animInfo);
		velocity = monster.position.GetWalkingVelocityShifted4(monster.direction, monster.animInfo);
	}
	file->WriteLE<int32_t>(offset.deltaX);
	file->WriteLE<int32_t>(offset.deltaY);
	file->WriteLE<int32_t>(velocity.deltaX);
	file->WriteLE<int32_t>(velocity.deltaY);
	file->WriteLE<int32_t>(static_cast<int32_t>(monster.direction));
	file->WriteLE<int32_t>(monster.enemy);
	file->WriteLE<uint8_t>(monster.enemyPosition.x);
	file->WriteLE<uint8_t>(monster.enemyPosition.y);
	file->Skip(2); // Unused

	file->Skip(4); // Skip pointer _mAnimData
	file->WriteLE<int32_t>(monster.animInfo.ticksPerFrame);
	file->WriteLE<int32_t>(monster.animInfo.tickCounterOfCurrentFrame);
	file->WriteLE<int32_t>(monster.animInfo.numberOfFrames);
	file->WriteLE<int32_t>(monster.animInfo.currentFrame + 1);
	file->Skip<uint32_t>(); // Skip _meflag
	file->WriteLE<uint32_t>(monster.isInvalid ? 1 : 0);
	file->WriteLE<int32_t>(monster.var1);
	file->WriteLE<int32_t>(monster.var2);
	file->WriteLE<int32_t>(monster.var3);
	file->WriteLE<int32_t>(monster.position.temp.x);
	file->WriteLE<int32_t>(monster.position.temp.y);
	file->WriteLE<int32_t>(offset2.deltaX);
	file->WriteLE<int32_t>(offset2.deltaY);
	file->Skip<int32_t>(); // Skip _mVar8
	file->WriteLE<int32_t>(monster.maxHitPoints);
	file->WriteLE<int32_t>(monster.hitPoints);

	file->WriteLE<uint8_t>(static_cast<int8_t>(monster.ai));
	file->WriteLE<uint8_t>(monster.intelligence);
	file->Skip(2); // Alignment
	file->WriteLE<uint32_t>(monster.flags);
	file->WriteLE<uint8_t>(monster.activeForTicks);
	file->Skip(3); // Alignment
	file->Skip(4); // Unused
	file->WriteLE<int32_t>(monster.position.last.x);
	file->WriteLE<int32_t>(monster.position.last.y);
	file->WriteLE<uint32_t>(monster.rndItemSeed);
	file->WriteLE<uint32_t>(monster.aiSeed);
	file->Skip(4); // Unused

	file->WriteLE<uint8_t>(static_cast<uint8_t>(monster.uniqueType) + 1);
	file->WriteLE<uint8_t>(monster.uniqTrans);
	file->WriteLE<int8_t>(monster.corpseId);

	file->WriteLE<int8_t>(monster.whoHit);
	if (monsterConversionData != nullptr)
		file->WriteLE<int8_t>(monsterConversionData->monsterLevel);
	else
		file->WriteLE<int8_t>(static_cast<int8_t>(monster.level(sgGameInitInfo.nDifficulty)));
	file->Skip(1); // Alignment
	if (monsterConversionData != nullptr)
		file->WriteLE<uint16_t>(monsterConversionData->experience);
	else
		file->WriteLE<uint16_t>(static_cast<uint16_t>(std::min<unsigned>(std::numeric_limits<uint16_t>::max(), monster.exp(sgGameInitInfo.nDifficulty))));

	if (monsterConversionData != nullptr)
		file->WriteLE<uint8_t>(monsterConversionData->toHit);
	else
		file->WriteLE<uint8_t>(static_cast<uint8_t>(std::min<uint16_t>(monster.toHit(sgGameInitInfo.nDifficulty), std::numeric_limits<uint8_t>::max()))); // For backwards compatibility
	file->WriteLE<uint8_t>(monster.minDamage);
	file->WriteLE<uint8_t>(monster.maxDamage);
	if (monsterConversionData != nullptr)
		file->WriteLE<uint8_t>(monsterConversionData->toHitSpecial);
	else
		file->WriteLE<uint8_t>(static_cast<uint8_t>(std::min<uint16_t>(monster.toHitSpecial(sgGameInitInfo.nDifficulty), std::numeric_limits<uint8_t>::max()))); // For backwards compatibility
	file->WriteLE<uint8_t>(monster.minDamageSpecial);
	file->WriteLE<uint8_t>(monster.maxDamageSpecial);
	file->WriteLE<uint8_t>(monster.armorClass);
	file->Skip(1); // Alignment
	file->WriteLE<uint16_t>(monster.resistance);
	file->Skip(2); // Alignment

	file->WriteLE<int32_t>(monster.talkMsg == TEXT_NONE ? 0 : monster.talkMsg);       // Replicate original bad mapping of none for monsters
	file->WriteLE<uint8_t>(monster.leader == Monster::NoLeader ? 0 : monster.leader); // Vanilla uses 0 as the default leader which corresponds to player 0s golem
	file->WriteLE<uint8_t>(static_cast<std::uint8_t>(monster.leaderRelation));
	file->WriteLE<uint8_t>(monster.packSize);
	// vanilla compatibility
	if (monster.lightId == NO_LIGHT)
		file->WriteLE<int8_t>(0);
	else
		file->WriteLE<int8_t>(monster.lightId);

	// Omit pointer name;
}

void SaveMissile(SaveHelper *file, const Missile &missile)
{
	file->WriteLE<int32_t>(static_cast<int8_t>(missile._mitype));
	file->WriteLE<int32_t>(missile.position.tile.x);
	file->WriteLE<int32_t>(missile.position.tile.y);
	file->WriteLE<int32_t>(missile.position.offset.deltaX);
	file->WriteLE<int32_t>(missile.position.offset.deltaY);
	file->WriteLE<int32_t>(missile.position.velocity.deltaX);
	file->WriteLE<int32_t>(missile.position.velocity.deltaY);
	file->WriteLE<int32_t>(missile.position.start.x);
	file->WriteLE<int32_t>(missile.position.start.y);
	file->WriteLE<int32_t>(missile.position.traveled.deltaX);
	file->WriteLE<int32_t>(missile.position.traveled.deltaY);
	file->WriteLE<int32_t>(missile._mimfnum);
	file->WriteLE<int32_t>(missile._mispllvl);
	file->WriteLE<uint32_t>(missile._miDelFlag ? 1 : 0);
	file->WriteLE<uint8_t>(static_cast<uint8_t>(missile._miAnimType));
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(static_cast<int32_t>(missile._miAnimFlags));
	file->Skip(4); // Skip pointer _miAnimData
	file->WriteLE<int32_t>(missile._miAnimDelay);
	file->WriteLE<int32_t>(missile._miAnimLen);
	file->WriteLE<int32_t>(missile._miAnimWidth);
	file->WriteLE<int32_t>(missile._miAnimWidth2);
	file->WriteLE<int32_t>(missile._miAnimCnt);
	file->WriteLE<int32_t>(missile._miAnimAdd);
	file->WriteLE<int32_t>(missile._miAnimFrame);
	file->WriteLE<uint32_t>(missile._miDrawFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miLightFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miPreFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miUniqTrans);
	file->WriteLE<int32_t>(missile.duration);
	file->WriteLE<int32_t>(missile._misource);
	file->WriteLE<int32_t>(missile._micaster);
	file->WriteLE<int32_t>(missile._midam);
	file->WriteLE<uint32_t>(missile._miHitFlag ? 1 : 0);
	file->WriteLE<int32_t>(missile._midist);
	file->WriteLE<int32_t>(missile._mlid);
	file->WriteLE<int32_t>(missile._mirnd);
	file->WriteLE<int32_t>(missile.var1);
	file->WriteLE<int32_t>(missile.var2);
	file->WriteLE<int32_t>(missile.var3);
	file->WriteLE<int32_t>(missile.var4);
	file->WriteLE<int32_t>(missile.var5);
	file->WriteLE<int32_t>(missile.var6);
	file->WriteLE<int32_t>(missile.var7);
	file->WriteLE<uint32_t>(missile.limitReached ? 1 : 0);
}

_object_id ConvertToHellfireObject(_object_id type)
{
	if (leveltype == DTYPE_NEST) {
		switch (type) {
		case OBJ_POD:
			return OBJ_BARREL;
		case OBJ_PODEX:
			return OBJ_BARRELEX;
		default:
			break;
		}
	}

	if (leveltype == DTYPE_CRYPT) {
		switch (type) {
		case OBJ_URN:
			return OBJ_BARREL;
		case OBJ_URNEX:
			return OBJ_BARRELEX;
		case OBJ_L5BOOKS:
			return OBJ_STORYBOOK;
		case OBJ_L5CANDLE:
			return OBJ_STORYCANDLE;
		case OBJ_L5LDOOR:
			return OBJ_L1LDOOR;
		case OBJ_L5RDOOR:
			return OBJ_L1RDOOR;
		case OBJ_L5LEVER:
			return OBJ_LEVER;
		case OBJ_L5SARC:
			return OBJ_SARC;
		default:
			break;
		}
	}

	return type;
}

void SaveObject(SaveHelper &file, const Object &object)
{
	file.WriteLE<int32_t>(ConvertToHellfireObject(object._otype));
	file.WriteLE<int32_t>(object.position.x);
	file.WriteLE<int32_t>(object.position.y);
	file.WriteLE<uint32_t>(object.applyLighting ? 1 : 0);
	file.WriteLE<uint32_t>(object._oAnimFlag ? 1 : 0);
	file.Skip(4); // Skip pointer _oAnimData
	file.WriteLE<int32_t>(object._oAnimDelay);
	file.WriteLE<int32_t>(object._oAnimCnt);
	file.WriteLE<uint32_t>(object._oAnimLen);
	file.WriteLE<uint32_t>(object._oAnimFrame);
	file.WriteLE<int32_t>(object._oAnimWidth);
	file.WriteLE<int32_t>(CalculateSpriteTileCenterX(static_cast<int>(object._oAnimWidth))); // Write _oAnimWidth2 for vanilla compatibility
	file.WriteLE<uint32_t>(object._oDelFlag ? 1 : 0);
	file.WriteLE<int8_t>(object._oBreak);
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(object._oSolidFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oMissFlag ? 1 : 0);

	file.WriteLE<int8_t>(static_cast<uint8_t>(object.selectionRegion));
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(object._oPreFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oTrapFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oDoorFlag ? 1 : 0);
	file.WriteLE<int32_t>(object._olid);
	file.WriteLE<uint32_t>(object._oRndSeed);

	/* Make dynamic light sources unseen when saving level data for level change */
	int32_t var1 = object._oVar1;
	switch (object._otype) {
	case OBJ_L1LIGHT:
	case OBJ_SKFIRE:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_BOOKCANDLE:
	case OBJ_STORYCANDLE:
	case OBJ_L5CANDLE:
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		if (var1 != -1)
			var1 = 0;
		break;
	default:
		break;
	}
	file.WriteLE<int32_t>(var1);
	file.WriteLE<int32_t>(object._oVar2);
	file.WriteLE<int32_t>(object._oVar3);
	file.WriteLE<int32_t>(object._oVar4);
	file.WriteLE<int32_t>(object._oVar5);
	file.WriteLE<uint32_t>(object._oVar6);
	file.WriteLE<int32_t>(object.bookMessage);
	file.WriteLE<int32_t>(object._oVar8);
}

void SaveQuest(SaveHelper *file, int i)
{
	auto &quest = Quests[i];

	file->WriteLE<uint8_t>(quest._qlevel);
	file->WriteLE<uint8_t>(quest._qidx); // _qtype for compatibility, used in DRLG_CheckQuests
	file->WriteLE<uint8_t>(quest._qactive);
	file->WriteLE<uint8_t>(quest._qlvltype);
	file->WriteLE<int32_t>(quest.position.x);
	file->WriteLE<int32_t>(quest.position.y);
	file->WriteLE<uint8_t>(quest._qslvl);
	file->WriteLE<uint8_t>(quest._qidx);
	if (gbIsHellfire) {
		file->Skip(2); // Alignment
		file->WriteLE<int32_t>(quest._qmsg);
	} else {
		file->WriteLE<uint8_t>(quest._qmsg);
	}
	file->WriteLE<uint8_t>(quest._qvar1);
	file->WriteLE<uint8_t>(quest._qvar2);
	file->Skip(2); // Alignment
	if (!gbIsHellfire)
		file->Skip(1); // Alignment
	file->WriteLE<uint32_t>(quest._qlog ? 1 : 0);

	file->WriteBE<int32_t>(ReturnLvlPosition.x);
	file->WriteBE<int32_t>(ReturnLvlPosition.y);
	file->WriteBE<int32_t>(ReturnLevel);
	file->WriteBE<int32_t>(ReturnLevelType);
	file->Skip(sizeof(int32_t)); // Skip DoomQuestState
}

void SaveLighting(SaveHelper *file, Light *pLight, bool vision = false)
{
	file->WriteLE<int32_t>(pLight->position.tile.x);
	file->WriteLE<int32_t>(pLight->position.tile.y);
	file->WriteLE<int32_t>(pLight->radius);
	file->WriteLE<int32_t>(vision ? 1 : 0); // _lid
	file->WriteLE<uint32_t>(pLight->isInvalid ? 1 : 0);
	file->WriteLE<uint32_t>(pLight->hasChanged ? 1 : 0);
	file->Skip(4); // Unused
	file->WriteLE<int32_t>(pLight->position.old.x);
	file->WriteLE<int32_t>(pLight->position.old.y);
	file->WriteLE<int32_t>(pLight->oldRadius);
	file->WriteLE<int32_t>(pLight->position.offset.deltaX);
	file->WriteLE<int32_t>(pLight->position.offset.deltaY);
	file->WriteLE<uint32_t>(vision ? 1 : 0);
}

void SavePortal(SaveHelper *file, int i)
{
	Portal *pPortal = &Portals[i];

	file->WriteLE<uint32_t>(pPortal->open ? 1 : 0);
	file->WriteLE<int32_t>(pPortal->position.x);
	file->WriteLE<int32_t>(pPortal->position.y);
	file->WriteLE<int32_t>(pPortal->level);
	file->WriteLE<int32_t>(pPortal->setlvl ? pPortal->ltype : getHellfireLevelType(pPortal->ltype));
	file->WriteLE<uint32_t>(pPortal->setlvl ? 1 : 0);
}

/**
 * @brief Saves items on the current dungeon floor
 * @param file interface to the save file
 * @return a map converting from runtime item indexes to the relative position in the save file, used by SaveDroppedItemLocations
 * @see SaveDroppedItemLocations
 */
ankerl::unordered_dense::map<uint8_t, uint8_t> SaveDroppedItems(SaveHelper &file)
{
	// Vanilla Diablo/Hellfire initialise the ActiveItems and AvailableItems arrays based on saved data, so write valid values for compatibility
	for (uint8_t i = 0; i < MAXITEMS; i++)
		file.WriteLE<uint8_t>(i); // Strictly speaking everything from ActiveItemCount onwards is unused but no harm writing non-zero values here.
	for (uint8_t i = 0; i < MAXITEMS; i++)
		file.WriteLE<uint8_t>((i + ActiveItemCount) % MAXITEMS);

	ankerl::unordered_dense::map<uint8_t, uint8_t> itemIndexes;
	itemIndexes.reserve(ActiveItemCount + 1);
	itemIndexes.emplace(0, 0);
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		itemIndexes[ActiveItems[i] + 1] = i + 1;
		SaveItem(file, Items[ActiveItems[i]]);
	}
	return itemIndexes;
}

/**
 * @brief Saves the position of dropped items (in dItem)
 * @param file interface to the save file
 * @param itemIndexes a map converting from runtime item indexes to the relative position in the save file
 */
void SaveDroppedItemLocations(SaveHelper &file, const ankerl::unordered_dense::map<uint8_t, uint8_t> &itemIndexes)
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(itemIndexes.at(dItem[i][j]));
	}
}

constexpr uint32_t VersionAdditionalMissiles = 0;

void SaveAdditionalMissiles(SaveWriter &saveWriter)
{
	constexpr size_t BytesWrittenBySaveMissile = 180;
	uint32_t missileCountAdditional = (Missiles.size() > MaxMissilesForSaveGame) ? static_cast<uint32_t>(Missiles.size() - MaxMissilesForSaveGame) : 0;
	SaveHelper file(saveWriter, "additionalMissiles", sizeof(uint32_t) + sizeof(uint32_t) + (missileCountAdditional * BytesWrittenBySaveMissile));

	file.WriteLE<uint32_t>(VersionAdditionalMissiles);
	file.WriteLE<uint32_t>(missileCountAdditional);

	if (missileCountAdditional > 0) {
		auto it = Missiles.cbegin();
		// std::list::const_iterator doesn't provide operator+() :/ using std::advance to get past the missiles we've already saved
		std::advance(it, MaxMissilesForSaveGame);
		for (; it != Missiles.cend(); it++) {
			SaveMissile(&file, *it);
		}
	}
}

void LoadAdditionalMissiles()
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "additionalMissiles");

	if (!file.IsValid()) {
		// no additional Missiles saved
		return;
	}

	auto loadedVersion = file.NextLE<uint32_t>();
	if (loadedVersion > VersionAdditionalMissiles) {
		// unknown version
		return;
	}
	auto missileCountAdditional = file.NextLE<uint32_t>();
	for (uint32_t i = 0U; i < missileCountAdditional; i++) {
		LoadMissile(&file);
	}
}

void SaveLevelSeeds(SaveWriter &saveWriter)
{
	SaveHelper file(saveWriter, "levelseeds", giNumberOfLevels * (sizeof(uint8_t) + sizeof(uint32_t)));

	for (int i = 0; i < giNumberOfLevels; i++) {
		file.WriteLE<uint8_t>(LevelSeeds[i] ? 1 : 0);
		if (LevelSeeds[i]) {
			file.WriteLE<uint32_t>(*LevelSeeds[i]);
		}
	}
}

void LoadLevelSeeds()
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "levelseeds");
	if (!file.IsValid())
		return;

	for (int i = 0; i < giNumberOfLevels; i++) {
		if (file.NextLE<uint8_t>() != 0) {
			LevelSeeds[i] = file.NextLE<uint32_t>();
		} else {
			LevelSeeds[i] = std::nullopt;
		}
	}
}

void SaveLevel(SaveWriter &saveWriter, LevelConversionData *levelConversionData)
{
	Player &myPlayer = *MyPlayer;

	DoUnVision(myPlayer.position.tile, myPlayer._pLightRad); // fix for vision staying on the level

	if (leveltype == DTYPE_TOWN)
		DungeonSeeds[0] = GenerateSeed();

	char szName[MaxMpqPathSize];
	GetTempLevelNames(szName);
	SaveHelper file(saveWriter, szName, 256 * 1024);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dCorpse[i][j]);
		}
	}

	file.WriteBE(static_cast<int32_t>(ActiveMonsterCount));
	file.WriteBE<int32_t>(ActiveItemCount);
	file.WriteBE<int32_t>(ActiveObjectCount);

	if (leveltype != DTYPE_TOWN) {
		for (unsigned monsterId : ActiveMonsters)
			file.WriteBE<uint32_t>(monsterId);
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			MonsterConversionData *monsterConversionData = nullptr;
			if (levelConversionData != nullptr)
				monsterConversionData = &levelConversionData->monsterConversionData[ActiveMonsters[i]];
			SaveMonster(&file, Monsters[ActiveMonsters[i]], monsterConversionData);
		}
		for (int objectId : ActiveObjects)
			file.WriteLE<int8_t>(objectId);
		for (int objectId : AvailableObjects)
			file.WriteLE<int8_t>(objectId);
		for (int i = 0; i < ActiveObjectCount; i++) {
			SaveObject(file, Objects[ActiveObjects[i]]);
		}
	}

	auto itemIndexes = SaveDroppedItems(file);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(static_cast<uint8_t>(dFlags[i][j] & DungeonFlag::SavedFlags));
	}
	SaveDroppedItemLocations(file, itemIndexes);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteBE<int32_t>(dMonster[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dObject[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dLight[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dPreLight[i][j]);
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(AutomapView[i][j]);
		}
	}

	if (!setlevel)
		myPlayer._pLvlVisited[currlevel] = true;
	else
		myPlayer._pSLvlVisited[setlvlnum] = true;
}

tl::expected<void, std::string> LoadLevel(LevelConversionData *levelConversionData)
{
	char szName[MaxMpqPathSize];
	std::optional<SaveReader> archive = OpenSaveArchive(gSaveNumber);
	GetTempLevelNames(szName);
	if (!archive || !archive->HasFile(szName))
		GetPermLevelNames(szName);
	LoadHelper file(std::move(archive), szName);
	if (!file.IsValid())
		return tl::make_unexpected(std::string(_("Unable to open save file archive")));

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dCorpse[i][j] = file.NextLE<int8_t>();
		}
		MoveLightsToCorpses();
	}

	ActiveMonsterCount = file.NextBE<int32_t>();
	auto savedItemCount = file.NextBE<uint32_t>();
	ActiveObjectCount = file.NextBE<int32_t>();

	if (leveltype != DTYPE_TOWN) {
		for (unsigned &monsterId : ActiveMonsters)
			monsterId = file.NextBE<uint32_t>();
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &monster = Monsters[ActiveMonsters[i]];
			MonsterConversionData *monsterConversionData = nullptr;
			if (levelConversionData != nullptr)
				monsterConversionData = &levelConversionData->monsterConversionData[ActiveMonsters[i]];
			LoadMonster(&file, monster, monsterConversionData);
			if (monster.isUnique() && monster.lightId != NO_LIGHT)
				Lights[monster.lightId].isInvalid = false;
		}
		if (!gbSkipSync) {
			for (size_t i = 0; i < ActiveMonsterCount; i++)
				RETURN_IF_ERROR(SyncMonsterAnim(Monsters[ActiveMonsters[i]]));
		}
		for (int &objectId : ActiveObjects)
			objectId = file.NextLE<int8_t>();
		for (int &objectId : AvailableObjects)
			objectId = file.NextLE<int8_t>();
		for (int i = 0; i < ActiveObjectCount; i++)
			LoadObject(file, Objects[ActiveObjects[i]]);
		if (!gbSkipSync) {
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
		}
	}

	LoadDroppedItems(file, savedItemCount);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dFlags[i][j] = static_cast<DungeonFlag>(file.NextLE<uint8_t>()) & DungeonFlag::LoadedFlags;
	}

	// skip dItem indexes, this gets populated in LoadDroppedItems
	file.Skip<uint8_t>(MAXDUNX * MAXDUNY);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dMonster[i][j] = file.NextBE<int32_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dObject[i][j] = file.NextLE<int8_t>();
		}
		file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dPreLight[i][j] = file.NextLE<uint8_t>();
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
				const auto automapView = static_cast<MapExplorationType>(file.NextLE<uint8_t>());
				AutomapView[i][j] = automapView == MAP_EXP_OLD ? MAP_EXP_SELF : automapView;
			}
		}

		// No need to load dLight, we can recreate it accurately from LightList
		memcpy(dLight, dPreLight, sizeof(dLight));                                     // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(Players[MyPlayerId].lightId, Players[MyPlayerId].position.tile); // forces player light refresh
	} else {
		memset(dLight, 0, sizeof(dLight));
	}

	if (!gbSkipSync) {
		AutomapZoomReset();
		ResyncQuests();
		RedoMissileFlags();
		UpdateLighting = true;
	}

	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel())
			Lights[player.lightId].hasChanged = true;
	}
	return {};
}

const int DiabloItemSaveSize = 368;
const int HellfireItemSaveSize = 372;

} // namespace

tl::expected<void, std::string> ConvertLevels(SaveWriter &saveWriter)
{
	// Backup current level state
	bool tmpSetlevel = setlevel;
	_setlevels tmpSetlvlnum = setlvlnum;
	int tmpCurrlevel = currlevel;
	dungeon_type tmpLeveltype = leveltype;

	gbSkipSync = true;

	setlevel = false; // Convert regular levels
	for (int i = 0; i < giNumberOfLevels; i++) {
		currlevel = i;
		if (!LevelFileExists(saveWriter))
			continue;

		leveltype = GetLevelType(currlevel);

		LevelConversionData levelConversionData;
		RETURN_IF_ERROR(LoadLevel(&levelConversionData));
		SaveLevel(saveWriter, &levelConversionData);
	}

	setlevel = true; // Convert quest levels
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_NOTAVAIL) {
			continue;
		}

		leveltype = quest._qlvltype;
		if (leveltype == DTYPE_NONE) {
			continue;
		}

		setlvlnum = quest._qslvl;
		if (!LevelFileExists(saveWriter))
			continue;

		LevelConversionData levelConversionData;
		RETURN_IF_ERROR(LoadLevel(&levelConversionData));
		SaveLevel(saveWriter, &levelConversionData);
	}

	gbSkipSync = false;

	// Restore current level state
	setlevel = tmpSetlevel;
	setlvlnum = tmpSetlvlnum;
	currlevel = tmpCurrlevel;
	leveltype = tmpLeveltype;
	return {};
}

void RemoveInvalidItem(Item &item)
{
	bool isInvalid = !IsItemAvailable(item.IDidx) || !IsUniqueAvailable(item._iUid);

	if (!gbIsHellfire) {
		isInvalid = isInvalid || (item._itype == ItemType::Staff && GetSpellStaffLevel(item._iSpell) == -1);
		isInvalid = isInvalid || (item._iMiscId == IMISC_BOOK && GetSpellBookLevel(item._iSpell) == -1);
		isInvalid = isInvalid || item._iDamAcFlags != ItemSpecialEffectHf::None;
		isInvalid = isInvalid || item._iPrePower > IPL_LASTDIABLO;
		isInvalid = isInvalid || item._iSufPower > IPL_LASTDIABLO;
	}

	if (isInvalid) {
		item.clear();
	}
}

_item_indexes RemapItemIdxFromDiablo(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) -> int {
		if (i == IDI_SORCERER) {
			return IDI_SORCERER_DIABLO;
		}
		if (i >= 156) {
			i += 5; // Hellfire exclusive items
		}
		if (i >= 88) {
			i += 1; // Scroll of Search
		}
		if (i >= 83) {
			i += 4; // Oils
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxToDiablo(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) -> int {
		if (i == IDI_SORCERER_DIABLO) {
			return IDI_SORCERER;
		}
		if ((i >= 83 && i <= 86) || i == 92 || i >= 161) {
			return -1; // Hellfire exclusive items
		}
		if (i >= 93) {
			i -= 1; // Scroll of Search
		}
		if (i >= 87) {
			i -= 4; // Oils
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxFromSpawn(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) {
		if (i >= 62) {
			i += 9; // Medium and heavy armors
		}
		if (i >= 96) {
			i += 1; // Scroll of Stone Curse
		}
		if (i >= 98) {
			i += 1; // Scroll of Guardian
		}
		if (i >= 99) {
			i += 1; // Scroll of ...
		}
		if (i >= 101) {
			i += 1; // Scroll of Golem
		}
		if (i >= 102) {
			i += 1; // Scroll of None
		}
		if (i >= 104) {
			i += 1; // Scroll of Apocalypse
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxToSpawn(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) {
		if (i >= 104) {
			i -= 1; // Scroll of Apocalypse
		}
		if (i >= 102) {
			i -= 1; // Scroll of None
		}
		if (i >= 101) {
			i -= 1; // Scroll of Golem
		}
		if (i >= 99) {
			i -= 1; // Scroll of ...
		}
		if (i >= 98) {
			i -= 1; // Scroll of Guardian
		}
		if (i >= 96) {
			i -= 1; // Scroll of Stone Curse
		}
		if (i >= 71) {
			i -= 9; // Medium and heavy armors
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

bool IsHeaderValid(uint32_t magicNumber)
{
	gbIsHellfireSaveGame = false;
	if (magicNumber == LoadLE32("SHAR")) {
		return true;
	}
	if (magicNumber == LoadLE32("SHLF")) {
		gbIsHellfireSaveGame = true;
		return true;
	}
	if (!gbIsSpawn && magicNumber == LoadLE32("RETL")) {
		return true;
	}
	if (!gbIsSpawn && magicNumber == LoadLE32("HELF")) {
		gbIsHellfireSaveGame = true;
		return true;
	}

	return false;
}

// Returns the size of the hotkeys file with the number of hotkeys passed and if a header with the number of hotkeys is present in the file
size_t HotkeysSize(size_t nHotkeys = NumHotkeys)
{
	//     header            spells                         spell types                    active spell      active spell type
	return sizeof(uint8_t) + (nHotkeys * sizeof(int32_t)) + (nHotkeys * sizeof(uint8_t)) + sizeof(int32_t) + sizeof(uint8_t);
}

void LoadHotkeys()
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "hotkeys");
	if (!file.IsValid())
		return;

	Player &myPlayer = *MyPlayer;
	size_t nHotkeys = 4; // Defaults to old save format number

	// Refill the spell arrays with no selection
	std::fill(myPlayer._pSplHotKey, myPlayer._pSplHotKey + NumHotkeys, SpellID::Invalid);
	std::fill(myPlayer._pSplTHotKey, myPlayer._pSplTHotKey + NumHotkeys, SpellType::Invalid);

	// Checking if the save file has the old format with only 4 hotkeys and no header
	if (file.IsValid(HotkeysSize(nHotkeys))) {
		// The file contains a header byte and at least 4 entries, so we can assume it's a new format save
		nHotkeys = file.NextLE<uint8_t>();
	}

	// Read all hotkeys in the file
	for (size_t i = 0; i < nHotkeys; i++) {
		// Do not load hotkeys past the size of the spell types array, discard the rest
		if (i < NumHotkeys) {
			myPlayer._pSplHotKey[i] = static_cast<SpellID>(file.NextLE<int32_t>());
		} else {
			file.Skip<int32_t>();
		}
	}
	for (size_t i = 0; i < nHotkeys; i++) {
		// Do not load hotkeys past the size of the spells array, discard the rest
		if (i < NumHotkeys) {
			myPlayer._pSplTHotKey[i] = static_cast<SpellType>(file.NextLE<uint8_t>());
		} else {
			file.Skip<uint8_t>();
		}
	}

	// Load the selected spell last
	myPlayer._pRSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	myPlayer._pRSplType = static_cast<SpellType>(file.NextLE<uint8_t>());
}

void SaveHotkeys(SaveWriter &saveWriter, const Player &player)
{
	SaveHelper file(saveWriter, "hotkeys", HotkeysSize());

	// Write the number of spell hotkeys
	file.WriteLE<uint8_t>(static_cast<uint8_t>(NumHotkeys));

	// Write the spell hotkeys
	for (auto &spellId : player._pSplHotKey) {
		file.WriteLE<int32_t>(static_cast<int8_t>(spellId));
	}
	for (auto &spellType : player._pSplTHotKey) {
		file.WriteLE<uint8_t>(static_cast<uint8_t>(spellType));
	}

	// Write the selected spell last
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pRSpell));
	file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pRSplType));
}

void LoadHeroItems(Player &player)
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "heroitems");
	if (!file.IsValid())
		return;

	gbIsHellfireSaveGame = file.NextBool8();

	LoadMatchingItems(file, player, NUM_INVLOC, player.InvBody);
	LoadMatchingItems(file, player, InventoryGridCells, player.InvList);
	LoadMatchingItems(file, player, MaxBeltItems, player.SpdList);

	gbIsHellfireSaveGame = gbIsHellfire;
}

constexpr uint8_t StashVersion = 0;

void LoadStash()
{
	const char *filename;
	if (!gbIsMultiplayer)
		filename = "spstashitems";
	else
		filename = "mpstashitems";

	Stash = {};

	LoadHelper file(OpenStashArchive(), filename);
	if (!file.IsValid())
		return;

	auto version = file.NextLE<uint8_t>();
	if (version > StashVersion)
		return;

	Stash.gold = file.NextLE<uint32_t>();

	auto pages = file.NextLE<uint32_t>();
	for (unsigned i = 0; i < pages; i++) {
		auto page = file.NextLE<uint32_t>();
		for (auto &row : Stash.stashGrids[page]) {
			for (uint16_t &cell : row) {
				cell = file.NextLE<uint16_t>();
			}
		}
	}

	auto itemCount = file.NextLE<uint32_t>();
	Stash.stashList.resize(itemCount);
	for (unsigned i = 0; i < itemCount; i++) {
		LoadAndValidateItemData(file, Stash.stashList[i]);
	}

	Stash.SetPage(file.NextLE<uint32_t>());
}

void RemoveEmptyInventory(Player &player)
{
	for (int i = InventoryGridCells; i > 0; i--) {
		int8_t idx = player.InvGrid[i - 1];
		if (idx > 0 && player.InvList[idx - 1].isEmpty()) {
			player.RemoveInvItem(idx - 1);
		}
	}
}

tl::expected<void, std::string> LoadGame(bool firstflag)
{
	FreeGameMem();

	LoadHelper file(OpenSaveArchive(gSaveNumber), "game");
	if (!file.IsValid()) {
		return tl::make_unexpected(std::string(_("Unable to open save file archive")));
	}

	if (!IsHeaderValid(file.NextLE<uint32_t>())) {
		return tl::make_unexpected(std::string(_("Invalid save file")));
	}

	if (gbIsHellfireSaveGame) {
		giNumberOfLevels = 25;
		giNumberQuests = 24;
		giNumberOfSmithPremiumItems = 15;
	} else {
		// Todo initialize additional levels and quests if we are running Hellfire
		giNumberOfLevels = 17;
		giNumberQuests = 16;
		giNumberOfSmithPremiumItems = 6;
	}

	pfile_remove_temp_files();

	setlevel = file.NextBool8();
	setlvlnum = static_cast<_setlevels>(file.NextBE<uint32_t>());
	currlevel = file.NextBE<uint32_t>();
	leveltype = static_cast<dungeon_type>(file.NextBE<uint32_t>());
	if (!setlevel)
		leveltype = GetLevelType(currlevel);
	int viewX = file.NextBE<int32_t>();
	int viewY = file.NextBE<int32_t>();
	invflag = file.NextBool8();
	CharFlag = file.NextBool8();
	int tmpNummonsters = file.NextBE<int32_t>();
	auto savedItemCount = file.NextBE<uint32_t>();
	int tmpNummissiles = file.NextBE<int32_t>();
	int tmpNobjects = file.NextBE<int32_t>();

	if (!gbIsHellfire && IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		return tl::make_unexpected(std::string(_("Player is on a Hellfire only level")));
	}

	for (uint8_t i = 0; i < giNumberOfLevels; i++) {
		DungeonSeeds[i] = file.NextBE<uint32_t>();
		LevelSeeds[i] = std::nullopt;
		file.Skip(4); // Skip loading gnLevelTypeTbl
	}
	LoadLevelSeeds();

	Player &myPlayer = *MyPlayer;

	LoadPlayer(file, myPlayer);

	if (sgGameInitInfo.nDifficulty < DIFF_NORMAL || sgGameInitInfo.nDifficulty > DIFF_HELL)
		sgGameInitInfo.nDifficulty = DIFF_NORMAL;

	for (int i = 0; i < giNumberQuests; i++)
		LoadQuest(&file, i);
	for (int i = 0; i < MAXPORTAL; i++)
		LoadPortal(&file, i);

	if (gbIsHellfireSaveGame != gbIsHellfire) {
		RETURN_IF_ERROR(pfile_convert_levels());
		RemoveEmptyInventory(myPlayer);
	}

	RETURN_IF_ERROR(LoadGameLevel(firstflag, ENTRY_LOAD));
	SetPlrAnims(myPlayer);
	SyncPlrAnim(myPlayer);

	ViewPosition = { viewX, viewY };
	ActiveMonsterCount = tmpNummonsters;
	ActiveObjectCount = tmpNobjects;

	for (int &monstkill : MonsterKillCounts)
		monstkill = file.NextBE<int32_t>();

	// skip ahead for vanilla save compatibility (Related to bugfix where MonsterKillCounts[MaxMonsters] was changed to MonsterKillCounts[NUM_MTYPES]
	file.Skip(4 * (MaxMonsters - NUM_MTYPES));
	if (leveltype != DTYPE_TOWN) {
		for (unsigned &monsterId : ActiveMonsters)
			monsterId = file.NextBE<uint32_t>();
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			LoadMonster(&file, Monsters[ActiveMonsters[i]]);
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			SyncPackSize(Monsters[ActiveMonsters[i]]);
		// Skip ActiveMissiles
		file.Skip<int8_t>(MaxMissilesForSaveGame);
		// Skip AvailableMissiles
		file.Skip<int8_t>(MaxMissilesForSaveGame);
		for (int i = 0; i < tmpNummissiles; i++)
			LoadMissile(&file);
		// For petrified monsters, the data in missile.var1 must be used to
		// load the appropriate animation data for the monster in missile.var2
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			RETURN_IF_ERROR(SyncMonsterAnim(Monsters[ActiveMonsters[i]]));
		for (int &objectId : ActiveObjects)
			objectId = file.NextLE<int8_t>();
		for (int &objectId : AvailableObjects)
			objectId = file.NextLE<int8_t>();
		for (int i = 0; i < ActiveObjectCount; i++)
			LoadObject(file, Objects[ActiveObjects[i]]);
		for (int i = 0; i < ActiveObjectCount; i++)
			SyncObjectAnim(Objects[ActiveObjects[i]]);

		ActiveLightCount = file.NextBE<int32_t>();

		for (uint8_t &lightId : ActiveLights)
			lightId = file.NextLE<uint8_t>();
		for (int i = 0; i < ActiveLightCount; i++)
			LoadLighting(&file, &Lights[ActiveLights[i]]);

		file.Skip<int32_t>(); // VisionId
		int visionCount = file.NextBE<int32_t>();

		for (int i = 0; i < visionCount; i++) {
			LoadLighting(&file, &VisionList[i]);
			VisionActive[i] = true;
		}
	}

	LoadDroppedItems(file, savedItemCount);

	LoadAdditionalMissiles();

	for (bool &uniqueItemFlag : UniqueItemFlags)
		uniqueItemFlag = file.NextBool8();

	file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dFlags[i][j] = static_cast<DungeonFlag>(file.NextLE<uint8_t>()) & DungeonFlag::LoadedFlags;
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dPlayer[i][j] = file.NextLE<int8_t>();
	}

	// skip dItem indexes, this gets populated in LoadDroppedItems
	file.Skip<uint8_t>(MAXDUNX * MAXDUNY);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dMonster[i][j] = file.NextBE<int32_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dCorpse[i][j] = file.NextLE<int8_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dObject[i][j] = file.NextLE<int8_t>();
		}
		file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dPreLight[i][j] = file.NextLE<uint8_t>();
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
				const auto automapView = static_cast<MapExplorationType>(file.NextLE<uint8_t>());
				AutomapView[i][j] = automapView == MAP_EXP_OLD ? MAP_EXP_SELF : automapView;
			}
		}
		file.Skip(MAXDUNX * MAXDUNY); // dMissile

		// No need to load dLight, we can recreate it accurately from LightList
		memcpy(dLight, dPreLight, sizeof(dLight));               // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(myPlayer.lightId, myPlayer.position.tile); // forces player light refresh
	} else {
		memset(dLight, 0, sizeof(dLight));
	}

	PremiumItemCount = file.NextBE<int32_t>();
	PremiumItemLevel = file.NextBE<int32_t>();

	for (int i = 0; i < giNumberOfSmithPremiumItems; i++)
		LoadPremium(file, i);
	if (gbIsHellfire && !gbIsHellfireSaveGame)
		SpawnPremium(myPlayer);

	AutomapActive = file.NextBool8();
	AutoMapScale = file.NextBE<int32_t>();
	AutomapZoomReset();
	ResyncQuests();

	if (leveltype != DTYPE_TOWN) {
		RedoPlayerVision();
		ProcessVisionList();
		ProcessLightList();
	}

	// convert stray manashield missiles into pManaShield flag
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::ManaShield && !missile._miDelFlag) {
			Players[missile._misource].pManaShield = true;
			missile._miDelFlag = true;
		}
	}

	SetUpMissileAnimationData();
	RedoMissileFlags();
	gbProcessPlayers = IsDiabloAlive(!firstflag);

	if (gbIsHellfireSaveGame != gbIsHellfire) {
		SaveGame();
	}

	gbIsHellfireSaveGame = gbIsHellfire;
	return {};
}

void SaveHeroItems(SaveWriter &saveWriter, Player &player)
{
	size_t itemCount = static_cast<size_t>(NUM_INVLOC) + InventoryGridCells + MaxBeltItems;
	SaveHelper file(saveWriter, "heroitems", itemCount * (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize) + sizeof(uint8_t));

	file.WriteLE<uint8_t>(gbIsHellfire ? 1 : 0);

	for (const Item &item : player.InvBody)
		SaveItem(file, item);
	for (const Item &item : player.InvList)
		SaveItem(file, item);
	for (const Item &item : player.SpdList)
		SaveItem(file, item);
}

void SaveStash(SaveWriter &stashWriter)
{
	const char *filename;
	if (!gbIsMultiplayer)
		filename = "spstashitems";
	else
		filename = "mpstashitems";

	const int itemSize = (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize);

	SaveHelper file(
	    stashWriter,
	    filename,
	    sizeof(uint8_t)
	        + sizeof(uint32_t)
	        + sizeof(uint32_t)
	        + (sizeof(uint32_t) + 10 * 10 * sizeof(uint16_t)) * Stash.stashGrids.size()
	        + sizeof(uint32_t)
	        + itemSize * Stash.stashList.size()
	        + sizeof(uint32_t));

	file.WriteLE<uint8_t>(StashVersion);

	file.WriteLE<uint32_t>(Stash.gold);

	std::vector<unsigned> pagesToSave;
	for (const auto &[page, grid] : Stash.stashGrids) {
		if (c_any_of(grid, [](const auto &row) {
			    return c_any_of(row, [](StashStruct::StashCell cell) {
				    return cell > 0;
			    });
		    })) {
			// found a page that contains at least one item
			pagesToSave.push_back(page);
		}
	};

	// Current stash size is 100 pages. Will definitely fit in a 32 bit value.
	file.WriteLE<uint32_t>(static_cast<uint32_t>(pagesToSave.size()));
	for (const auto &page : pagesToSave) {
		file.WriteLE<uint32_t>(page);
		for (const auto &row : Stash.stashGrids[page]) {
			for (uint16_t cell : row) {
				file.WriteLE<uint16_t>(cell);
			}
		}
	}

	// 100 pages of 100 items is still only 10 000, as with the page count will definitely fit in 32 bits even in the worst case.
	file.WriteLE<uint32_t>(static_cast<uint32_t>(Stash.stashList.size()));
	for (const Item &item : Stash.stashList) {
		SaveItem(file, item);
	}

	file.WriteLE<uint32_t>(static_cast<uint32_t>(Stash.GetPage()));
}

void SaveGameData(SaveWriter &saveWriter)
{
	SaveHelper file(saveWriter, "game", 320 * 1024);

	if (gbIsSpawn && !gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("SHAR"));
	else if (gbIsSpawn && gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("SHLF"));
	else if (!gbIsSpawn && gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("HELF"));
	else if (!gbIsSpawn && !gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("RETL"));
	else
		app_fatal(_("Invalid game state"));

	if (gbIsHellfire) {
		giNumberOfLevels = 25;
		giNumberQuests = 24;
		giNumberOfSmithPremiumItems = 15;
	} else {
		giNumberOfLevels = 17;
		giNumberQuests = 16;
		giNumberOfSmithPremiumItems = 6;
	}

	file.WriteLE<uint8_t>(setlevel ? 1 : 0);
	file.WriteBE<uint32_t>(setlvlnum);
	file.WriteBE<uint32_t>(currlevel);
	file.WriteBE<uint32_t>(getHellfireLevelType(leveltype));
	file.WriteBE<int32_t>(ViewPosition.x);
	file.WriteBE<int32_t>(ViewPosition.y);
	file.WriteLE<uint8_t>(invflag ? 1 : 0);
	file.WriteLE<uint8_t>(CharFlag ? 1 : 0);
	file.WriteBE(static_cast<int32_t>(ActiveMonsterCount));
	file.WriteBE<int32_t>(ActiveItemCount);
	// ActiveMissileCount will be a value from 0-125 (for vanilla compatibility). Writing an unsigned value here to avoid
	// warnings about casting from unsigned to signed, but there's no sign extension issues when reading this as a signed
	// value later so it doesn't have to match in LoadGameData().
	file.WriteBE<uint32_t>(static_cast<uint32_t>(std::min(Missiles.size(), MaxMissilesForSaveGame)));
	file.WriteBE<int32_t>(ActiveObjectCount);

	for (uint8_t i = 0; i < giNumberOfLevels; i++) {
		file.WriteBE<uint32_t>(DungeonSeeds[i]);
		file.WriteBE<int32_t>(getHellfireLevelType(GetLevelType(i)));
	}

	Player &myPlayer = *MyPlayer;
	SavePlayer(file, myPlayer);

	for (int i = 0; i < giNumberQuests; i++)
		SaveQuest(&file, i);
	for (int i = 0; i < MAXPORTAL; i++)
		SavePortal(&file, i);
	for (int monstkill : MonsterKillCounts)
		file.WriteBE<int32_t>(monstkill);
	// add padding for vanilla save compatibility (Related to bugfix where MonsterKillCounts[MaxMonsters] was changed to MonsterKillCounts[NUM_MTYPES]
	file.Skip(4 * (MaxMonsters - NUM_MTYPES));

	if (leveltype != DTYPE_TOWN) {
		for (unsigned monsterId : ActiveMonsters)
			file.WriteBE<uint32_t>(monsterId);
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			SaveMonster(&file, Monsters[ActiveMonsters[i]]);
		// Write ActiveMissiles
		for (uint8_t activeMissile = 0; activeMissile < MaxMissilesForSaveGame; activeMissile++)
			file.WriteLE<uint8_t>(activeMissile);
		// Write AvailableMissiles
		for (size_t availableMissiles = Missiles.size(); availableMissiles < MaxMissilesForSaveGame; availableMissiles++)
			file.WriteLE(static_cast<uint8_t>(availableMissiles));
		const size_t savedMissiles = std::min(Missiles.size(), MaxMissilesForSaveGame);
		file.Skip<uint8_t>(savedMissiles);
		// Write Missile Data
		{
			auto missilesEnd = Missiles.cbegin();
			std::advance(missilesEnd, savedMissiles);
			for (auto it = Missiles.cbegin(); it != missilesEnd; it++) {
				SaveMissile(&file, *it);
			}
		}
		for (int objectId : ActiveObjects)
			file.WriteLE(static_cast<int8_t>(objectId));
		for (int objectId : AvailableObjects)
			file.WriteLE(static_cast<int8_t>(objectId));
		for (int i = 0; i < ActiveObjectCount; i++)
			SaveObject(file, Objects[ActiveObjects[i]]);

		file.WriteBE<int32_t>(ActiveLightCount);

		for (uint8_t lightId : ActiveLights)
			file.WriteLE<uint8_t>(lightId);
		for (int i = 0; i < ActiveLightCount; i++)
			SaveLighting(&file, &Lights[ActiveLights[i]]);

		const auto visionCount = static_cast<int32_t>(Players.size());
		file.WriteBE<int32_t>(visionCount + 1); // VisionId
		file.WriteBE<int32_t>(visionCount);

		for (const Player &player : Players)
			SaveLighting(&file, &VisionList[player.getId()], true);
	}

	auto itemIndexes = SaveDroppedItems(file);

	for (bool uniqueItemFlag : UniqueItemFlags)
		file.WriteLE<uint8_t>(uniqueItemFlag ? 1 : 0);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(dLight[i][j]);
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(static_cast<uint8_t>(dFlags[i][j] & DungeonFlag::SavedFlags));
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<int8_t>(dPlayer[i][j]);
	}

	SaveDroppedItemLocations(file, itemIndexes);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteBE<int32_t>(dMonster[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dCorpse[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dObject[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++)        // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dLight[i][j]); // BUGFIX: dLight got saved already
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dPreLight[i][j]);
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(AutomapView[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++)                                 // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(TileContainsMissile({ i, j }) ? -1 : 0); // For backwards compatibility
		}
	}

	file.WriteBE<int32_t>(PremiumItemCount);
	file.WriteBE<int32_t>(PremiumItemLevel);

	for (int i = 0; i < giNumberOfSmithPremiumItems; i++)
		SaveItem(file, PremiumItems[i]);

	file.WriteLE<uint8_t>(AutomapActive ? 1 : 0);
	file.WriteBE<int32_t>(AutoMapScale);

	SaveAdditionalMissiles(saveWriter);
	SaveLevelSeeds(saveWriter);
}

void SaveGame()
{
	gbValidSaveFile = true;
	pfile_write_hero(/*writeGameData=*/true);
	sfile_write_stash();
}

void SaveLevel(SaveWriter &saveWriter)
{
	SaveLevel(saveWriter, nullptr);
}

tl::expected<void, std::string> LoadLevel()
{
	return LoadLevel(nullptr);
}

} // namespace devilution
