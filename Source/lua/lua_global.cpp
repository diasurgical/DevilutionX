#include "lua/lua_global.hpp"

#include <optional>
#include <string_view>

#include <ankerl/unordered_dense.h>
#include <sol/debug.hpp>
#include <sol/sol.hpp>

#include <config.h>

#include "appfat.h"
#include "effects.h"
#include "engine/assets.hpp"
#include "lua/modules/audio.hpp"
#include "lua/modules/floatingnumbers.hpp"
#include "lua/modules/hellfire.hpp"
#include "lua/modules/i18n.hpp"
#include "lua/modules/items.hpp"
#include "lua/modules/log.hpp"
#include "lua/modules/monsters.hpp"
#include "lua/modules/player.hpp"
#include "lua/modules/render.hpp"
#include "lua/modules/system.hpp"
#include "lua/modules/towners.hpp"
#include "monster.h"
#include "options.h"
#include "player.h"
#include "plrmsg.h"
#include "utils/console.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifdef _DEBUG
#include "lua/modules/dev.hpp"
#include "lua/repl.hpp"
#endif

namespace devilution {

namespace {

struct LuaState {
	sol::state sol = {};
	sol::table commonPackages = {};
	ankerl::unordered_dense::segmented_map<std::string, sol::bytecode> compiledScripts = {};
	sol::environment sandbox = {};
	sol::table events = {};
};

std::optional<LuaState> CurrentLuaState;

std::vector<tl::function_ref<void()>> IsModChangeHandlers;

// A Lua function that we use to generate a `require` implementation.
constexpr std::string_view RequireGenSrc = R"lua(
function requireGen(env, loaded, loadFn)
  return function(packageName)
      local p = loaded[packageName]
      if p == nil then
          local loader = loadFn(packageName)
          if type(loader) == "string" then
            error(loader)
          end
          setEnvironment(env, loader)
          p = loader(packageName)
          loaded[packageName] = p
      end
      return p
  end
end
)lua";

sol::object LuaLoadScriptFromAssets(std::string_view packageName)
{
	LuaState &luaState = *CurrentLuaState;
	constexpr std::string_view PathPrefix = "lua\\";
	constexpr std::string_view PathSuffix = ".lua";
	std::string path;
	path.reserve(PathPrefix.size() + packageName.size() + PathSuffix.size());
	StrAppend(path, PathPrefix, packageName, PathSuffix);
	std::replace(path.begin() + PathPrefix.size(), path.end() - PathSuffix.size(), '.', '\\');

	auto iter = luaState.compiledScripts.find(path);
	if (iter != luaState.compiledScripts.end()) {
		return luaState.sol.load(iter->second.as_string_view(), path, sol::load_mode::binary);
	}

	tl::expected<AssetData, std::string> assetData = LoadAsset(path);
	if (!assetData.has_value()) {
		sol::stack::push(luaState.sol.lua_state(), assetData.error());
		return sol::stack_object(luaState.sol.lua_state(), -1);
	}
	const sol::load_result result = luaState.sol.load(std::string_view(*assetData), path, sol::load_mode::text);
	if (!result.valid()) {
		sol::stack::push(luaState.sol.lua_state(),
		    StrCat("Lua error when loading ", path, ": ", result.get<std::string>()));
		return sol::stack_object(luaState.sol.lua_state(), -1);
	}
	const sol::function fn = result;
	luaState.compiledScripts[path] = fn.dump();
	return result;
}

int LuaPrint(lua_State *state)
{
	const int n = lua_gettop(state);
	for (int i = 1; i <= n; i++) {
		size_t l;
		const char *s = luaL_tolstring(state, i, &l);
		if (i > 1)
			printInConsole("\t");
		printInConsole(std::string_view(s, l));
		lua_pop(state, 1);
	}
	printNewlineInConsole();
	return 0;
}

void LuaWarn(void *userData, const char *message, int continued)
{
	static std::string warnBuffer;
	warnBuffer.append(message);
	if (continued != 0)
		return;
	LogWarn("{}", warnBuffer);
	warnBuffer.clear();
}

sol::object RunScript(std::optional<sol::environment> env, std::string_view packageName, bool optional)
{
	const sol::object result = LuaLoadScriptFromAssets(packageName);
	// We return a string on error:
	if (result.get_type() == sol::type::string) {
		if (!optional)
			app_fatal(result.as<std::string>());
		LogInfo("{}", result.as<std::string>());
		return sol::lua_nil;
	}
	auto fn = result.as<sol::protected_function>();
	if (env.has_value()) {
		sol::set_environment(*env, fn);
	}
	return SafeCallResult(fn(), optional);
}

void LuaPanic(sol::optional<std::string> message)
{
	LogError("Lua is in a panic state and will now abort() the application:\n{}",
	    message.value_or("unknown error"));
}

} // namespace

