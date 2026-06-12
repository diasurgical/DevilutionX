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

/**
 * @brief Paths for town tile graphics: dungeon CEL, mega TIL, special CELs, SOL, and palette.
 *
 * Defaults match Tristram (Hellfire `nlevels\\` where applicable). On load failure, the engine
 * retries Tristram’s defaults, then the retail (vanilla Diablo) fallbacks via
 * LoadTownAssetWithFallback. Lua `assets` only sets the instance fields; the retail
 * chain is not configurable.
 */
struct TownVisualAssets {
	std::string dungeonCelPath = R"(nlevels\towndata\town.cel)";
	std::string megaTilePath = R"(nlevels\towndata\town.til)";
	/** @brief Piece/min data for SetDungeonMicros (mega-tile → micro CEL frames). */
	std::string pieceMinPath = R"(nlevels\towndata\town.min)";
	std::string specialCelsPath = R"(levels\towndata\towns)";
	std::string solPath = R"(nlevels\towndata\town.sol)";
	std::string palettePath = R"(levels\towndata\town.pal)";
	/**
	 * @brief Sub-tile count per mega-tile for SetDungeonMicros.
	 * Must match the CEL and MIN format: 16 for town.cel/min, 10 for L1/l2/l3/l6, 12 for L4.
	 */
	uint_fast8_t microTileLen = 16;

	/** @brief Retail (DIABDAT.MPQ) fallback paths for vanilla Diablo (no Hellfire nlevels). */
	static constexpr char RetailDungeonCel[] = R"(levels\towndata\town.cel)";
	static constexpr char RetailMegaTile[] = R"(levels\towndata\town.til)";
	static constexpr char RetailPieceMin[] = R"(levels\towndata\town.min)";
	static constexpr char RetailSol[] = R"(levels\towndata\town.sol)";
};

/**
 * @brief Loads a town asset with a 3-tier fallback: activePath → hellfireFallback → retailFallback.
 *
 * Skips hellfireFallback if it matches activePath. Skips retailFallback if nullptr.
 * The `load` callable must accept `const char*` and return a type with `.has_value()`.
 */
template <typename LoadFn>
auto LoadTownAssetWithFallback(
    const std::string &activePath,
    const char *hellfireFallback,
    const char *retailFallback,
    LoadFn &&load) -> decltype(load(static_cast<const char *>(nullptr)))
{
	auto result = load(activePath.c_str());
	if (result.has_value())
		return result;
	if (activePath != hellfireFallback) {
		result = load(hellfireFallback);
		if (result.has_value())
			return result;
	}
	if (retailFallback != nullptr)
		return load(retailFallback);
	return result;
}

/**
 * @brief Complete configuration for a town
 */
struct TownConfig {
	std::string name;
	uint8_t saveId = 0;
	Point dminPosition = { 10, 10 };
	Point dmaxPosition = { 84, 84 };
	TownVisualAssets visualAssets;
	std::vector<TownSector> sectors;
	std::vector<TownEntryPoint> entries;
	std::vector<TownTrigger> triggers;
	std::vector<TownerPositionOverride> townerOverrides;
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

/**
 * @brief Town used for DTYPE_TOWN tile loading (CEL/TIL/SOL/palette): current town if registered, else Tristram.
 */
const TownConfig &GetActiveTownConfigForTileLoad();

/**
 * @brief Town ID to switch to, set before queuing WM_DIABTOWNSWITCH.
 * Written by: towns Lua module, OnTownTravel (network), NetSendCmdTownTravel.
 * Read by: WM_DIABTOWNSWITCH handler in interfac.cpp.
 */
extern std::string DestinationTownID;

void InitializeTristram();

} // namespace devilution
