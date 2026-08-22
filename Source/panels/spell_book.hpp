#pragma once

#include <expected>
#include <string>

#include "engine/clx_sprite.hpp"

namespace devilution {

std::expected<void, std::string> InitSpellBook();
void FreeSpellBook();
void CheckSBook();
void DrawSpellBook();

} // namespace devilution
