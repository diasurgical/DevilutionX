#pragma once

#include <string_view>

namespace devilution {

/**
 * @brief Triggers a Lua event by name.
 * This is a minimal header for code that only needs to trigger events.
 */
void LuaEvent(std::string_view name);
void LuaEvent(std::string_view name, std::string_view arg);

} // namespace devilution
