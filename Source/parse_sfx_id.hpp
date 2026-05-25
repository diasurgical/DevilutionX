#pragma once

#include <string>
#include <string_view>

#include <expected.hpp>

#include "sound_effect_enums.h"

namespace devilution {

tl::expected<SfxID, std::string> ParseSfxId(std::string_view value);

} // namespace devilution
