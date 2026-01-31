#pragma once

#include <string_view>

#include <expected.hpp>
#include <function_ref.hpp>
#include <sol/forward.hpp>

namespace devilution {

struct Player;
struct Monster;

void LuaInitialize();
void LuaReloadActiveMods();
void LuaShutdown();
void LuaEvent(std::string_view name);
void LuaEvent(std::string_view name, std::string_view arg);
void LuaEvent(std::string_view name, const Player *player, int arg1, int arg2);
void LuaEvent(std::string_view name, const Monster *monster, int arg1, int arg2);
void LuaEvent(std::string_view name, const Player *player, uint32_t arg1);
sol::state &GetLuaState();
sol::environment CreateLuaSandbox();
sol::object SafeCallResult(sol::protected_function_result result, bool optional);

/** Adds a handler to be called when mods status changes after the initial startup. */
void AddModsChangedHandler(tl::function_ref<void()> callback);

} // namespace devilution
