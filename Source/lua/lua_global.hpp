#pragma once

#include <string_view>

#include <expected.hpp>
#include <function_ref.hpp>
#include <sol/forward.hpp>

namespace devilution {

void LuaInitialize();
void LuaReloadActiveMods();
void LuaShutdown();
void LuaEvent(std::string_view name);
sol::state &GetLuaState();
sol::environment CreateLuaSandbox();
sol::object SafeCallResult(sol::protected_function_result result, bool optional);

/** Adds a handler to be called when mods status changes after the initial startup. */
void AddModsChangedHandler(tl::function_ref<void()> callback);

} // namespace devilution
