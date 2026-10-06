#pragma once

#include <expected>
#include <string>

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

extern OptionalOwnedClxSpriteList pChrButtons;

void DrawChr(const Surface &);
std::expected<void, std::string> LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
