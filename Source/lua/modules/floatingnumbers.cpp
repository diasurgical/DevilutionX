#include "lua/modules/floatingnumbers.hpp"

#include <sol/sol.hpp>

#include "DiabloUI/ui_flags.hpp"
#include "engine/point.hpp"
#include "lua/metadoc.hpp"
#include "qol/floatingnumbers.h"

namespace devilution {

sol::table LuaFloatingNumbersModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	LuaSetDocFn(table, "add", "(text: string, pos: Point, style: UiFlags, id: integer = 0, reverseDirection: boolean = false)",
	    "Add a floating number",
	    [](const std::string &text, Point pos, UiFlags style, std::optional<int> id, std::optional<bool> reverseDirection) {
		    AddFloatingNumber(pos, { 0, 0 }, text, style, id.value_or(0), reverseDirection.value_or(false));
	    });

	auto flags = lua.create_table();
	flags["DarkRed"] = UiFlags::ColorUiSilver;
	flags["Yellow"] = UiFlags::ColorYellow;
	flags["Gold"] = UiFlags::ColorGold;
	flags["Black"] = UiFlags::ColorBlack;
	flags["White"] = UiFlags::ColorWhite;
	flags["WhiteGold"] = UiFlags::ColorWhitegold;
	flags["Red"] = UiFlags::ColorRed;
	flags["Blue"] = UiFlags::ColorBlue;
	flags["Orange"] = UiFlags::ColorOrange;

	flags["Small"] = UiFlags::FontSize12;
	flags["Medium"] = UiFlags::FontSize24;
	flags["Large"] = UiFlags::FontSize30;

	table["Flags"] = flags;

	return table;
}

} // namespace devilution
