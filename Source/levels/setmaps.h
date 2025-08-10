/**
 * @file setmaps.cpp
 *
 * Interface of functionality for the special quest dungeons.
 */
#pragma once

#include "levels/gendung.h"

namespace devilution {

/**
 * @brief Get the tile type used to render the given arena level
 */
inline dungeon_type GetArenaLevelType(_setlevels arenaLevel)
{
	constexpr dungeon_type DungeonTypeForArena[] = {
		dungeon_type::DTYPE_CATHEDRAL, // SL_ARENA_CHURCH
		dungeon_type::DTYPE_HELL,      // SL_ARENA_HELL
		dungeon_type::DTYPE_HELL,      // SL_ARENA_CIRCLE_OF_LIFE
	};

	constexpr size_t arenaCount = sizeof(DungeonTypeForArena) / sizeof(dungeon_type);
	const size_t index = arenaLevel - SL_FIRST_ARENA;
	return index < arenaCount ? DungeonTypeForArena[index] : DTYPE_NONE;
}

/**
 * @brief Load a quest map, the given map is specified via the global setlvlnum
 */
void LoadSetMap();

/* rdata */
extern const char *const QuestLevelNames[];

} // namespace devilution
