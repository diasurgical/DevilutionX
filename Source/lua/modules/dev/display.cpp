#ifdef _DEBUG
#include "lua/modules/dev/display.hpp"

#include <array>
#include <optional>
#include <string>

#include <sol/sol.hpp>

#include "debug.h"
#include "lighting.h"
#include "lua/metadoc.hpp"
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

std::string DebugCmdShowGrid(std::optional<bool> on)
{
	DebugGrid = on.value_or(!DebugGrid);
	return StrCat("Tile grid highlighting: ", DebugGrid ? "On" : "Off");
}

std::string DebugCmdVision(std::optional<bool> on)
{
	DebugVision = on.value_or(!DebugVision);
	return StrCat("Vision highlighting: ", DebugVision ? "On" : "Off");
}

std::string DebugCmdPath(std::optional<bool> on)
{
	DebugPath = on.value_or(!DebugPath);
	return StrCat("Path highlighting: ", DebugPath ? "On" : "Off");
}

std::string DebugCmdFullbright(std::optional<bool> on)
{
	ToggleLighting();
	return StrCat("Fullbright: ", DisableLighting ? "On" : "Off");
}

std::string DebugCmdShowTileData(std::optional<std::string_view> dataType)
{
	static const std::array<std::string_view, 23> DataTypes {
		"microTiles",
		"dPiece",
		"dTransVal",
		"dLight",
		"dPreLight",
		"dFlags",
		"dPlayer",
		"dMonster",
		"missiles",
		"dCorpse",
		"dObject",
		"dItem",
		"dSpecial",
		"coords",
		"cursorcoords",
		"objectindex",
		"solid",
		"transparent",
		"trap",
		"AutomapView",
		"dungeon",
		"pdungeon",
		"Protected",
	};
	if (!dataType.has_value()) {
		std::string result = "Valid values for the first argument:\nclear";
		for (const std::string_view &str : DataTypes)
			StrAppend(result, ", ", str);
		return result;
	}
	if (*dataType == "clear") {
		SetDebugGridTextType(DebugGridTextItem::None);
		return "Tile data cleared.";
	}
	bool found = false;
	int index = 0;
	for (const std::string_view &param : DataTypes) {
		index++;
		if (*dataType != param)
			continue;
		found = true;
		auto newGridText = static_cast<DebugGridTextItem>(index);
		if (newGridText == GetDebugGridTextType()) {
			SetDebugGridTextType(DebugGridTextItem::None);
			return "Tile data: Off";
		}
		SetDebugGridTextType(newGridText);
		break;
	}
	if (!found) {
		std::string result = "Invalid name! Valid names are:\nclear";
		for (const std::string_view &str : DataTypes)
			StrAppend(result, ", ", str);
		return result;
	}

	return "Tile data: On";
}

std::string DebugCmdScrollView(std::optional<bool> on)
{
	DebugScrollViewEnabled = on.value_or(!DebugScrollViewEnabled);
	if (!DebugScrollViewEnabled)
		InitMultiView();
	return StrCat("Scroll view: ", DebugScrollViewEnabled ? "On" : "Off");
}

std::string DebugCmdToggleFPS(std::optional<bool> on)
{
	frameflag = on.value_or(!frameflag);
	return StrCat("FPS counter: ", frameflag ? "On" : "Off");
}

} // namespace

sol::table LuaDevDisplayModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "fps", "(name: string = nil)", "Toggle FPS display.", &DebugCmdToggleFPS);
	LuaSetDocFn(table, "fullbright", "(on: boolean = nil)", "Toggle light shading.", &DebugCmdFullbright);
	LuaSetDocFn(table, "grid", "(on: boolean = nil)", "Toggle showing the grid.", &DebugCmdShowGrid);
	LuaSetDocFn(table, "path", "(on: boolean = nil)", "Toggle path debug rendering.", &DebugCmdPath);
	LuaSetDocFn(table, "scrollView", "(on: boolean = nil)", "Toggle view scrolling via Shift+Mouse.", &DebugCmdScrollView);
	LuaSetDocFn(table, "tileData", "(name: string = nil)", "Toggle showing tile data.", &DebugCmdShowTileData);
	LuaSetDocFn(table, "vision", "(on: boolean = nil)", "Toggle vision debug rendering.", &DebugCmdVision);
	return table;
}

} // namespace devilution
#endif // _DEBUG
