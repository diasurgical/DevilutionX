#pragma once

#include <expected>
#include <string>

#include "engine/clx_sprite.hpp"

namespace devilution {

extern OptionalOwnedClxSpriteList PanelButtonDown;
extern OptionalOwnedClxSpriteList TalkButton;

std::expected<void, std::string> LoadMainPanel();
void FreeMainPanel();

} // namespace devilution
