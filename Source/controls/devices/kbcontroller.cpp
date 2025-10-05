#include "controls/devices/kbcontroller.h"

#if HAS_KBCTRL == 1
#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "controls/controller_motion.h"
#include "utils/sdl_compat.h"
#include "utils/stubs.h"

namespace devilution {

ControllerButton KbCtrlToControllerButton(const SDL_Event &event)
{
	switch (event.type) {
#ifdef USE_SDL3
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_DOWN:
		switch (event.key.key)
#else
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch (event.key.keysym.sym)
#endif
		{
#ifdef KBCTRL_IGNORE_1
		case KBCTRL_IGNORE_1:
			return ControllerButton_IGNORE;
#endif
#ifdef KBCTRL_BUTTON_A
		case KBCTRL_BUTTON_A:
			return ControllerButton_BUTTON_A;
#endif
#ifdef KBCTRL_BUTTON_B
		case KBCTRL_BUTTON_B: // Right button
			return ControllerButton_BUTTON_B;
#endif
#ifdef KBCTRL_BUTTON_X
		case KBCTRL_BUTTON_X: // Left button
			return ControllerButton_BUTTON_X;
#endif
#ifdef KBCTRL_BUTTON_Y
		case KBCTRL_BUTTON_Y:
			return ControllerButton_BUTTON_Y;
#endif
#ifdef KBCTRL_BUTTON_LEFTSTICK
		case KBCTRL_BUTTON_LEFTSTICK:
			return ControllerButton_BUTTON_LEFTSTICK;
#endif
#ifdef KBCTRL_BUTTON_RIGHTSTICK
		case KBCTRL_BUTTON_RIGHTSTICK:
			return ControllerButton_BUTTON_RIGHTSTICK;
#endif
#ifdef KBCTRL_BUTTON_LEFTSHOULDER
		case KBCTRL_BUTTON_LEFTSHOULDER:
			return ControllerButton_BUTTON_LEFTSHOULDER;
#endif
#ifdef KBCTRL_BUTTON_RIGHTSHOULDER
		case KBCTRL_BUTTON_RIGHTSHOULDER:
			return ControllerButton_BUTTON_RIGHTSHOULDER;
#endif
#ifdef KBCTRL_BUTTON_TRIGGERLEFT
		case KBCTRL_BUTTON_TRIGGERLEFT:
			return ControllerButton_AXIS_TRIGGERLEFT;
#endif
#ifdef KBCTRL_BUTTON_TRIGGERRIGHT
		case KBCTRL_BUTTON_TRIGGERRIGHT:
			return ControllerButton_AXIS_TRIGGERRIGHT;
#endif
#ifdef KBCTRL_BUTTON_START
		case KBCTRL_BUTTON_START:
			return ControllerButton_BUTTON_START;
#endif
#ifdef KBCTRL_BUTTON_BACK
		case KBCTRL_BUTTON_BACK:
			return ControllerButton_BUTTON_BACK;
#endif
#ifdef KBCTRL_BUTTON_DPAD_UP
		case KBCTRL_BUTTON_DPAD_UP:
			return ControllerButton_BUTTON_DPAD_UP;
#endif
#ifdef KBCTRL_BUTTON_DPAD_DOWN
		case KBCTRL_BUTTON_DPAD_DOWN:
			return ControllerButton_BUTTON_DPAD_DOWN;
#endif
#ifdef KBCTRL_BUTTON_DPAD_LEFT
		case KBCTRL_BUTTON_DPAD_LEFT:
			return ControllerButton_BUTTON_DPAD_LEFT;
#endif
#ifdef KBCTRL_BUTTON_DPAD_RIGHT
		case KBCTRL_BUTTON_DPAD_RIGHT:
			return ControllerButton_BUTTON_DPAD_RIGHT;
#endif
		default:
			return ControllerButton_NONE;
		}
	default:
		return ControllerButton_NONE;
	}
}

SDL_Keycode ControllerButtonToKbCtrlKeyCode(ControllerButton button)
{
	switch (button) {
#ifdef KBCTRL_BUTTON_A
	case ControllerButton_BUTTON_A:
		return KBCTRL_BUTTON_A;
#endif
#ifdef KBCTRL_BUTTON_B
	case ControllerButton_BUTTON_B:
		return KBCTRL_BUTTON_B;
#endif
#ifdef KBCTRL_BUTTON_X
	case ControllerButton_BUTTON_X:
		return KBCTRL_BUTTON_X;
#endif
#ifdef KBCTRL_BUTTON_Y
	case ControllerButton_BUTTON_Y:
		return KBCTRL_BUTTON_Y;
#endif
#ifdef KBCTRL_BUTTON_BACK
	case ControllerButton_BUTTON_BACK:
		return KBCTRL_BUTTON_BACK;
#endif
#ifdef KBCTRL_BUTTON_START
	case ControllerButton_BUTTON_START:
		return KBCTRL_BUTTON_START;
#endif
#ifdef KBCTRL_BUTTON_LEFTSTICK
	case ControllerButton_BUTTON_LEFTSTICK:
		return KBCTRL_BUTTON_LEFTSTICK;
#endif
#ifdef KBCTRL_BUTTON_RIGHTSTICK
	case ControllerButton_BUTTON_RIGHTSTICK:
		return KBCTRL_BUTTON_RIGHTSTICK;
#endif
#ifdef KBCTRL_BUTTON_LEFTSHOULDER
	case ControllerButton_BUTTON_LEFTSHOULDER:
		return KBCTRL_BUTTON_LEFTSHOULDER;
#endif
#ifdef KBCTRL_BUTTON_RIGHTSHOULDER
	case ControllerButton_BUTTON_RIGHTSHOULDER:
		return KBCTRL_BUTTON_RIGHTSHOULDER;
#endif
#ifdef KBCTRL_BUTTON_TRIGGERLEFT
	case ControllerButton_AXIS_TRIGGERLEFT:
		return KBCTRL_BUTTON_TRIGGERLEFT;
#endif
#ifdef KBCTRL_BUTTON_TRIGGERRIGHT
	case ControllerButton_AXIS_TRIGGERRIGHT:
		return KBCTRL_BUTTON_TRIGGERRIGHT;
#endif
#ifdef KBCTRL_BUTTON_DPAD_UP
	case ControllerButton_BUTTON_DPAD_UP:
		return KBCTRL_BUTTON_DPAD_UP;
#endif
#ifdef KBCTRL_BUTTON_DPAD_DOWN
	case ControllerButton_BUTTON_DPAD_DOWN:
		return KBCTRL_BUTTON_DPAD_DOWN;
#endif
#ifdef KBCTRL_BUTTON_DPAD_LEFT
	case ControllerButton_BUTTON_DPAD_LEFT:
		return KBCTRL_BUTTON_DPAD_LEFT;
#endif
#ifdef KBCTRL_BUTTON_DPAD_RIGHT
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return KBCTRL_BUTTON_DPAD_RIGHT;
#endif
	default:
		return SDLK_UNKNOWN;
	}
}

bool IsKbCtrlButtonPressed(ControllerButton button)
{
	SDL_Keycode key_code = ControllerButtonToKbCtrlKeyCode(button);
	if (key_code == SDLK_UNKNOWN)
		return false;
#ifdef USE_SDL3
	return SDL_GetKeyboardState(nullptr)[SDL_GetScancodeFromKey(key_code, nullptr)];
#elif !defined(USE_SDL1)
	return SDL_GetKeyboardState(nullptr)[SDL_GetScancodeFromKey(key_code)];
#else
	return SDL_GetKeyState(nullptr)[key_code];
#endif
}

} // namespace devilution
#endif