void Sol2DebugPrintStack(lua_State *state)
{
	LogDebug("{}", sol::detail::debug::dump_types(state));
}

void Sol2DebugPrintSection(const std::string &message, lua_State *state)
{
	LogDebug("-- {} -- [ {} ]", message, sol::detail::debug::dump_types(state));
}

sol::environment CreateLuaSandbox()
{
	sol::state &lua = CurrentLuaState->sol;
	sol::environment sandbox(CurrentLuaState->sol, sol::create);

	// Registering globals
	sandbox.set(
	    "print", LuaPrint,
	    "_DEBUG",
#ifdef _DEBUG
	    true,
#else
	    false,
#endif
	    "_VERSION", LUA_VERSION);

	// Register safe built-in globals.
	for (const std::string_view global : {
	         // Built-ins:
	         "assert", "warn", "error", "ipairs", "next", "pairs", "pcall",
	         "select", "tonumber", "tostring", "type", "xpcall",
	         "rawequal", "rawget", "rawset", "setmetatable",
	// Built-in packages:
#ifdef _DEBUG
	         "debug",
#endif
	         "base", "coroutine", "table", "string", "math", "utf8" }) {
		const sol::object obj = lua[global];
		if (obj.get_type() == sol::type::lua_nil) {
			app_fatal(StrCat("Missing Lua global [", global, "]"));
		}
		sandbox[global] = obj;
	}

	// We only allow datetime-related functions from `os`:
	const sol::table os = lua["os"];
	sandbox.create_named("os",
	    "date", os["date"],
	    "difftime", os["difftime"],
	    "time", os["time"]);

	sandbox["require"] = lua["requireGen"](sandbox, CurrentLuaState->commonPackages, LuaLoadScriptFromAssets);

	// Expose commonly used enums globally for mods
	sandbox["SfxID"] = lua["SfxID"];

	return sandbox;
}

void AddModsChangedHandler(tl::function_ref<void()> callback)
{
	IsModChangeHandlers.push_back(callback);
}

void LuaReloadActiveMods()
{
	// Loaded without a sandbox.
	CurrentLuaState->events = RunScript(/*env=*/std::nullopt, "devilutionx.events", /*optional=*/false);
	CurrentLuaState->commonPackages["devilutionx.events"] = CurrentLuaState->events;

	gbIsHellfire = false;
	UnloadModArchives();

	std::vector<std::string_view> modnames = GetOptions().Mods.GetActiveModList();
	LoadModArchives(modnames);

	for (const std::string_view modname : modnames) {
		const std::string packageName = StrCat("mods.", modname, ".init");
		RunScript(CreateLuaSandbox(), packageName, /*optional=*/true);
	}

	for (const tl::function_ref<void()> handler : IsModChangeHandlers) {
		handler();
	}

	// Reload sound effects in case a mod archive overrides effects.tsv
	effects_cleanup_sfx();
	if (gbRunGame)
		sound_init();
	else
		ui_sound_init();

	// Reload game data (this can probably be done later in the process to avoid having to reload it)
	LoadTextData();
	LoadPlayerDataFiles();
	LoadSpellData();
	LoadMissileData();
	LoadMonsterData();
	LoadItemData();
	LoadObjectData();
	LoadQuestData();

	LuaEvent("LoadModsComplete");
}

