#pragma once

#include <string>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"

namespace devilution {

extern bool PartySidePanelOpen;
extern bool InspectingFromPartyPanel;
extern int PortraitIdUnderCursor;

tl::expected<void, std::string> LoadPartyPanel();
void FreePartyPanel();
void DrawPartyMemberInfoPanel();
bool DidRightClickPartyPortrait();

} // namespace devilution
