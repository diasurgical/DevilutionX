#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/point.hpp"
#include "interfac.h"
#include "levels/gendung_defs.hpp"

namespace devilution {

/** @brief Canonical town registry id for vanilla Tristram (lowercase). */
inline constexpr char TristramTownId[] = "tristram";

/**
 * @brief Represents a town sector (map piece)
 */
struct TownSector {
	std::string filePath;
	int x, y;
};

/**
 * @brief Represents a town entry/spawn point
 */
struct TownEntryPoint {
	lvl_entry entryType;
	Point viewPosition;
	int warpFromLevel; // Source level for ENTRY_TWARPUP (-1 for any)
};

/**
 * @brief Position override for a towner NPC
 */
struct TownerPositionOverride {
	std::string shortName; // e.g. "griswold", "farnham"
	Point position;
};

/**
 * @brief Dungeon entrance / town warp trigger (see InitTownTriggers)
 */
struct TownTrigger {
	Point position;
	interface_mode msg;
	/** For WM_DIABTOWNWARP; unused for other message types */
	int level = 0;
	/** If set, trigger is only active when IsWarpOpen(*warpGate) */
	std::optional<dungeon_type> warpGate;
};

struct TownWarpFillTile {
	int x;
	int y;
	int tile;
};

/**
 * @brief Fills random ground micros (1-4) for each x in [xStart, xEnd).
 */
struct TownWarpClosedRandomGroundStrip {
	int xStart;
	int xEndExclusive;
	int y;
};

/**
 * @brief Visual dungeon/dPiece patches when a town warp is still closed (see DrlgTPass3).
 */
struct TownWarpPatch {
	dungeon_type requiredWarp;
	std::vector<std::pair<Point, int>> dungeonCells;
	std::vector<TownWarpFillTile> fillTiles;
	std::optional<TownWarpClosedRandomGroundStrip> randomGroundStrip;
};

/**
 * @brief Complete configuration for a town
 */
struct TownConfig {
	std::string name;
	uint8_t saveId = 0;
	Point dminPosition = { 10, 10 };
	Point dmaxPosition = { 84, 84 };
	std::vector<TownSector> sectors;
	std::vector<TownEntryPoint> entries;
	std::vector<TownTrigger> triggers;
	std::vector<TownWarpPatch> warpClosedPatches;
	std::vector<TownerPositionOverride> townerOverrides;

	/**
	 * @brief Gets the spawn point for a given entry type and warp source
	 */
	Point GetEntryPoint(lvl_entry entry, int warpFrom = -1) const;
};

/**
 * @brief Registry for managing multiple town configurations
 */
class TownRegistry {
private:
	std::unordered_map<std::string, TownConfig> towns;
	std::string currentTownID;

public:
	void RegisterTown(const std::string &id, const TownConfig &config);
	const TownConfig &GetTown(const std::string &id) const;
	TownConfig &GetTown(const std::string &id);
	bool HasTown(const std::string &id) const;
	void SetCurrentTown(const std::string &id);
	const std::string &GetCurrentTown() const;
	const std::unordered_map<std::string, TownConfig> &GetTowns() const { return towns; }

	/** @brief Finds town string ID by its saveId. Returns TristramTownId if not found. */
	std::string GetTownBySaveId(uint8_t saveId) const;
};

TownRegistry &GetTownRegistry();

/**
 * @brief Town ID to switch to, set before queuing WM_DIABTOWNSWITCH.
 * Written by: towns Lua module, OnTownTravel (network), NetSendCmdTownTravel.
 * Read by: WM_DIABTOWNSWITCH handler in interfac.cpp.
 */
extern std::string DestinationTownID;

void InitializeTristram();

} // namespace devilution
