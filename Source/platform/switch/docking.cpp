#include "platform/switch/docking.h"

#include <cstdint>

#include <SDL.h>
#include <switch.h>

#include "engine/render/renderer.h"
#include "utils/display.h"

namespace devilution {
namespace {
enum class OperationMode : int8_t {
	Handheld,
	Docked,
	Uninitialized = -1
};
}

/**
 * @brief Do a manual window resize when docking/undocking the Switch
 */
void HandleDocking()
{
	static OperationMode currentMode = OperationMode::Uninitialized; // keep track of docked or handheld mode

	OperationMode newMode;
	switch (appletGetOperationMode()) {
	case AppletOperationMode_Console:
		newMode = OperationMode::Docked;
		break;
	case AppletOperationMode_Handheld:
	default:
		newMode = OperationMode::Handheld;
	}

	if (currentMode != newMode) {
		int display_width;
		int display_height;

		// docked mode has changed, update window size
		if (newMode == OperationMode::Docked) {
			display_width = 1920;
			display_height = 1080;
		} else {
			display_width = 1280;
			display_height = 720;
		}
		currentMode = newMode;

		// remove leftover-garbage on screen. Need to perform three clears to ensure all buffers get cleared, otherwise
		//  the display flickers showing a stale frame at certain refresh rates/dock modes.
		for (auto i = 0; i < 3; i++) {
			GetRenderer().ClearScreen();
			GetRenderer().EndFrame();
		}
		SDL_SetWindowSize(ghMainWnd, display_width, display_height);
	}
}

} // namespace devilution
