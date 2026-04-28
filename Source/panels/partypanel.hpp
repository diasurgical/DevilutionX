#pragma once

#include <expected>
#include <string>

#include "engine/clx_sprite.hpp"

namespace devilution {

extern bool PartySidePanelOpen;
extern bool InspectingFromPartyPanel;
extern int PortraitIdUnderCursor;

std::expected<void, std::string> LoadPartyPanel();
void FreePartyPanel();
void DrawPartyMemberInfoPanel();
bool DidRightClickPartyPortrait();

} // namespace devilution
