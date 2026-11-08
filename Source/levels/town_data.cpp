#include "levels/town_data.h"

#include "utils/log.hpp"

namespace devilution {

namespace {

TownRegistry g_townRegistry;

/** @brief Spawn used when no TownEntryPoint matches (matches legacy hard-coded default). */
constexpr Point DefaultTownEntryPoint = { 75, 68 };

} // namespace

TownRegistry &GetTownRegistry()
{
	return g_townRegistry;
}

const TownConfig *GetCurrentTownConfig()
{
	const TownRegistry &registry = GetTownRegistry();
	const std::optional<uint8_t> townId = registry.GetCurrentTown();
	if (townId.has_value() && registry.HasTown(*townId))
		return &registry.GetTown(*townId);
	return nullptr;
}

const TownConfig &GetActiveTownConfigForTileLoad()
{
	const TownConfig *config = GetCurrentTownConfig();
	if (config != nullptr)
		return *config;
	return GetTownRegistry().GetTown(TristramTownId);
}

void TownRegistry::RegisterTown(const TownConfig &config)
{
	const auto existing = towns.find(config.townId);
	if (existing != towns.end())
		LogWarn("RegisterTown: overwriting town {} ({})", config.townId, existing->second.name);
	towns.insert_or_assign(config.townId, config);
	LogInfo("Registered town {} ({})", config.townId, config.name);
}

const TownConfig &TownRegistry::GetTown(uint8_t townId) const
{
	return towns.at(townId);
}

TownConfig &TownRegistry::GetTown(uint8_t townId)
{
	return towns.at(townId);
}

bool TownRegistry::HasTown(uint8_t townId) const
{
	return towns.contains(townId);
}

void TownRegistry::SetCurrentTown(uint8_t townId)
{
	currentTownId = townId;
}

const std::optional<uint8_t> &TownRegistry::GetCurrentTown() const
{
	return currentTownId;
}

Point GetPortalTownPosition(size_t portalIndex)
{
	if (portalIndex >= NumTownPortalSlots)
		return DefaultTristramPortalPositions[0];
	const TownConfig *config = GetCurrentTownConfig();
	if (config != nullptr)
		return config->portalPositions[portalIndex];
	return DefaultTristramPortalPositions[portalIndex];
}

Point TownConfig::GetEntryPoint(lvl_entry entry, int warpFrom) const
{
	// For ENTRY_TWARPUP, match both entry type and warpFromLevel
	if (entry == ENTRY_TWARPUP) {
		for (const auto &ep : entries) {
			if (ep.entryType == entry && ep.warpFromLevel == warpFrom) {
				return ep.viewPosition;
			}
		}
		return DefaultTownEntryPoint;
	}

	// For other entry types, just match the type
	for (const auto &ep : entries) {
		if (ep.entryType == entry) {
			return ep.viewPosition;
		}
	}

	// Default fallback
	return DefaultTownEntryPoint;
}

void InitializeTristram()
{
	TownConfig tristram;
	tristram.name = "Tristram";
	tristram.townId = TristramTownId;
	tristram.sectors = {
		{ "levels\\towndata\\sector1s.dun", 46, 46 },
		{ "levels\\towndata\\sector2s.dun", 46, 0 },
		{ "levels\\towndata\\sector3s.dun", 0, 46 },
		{ "levels\\towndata\\sector4s.dun", 0, 0 },
	};
	// Spawn positions (view): one tile stepped from matching town triggers / stairs.
	tristram.entries = {
		{ ENTRY_MAIN, { 75, 68 }, -1 },
		{ ENTRY_PREV, { 25, 31 }, -1 },
		{ ENTRY_TWARPUP, { 49, 22 }, 5 },
		{ ENTRY_TWARPUP, { 18, 69 }, 9 },
		{ ENTRY_TWARPUP, { 41, 81 }, 13 },
		{ ENTRY_TWARPUP, { 36, 25 }, 21 },
		{ ENTRY_TWARPUP, { 79, 62 }, 17 },
		{ ENTRY_TOWNSWITCH, { 75, 68 }, -1 },
	};
	tristram.triggers = {
		// Cathedral stairs (down to dungeon level 1)
		{ { 25, 29 }, WM_DIABNEXTLVL, 0, std::nullopt },
		// Town warp portals (active only when the respective warp is open)
		{ { 49, 21 }, WM_DIABTOWNWARP, 5, DTYPE_CATACOMBS },
		{ { 17, 69 }, WM_DIABTOWNWARP, 9, DTYPE_CAVES },
		{ { 41, 80 }, WM_DIABTOWNWARP, 13, DTYPE_HELL },
		{ { 80, 62 }, WM_DIABTOWNWARP, 17, DTYPE_NEST },
		{ { 36, 24 }, WM_DIABTOWNWARP, 21, DTYPE_CRYPT },
	};
	TownRegistry &registry = GetTownRegistry();
	registry.RegisterTown(tristram);
	const std::optional<uint8_t> current = registry.GetCurrentTown();
	if (!current.has_value() || !registry.HasTown(*current))
		registry.SetCurrentTown(TristramTownId);
}

} // namespace devilution
