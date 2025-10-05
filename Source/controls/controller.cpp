#include "controls/controller.h"

#include <cmath>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "controls/devices/kbcontroller.h"

#include "engine/demomode.h"

namespace devilution {

void UnlockControllerState(const SDL_Event &event)
{
#ifndef USE_SDL1
	GameController *const controller = GameController::Get(event);
	if (controller != nullptr) {
		controller->UnlockTriggerState();
	}
#endif
	Joystick *const joystick = Joystick::Get(event);
	if (joystick != nullptr) {
		joystick->UnlockHatState();
	}
}

StaticVector<ControllerButtonEvent, 4> ToControllerButtonEvents(const SDL_Event &event)
{
	ControllerButtonEvent result { ControllerButton_NONE, false };
	switch (event.type) {
#ifdef USE_SDL3
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
	case SDL_EVENT_KEY_UP:
#else
#ifndef USE_SDL1
	case SDL_CONTROLLERBUTTONUP:
#endif
	case SDL_JOYBUTTONUP:
	case SDL_KEYUP:
#endif
		result.up = true;
		break;
	default:
		break;
	}
#if HAS_KBCTRL == 1
	if (!demo::IsRunning()) {
		result.button = KbCtrlToControllerButton(event);
		if (result.button != ControllerButton_NONE)
			return { result };
	}
#endif
#ifndef USE_SDL1
	GameController *const controller = GameController::Get(event);
	if (controller != nullptr) {
		result.button = controller->ToControllerButton(event);
		if (result.button != ControllerButton_NONE) {
			if (result.button == ControllerButton_AXIS_TRIGGERLEFT || result.button == ControllerButton_AXIS_TRIGGERRIGHT) {
				result.up = !controller->IsPressed(result.button);
			}
			return { result };
		}
	}
#endif

	const Joystick *joystick = Joystick::Get(event);
	if (joystick != nullptr) {
		return devilution::Joystick::ToControllerButtonEvents(event);
	}

	return { result };
}

bool IsControllerButtonPressed(ControllerButton button)
{
#ifndef USE_SDL1
	if (GameController::IsPressedOnAnyController(button))
		return true;
#endif
#if HAS_KBCTRL == 1
	if (!demo::IsRunning() && IsKbCtrlButtonPressed(button))
		return true;
#endif
	return Joystick::IsPressedOnAnyJoystick(button);
}

bool IsControllerButtonComboPressed(ControllerButtonCombo combo)
{
	return IsControllerButtonPressed(combo.button)
	    && (combo.modifier == ControllerButton_NONE || IsControllerButtonPressed(combo.modifier));
}

bool HandleControllerAddedOrRemovedEvent(const SDL_Event &event)
{
#ifndef USE_SDL1
	switch (event.type) {
#ifdef USE_SDL3
	case SDL_EVENT_GAMEPAD_ADDED:
		GameController::Add(event.gdevice.which);
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		GameController::Remove(event.gdevice.which);
		break;
#else
	case SDL_CONTROLLERDEVICEADDED:
		GameController::Add(event.cdevice.which);
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		GameController::Remove(event.cdevice.which);
		break;
#endif
#ifdef USE_SDL3
	case SDL_EVENT_JOYSTICK_ADDED:
#else
	case SDL_JOYDEVICEADDED:
#endif
		Joystick::Add(event.jdevice.which);
		break;
#ifdef USE_SDL3
	case SDL_EVENT_JOYSTICK_REMOVED:
#else
	case SDL_JOYDEVICEREMOVED:
#endif
		Joystick::Remove(event.jdevice.which);
		break;
	default:
		return false;
	}
	return true;
#else
	return false;
#endif
}

} // namespace devilution
