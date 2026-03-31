#include "levels/town_data.h"

#include "utils/log.hpp"

namespace devilution {

namespace {

TownRegistry g_townRegistry;

} // namespace

std::string DestinationTownID;

TownRegistry &GetTownRegistry()
{
	return g_townRegistry;
}

void TownRegistry::RegisterTown(const std::string &id, const TownConfig &config)
{
	towns[id] = config;
	LogInfo("Registered town: {}", id);
}

TownConfig &TownRegistry::GetTown(const std::string &id)
{
	return towns.at(id);
}

bool TownRegistry::HasTown(const std::string &id) const
{
	return towns.count(id) > 0;
}

void TownRegistry::SetCurrentTown(const std::string &id)
{
	currentTownID = id;
}

std::string TownRegistry::GetCurrentTown() const
{
	return currentTownID;
}

std::string TownRegistry::GetTownBySaveId(uint8_t saveId) const
{
	for (const auto &[id, config] : towns) {
		if (config.saveId == saveId) {
			return id;
		}
	}
	return "tristram";
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
	}

	// For other entry types, just match the type
	for (const auto &ep : entries) {
		if (ep.entryType == entry) {
			return ep.viewPosition;
		}
	}

	// Default fallback
	return { 75, 68 };
}

void InitializeTristram()
{
	TownConfig tristram;
	tristram.name = "Tristram";
	tristram.saveId = 0;
	tristram.dminPosition = { 10, 10 };
	tristram.dmaxPosition = { 84, 84 };
	tristram.sectors = {
		{ "levels\\towndata\\sector1s.dun", 46, 46 },
		{ "levels\\towndata\\sector2s.dun", 46, 0 },
		{ "levels\\towndata\\sector3s.dun", 0, 46 },
		{ "levels\\towndata\\sector4s.dun", 0, 0 },
	};
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
	GetTownRegistry().RegisterTown("tristram", tristram);
	GetTownRegistry().SetCurrentTown("tristram");
}

} // namespace devilution
