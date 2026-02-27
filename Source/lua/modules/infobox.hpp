#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include <sol/forward.hpp>

#include "DiabloUI/ui_flags.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution {

struct Item;

struct InfoBoxLineColors {
	std::vector<UiFlags> lineColors;
	std::vector<size_t> dividerBeforeLines;

	void clear()
	{
		lineColors.clear();
		dividerBeforeLines.clear();
	}

	bool hasCustomColors() const
	{
		return !lineColors.empty() || !dividerBeforeLines.empty();
	}

	UiFlags getLineColor(size_t lineIndex) const
	{
		if (lineIndex < lineColors.size()) {
			return lineColors[lineIndex];
		}
		return UiFlags::ColorWhite;
	}

	void setLineColor(size_t lineIndex, UiFlags color)
	{
		if (lineIndex >= lineColors.size()) {
			lineColors.resize(lineIndex + 1, UiFlags::ColorWhite);
		}
		lineColors[lineIndex] = color;
	}

	void addDividerBeforeLine(size_t lineIndex)
	{
		if (std::find(dividerBeforeLines.begin(), dividerBeforeLines.end(), lineIndex) == dividerBeforeLines.end()) {
			dividerBeforeLines.push_back(lineIndex);
		}
	}

	bool hasDividerBeforeLine(size_t lineIndex) const
	{
		return std::find(dividerBeforeLines.begin(), dividerBeforeLines.end(), lineIndex) != dividerBeforeLines.end();
	}
};

extern InfoBoxLineColors FloatingInfoBoxColors;
extern InfoBoxLineColors FloatingComparisonInfoBoxColors;
void PrepareInfoBoxColors(const Item *item, bool floatingBox);
void SetLastFloatingInfoBoxRect(const Rectangle &rect);
void ClearLastFloatingInfoBoxRect();
bool DrawInfoBoxTextWithColors(const Surface &out, std::string_view text, const Rectangle &rect, UiFlags flags, int spacing, int lineHeight);
sol::table LuaInfoBoxModule(sol::state_view &lua);

} // namespace devilution
