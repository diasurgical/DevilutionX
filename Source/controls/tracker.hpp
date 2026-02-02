/**
 * @file controls/tracker.hpp
 *
 * Tracker system for accessibility: target cycling, pathfinding, and auto-walk.
 */
#pragma once

#include <cstdint>

namespace devilution {

enum class TrackerTargetCategory : uint8_t {
	Items,
	Chests,
	Doors,
	Shrines,
	Objects,
	Breakables,
	Monsters,
	DeadBodies,
	Npcs,
	Players,
	DungeonEntrances,
	Stairs,
	QuestLocations,
	Portals,
};

void TrackerPageUpKeyPressed();
void TrackerPageDownKeyPressed();
void TrackerHomeKeyPressed();
void UpdateAutoWalkTracker();
void ResetAutoWalkTracker();

} // namespace devilution
