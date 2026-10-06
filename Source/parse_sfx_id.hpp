#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sound_effect_enums.h"

namespace devilution {

std::expected<SfxID, std::string> ParseSfxId(std::string_view value);

} // namespace devilution
