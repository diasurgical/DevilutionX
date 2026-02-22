#include "lua/modules/infobox.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include <SDL.h>

#include <fmt/format.h>
#include <sol/sol.hpp>

#include "control/control.hpp"
#include "engine/dx.h"
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "items.h"
#include "lua/lua_global.hpp"
#include "lua/metadoc.hpp"
#include "options.h"
#include "player.h"
#include "tables/itemdat.h"
#include "utils/format_int.hpp"
#include "utils/str_split.hpp"

namespace devilution {

InfoBoxLineColors FloatingInfoBoxColors;
InfoBoxLineColors FloatingComparisonInfoBoxColors;

namespace {

bool HasInfoBoxPrepareHandlersKnown = false;
bool HasInfoBoxPrepareHandlers = false;
const Item *CurrentInfoBoxItemForRender = nullptr;
std::optional<Rectangle> LastFloatingInfoBoxRect;

enum class InfoBoxTarget : uint8_t {
	Floating,
	Info,
};

InfoBoxTarget ActiveInfoBoxTarget = InfoBoxTarget::Floating;

void InvalidateInfoBoxHooks()
{
	HasInfoBoxPrepareHandlersKnown = false;
	HasInfoBoxPrepareHandlers = false;
}

bool InfoBoxPrepareHandlersActive()
{
	if (!HasInfoBoxPrepareHandlersKnown) {
		HasInfoBoxPrepareHandlers = LuaEventHasHandlers("InfoBoxPrepare");
		HasInfoBoxPrepareHandlersKnown = true;
	}
	return HasInfoBoxPrepareHandlers;
}

[[maybe_unused]] const bool ModChangedHandler = (AddModsChangedHandler(InvalidateInfoBoxHooks), true);

StringOrView &GetActiveInfoBoxString()
{
	if (ActiveInfoBoxTarget == InfoBoxTarget::Floating)
		return FloatingInfoString;
	return InfoString;
}

std::string GetInfoBoxText()
{
	return std::string(GetActiveInfoBoxString());
}

void SetInfoBoxText(std::string_view text)
{
	GetActiveInfoBoxString() = std::string(text);
}

void AppendUniquePowersToActiveInfoBox(const Item &item)
{
	std::string text = std::string(GetActiveInfoBoxString());

	if (item._iMagical != ITEM_QUALITY_UNIQUE)
		return;
	if (item._iUid < 0 || static_cast<size_t>(item._iUid) >= UniqueItems.size())
		return;

	auto lines = SplitByChar(text, '\n');
	std::vector<std::string> outLines;
	outLines.reserve(16);

	const UniqueItem &uitem = UniqueItems[item._iUid];
	bool inserted = false;
	for (const std::string_view line : lines) {
		if (!inserted && line == _("unique item")) {
			outLines.emplace_back(line);
			for (const auto &power : uitem.powers) {
				if (power.type == IPL_INVALID)
					break;
				outLines.push_back(std::string(PrintItemPower(power.type, item)));
			}
			inserted = true;
		} else {
			outLines.emplace_back(line);
		}
	}

	if (!inserted)
		return;

	std::string rebuilt;
	for (size_t i = 0; i < outLines.size(); i++) {
		if (i != 0)
			rebuilt.push_back('\n');
		rebuilt += outLines[i];
	}
	GetActiveInfoBoxString() = std::move(rebuilt);
}

bool IsRequirementLine(std::string_view line)
{
	return line.starts_with(_("Required:"));
}

bool HasUnmetRequirement(const Item &item)
{
	const Player &myPlayer = *MyPlayer;
	return item._iMinStr > myPlayer._pStrength || item._iMinDex > myPlayer._pDexterity || item._iMinMag > myPlayer._pMagic;
}

void DrawRequirementLineInline(const Surface &out, const Rectangle &lineRect, UiFlags lineColor, UiFlags flags, int spacing, const Item &item)
{
	const Player &myPlayer = *MyPlayer;

	std::array<std::string, 4> segments;
	std::array<UiFlags, 4> colors;
	size_t count = 0;

	segments[count] = _("Required:");
	colors[count] = lineColor;
	count++;

	if (item._iMinStr != 0) {
		segments[count] = fmt::format(fmt::runtime(_(" {:d} Str")), item._iMinStr);
		colors[count] = item._iMinStr > myPlayer._pStrength ? UiFlags::ColorRed : lineColor;
		count++;
	}
	if (item._iMinMag != 0) {
		segments[count] = fmt::format(fmt::runtime(_(" {:d} Mag")), item._iMinMag);
		colors[count] = item._iMinMag > myPlayer._pMagic ? UiFlags::ColorRed : lineColor;
		count++;
	}
	if (item._iMinDex != 0) {
		segments[count] = fmt::format(fmt::runtime(_(" {:d} Dex")), item._iMinDex);
		colors[count] = item._iMinDex > myPlayer._pDexterity ? UiFlags::ColorRed : lineColor;
		count++;
	}

	std::vector<DrawStringFormatArg> args;
	args.reserve(count);
	std::string formatString;
	formatString.reserve(count * 2);
	for (size_t i = 0; i < count; i++) {
		formatString += "{}";
		args.emplace_back(segments[i], colors[i]);
	}

	DrawStringWithColors(out, formatString, args.data(), count, lineRect,
	    {
	        .flags = flags,
	        .spacing = spacing,
	    });
}

bool DrawInfoBoxTextWithColorSet(const Surface &out, std::string_view text, const Rectangle &rect, UiFlags flags, int spacing, int lineHeight, const InfoBoxLineColors &lineColors, const Item *itemForRequirementColoring)
{
	if (!lineColors.hasCustomColors())
		return false;

	const std::string txt(text);
	auto lines = SplitByChar(txt, '\n');

	Point linePos = rect.position;
	if ((flags & UiFlags::VerticalCenter) != UiFlags::None) {
		const int lineCount = 1 + static_cast<int>(std::count(text.begin(), text.end(), '\n'));
		const int totalHeight = lineCount * lineHeight;
		linePos.y += (rect.size.height - totalHeight) / 2;
		flags &= ~UiFlags::VerticalCenter;
	}

	size_t lineIndex = 0;
	for (const std::string_view &line : lines) {
		if (lineColors.hasDividerBeforeLine(lineIndex) && lineIndex > 0) {
			DrawHalfTransparentHorizontalLine(out, { linePos.x, linePos.y - (lineHeight / 2) }, rect.size.width, PAL16_GRAY + 10);
		}

		const UiFlags lineColor = lineColors.getLineColor(lineIndex);
		const Rectangle lineRect { linePos, rect.size };

		if (itemForRequirementColoring != nullptr && IsRequirementLine(line) && HasUnmetRequirement(*itemForRequirementColoring)) {
			DrawRequirementLineInline(out, lineRect, lineColor, flags, spacing, *itemForRequirementColoring);
		} else {
			DrawString(out, line, lineRect,
			    {
			        .flags = lineColor | flags,
			        .spacing = spacing,
			    });
		}

		linePos.y += lineHeight;
		lineIndex++;
	}

	return true;
}

std::string GetInfoBoxTextForItem(const Item &item)
{
	const std::string savedInfo = std::string(InfoString);
	const std::string savedFloatingInfo = std::string(FloatingInfoString);
	const UiFlags savedInfoColor = InfoColor;

	InfoString = StringOrView {};
	FloatingInfoString = StringOrView {};

	if (item._itype == ItemType::Gold) {
		const int nGold = item._ivalue;
		InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
		FloatingInfoString = std::string(InfoString);
	} else {
		InfoColor = item.getTextColor();
		InfoString = item.getName();
		FloatingInfoString = item.getName();
		if (item._iIdentified) {
			PrintItemDetails(item);
		} else {
			PrintItemDur(item);
		}
	}

	const std::string floating = std::string(FloatingInfoString);
	const std::string info = std::string(InfoString);
	const std::string result = floating.size() >= info.size() ? floating : info;

	InfoString = savedInfo;
	FloatingInfoString = savedFloatingInfo;
	InfoColor = savedInfoColor;

	return result;
}

bool DrawInfoBoxTextWithCustomColors(const Surface &out, std::string_view text, const Rectangle &rect, UiFlags flags, int spacing, int lineHeight, const InfoBoxLineColors &lineColors, const Item *itemForRequirementColoring)
{
	return DrawInfoBoxTextWithColorSet(out, text, rect, flags, spacing, lineHeight, lineColors, itemForRequirementColoring);
}

} // namespace

void PrepareInfoBoxColors(const Item *item, bool floatingBox)
{
	const InfoBoxTarget prevTarget = ActiveInfoBoxTarget;
	ActiveInfoBoxTarget = floatingBox ? InfoBoxTarget::Floating : InfoBoxTarget::Info;
	CurrentInfoBoxItemForRender = item;

	FloatingInfoBoxColors.clear();

	const bool hasInfoBoxPrepareHandlers = InfoBoxPrepareHandlersActive();
	if (!hasInfoBoxPrepareHandlers) {
		ActiveInfoBoxTarget = prevTarget;
		return;
	}

	LuaEvent("InfoBoxPrepare", item, floatingBox);

	ActiveInfoBoxTarget = prevTarget;
}

void SetLastFloatingInfoBoxRect(const Rectangle &rect)
{
	LastFloatingInfoBoxRect = rect;
}

void ClearLastFloatingInfoBoxRect()
{
	LastFloatingInfoBoxRect.reset();
}

bool DrawInfoBoxTextWithColors(const Surface &out, std::string_view text, const Rectangle &rect, UiFlags flags, int spacing, int lineHeight)
{
	return DrawInfoBoxTextWithColorSet(out, text, rect, flags, spacing, lineHeight, FloatingInfoBoxColors, CurrentInfoBoxItemForRender);
}

namespace {

bool IsFloatingInfoBoxEnabled()
{
	return *GetOptions().Gameplay.floatingInfoBox;
}

bool IsShiftHeld()
{
	return (SDL_GetModState() & KMOD_SHIFT) != 0;
}

void SetShowUniqueItemInfoBox(bool enabled)
{
	ShowUniqueItemInfoBox = enabled;
}

bool IsShowUniqueItemInfoBoxEnabled()
{
	return ShowUniqueItemInfoBox;
}

} // namespace

sol::table LuaInfoBoxModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	sol::usertype<InfoBoxLineColors> infoBoxColorsType = lua.new_usertype<InfoBoxLineColors>(
	    "InfoBoxLineColors",
	    sol::no_constructor);

