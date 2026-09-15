#pragma once

#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_log.h>
#else
#include <SDL.h>
#endif

#include <format>
#include <string>

#include "utils/attributes.h"
#include "utils/str_cat.hpp"

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

namespace devilution {

// Local definition to fix compilation issue due to header conflict.
[[noreturn]] extern void app_fatal(std::string_view);

enum class LogCategory {
	Application = SDL_LOG_CATEGORY_APPLICATION,
	Error = SDL_LOG_CATEGORY_ERROR,
	Assert = SDL_LOG_CATEGORY_ASSERT,
	System = SDL_LOG_CATEGORY_SYSTEM,
	Audio = SDL_LOG_CATEGORY_AUDIO,
	Video = SDL_LOG_CATEGORY_VIDEO,
	Render = SDL_LOG_CATEGORY_RENDER,
	Input = SDL_LOG_CATEGORY_INPUT,
	Test = SDL_LOG_CATEGORY_TEST,
};

constexpr auto defaultCategory = LogCategory::Application;

enum class LogPriority {
	Verbose = SDL_LOG_PRIORITY_VERBOSE,
	Debug = SDL_LOG_PRIORITY_DEBUG,
	Info = SDL_LOG_PRIORITY_INFO,
	Warn = SDL_LOG_PRIORITY_WARN,
	Error = SDL_LOG_PRIORITY_ERROR,
	Critical = SDL_LOG_PRIORITY_CRITICAL,
};

namespace detail {

template <typename... Args>
std::string format(std::string_view fmt, Args &&...args)
{
#if DVL_EXCEPTIONS
	try {
		return std::vformat(fmt, std::make_format_args(args...));
	} catch (const std::format_error &e) {
		const std::string fullError = StrCat("Format error, fmt: ", fmt, " error: ", e.what());
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", fullError.c_str());
		app_fatal(fullError);
	}
#else
	// `std::vformat` terminates the program on error when exceptions are disabled.
	return std::vformat(fmt, std::make_format_args(args...));
#endif
}

} // namespace detail

inline void Log(std::string_view str)
{
	SDL_Log("%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void Log(std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_Log("%s", str.c_str());
}

inline void LogVerbose(LogCategory category, std::string_view str)
{
	SDL_LogVerbose(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

inline bool IsLogLevel(LogCategory category, SDL_LogPriority priority)
{
#ifdef USE_SDL3
	return SDL_GetLogPriority(static_cast<int>(category)) <= priority;
#else
	return SDL_LogGetPriority(static_cast<int>(category)) <= priority;
#endif
}

template <typename... Args>
void LogVerbose(LogCategory category, std::string_view fmt, Args &&...args)
{
	if (!IsLogLevel(category, SDL_LOG_PRIORITY_VERBOSE)) return;
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogVerbose(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogVerbose(std::string_view fmt, Args &&...args)
{
#ifndef PS2
	LogVerbose(defaultCategory, fmt, std::forward<Args>(args)...);
#endif
}

inline void LogDebug(LogCategory category, std::string_view str)
{
	SDL_LogDebug(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogDebug(LogCategory category, std::string_view fmt, Args &&...args)
{
	if (!IsLogLevel(category, SDL_LOG_PRIORITY_DEBUG)) return;
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogDebug(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogDebug(std::string_view fmt, Args &&...args)
{
	LogDebug(defaultCategory, fmt, std::forward<Args>(args)...);
}

inline void LogInfo(LogCategory category, std::string_view str)
{
	SDL_LogInfo(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogInfo(LogCategory category, std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogInfo(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogInfo(std::string_view fmt, Args &&...args)
{
	LogInfo(defaultCategory, fmt, std::forward<Args>(args)...);
}

inline void LogWarn(LogCategory category, std::string_view str)
{
	SDL_LogWarn(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogWarn(LogCategory category, std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogWarn(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogWarn(std::string_view fmt, Args &&...args)
{
	LogWarn(defaultCategory, fmt, std::forward<Args>(args)...);
}

inline void LogError(LogCategory category, std::string_view str)
{
	SDL_LogError(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogError(LogCategory category, std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogError(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogError(std::string_view fmt, Args &&...args)
{
	LogError(defaultCategory, fmt, std::forward<Args>(args)...);
}

inline void LogCritical(LogCategory category, std::string_view str)
{
	SDL_LogCritical(static_cast<int>(category), "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogCritical(LogCategory category, std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogCritical(static_cast<int>(category), "%s", str.c_str());
}

template <typename... Args>
void LogCritical(std::string_view fmt, Args &&...args)
{
	LogCritical(defaultCategory, fmt, std::forward<Args>(args)...);
}

inline void LogMessageV(LogCategory category, LogPriority priority, std::string_view str)
{
	SDL_LogMessage(static_cast<int>(category), static_cast<SDL_LogPriority>(priority),
	    "%.*s", static_cast<int>(str.size()), str.data());
}

template <typename... Args>
void LogMessageV(LogCategory category, LogPriority priority, std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	SDL_LogMessageV(static_cast<int>(category), static_cast<SDL_LogPriority>(priority), "%s", str.c_str());
}

template <typename... Args>
void LogMessageV(std::string_view fmt, Args &&...args)
{
	LogMessageV(defaultCategory, fmt, std::forward<Args>(args)...);
}

} // namespace devilution
