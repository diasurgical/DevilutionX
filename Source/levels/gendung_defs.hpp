#pragma once

#include <cstdint>

#define DMAXX 40
#define DMAXY 40

#define MAXDUNX (16 + DMAXX * 2 + 16)
#define MAXDUNY (16 + DMAXY * 2 + 16)

#define NUMLEVELS 25

namespace devilution {

enum dungeon_type : int8_t {
	DTYPE_TOWN,
	DTYPE_CATHEDRAL,
	DTYPE_CATACOMBS,
	DTYPE_CAVES,
	DTYPE_HELL,
	DTYPE_NEST,
	DTYPE_CRYPT,

	DTYPE_LAST = DTYPE_CRYPT,
	DTYPE_NONE = -1,
};

enum lvl_entry : uint8_t {
	ENTRY_MAIN,
	ENTRY_PREV,
	ENTRY_SETLVL,
	ENTRY_RTNLVL,
	ENTRY_LOAD,
	ENTRY_WARPLVL,
	ENTRY_TWARPDN,
	ENTRY_TWARPUP,
};

enum _difficulty : uint8_t {
	DIFF_NORMAL,
	DIFF_NIGHTMARE,
	DIFF_HELL,

	DIFF_LAST = DIFF_HELL,
};

enum _setlevels : int8_t {
	SL_NONE,
	SL_SKELKING,
	SL_BONECHAMB,
	SL_MAZE,
	SL_POISONWATER,
	SL_VILEBETRAYER,

	SL_ARENA_CHURCH,
	SL_ARENA_HELL,
	SL_ARENA_CIRCLE_OF_LIFE,

	SL_FIRST_ARENA = SL_ARENA_CHURCH,
	SL_LAST = SL_ARENA_CIRCLE_OF_LIFE,
};

inline bool IsArenaLevel(_setlevels setLevel)
{
	switch (setLevel) {
	case SL_ARENA_CHURCH:
	case SL_ARENA_HELL:
	case SL_ARENA_CIRCLE_OF_LIFE:
		return true;
	default:
		return false;
	}
}

} // namespace devilution