	LuaSetDocFn(infoBoxColorsType, "clear", "()",
	    "Clears all custom line colors",
	    &InfoBoxLineColors::clear);

	LuaSetDocFn(infoBoxColorsType, "hasCustomColors", "() -> boolean",
	    "Checks if any custom colors have been set",
	    &InfoBoxLineColors::hasCustomColors);

	LuaSetDocFn(infoBoxColorsType, "getLineColor", "(lineIndex: number) -> number",
	    "Gets the color flag for a specific line (defaults to white if not set)",
	    &InfoBoxLineColors::getLineColor);

	LuaSetDocFn(infoBoxColorsType, "setLineColor", "(lineIndex: number, color: number) -> void",
	    "Sets the color flag for a specific line (0-indexed)",
	    &InfoBoxLineColors::setLineColor);

	LuaSetDocFn(infoBoxColorsType, "addDividerBeforeLine", "(lineIndex: number) -> void",
	    "Adds a horizontal divider before the specified line index (0-indexed)",
	    &InfoBoxLineColors::addDividerBeforeLine);

	table["lineColors"] = &FloatingInfoBoxColors;
	table["secondaryLineColors"] = &FloatingComparisonInfoBoxColors;
	LuaSetDocFn(table, "getInfoText", "() -> string",
	    "Returns the current info box text.",
	    &GetInfoBoxText);

