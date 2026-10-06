#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "levels/gendung_defs.hpp"

namespace devilution {

std::expected<dungeon_type, std::string> ParseDungeonType(std::string_view value);

} // namespace devilution
