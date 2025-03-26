#pragma once

#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

tl::expected<void, std::string> LoadPartyPanel();
void FreePartyPanel();
void DrawPartyMemberInfoPanel(const Surface &out);

} // namespace devilution
