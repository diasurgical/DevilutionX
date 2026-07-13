#pragma once

#include <format>
#include <string>
#include <string_view>

namespace devilution {

/**
 * @brief Formats using a format string that is only known at runtime, such as a translation.
 *
 * Unlike `std::format`, the format string is not checked at compile time.
 * An invalid format string results in `std::format_error` being thrown
 * (or program termination if exceptions are disabled).
 */
template <typename... Args>
[[nodiscard]] std::string FormatRuntime(std::string_view fmt, Args &&...args)
{
	return std::vformat(fmt, std::make_format_args(args...));
}

} // namespace devilution
