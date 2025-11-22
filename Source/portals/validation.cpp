/**
 * @file portals/validation.cpp
 *
 * Implementation of functions for validation of portal data.
 */

#include "portals/validation.hpp"

#include <cstdint>

#include "engine/world_tile.hpp"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "quests.h"

namespace devilution {

namespace {

dungeon_type GetQuestLevelType(_setlevels questLevel)
{
	for (const Quest &quest : Quests) {
		if (quest._qslvl == questLevel)
			return quest._qlvltype;
	}
	return DTYPE_NONE;
}

dungeon_type GetSetLevelType(_setlevels setLevel)
{
	const bool isArenaLevel = setLevel >= SL_FIRST_ARENA && setLevel <= SL_LAST;
	return isArenaLevel ? GetArenaLevelType(setLevel) : GetQuestLevelType(setLevel);
}

} // namespace

bool IsPortalDeltaValid(WorldTilePosition location, uint8_t level, uint8_t ltype, bool isOnSetLevel)
{
	if (!InDungeonBounds(location))
		return false;
	const dungeon_type levelType = static_cast<dungeon_type>(ltype);
	if (levelType == DTYPE_NONE)
		return false;
	if (isOnSetLevel)
		return levelType == GetSetLevelType(static_cast<_setlevels>(level));
	return levelType == GetLevelType(level);
}

} // namespace devilution
