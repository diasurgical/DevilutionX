#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace devilution {

std::string ConvertLogicalToVisual(std::string_view input);
int ConvertLogicalToVisualPosition(std::string_view text, int logicalPos);
int ConvertVisualToLogicalPosition(std::string_view text, int visualPos);
} // namespace devilution
