#pragma once

#include <cstdint>
#include <string>

namespace devilution {

/**
 * @brief Formats integer with thousands separator.
 */
std::string FormatInteger(int n);
std::string FormatInteger(uint32_t n);

} // namespace devilution
