#pragma once
#ifdef _DEBUG
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <sol/forward.hpp>

namespace devilution {

struct LuaAutocompleteSuggestion {
	std::string displayText;
	std::string completionText;
	int cursorAdjust = 0;

	bool operator==(const LuaAutocompleteSuggestion &other) const
	{
		return displayText == other.displayText;
	}

	bool operator<(const LuaAutocompleteSuggestion &other) const
	{
		return displayText < other.displayText;
	}
};

void GetLuaAutocompleteSuggestions(
    std::string_view text, size_t cursorPos, const sol::environment &lua,
    size_t maxSuggestions, std::vector<LuaAutocompleteSuggestion> &out);

} // namespace devilution

namespace std {
template <>
struct hash<devilution::LuaAutocompleteSuggestion> {
	size_t operator()(const devilution::LuaAutocompleteSuggestion &suggestion) const
	{
		return hash<std::string>()(suggestion.displayText);
	}
};
} // namespace std

#endif // _DEBUG
