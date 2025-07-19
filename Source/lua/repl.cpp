#ifdef _DEBUG
#include "lua/repl.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <expected.hpp>
#include <sol/sol.hpp>
#include <sol/utility/to_string.hpp>

#include "lua/lua_global.hpp"
#include "panels/console.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

std::optional<sol::environment> replEnv;

void LuaConsoleWarn(void *userData, const char *message, int continued)
{
	static std::string warnBuffer;
	warnBuffer.append(message);
	if (continued != 0)
		return;
	PrintWarningToConsole(warnBuffer);
	warnBuffer.clear();
}

int LuaPrintToConsole(lua_State *state)
{
	std::string result;
	const int n = lua_gettop(state);
	for (int i = 1; i <= n; i++) {
		size_t l;
		const char *s = luaL_tolstring(state, i, &l);
		if (i > 1)
			result += '\t';
		result.append(s, l);
		lua_pop(state, 1);
	}
	PrintToConsole(result);
	return 0;
}

void CreateReplEnvironment()
{
	sol::environment env = CreateLuaSandbox();
	env["print"] = LuaPrintToConsole;
	lua_setwarnf(env.lua_state(), LuaConsoleWarn, /*ud=*/nullptr);
	replEnv.emplace(env);
}

sol::protected_function_result TryRunLuaAsExpressionThenStatement(std::string_view code)
{
	// Try to compile as an expression first. This also how the `lua` repl is implemented.
	sol::state &lua = GetLuaState();
	std::string expression = StrCat("return ", code, ";");
	sol::detail::typical_chunk_name_t basechunkname = {};
	sol::load_status status = static_cast<sol::load_status>(
	    luaL_loadbufferx(lua.lua_state(), expression.data(), expression.size(),
	        sol::detail::make_chunk_name(expression, sol::detail::default_chunk_name(), basechunkname), "text"));
	if (status != sol::load_status::ok) {
		// Try as a statement:
		status = static_cast<sol::load_status>(
		    luaL_loadbufferx(lua.lua_state(), code.data(), code.size(),
		        sol::detail::make_chunk_name(code, sol::detail::default_chunk_name(), basechunkname), "text"));
		if (status != sol::load_status::ok) {
			return sol::protected_function_result(
			    lua.lua_state(), sol::absolute_index(lua.lua_state(), -1), 0, 1, static_cast<sol::call_status>(status));
		}
	}
	sol::stack_aligned_protected_function fn(lua.lua_state(), -1);
	sol::set_environment(GetLuaReplEnvironment(), fn);
	return fn();
}

} // namespace

sol::environment &GetLuaReplEnvironment()
{
	if (!replEnv.has_value())
		CreateReplEnvironment();
	return *replEnv;
}

tl::expected<std::string, std::string> RunLuaReplLine(std::string_view code)
{
	const sol::protected_function_result result = TryRunLuaAsExpressionThenStatement(code);
	if (!result.valid()) {
		if (result.get_type() == sol::type::string) {
			return tl::make_unexpected(result.get<std::string>());
		}
		return tl::make_unexpected("Unknown Lua error");
	}
	if (result.get_type() == sol::type::none) {
		return std::string {};
	}
	return sol::utility::to_string(sol::stack_object(result));
}

void LuaReplShutdown()
{
	replEnv = std::nullopt;
}

} // namespace devilution
#endif // _DEBUG
