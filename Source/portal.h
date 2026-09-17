/**
 * @file portal.h
 *
 * Interface of functionality for handling town portals.
 */
#pragma once

#include <array>
#include <cstddef>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {

// Defined in player.h, forward declared here to allow for functions which operate in the context of a player.
struct Player;

#define MAXPORTAL 4

struct Portal {
	bool open;
	Point position;
	int level;
	dungeon_type ltype;
	bool setlvl;
};

extern Portal Portals[MAXPORTAL];

void InitPortals();
void SetPortalStats(int i, bool o, Point position, int lvl, dungeon_type lvltype, bool isSetLevel);
void AddPortalMissile(const Player &player, Point position, bool sync);
void SyncPortals();
void AddPortalInTown(const Player &player);
void ActivatePortal(const Player &player, Point position, int lvl, dungeon_type lvltype, bool sp);
void DeactivatePortal(const Player &player);
bool PortalOnLevel(const Player &player);
void RemovePortalMissile(const Player &player);
void SetCurrentPortal(size_t p);
void GetPortalLevel();
void GetPortalLvlPos();
bool PosOkPortal(int lvl, Point position);

/** Returns the town-map position for portal slot `portalIndex`. */
Point GetPortalTownPosition(size_t portalIndex);

/** Overrides all portal positions for the current town. */
void SetPortalTownPositions(std::array<Point, MAXPORTAL> positions);

} // namespace devilution
