#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/point.hpp"

namespace devilution {

struct Player;

enum class TrackerTargetCategory : uint8_t {
	Items,
	Chests,
	Doors,
	Shrines,
	Objects,
	Breakables,
	Monsters,
	DeadBodies,
};

extern TrackerTargetCategory SelectedTrackerTargetCategory;
extern TrackerTargetCategory AutoWalkTrackerTargetCategory;
extern int AutoWalkTrackerTargetId;

// Position-check predicates used by both tracker and location_speech path-finding.
bool PosOkPlayerIgnoreDoors(const Player &player, Point position);
bool PosOkPlayerIgnoreMonsters(const Player &player, Point position);
bool PosOkPlayerIgnoreDoorsAndMonsters(const Player &player, Point position);
bool PosOkPlayerIgnoreDoorsMonstersAndBreakables(const Player &player, Point position);

void CycleTrackerTargetKeyPressed();
void NavigateToTrackerTargetKeyPressed();
void AutoWalkToTrackerTargetKeyPressed();
void UpdateAutoWalkTracker();

} // namespace devilution
