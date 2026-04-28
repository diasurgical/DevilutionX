#include "engine/backbuffer_state.hpp"

#include <array>
#include <vector>

#include "engine/dx.h"
#include "engine/render/renderer.h"
#include "utils/enum_traits.h"

namespace devilution {
namespace {

struct RedrawState {
	enum {
		RedrawNone,
		RedrawViewportOnly,
		RedrawAll
	} Redraw;
	std::array<bool, enum_size<PanelDrawComponent>::value> redrawComponents;
};

struct BackbufferState {
	RedrawState redrawState;
};

struct BackbufferPtrAndState {
	void *ptr;
	BackbufferState state;
};

std::vector<BackbufferPtrAndState> States;

BackbufferState &GetBackbufferState()
{
	void *ptr = GetRenderer().GetBackBufferKey();
	for (BackbufferPtrAndState &ptrAndState : States) {
		if (ptrAndState.ptr == ptr)
			return ptrAndState.state;
	}
	States.emplace_back();
	BackbufferPtrAndState &ptrAndState = States.back();
	ptrAndState.ptr = ptr;
	ptrAndState.state.redrawState.Redraw = RedrawState::RedrawAll;

	return ptrAndState.state;
}

} // namespace

bool IsRedrawEverything()
{
	return GetBackbufferState().redrawState.Redraw == RedrawState::RedrawAll;
}

void RedrawViewport()
{
	for (BackbufferPtrAndState &ptrAndState : States) {
		if (ptrAndState.state.redrawState.Redraw != RedrawState::RedrawAll) {
			ptrAndState.state.redrawState.Redraw = RedrawState::RedrawViewportOnly;
		}
	}
}

bool IsRedrawViewport()
{
	return GetBackbufferState().redrawState.Redraw == RedrawState::RedrawViewportOnly;
}

void RedrawComplete()
{
	GetBackbufferState().redrawState.Redraw = RedrawState::RedrawNone;
}

void RedrawEverything()
{
	for (BackbufferPtrAndState &ptrAndState : States) {
		ptrAndState.state.redrawState.Redraw = RedrawState::RedrawAll;
	}
}

void InitBackbufferState()
{
	States.clear();
}

void RedrawComponent(PanelDrawComponent component)
{
	for (BackbufferPtrAndState &ptrAndState : States) {
		ptrAndState.state.redrawState.redrawComponents[static_cast<size_t>(component)] = true;
	}
}

bool IsRedrawComponent(PanelDrawComponent component)
{
	return GetBackbufferState().redrawState.redrawComponents[static_cast<size_t>(component)];
}

void RedrawComponentComplete(PanelDrawComponent component)
{
	GetBackbufferState().redrawState.redrawComponents[static_cast<size_t>(component)] = false;
}

} // namespace devilution
