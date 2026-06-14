#pragma once

#include <string>
#include <string_view>

#include <expected.hpp>

#include "levels/gendung_defs.hpp"

namespace devilution {

tl::expected<dungeon_type, std::string> ParseDungeonType(std::string_view value);

} // namespace devilution
