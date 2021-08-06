/**
 * @file misdat.h
 *
 * Interface of data related to missiles.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "engine.h"
#include "effects.h"
#include "utils/autofill_array.h"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {

enum mienemy_type : uint8_t {
	TARGET_MONSTERS,
	TARGET_PLAYERS,
	TARGET_BOTH,
};

enum missile_resistance : uint8_t {
	MISR_NONE,
	MISR_FIRE,
	MISR_LIGHTNING,
	MISR_MAGIC,
	MISR_ACID,
};

typedef enum missile_graphic_id : uint8_t {
	MFILE_ARROWS,
	MFILE_FIREBA,
	MFILE_GUARD,
	MFILE_LGHNING,
	MFILE_FIREWAL,
	MFILE_MAGBLOS,
	MFILE_PORTAL,
	MFILE_BLUEXFR,
	MFILE_BLUEXBK,
	MFILE_MANASHLD,
	MFILE_BLOOD,
	MFILE_BONE,
	MFILE_METLHIT,
	MFILE_FARROW,
	MFILE_DOOM,
	MFILE_0F,
	MFILE_BLODBUR,
	MFILE_NEWEXP,
	MFILE_SHATTER1,
	MFILE_BIGEXP,
	MFILE_INFERNO,
	MFILE_THINLGHT,
	MFILE_FLARE,
	MFILE_FLAREEXP,
	MFILE_MAGBALL,
	MFILE_KRULL,
	MFILE_MINILTNG,
	MFILE_HOLY,
	MFILE_HOLYEXPL,
	MFILE_LARROW,
	MFILE_FIRARWEX,
	MFILE_ACIDBF,
	MFILE_ACIDSPLA,
	MFILE_ACIDPUD,
	MFILE_ETHRSHLD,
	MFILE_FIRERUN,
	MFILE_RESSUR1,
	MFILE_SKLBALL,
	MFILE_RPORTAL,
	MFILE_FIREPLAR,
	MFILE_SCUBMISB,
	MFILE_SCBSEXPB,
	MFILE_SCUBMISC,
	MFILE_SCBSEXPC,
	MFILE_SCUBMISD,
	MFILE_SCBSEXPD,
	MFILE_SPAWNS,
	MFILE_REFLECT,
	MFILE_LICH,
	MFILE_MSBLA,
	MFILE_NECROMORB,
	MFILE_ARCHLICH,
	MFILE_RUNE,
	MFILE_EXYEL2,
	MFILE_EXBL2,
	MFILE_EXRED3,
	MFILE_BONEDEMON,
	MFILE_EXORA1,
	MFILE_EXBL3,
	MFILE_NONE, // BUGFIX: should be `MFILE_NONE = MFILE_SCBSEXPD+1`, i.e. MFILE_NULL, since there would otherwise be an out-of-bounds in SetMissAnim when accessing MissileSpriteData for any of the missiles that have MFILE_NONE as mFileNum in MissileData. (fixed)
} missile_graphic_id;

/**
 * @brief Specifies what if and how movement distribution is applied
 */
enum class MissileMovementDistrubution {
	/**
      * @brief No movement distribution is calculated. Normally this means the missile doesn't move at all.
      */
	Disabled,
	/**
      * @brief The missile moves and if it hits a enemey it stops (for example firebolt)
      */
	Blockable,
	/**
      * @brief The missile moves and even it hits a enemy it keeps moving (for example flame wave)
      */
	Unblockable,
};

typedef struct MissileData {
	void (*mAddProc)(int, Point, Point, int, int8_t, int, int);
	void (*mProc)(int);
	uint8_t mName;
	bool mDraw;
	uint8_t mType;
	missile_resistance mResist;
	uint8_t mFileNum;
	_sfx_id mlSFX;
	_sfx_id miSFX;
	MissileMovementDistrubution MovementDistribution;
} MissileDataStruct;

enum class MissileDataFlags {
	None        = 0,
	NoGraphics  = 1 << 0,
	NotAnimated = 1 << 1,
};

struct MissileFileData {
	const char *name;
	uint8_t animName;
	uint8_t animFAmt;
	MissileDataFlags flags;
	std::array<std::unique_ptr<byte[]>, 16> animData;
	AutofillArray<uint8_t, 16> animDelay = {};
	AutofillArray<uint8_t, 16> animLen = {};
	AutofillArray<int16_t, 16> animWidth = {};
	AutofillArray<int16_t, 16> animWidth2 = {};

	MissileFileData(const char *name, uint8_t animName, uint8_t animFAmt, MissileDataFlags flags)
	    : name(name)
	    , animName(animName)
	    , animFAmt(animFAmt)
	    , flags(flags)
	{
	}

	MissileFileData(const char *name, uint8_t animName, uint8_t animFAmt, MissileDataFlags flags,
			AutofillArray<uint8_t, 16> animDelay, AutofillArray<uint8_t, 16> animLen,
			AutofillArray<int16_t, 16> animWidth, AutofillArray<int16_t, 16> animWidth2);

	void LoadGFX();

	void FreeGFX()
	{
		animData = {};
	}
};

extern MissileDataStruct MissileData[];
extern MissileFileData MissileSpriteData[];

} // namespace devilution
