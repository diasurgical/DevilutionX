#pragma once

#include <cstdint>

#include "engine/surface.hpp"

namespace devilution {

enum class PanelDrawComponent {
	Health,
	Mana,
	ControlButtons,
	Belt,

	FIRST = Health,
	LAST = Belt
};

void InitBackbufferState();

void RedrawEverything();
bool IsRedrawEverything();
void RedrawViewport();
bool IsRedrawViewport();
void RedrawComplete();

void RedrawComponent(PanelDrawComponent component);
bool IsRedrawComponent(PanelDrawComponent component);
void RedrawComponentComplete(PanelDrawComponent component);

} // namespace devilution
