#include "lua/lua_event.hpp"

#include <optional>
#include <string_view>
#include <utility>

#include <sol/sol.hpp>

#include "lua/lua_global.hpp"
#include "monster.h"
#include "player.h"
#include "utils/log.hpp"

namespace devilution {

namespace lua {

template <typename... Args>
void CallLuaEvent(std::string_view name, Args &&...args)
{
	sol::table *events = GetLuaEvents();
	if (events == nullptr) {
		return;
	}

	const auto trigger = events->traverse_get<std::optional<sol::object>>(name, "trigger");
	if (!trigger.has_value() || !trigger->is<sol::protected_function>()) {
		LogError("events.{}.trigger is not a function", name);
		return;
	}
	const sol::protected_function fn = trigger->as<sol::protected_function>();
	SafeCallResult(fn(std::forward<Args>(args)...), /*optional=*/true);
}

template <typename T, typename... Args>
T CallLuaEventReturn(T defaultValue, std::string_view name, Args &&...args)
{
	sol::table *events = GetLuaEvents();
	if (events == nullptr) {
		return defaultValue;
	}

	const auto trigger = events->traverse_get<std::optional<sol::object>>(name, "trigger");
	if (!trigger.has_value() || !trigger->is<sol::protected_function>()) {
		return defaultValue;
	}
	const sol::protected_function fn = trigger->as<sol::protected_function>();
	sol::object result = SafeCallResult(fn(std::forward<Args>(args)...), /*optional=*/true);
	if (result.is<T>()) {
		return result.as<T>();
	}
	return defaultValue;
}

void MonsterDataLoaded()
{
	CallLuaEvent("MonsterDataLoaded");
}
void UniqueMonsterDataLoaded()
{
	CallLuaEvent("UniqueMonsterDataLoaded");
}
void ItemDataLoaded()
{
	CallLuaEvent("ItemDataLoaded");
}
void UniqueItemDataLoaded()
{
	CallLuaEvent("UniqueItemDataLoaded");
}

void StoreOpened(std::string_view name)
{
	CallLuaEvent("StoreOpened", name);
}

void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType)
{
	CallLuaEvent("OnMonsterTakeDamage", monster, damage, damageType);
}

void OnPlayerGainExperience(const Player *player, uint32_t exp)
{
	CallLuaEvent("OnPlayerGainExperience", player, exp);
}
void OnPlayerTakeDamage(const Player *player, int damage, int damageType)
{
	CallLuaEvent("OnPlayerTakeDamage", player, damage, damageType);
}

void LoadModsComplete()
{
	CallLuaEvent("LoadModsComplete");
}
void GameDrawComplete()
{
	CallLuaEvent("GameDrawComplete");
}
void GameStart()
{
	CallLuaEvent("GameStart");
}

} // namespace lua

} // namespace devilution
