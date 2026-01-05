/**
 * @file monstdat.h
 *
 * Interface of all monster data.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include "cursor.h"
#include "textdat.h"

namespace devilution {

class DataFile;

enum class MonsterAIID : int8_t {
	Zombie,
	Fat,
	SkeletonMelee,
	SkeletonRanged,
	Scavenger,
	Rhino,
	GoatMelee,
	GoatRanged,
	Fallen,
	Magma,
	SkeletonKing,
	Bat,
	Gargoyle,
	Butcher,
	Succubus,
	Sneak,
	Storm,
	FireMan,
	Gharbad,
	Acid,
	AcidUnique,
	Golem,
	Zhar,
	Snotspill,
	Snake,
	Counselor,
	Mega,
	Diablo,
	Lazarus,
	LazarusSuccubus,
	Lachdanan,
	Warlord,
	FireBat,
	Torchant,
	HorkDemon,
	Lich,
	ArchLich,
	Psychorb,
	Necromorb,
	BoneDemon,
	Invalid = -1,
};

enum class MonsterClass : uint8_t {
	Undead,
	Demon,
	Animal,
};

enum monster_resistance : uint8_t {
	// clang-format off
	RESIST_MAGIC     = 1 << 0,
	RESIST_FIRE      = 1 << 1,
	RESIST_LIGHTNING = 1 << 2,
	IMMUNE_MAGIC     = 1 << 3,
	IMMUNE_FIRE      = 1 << 4,
	IMMUNE_LIGHTNING = 1 << 5,
	IMMUNE_ACID      = 1 << 7,
	// clang-format on
};

enum monster_treasure : uint16_t {
	// clang-format off
	T_MASK    = 0xFFF,
	T_NODROP = 0x4000, // monster doesn't drop any loot
	T_UNIQ    = 0x8000, // use combined with unique item's ID - for example butcher's cleaver = T_UNIQ+UITEM_CLEAVE
	// clang-format on
};

enum class MonsterAvailability : uint8_t {
	Never,
	Always,
	Retail,
};

struct MonsterData {
	std::string name;
	std::string soundSuffix;
	std::string trnFile;
	uint16_t spriteId = 0;
	MonsterAvailability availability = MonsterAvailability::Never;
	uint16_t width = 0;
	uint16_t image = 0;
	bool hasSpecial = false;
	bool hasSpecialSound = false;
	int8_t frames[6] {};
	int8_t rate[6] {};
	int8_t minDunLvl = 0;
	int8_t maxDunLvl = 0;
	int8_t level = 0;
	uint16_t hitPointsMinimum = 0;
	uint16_t hitPointsMaximum = 0;
	MonsterAIID ai = MonsterAIID::Invalid;
	/**
	 * @brief Denotes monster's abilities defined in @p monster_flag as bitflags
	 * For usage, see @p MonstersData in monstdat.cpp
	 */
	uint16_t abilityFlags = 0;
	uint8_t intelligence = 0;
	uint8_t toHit = 0;
	int8_t animFrameNum = 0;
	uint8_t minDamage = 0;
	uint8_t maxDamage = 0;
	uint8_t toHitSpecial = 0;
	int8_t animFrameNumSpecial = 0;
	uint8_t minDamageSpecial = 0;
	uint8_t maxDamageSpecial = 0;
	uint8_t armorClass = 0;
	MonsterClass monsterClass {};
	/** Using monster_resistance as bitflags */
	uint8_t resistance = 0;
	/** Using monster_resistance as bitflags */
	uint8_t resistanceHell = 0;
	SelectionRegion selectionRegion = SelectionRegion::None;
	/** Using monster_treasure */
	uint16_t treasure = 0;
	uint16_t exp = 0;

	[[nodiscard]] const char *spritePath() const;

	[[nodiscard]] const char *soundPath() const
	{
		return !soundSuffix.empty() ? soundSuffix.c_str() : spritePath();
	}

	[[nodiscard]] bool hasAnim(size_t index) const
	{
		return frames[index] != 0;
	}
};

