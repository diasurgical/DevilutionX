#pragma once

#include <string_view>

namespace devilution {

bool IsFbbAsset(std::string_view filename);
void SanitizeFbbAsset();

} // namespace devilution
