#include "lua/modules/render.hpp"

#include <sol/sol.hpp>

#include "DiabloUI/ui_flags.hpp"
#include "engine/dx.h"
#include "engine/render/text_render.hpp"
#include "lua/metadoc.hpp"
#include "utils/display.h"

namespace devilution {

sol::table LuaRenderModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "string", "(text: string, x: integer, y: integer)",
	    "Renders a string at the given coordinates",
	    [](std::string_view text, int x, int y) { DrawString(GlobalBackBuffer(), text, { x, y }); });
	LuaSetDocFn(table, "screen_width", "()",
	    "Returns the screen width", []() { return gnScreenWidth; });
	LuaSetDocFn(table, "screen_height", "()",
	    "Returns the screen height", []() { return gnScreenHeight; });

	auto uiFlags = lua.create_table();
	uiFlags["None"] = UiFlags::None;

	uiFlags["FontSize12"] = UiFlags::FontSize12;
	uiFlags["FontSize24"] = UiFlags::FontSize24;
	uiFlags["FontSize30"] = UiFlags::FontSize30;
	uiFlags["FontSize42"] = UiFlags::FontSize42;
	uiFlags["FontSize46"] = UiFlags::FontSize46;
	uiFlags["FontSizeDialog"] = UiFlags::FontSizeDialog;

	uiFlags["ColorUiGold"] = UiFlags::ColorUiGold;
	uiFlags["ColorUiSilver"] = UiFlags::ColorUiSilver;
	uiFlags["ColorUiGoldDark"] = UiFlags::ColorUiGoldDark;
	uiFlags["ColorUiSilverDark"] = UiFlags::ColorUiSilverDark;
	uiFlags["ColorDialogWhite"] = UiFlags::ColorDialogWhite;
	uiFlags["ColorDialogYellow"] = UiFlags::ColorDialogYellow;
	uiFlags["ColorDialogRed"] = UiFlags::ColorDialogRed;
	uiFlags["ColorYellow"] = UiFlags::ColorYellow;
	uiFlags["ColorGold"] = UiFlags::ColorGold;
	uiFlags["ColorBlack"] = UiFlags::ColorBlack;
	uiFlags["ColorWhite"] = UiFlags::ColorWhite;
	uiFlags["ColorWhitegold"] = UiFlags::ColorWhitegold;
	uiFlags["ColorRed"] = UiFlags::ColorRed;
	uiFlags["ColorBlue"] = UiFlags::ColorBlue;
	uiFlags["ColorOrange"] = UiFlags::ColorOrange;
	uiFlags["ColorButtonface"] = UiFlags::ColorButtonface;
	uiFlags["ColorButtonpushed"] = UiFlags::ColorButtonpushed;

	uiFlags["AlignCenter"] = UiFlags::AlignCenter;
	uiFlags["AlignRight"] = UiFlags::AlignRight;
	uiFlags["VerticalCenter"] = UiFlags::VerticalCenter;

	uiFlags["KerningFitSpacing"] = UiFlags::KerningFitSpacing;

	uiFlags["ElementDisabled"] = UiFlags::ElementDisabled;
	uiFlags["ElementHidden"] = UiFlags::ElementHidden;

	uiFlags["PentaCursor"] = UiFlags::PentaCursor;
	uiFlags["Outlined"] = UiFlags::Outlined;

	uiFlags["NeedsNextElement"] = UiFlags::NeedsNextElement;

	table["UiFlags"] = uiFlags;

	return table;
}

} // namespace devilution
