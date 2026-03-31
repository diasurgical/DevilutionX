#pragma once

#include <sol/sol.hpp>

namespace devilution {

/**
 * @brief Creates and returns the Lua towns module
 *
 * Exposes town registration and travel functionality to Lua mods.
 *
 * @param lua The Sol2 state view
 * @return sol::table The towns module table
 */
sol::table LuaTownsModule(sol::state_view &lua);

} // namespace devilution
