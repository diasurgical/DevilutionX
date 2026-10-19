#pragma once

#include <cstddef>
#include <expected>
#include <string>

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

std::expected<void, std::string> InitSpellBook();
void FreeSpellBook();
size_t GetSpellBookPageCount();
void LoadSpellBookLayout();
void CheckSBook();
void DrawSpellBook(const Surface &out);

} // namespace devilution