void LuaInitialize()
{
	CurrentLuaState.emplace(LuaState { .sol = { sol::c_call<decltype(&LuaPanic), &LuaPanic> } });
	sol::state &lua = CurrentLuaState->sol;
	lua_setwarnf(lua.lua_state(), LuaWarn, /*ud=*/nullptr);
	lua.open_libraries(
	    sol::lib::base,
	    sol::lib::coroutine,
	    sol::lib::debug,
	    sol::lib::math,
	    sol::lib::os,
	    sol::lib::package,
	    sol::lib::string,
	    sol::lib::table,
	    sol::lib::utf8);

	// Registering devilutionx object table
	SafeCallResult(lua.safe_script(RequireGenSrc), /*optional=*/false);

	CurrentLuaState->commonPackages = lua.create_table_with(
#ifdef _DEBUG
	    "devilutionx.dev", LuaDevModule(lua),
#endif
	    "devilutionx.version", PROJECT_VERSION,
	    "devilutionx.i18n", LuaI18nModule(lua),
	    "devilutionx.items", LuaItemModule(lua),
	    "devilutionx.log", LuaLogModule(lua),
	    "devilutionx.audio", LuaAudioModule(lua),
	    "devilutionx.monsters", LuaMonstersModule(lua),
	    "devilutionx.player", LuaPlayerModule(lua),
	    "devilutionx.render", LuaRenderModule(lua),
	    "devilutionx.towners", LuaTownersModule(lua),
	    "devilutionx.hellfire", LuaHellfireModule(lua),
	    "devilutionx.system", LuaSystemModule(lua),
	    "devilutionx.floatingnumbers", LuaFloatingNumbersModule(lua),
	    "devilutionx.message", [](std::string_view text) { EventPlrMsg(text, UiFlags::ColorRed); },
	    // This package is loaded without a sandbox:
	    "inspect", RunScript(/*env=*/std::nullopt, "inspect", /*optional=*/false));

	// Used by the custom require implementation.
	lua["setEnvironment"] = [](const sol::environment &env, const sol::function &fn) { sol::set_environment(env, fn); };

	for (OptionEntryBase *mod : GetOptions().Mods.GetEntries()) {
		mod->SetValueChangedCallback(LuaReloadActiveMods);
	}

	LuaReloadActiveMods();
}

void LuaShutdown()
{
#ifdef _DEBUG
	LuaReplShutdown();
#endif
	CurrentLuaState = std::nullopt;
}

template <typename... Args>
void CallLuaEvent(std::string_view name, Args &&...args)
{
	if (!CurrentLuaState.has_value()) {
		return;
	}

	const auto trigger = CurrentLuaState->events.traverse_get<std::optional<sol::object>>(name, "trigger");
	if (!trigger.has_value() || !trigger->is<sol::protected_function>()) {
		LogError("events.{}.trigger is not a function", name);
		return;
	}
	const sol::protected_function fn = trigger->as<sol::protected_function>();
	SafeCallResult(fn(std::forward<Args>(args)...), /*optional=*/true);
}

void LuaEvent(std::string_view name)
{
	CallLuaEvent(name);
}

void LuaEvent(std::string_view name, std::string_view arg)
{
	CallLuaEvent(name, arg);
}

void LuaEvent(std::string_view name, const Player *player, int arg1, int arg2)
{
	CallLuaEvent(name, player, arg1, arg2);
}

void LuaEvent(std::string_view name, const Monster *monster, int arg1, int arg2)
{
	CallLuaEvent(name, monster, arg1, arg2);
}

void LuaEvent(std::string_view name, const Player *player, uint32_t arg1)
{
	CallLuaEvent(name, player, arg1);
}

sol::state &GetLuaState()
{
	return CurrentLuaState->sol;
}

sol::object SafeCallResult(sol::protected_function_result result, bool optional)
{
	const bool valid = result.valid();
	if (!valid) {
		const std::string error = result.get_type() == sol::type::string
		    ? StrCat("Lua error: ", result.get<std::string>())
		    : "Unknown Lua error";
		if (!optional)
			app_fatal(error);
		LogError(error);
	}
	return result;
}

} // namespace devilution
