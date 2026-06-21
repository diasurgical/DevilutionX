#include "lua/modules/towns.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <sol/sol.hpp>

#include "interfac.h"
#include "levels/gendung_defs.hpp"
#include "levels/town_data.h"
#include "lua/metadoc.hpp"
#include "msg.h"
#include "player.h"
#include "utils/log.hpp"

namespace devilution {

namespace {

std::optional<dungeon_type> ParseWarpGateString(std::string_view w)
{
	if (w == "catacombs")
		return DTYPE_CATACOMBS;
	if (w == "caves")
		return DTYPE_CAVES;
	if (w == "hell")
		return DTYPE_HELL;
	if (w == "nest")
		return DTYPE_NEST;
	if (w == "crypt")
		return DTYPE_CRYPT;
	return std::nullopt;
}

std::string LuaRegisterTown(std::string_view townId, const sol::table &config)
{
	TownConfig townConfig;
	sol::optional<std::string> name = config["name"];
	townConfig.name = name.has_value() ? *name : std::string(townId);
	sol::optional<int> saveId = config["saveId"];
	townConfig.saveId = saveId.has_value() ? static_cast<uint8_t>(*saveId) : 0;

	if (sol::optional<sol::table> assets = config["assets"]) {
		sol::table t = *assets;
		if (sol::optional<std::string> v = t["dungeonCel"])
			townConfig.visualAssets.dungeonCelPath = *v;
		if (sol::optional<std::string> v = t["megaTile"])
			townConfig.visualAssets.megaTilePath = *v;
		if (sol::optional<std::string> v = t["min"])
			townConfig.visualAssets.pieceMinPath = *v;
		if (sol::optional<std::string> v = t["specialCels"])
			townConfig.visualAssets.specialCelsPath = *v;
		if (sol::optional<std::string> v = t["sol"])
			townConfig.visualAssets.solPath = *v;
		if (sol::optional<std::string> v = t["palette"])
			townConfig.visualAssets.palettePath = *v;
		if (sol::optional<int> v = t["microTileLen"])
			townConfig.visualAssets.microTileLen = static_cast<uint_fast8_t>(*v);
	}

	if (sol::optional<sol::table> bounds = config["bounds"]) {
		if (sol::optional<sol::table> min = (*bounds)["min"]) {
			sol::optional<int> minX = (*min)["x"];
			sol::optional<int> minY = (*min)["y"];
			townConfig.dminPosition = { minX.value_or(10), minY.value_or(10) };
		}
		if (sol::optional<sol::table> max = (*bounds)["max"]) {
			sol::optional<int> maxX = (*max)["x"];
			sol::optional<int> maxY = (*max)["y"];
			townConfig.dmaxPosition = { maxX.value_or(84), maxY.value_or(84) };
		}
	}

	if (sol::optional<sol::table> sectors = config["sectors"]) {
		for (const auto &kv : *sectors) {
			sol::table sector = kv.second.as<sol::table>();
			TownSector s;
			sol::optional<std::string> path = sector["path"];
			s.filePath = path.value_or("");
			sol::optional<int> sx = sector["x"];
			sol::optional<int> sy = sector["y"];
			s.x = sx.value_or(0);
			s.y = sy.value_or(0);
			townConfig.sectors.push_back(s);
		}
	}

	if (sol::optional<sol::table> entries = config["entries"]) {
		for (const auto &kv : *entries) {
			sol::table entry = kv.second.as<sol::table>();
			TownEntryPoint ep;
			sol::optional<std::string> typeStr = entry["type"];
			std::string type = typeStr.value_or("main");
			if (type == "prev")
				ep.entryType = ENTRY_PREV;
			else if (type == "twarpdn")
				ep.entryType = ENTRY_TWARPDN;
			else if (type == "twarpup")
				ep.entryType = ENTRY_TWARPUP;
			else if (type == "townswitch")
				ep.entryType = ENTRY_TOWNSWITCH;
			else
				ep.entryType = ENTRY_MAIN;
			sol::optional<int> ex = entry["x"];
			sol::optional<int> ey = entry["y"];
			sol::optional<int> warpFrom = entry["warpFrom"];
			ep.viewPosition = { ex.value_or(75), ey.value_or(68) };
			ep.warpFromLevel = warpFrom.value_or(-1);
			townConfig.entries.push_back(ep);
		}
	}

	if (sol::optional<sol::table> towners = config["towners"]) {
		for (const auto &kv : *towners) {
			sol::table t = kv.second.as<sol::table>();
			TownerPositionOverride override;
			sol::optional<std::string> tName = t["name"];
			override.shortName = tName.value_or("");
			sol::optional<int> tx = t["x"];
			sol::optional<int> ty = t["y"];
			override.position = { tx.value_or(0), ty.value_or(0) };
			townConfig.townerOverrides.push_back(override);
		}
	}

	if (sol::optional<sol::table> triggerList = config["triggers"]) {
		for (const auto &kv : *triggerList) {
			sol::table t = kv.second.as<sol::table>();
			TownTrigger tr;
			sol::optional<int> tx = t["x"];
			sol::optional<int> ty = t["y"];
			tr.position = { tx.value_or(0), ty.value_or(0) };
			sol::optional<std::string> kindStr = t["kind"];
			const std::string kind = kindStr.value_or("nextlevel");
			if (kind == "townwarp") {
				tr.msg = WM_DIABTOWNWARP;
				sol::optional<int> lvl = t["level"];
				tr.level = lvl.value_or(0);
				sol::optional<std::string> warpStr = t["warp"];
				if (warpStr.has_value() && !warpStr->empty()) {
					std::optional<dungeon_type> gate = ParseWarpGateString(*warpStr);
					if (!gate.has_value()) {
						LogError("registerTown: unknown triggers[].warp '{}'", *warpStr);
						continue;
					}
					tr.warpGate = gate;
				}
			} else if (kind == "nextlevel") {
				tr.msg = WM_DIABNEXTLVL;
				tr.level = 0;
			} else {
				LogError("registerTown: unknown triggers[].kind '{}', expected nextlevel or townwarp", kind);
				continue;
			}
			townConfig.triggers.push_back(tr);
		}
	}

	if (sol::optional<sol::table> portals = config["portals"]) {
		size_t slot = 0;
		for (const auto &kv : *portals) {
			if (slot >= NumTownPortalSlots) {
				LogWarn("registerTown: portals list has more than {} entries; ignoring extras", NumTownPortalSlots);
				break;
			}
			sol::table t = kv.second.as<sol::table>();
			sol::optional<int> px = t["x"];
			sol::optional<int> py = t["y"];
			townConfig.portalPositions[slot] = { px.value_or(0), py.value_or(0) };
			++slot;
		}
	}

	std::string townIdStr(townId);
	GetTownRegistry().RegisterTown(townIdStr, townConfig);
	return townIdStr;
}

void LuaTravelToTown(std::string_view townId)
{
	std::string townIdStr(townId);

	if (!GetTownRegistry().HasTown(townIdStr)) {
		LogError("Town '{}' not registered", townId);
		return;
	}

	DestinationTownID = townIdStr;

	if (gbIsMultiplayer) {
		NetSendCmdTownTravel(0xFFFFFFFF, townIdStr.c_str());
		return;
	}

	QueueTownSwitch();
}

std::string LuaGetCurrentTown()
{
	return GetTownRegistry().GetCurrentTown();
}

bool LuaHasTown(std::string_view townId)
{
	return GetTownRegistry().HasTown(std::string(townId));
}

} // namespace

sol::table LuaTownsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "register", "(townId: string, config: table) -> string",
	    "Registers a new town from a config table. Returns town ID.\n"
	    "Optional assets: { dungeonCel, megaTile, min, specialCels, sol, palette, microTileLen } — paths for the tile gfx/palette.\n"
	    "  microTileLen: sub-tile count per mega-tile (16 for town.cel, 10 for L1/L2/L3/L6 CEL, 12 for L4). Default: 16.\n"
	    "  Omitted path keys keep the Tristram default. On load failure the engine falls back through Tristram's full path chain.\n"
	    "  Omitting palette uses levels\\towndata\\town.pal regardless of other assets.\n"
	    "Optional triggers: array of tables with x, y, kind (\"nextlevel\" or \"townwarp\").\n"
	    "For townwarp, set level (dungeon level) and warp (\"catacombs\", \"caves\", \"hell\", \"nest\", \"crypt\") for IsWarpOpen gating.\n"
	    "Optional portals: up to four { x, y } tables for town portal spell positions (defaults match Tristram).",
	    LuaRegisterTown);

	LuaSetDocFn(table, "travel", "(townId: string)",
	    "Travels to the specified town.",
	    LuaTravelToTown);

	LuaSetDocFn(table, "current", "() -> string",
	    "Returns the current town ID.",
	    LuaGetCurrentTown);

	LuaSetDocFn(table, "exists", "(townId: string) -> boolean",
	    "Checks if a town is registered.",
	    LuaHasTown);

	return table;
}

} // namespace devilution
