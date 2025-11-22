#include "lua/modules/log.hpp"

#ifdef USE_SDL3
#include <SDL3/SDL_log.h>
#else
#include <SDL.h>
#endif

#include <fmt/args.h>
#include <fmt/format.h>
#include <sol/sol.hpp>
#include <sol/utility/to_string.hpp>

#include "utils/log.hpp"

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

namespace {

void LuaLogMessage(LogPriority priority, std::string_view fmt, sol::variadic_args args)
{
	std::string formatted;
	FMT_TRY
	{
		fmt::dynamic_format_arg_store<fmt::format_context> store;
		for (const sol::stack_proxy arg : args) {
			switch (arg.get_type()) {
			case sol::type::boolean:
				store.push_back(arg.as<bool>());
				break;
			case sol::type::number:
				if (lua_isinteger(arg.lua_state(), arg.stack_index())) {
					store.push_back(lua_tointeger(arg.lua_state(), arg.stack_index()));
				} else {
					store.push_back(lua_tonumber(arg.lua_state(), arg.stack_index()));
				}
				break;
			case sol::type::string:
				store.push_back(arg.as<std::string>());
				break;
			default:
				store.push_back(sol::utility::to_string(sol::stack_object(arg)));
				break;
			}
		}
		formatted = fmt::vformat(fmt, store);
	}
#if FMT_EXCEPTIONS
	FMT_CATCH(const fmt::format_error &e)
	{
		// e.what() is undefined if exceptions are disabled, so we wrap it
		// with an `FMT_EXCEPTIONS` check.
		std::string error = e.what();
#else
	FMT_CATCH(const fmt::format_error)
	{
		std::string error = "unknown (FMT_EXCEPTIONS disabled)";
#endif
		std::string fullError = StrCat("Format error, fmt: ", fmt, " error: ", error);
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", fullError.c_str());
		return;
	}
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, static_cast<SDL_LogPriority>(priority), "%s", formatted.c_str());
}

void LuaLogInfo(std::string_view fmt, sol::variadic_args args)
{
	LuaLogMessage(LogPriority::Info, fmt, std::move(args));
}
void LuaLogVerbose(std::string_view fmt, sol::variadic_args args)
{
	LuaLogMessage(LogPriority::Verbose, fmt, std::move(args));
}
void LuaLogDebug(std::string_view fmt, sol::variadic_args args)
{
	LuaLogMessage(LogPriority::Debug, fmt, std::move(args));
}
void LuaLogWarn(std::string_view fmt, sol::variadic_args args)
{
	LuaLogMessage(LogPriority::Warn, fmt, std::move(args));
}
void LuaLogError(std::string_view fmt, sol::variadic_args args)
{
	LuaLogMessage(LogPriority::Error, fmt, std::move(args));
}
} // namespace

sol::table LuaLogModule(sol::state_view &lua)
{
	return lua.create_table_with(
	    "info", LuaLogInfo,
	    "verbose", LuaLogVerbose,
	    "debug", LuaLogDebug,
	    "warn", LuaLogWarn,
	    "error", LuaLogError);
}

} // namespace devilution