	LuaSetDocFn(table, "setInfoText", "(text: string) -> void",
	    "Replaces the current info box text.",
	    [](std::string_view text) { SetInfoBoxText(text); });

	LuaSetDocFn(table, "getFloatingRect", "() -> table | nil",
	    "Returns the current floating info box rectangle as {x, y, width, height}, or nil.",
	    [&lua]() -> sol::object {
		    if (!LastFloatingInfoBoxRect.has_value())
			    return sol::make_object(lua, sol::nil);
		    const Rectangle &rect = *LastFloatingInfoBoxRect;
		    sol::table t = lua.create_table();
		    t["x"] = rect.position.x;
		    t["y"] = rect.position.y;
		    t["width"] = rect.size.width;
		    t["height"] = rect.size.height;
		    return sol::make_object(lua, t);
	    });

	LuaSetDocFn(table, "isFloatingInfoBoxEnabled", "() -> boolean",
	    "Returns whether gameplay floating info box is enabled.",
	    &IsFloatingInfoBoxEnabled);

	LuaSetDocFn(table, "isShiftHeld", "() -> boolean",
	    "Returns whether Shift key is currently held.",
	    &IsShiftHeld);

	LuaSetDocFn(table, "setShowUniqueItemInfoBox", "(enabled: boolean) -> void",
	    "Sets whether the unique item info box should be shown this frame.",
	    &SetShowUniqueItemInfoBox);

	LuaSetDocFn(table, "isShowUniqueItemInfoBox", "() -> boolean",
	    "Returns whether the unique item info box is currently marked visible.",
	    &IsShowUniqueItemInfoBoxEnabled);

	LuaSetDocFn(table, "appendUniquePowers", "(item: Item) -> void",
	    "Appends unique powers to the active info box text after the unique marker line.",
	    [](const Item &item) { AppendUniquePowersToActiveInfoBox(item); });

	LuaSetDocFn(table, "getInfoTextForItem", "(item: Item) -> string",
	    "Returns generated info box text for the provided item.",
	    &GetInfoBoxTextForItem);

	LuaSetDocFn(table, "drawTextWithColors", "(text: string, x: integer, y: integer, width: integer, height: integer, flags: integer, spacing: integer, lineHeight: integer, colors: InfoBoxLineColors, item: Item | nil = nil) -> boolean",
	    "Draws text using the provided InfoBoxLineColors in the given rectangle.",
	    [](std::string_view text, int x, int y, int width, int height, int flags, int spacing, int lineHeight, const InfoBoxLineColors &colors, const sol::object &itemObj) {
		    const Item *itemForRequirementColoring = nullptr;
		    if (itemObj.valid() && itemObj.get_type() != sol::type::lua_nil) {
			    if (itemObj.is<Item>()) {
				    const Item &item = itemObj.as<const Item &>();
				    itemForRequirementColoring = &item;
			    }
		    }
		    return DrawInfoBoxTextWithCustomColors(GlobalBackBuffer(), text, { { x, y }, { width, height } }, static_cast<UiFlags>(flags), spacing, lineHeight, colors, itemForRequirementColoring);
	    });

	return table;
}

} // namespace devilution
