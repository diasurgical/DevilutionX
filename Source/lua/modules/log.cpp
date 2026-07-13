#include "lua/modules/log.hpp"

#ifdef USE_SDL3
#include <SDL3/SDL_log.h>
#else
#include <SDL.h>
#endif

#include <charconv>
#include <cstddef>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

#include <sol/sol.hpp>
#include <sol/utility/to_string.hpp>

#include "utils/attributes.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

namespace {

using LuaFormatArg = std::variant<bool, lua_Integer, lua_Number, std::string>;

/**
 * @brief Formats a message with arguments only known at runtime.
 *
 * `std::format` has no equivalent of `fmt::dynamic_format_arg_store`, so we
 * parse the replacement fields ourselves and format each one individually.
 */
std::expected<std::string, std::string> FormatLuaMessage(std::string_view fmt, std::span<const LuaFormatArg> args)
{
	std::string result;
	result.reserve(fmt.size());
	size_t autoIndex = 0;
	bool manualIndexing = false;
	bool automaticIndexing = false;
	for (size_t i = 0; i < fmt.size(); ++i) {
		const char c = fmt[i];
		if (c == '}') {
			if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
				result += '}';
				++i;
				continue;
			}
			return std::unexpected("unmatched '}' in format string");
		}
		if (c != '{') {
			result += c;
			continue;
		}
		if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
			result += '{';
			++i;
			continue;
		}
		const size_t fieldEnd = fmt.find_first_of("{}", i + 1);
		if (fieldEnd == std::string_view::npos || fmt[fieldEnd] == '{')
			return std::unexpected("unmatched '{' in format string");
		const std::string_view field = fmt.substr(i + 1, fieldEnd - (i + 1));
		const size_t colonPos = field.find(':');
		const std::string_view argId = field.substr(0, colonPos);
		size_t argIndex;
		if (argId.empty()) {
			if (manualIndexing)
				return std::unexpected("cannot switch from manual to automatic argument indexing");
			automaticIndexing = true;
			argIndex = autoIndex++;
		} else {
			if (automaticIndexing)
				return std::unexpected("cannot switch from automatic to manual argument indexing");
			manualIndexing = true;
			const std::from_chars_result parseResult = std::from_chars(argId.data(), argId.data() + argId.size(), argIndex);
			if (parseResult.ec != std::errc() || parseResult.ptr != argId.data() + argId.size())
				return std::unexpected(StrCat("invalid argument id \"", argId, "\""));
		}
		if (argIndex >= args.size())
			return std::unexpected(StrCat("argument index ", argIndex, " out of range"));
		const std::string fieldFmt = colonPos == std::string_view::npos
		    ? "{}"
		    : StrCat("{:", field.substr(colonPos + 1), "}");
#if DVL_EXCEPTIONS
		try {
#endif
			std::visit([&](const auto &value) {
				result += std::vformat(fieldFmt, std::make_format_args(value));
			},
			    args[argIndex]);
#if DVL_EXCEPTIONS
		} catch (const std::format_error &e) {
			return std::unexpected(e.what());
		}
#endif
		i = fieldEnd;
	}
	return result;
}

void LuaLogMessage(LogPriority priority, std::string_view fmt, sol::variadic_args args)
{
	std::vector<LuaFormatArg> formatArgs;
	formatArgs.reserve(args.size());
	for (const sol::stack_proxy arg : args) {
		switch (arg.get_type()) {
		case sol::type::boolean:
			formatArgs.emplace_back(arg.as<bool>());
			break;
		case sol::type::number:
			if (lua_isinteger(arg.lua_state(), arg.stack_index())) {
				formatArgs.emplace_back(lua_tointeger(arg.lua_state(), arg.stack_index()));
			} else {
				formatArgs.emplace_back(lua_tonumber(arg.lua_state(), arg.stack_index()));
			}
			break;
		case sol::type::string:
			formatArgs.emplace_back(arg.as<std::string>());
			break;
		default:
			formatArgs.emplace_back(sol::utility::to_string(sol::stack_object(arg)));
			break;
		}
	}
	const std::expected<std::string, std::string> formatted = FormatLuaMessage(fmt, formatArgs);
	if (!formatted.has_value()) {
		const std::string fullError = StrCat("Format error, fmt: ", fmt, " error: ", formatted.error());
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", fullError.c_str());
		return;
	}
	SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, static_cast<SDL_LogPriority>(priority), "%s", formatted->c_str());
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
