#pragma once

#include <string_view>

#include <expected.hpp>
#include <sol/forward.hpp>

namespace devilution {

void LuaInitialize();
void LuaReloadActiveMods();
void LuaShutdown();
void LuaEvent(std::string_view name);
sol::state &GetLuaState();
sol::table &GetLuaEvents();
sol::environment CreateLuaSandbox();
sol::object SafeCallResult(sol::protected_function_result result, bool optional);

} // namespace devilution