enum _monster_id : int16_t {
	MT_NZOMBIE,
	MT_BZOMBIE,
	MT_GZOMBIE,
	MT_YZOMBIE,
	MT_RFALLSP,
	MT_DFALLSP,
	MT_YFALLSP,
	MT_BFALLSP,
	MT_WSKELAX,
	MT_TSKELAX,
	MT_RSKELAX,
	MT_XSKELAX,
	MT_RFALLSD,
	MT_DFALLSD,
	MT_YFALLSD,
	MT_BFALLSD,
	MT_NSCAV,
	MT_BSCAV,
	MT_WSCAV,
	MT_YSCAV,
	MT_WSKELBW,
	MT_TSKELBW,
	MT_RSKELBW,
	MT_XSKELBW,
	MT_WSKELSD,
	MT_TSKELSD,
	MT_RSKELSD,
	MT_XSKELSD,
	MT_INVILORD,
	MT_SNEAK,
	MT_STALKER,
	MT_UNSEEN,
	MT_ILLWEAV,
	MT_LRDSAYTR,
	MT_NGOATMC,
	MT_BGOATMC,
	MT_RGOATMC,
	MT_GGOATMC,
	MT_FIEND,
	MT_BLINK,
	MT_GLOOM,
	MT_FAMILIAR,
	MT_NGOATBW,
	MT_BGOATBW,
	MT_RGOATBW,
	MT_GGOATBW,
	MT_NACID,
	MT_RACID,
	MT_BACID,
	MT_XACID,
	MT_SKING,
	MT_CLEAVER,
	MT_FAT,
	MT_MUDMAN,
	MT_TOAD,
	MT_FLAYED,
	MT_WYRM,
	MT_CAVSLUG,
	MT_DVLWYRM,
	MT_DEVOUR,
	MT_NMAGMA,
	MT_YMAGMA,
	MT_BMAGMA,
	MT_WMAGMA,
	MT_HORNED,
	MT_MUDRUN,
	MT_FROSTC,
	MT_OBLORD,
	MT_BONEDMN,
	MT_REDDTH,
	MT_LTCHDMN,
	MT_UDEDBLRG,
	MT_INCIN,
	MT_FLAMLRD,
	MT_DOOMFIRE,
	MT_HELLBURN,
	MT_STORM,
	MT_RSTORM,
	MT_STORML,
	MT_MAEL,
	MT_BIGFALL,
	MT_WINGED,
	MT_GARGOYLE,
	MT_BLOODCLW,
	MT_DEATHW,
	MT_MEGA,
	MT_GUARD,
	MT_VTEXLRD,
	MT_BALROG,
	MT_NSNAKE,
	MT_RSNAKE,
	MT_BSNAKE,
	MT_GSNAKE,
	MT_NBLACK,
	MT_RTBLACK,
	MT_BTBLACK,
	MT_RBLACK,
	MT_UNRAV,
	MT_HOLOWONE,
	MT_PAINMSTR,
	MT_REALWEAV,
	MT_SUCCUBUS,
	MT_SNOWWICH,
	MT_HLSPWN,
	MT_SOLBRNR,
	MT_COUNSLR,
	MT_MAGISTR,
	MT_CABALIST,
	MT_ADVOCATE,
	MT_GOLEM,
	MT_DIABLO,
	MT_DARKMAGE,
	MT_HELLBOAR,
	MT_STINGER,
	MT_PSYCHORB,
	MT_ARACHNON,
	MT_FELLTWIN,
	MT_HORKSPWN,
	MT_VENMTAIL,
	MT_NECRMORB,
	MT_SPIDLORD,
	MT_LASHWORM,
	MT_TORCHANT,
	MT_HORKDMN,
	MT_DEFILER,
	MT_GRAVEDIG,
	MT_TOMBRAT,
	MT_FIREBAT,
	MT_SKLWING,
	MT_LICH,
	MT_CRYPTDMN,
	MT_HELLBAT,
	MT_BONEDEMN,
	MT_ARCHLICH,
	MT_BICLOPS,
	MT_FLESTHNG,
	MT_REAPER,
	MT_NAKRUL,
	NUM_DEFAULT_MTYPES,
	NUM_MAX_MTYPES = 200, // same as MaxMonsters, for the sake of save game compability
	MT_INVALID = -1,
};

/**
 * @brief Defines if and how a group of monsters should be spawned with the unique monster
 */
enum class UniqueMonsterPack : uint8_t {
	/**
	 * @brief Don't spawn a group of monsters with the unique monster
	 */
	None,
	/**
	 * @brief Spawn a group of monsters that are independent from the unique monster
	 */
	Independent,
	/**
	 * @brief Spawn a group of monsters that are leashed to the unique monster
	 */
	Leashed,
};

struct UniqueMonsterData {
	_monster_id mtype;
	std::string mName;
	std::string mTrnName;
	uint8_t mlevel;
	uint16_t mmaxhp;
	MonsterAIID mAi;
	uint8_t mint;
	uint8_t mMinDamage;
	uint8_t mMaxDamage;
	/** Using monster_resistance as bitflags */
	uint16_t mMagicRes;
	/**
	 * @brief Defines if and how a group of monsters should be spawned with the unique monster
	 */

	UniqueMonsterPack monsterPack;
	uint8_t customToHit;
	uint8_t customArmorClass;
	_speech_id mtalkmsg;
};

extern std::vector<MonsterData> MonstersData;
extern const _monster_id MonstConvTbl[];
extern std::vector<UniqueMonsterData> UniqueMonstersData;

void LoadMonstDatFromFile(DataFile &dataFile, std::string_view filename, bool grow);
void LoadUniqueMonstDatFromFile(DataFile &dataFile, std::string_view filename);
void LoadMonsterData();

/**
 * @brief Returns the number of the monster sprite files.
 *
 * Different monsters can use the same sprite with different TRNs, these count as 1.
 */
size_t GetNumMonsterSprites();

} // namespace devilution

template <>
struct magic_enum::customize::enum_range<devilution::_monster_id> {
	static constexpr int min = devilution::MT_INVALID;
	static constexpr int max = devilution::NUM_DEFAULT_MTYPES;
};

template <>
struct magic_enum::customize::enum_range<devilution::monster_resistance> {
	static constexpr int min = 0;
	static constexpr int max = 128;
};
