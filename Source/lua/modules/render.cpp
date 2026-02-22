#include "lua/modules/render.hpp"

#include <sol/sol.hpp>

#include "DiabloUI/ui_flags.hpp"
#include "engine/dx.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "lua/metadoc.hpp"
#include "utils/display.h"
#include "utils/str_split.hpp"

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
	LuaSetDocFn(table, "drawHalfTransparentRect", "(x: integer, y: integer, width: integer, height: integer, passes: integer = 1)",
	    "Draws a half-transparent rectangle on the back buffer.",
	    [](int x, int y, int width, int height, sol::optional<int> passes) {
		    const int passCount = std::max(1, passes.value_or(1));
		    for (int i = 0; i < passCount; ++i)
			    DrawHalfTransparentRectTo(GlobalBackBuffer(), x, y, width, height);
	    });
	LuaSetDocFn(table, "drawHalfTransparentHLine", "(x: integer, y: integer, width: integer, colorIndex: integer)",
	    "Draws a half-transparent horizontal line on the back buffer.",
	    [](int x, int y, int width, int colorIndex) {
		    DrawHalfTransparentHorizontalLine(GlobalBackBuffer(), { x, y }, width, static_cast<uint8_t>(colorIndex));
	    });
	LuaSetDocFn(table, "drawHalfTransparentVLine", "(x: integer, y: integer, height: integer, colorIndex: integer)",
	    "Draws a half-transparent vertical line on the back buffer.",
	    [](int x, int y, int height, int colorIndex) {
		    DrawHalfTransparentVerticalLine(GlobalBackBuffer(), { x, y }, height, static_cast<uint8_t>(colorIndex));
	    });
	LuaSetDocFn(table, "measureTextBlock", "(text: string, spacing: integer = 2, lineHeight: integer = 15) -> table",
	    "Measures multiline text block and returns { width, height, lineCount }.",
	    [&lua](std::string_view text, sol::optional<int> spacing, sol::optional<int> lineHeight) {
		    const int textSpacing = spacing.value_or(2);
		    const int textLineHeight = lineHeight.value_or(15);
		    auto lines = SplitByChar(text, '\n');
		    int maxWidth = 0;
		    for (const std::string_view line : lines) {
			    maxWidth = std::max(maxWidth, GetLineWidth(line, GameFont12, textSpacing, nullptr));
		    }
		    const int lineCount = std::max(1, 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n')));
		    sol::table result = lua.create_table();
		    result["width"] = maxWidth;
		    result["height"] = lineCount * textLineHeight;
		    result["lineCount"] = lineCount;
		    return result;
	    });

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
