#pragma once

#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

extern OptionalOwnedClxSpriteList pChrButtons;

void DrawChr();
tl::expected<void, std::string> LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
