#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/point.hpp"
#include "interfac.h"
#include "levels/gendung_defs.hpp"

namespace devilution {

/** @brief Canonical town registry id for vanilla Tristram (lowercase). */
inline constexpr char TristramTownId[] = "tristram";

/** @brief Town portal spell anchor slots; must match MAXPORTAL in portal.h. */
inline constexpr size_t NumTownPortalSlots = 4;

/** @brief Legacy positions for town portal missiles (one per player slot). */
inline constexpr std::array<Point, NumTownPortalSlots> DefaultTristramPortalPositions = { {
	Point { 57, 40 },
	Point { 59, 40 },
	Point { 61, 40 },
	Point { 63, 40 },
} };

/**
 * @brief World position for the town portal missile / portal entry for player slot `portalIndex`.
 */
Point GetPortalTownPosition(size_t portalIndex);

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
	std::array<Point, NumTownPortalSlots> portalPositions = DefaultTristramPortalPositions;

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
 * @brief Returns the current town config, or nullptr if no town is set / registered.
 */
const TownConfig *GetCurrentTownConfig();

void InitializeTristram();

} // namespace devilution
