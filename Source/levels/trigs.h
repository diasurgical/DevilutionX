/**
 * @file trigs.h
 *
 * Interface of functionality for triggering events when the player enters an area.
 */
#pragma once

#include <optional>
#include <vector>

#include "engine/point.hpp"
#include "interfac.h"
#include "levels/gendung.h"

namespace devilution {

#define MAXTRIGGERS 7

struct TriggerStruct {
	WorldTilePosition position;
	interface_mode _tmsg;
	int _tlvl;
};

extern bool trigflag;
extern int numtrigs;
extern TriggerStruct trigs[MAXTRIGGERS];
extern int TWarpFrom;

/**
 * @brief Town trigger definition: position, message, optional level, and optional warp-gate condition.
 */
struct TownTrigger {
	WorldTilePosition position;
	interface_mode msg;
	int level = 0;
	/** If set, the trigger is only active when IsWarpOpen(*warpGate). */
	std::optional<dungeon_type> warpGate;
};

void InitNoTriggers();
bool IsWarpOpen(dungeon_type type);
void InitTownTriggers();

/** Replaces the active town trigger list used by InitTownTriggers(). */
void SetTownTriggers(std::vector<TownTrigger> triggers);
void InitL1Triggers();
void InitL2Triggers();
void InitL3Triggers();
void InitL4Triggers();
void InitHiveTriggers();
void InitCryptTriggers();
void InitSKingTriggers();
void InitSChambTriggers();
void InitPWaterTriggers();
void InitVPTriggers();
void Freeupstairs();
void CheckTrigForce();
void CheckTriggers();

/**
 * @brief Check if the provided position is in the entrance boundary of the entrance.
 * @param entrance The entrance to check.
 * @param position The position to check against the entrance boundary.
 */
bool EntranceBoundaryContains(Point entrance, Point position);

} // namespace devilution
