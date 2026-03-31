#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/point.hpp"
#include "levels/gendung_defs.hpp"

namespace devilution {

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
 * @brief Complete configuration for a town
 */
struct TownConfig {
	std::string name;
	uint8_t saveId = 0;
	Point dminPosition = { 10, 10 };
	Point dmaxPosition = { 84, 84 };
	std::vector<TownSector> sectors;
	std::string solFile;
	std::vector<TownEntryPoint> entries;
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
	TownConfig &GetTown(const std::string &id);
	bool HasTown(const std::string &id) const;
	void SetCurrentTown(const std::string &id);
	std::string GetCurrentTown() const;
	const std::unordered_map<std::string, TownConfig> &GetTowns() const { return towns; }

	/** @brief Finds town string ID by its saveId. Returns "tristram" if not found. */
	std::string GetTownBySaveId(uint8_t saveId) const;
};

TownRegistry &GetTownRegistry();

extern std::string DestinationTownID;

void InitializeTristram();

} // namespace devilution
