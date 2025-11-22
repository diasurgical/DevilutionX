#include "controls/devices/game_controller.h"

#include <cstddef>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#else
#include <SDL.h>

#include "utils/sdl2_backports.h"
#endif

#include "controls/controller_motion.h"
#include "controls/devices/joystick.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_ptrs.h"
#include "utils/stubs.h"

namespace devilution {

std::vector<GameController> GameController::controllers_;

void GameController::UnlockTriggerState()
{
	trigger_left_state_ = ControllerButton_NONE;
	trigger_right_state_ = ControllerButton_NONE;
}

ControllerButton GameController::ToControllerButton(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		const SDL_GamepadAxisEvent &axis = SDLC_EventGamepadAxis(event);
		switch (axis.axis) {
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
			if (axis.value < 8192 && trigger_left_is_down_) { // 25% pressed
				trigger_left_is_down_ = false;
				trigger_left_state_ = ControllerButton_AXIS_TRIGGERLEFT;
			}
			if (axis.value > 16384 && !trigger_left_is_down_) { // 50% pressed
				trigger_left_is_down_ = true;
				trigger_left_state_ = ControllerButton_AXIS_TRIGGERLEFT;
			}
			return trigger_left_state_;
		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
			if (axis.value < 8192 && trigger_right_is_down_) { // 25% pressed
				trigger_right_is_down_ = false;
				trigger_right_state_ = ControllerButton_AXIS_TRIGGERRIGHT;
			}
			if (axis.value > 16384 && !trigger_right_is_down_) { // 50% pressed
				trigger_right_is_down_ = true;
				trigger_right_state_ = ControllerButton_AXIS_TRIGGERRIGHT;
			}
			return trigger_right_state_;
		}
	} break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		switch (SDLC_EventGamepadButton(event).button) {
		case SDL_GAMEPAD_BUTTON_SOUTH:
			return ControllerButton_BUTTON_A;
		case SDL_GAMEPAD_BUTTON_EAST:
			return ControllerButton_BUTTON_B;
		case SDL_GAMEPAD_BUTTON_WEST:
			return ControllerButton_BUTTON_X;
		case SDL_GAMEPAD_BUTTON_NORTH:
			return ControllerButton_BUTTON_Y;
		case SDL_GAMEPAD_BUTTON_LEFT_STICK:
			return ControllerButton_BUTTON_LEFTSTICK;
		case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
			return ControllerButton_BUTTON_RIGHTSTICK;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
			return ControllerButton_BUTTON_LEFTSHOULDER;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
			return ControllerButton_BUTTON_RIGHTSHOULDER;
		case SDL_GAMEPAD_BUTTON_START:
			return ControllerButton_BUTTON_START;
		case SDL_GAMEPAD_BUTTON_BACK:
			return ControllerButton_BUTTON_BACK;
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			return ControllerButton_BUTTON_DPAD_UP;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			return ControllerButton_BUTTON_DPAD_DOWN;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			return ControllerButton_BUTTON_DPAD_LEFT;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			return ControllerButton_BUTTON_DPAD_RIGHT;
		default:
			break;
		}
	default:
		break;
	}
	return ControllerButton_NONE;
}

SDL_GamepadButton GameController::ToSdlGameControllerButton(ControllerButton button)
{
	if (button == ControllerButton_AXIS_TRIGGERLEFT || button == ControllerButton_AXIS_TRIGGERRIGHT)
		UNIMPLEMENTED();
	switch (button) {
	case ControllerButton_BUTTON_A:
		return SDL_GAMEPAD_BUTTON_SOUTH;
	case ControllerButton_BUTTON_B:
		return SDL_GAMEPAD_BUTTON_EAST;
	case ControllerButton_BUTTON_X:
		return SDL_GAMEPAD_BUTTON_WEST;
	case ControllerButton_BUTTON_Y:
		return SDL_GAMEPAD_BUTTON_NORTH;
	case ControllerButton_BUTTON_BACK:
		return SDL_GAMEPAD_BUTTON_BACK;
	case ControllerButton_BUTTON_START:
		return SDL_GAMEPAD_BUTTON_START;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDL_GAMEPAD_BUTTON_LEFT_STICK;
	case ControllerButton_BUTTON_RIGHTSTICK:
		return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
	case ControllerButton_BUTTON_LEFTSHOULDER:
		return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
	case ControllerButton_BUTTON_RIGHTSHOULDER:
		return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
	case ControllerButton_BUTTON_DPAD_UP:
		return SDL_GAMEPAD_BUTTON_DPAD_UP;
	case ControllerButton_BUTTON_DPAD_DOWN:
		return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
	case ControllerButton_BUTTON_DPAD_LEFT:
		return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
	default:
		return SDL_GAMEPAD_BUTTON_INVALID;
	}
}

bool GameController::IsPressed(ControllerButton button) const
{
	if (button == ControllerButton_AXIS_TRIGGERLEFT)
		return trigger_left_is_down_;
	if (button == ControllerButton_AXIS_TRIGGERRIGHT)
		return trigger_right_is_down_;

	const SDL_GamepadButton gcButton = ToSdlGameControllerButton(button);
	return SDL_GamepadHasButton(sdl_game_controller_, gcButton) && SDL_GetGamepadButton(sdl_game_controller_, gcButton);
}

bool GameController::ProcessAxisMotion(const SDL_Event &event)
{
	if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION) return false;
	const SDL_GamepadAxisEvent &axis = SDLC_EventGamepadAxis(event);
	switch (axis.axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		leftStickXUnscaled = static_cast<float>(axis.value);
		leftStickNeedsScaling = true;
		break;
	case SDL_GAMEPAD_AXIS_LEFTY:
		leftStickYUnscaled = static_cast<float>(-axis.value);
		leftStickNeedsScaling = true;
		break;
	case SDL_GAMEPAD_AXIS_RIGHTX:
		rightStickXUnscaled = static_cast<float>(axis.value);
		rightStickNeedsScaling = true;
		break;
	case SDL_GAMEPAD_AXIS_RIGHTY:
		rightStickYUnscaled = static_cast<float>(-axis.value);
		rightStickNeedsScaling = true;
		break;
	default:
		return false;
	}
	return true;
}

#ifdef USE_SDL3
void GameController::Add(SDL_JoystickID joystickId)
#else
void GameController::Add(int joystickIndex)
#endif
{
	GameController result;
#ifdef USE_SDL3
	Log("Opening game controller for joystick with ID {}", joystickId);
	result.sdl_game_controller_ = SDL_OpenGamepad(joystickId);
#else
	Log("Opening game controller for joystick at index {}", joystickIndex);
	result.sdl_game_controller_ = SDL_GameControllerOpen(joystickIndex);
#endif

	if (result.sdl_game_controller_ == nullptr) {
		Log("{}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	controllers_.push_back(result);

#ifdef USE_SDL3
	result.instance_id_ = joystickId;
	const SDLUniquePtr<char> mapping { SDL_GetGamepadMappingForID(joystickId) };
#else
	SDL_Joystick *const sdlJoystick = SDL_GameControllerGetJoystick(result.sdl_game_controller_);
	result.instance_id_ = SDL_JoystickInstanceID(sdlJoystick);
	const SDL_JoystickGUID guid = SDL_JoystickGetGUID(sdlJoystick);
	const SDLUniquePtr<char> mapping { SDL_GameControllerMappingForGUID(guid) };
#endif
	if (mapping) {
		Log("Opened game controller with mapping:\n{}", mapping.get());
	}
}

void GameController::Remove(SDL_JoystickID instanceId)
{
	Log("Removing game controller with instance id {}", instanceId);
	for (std::size_t i = 0; i < controllers_.size(); ++i) {
		const GameController &controller = controllers_[i];
		if (controller.instance_id_ != instanceId)
			continue;
		controllers_.erase(controllers_.begin() + i);
		return;
	}
	Log("Game controller not found with instance id: {}", instanceId);
}

GameController *GameController::Get(SDL_JoystickID instanceId)
{
	for (auto &controller : controllers_) {
		if (controller.instance_id_ == instanceId)
			return &controller;
	}
	return nullptr;
}

GameController *GameController::Get(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		return Get(SDLC_EventGamepadAxis(event).which);
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		return Get(SDLC_EventGamepadButton(event).which);
	default:
		return nullptr;
	}
}

const std::vector<GameController> &GameController::All()
{
	return controllers_;
}

bool GameController::IsPressedOnAnyController(ControllerButton button, SDL_JoystickID *which)
{
	for (auto &controller : controllers_)
		if (controller.IsPressed(button)) {
			if (which != nullptr)
				*which = controller.instance_id_;

			return true;
		}
	return false;
}

GamepadLayout GameController::getLayout(const SDL_Event &event)
{
#if defined(DEVILUTIONX_GAMEPAD_TYPE)
	return GamepadLayout::
	    DEVILUTIONX_GAMEPAD_TYPE;
#elif USE_SDL3
	switch (SDL_GetGamepadTypeForID(event.gdevice.which)) {
	case SDL_GAMEPAD_TYPE_XBOX360:
	case SDL_GAMEPAD_TYPE_XBOXONE:
		return GamepadLayout::Xbox;
	case SDL_GAMEPAD_TYPE_PS3:
	case SDL_GAMEPAD_TYPE_PS4:
	case SDL_GAMEPAD_TYPE_PS5:
		return GamepadLayout::PlayStation;
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
		return GamepadLayout::Nintendo;
	default:
		return GamepadLayout::Generic;
	}
#else
#if SDL_VERSION_ATLEAST(2, 0, 12)
	const int index = event.cdevice.which;
	const SDL_GameControllerType gamepadType = SDL_GameControllerTypeForIndex(index);
	switch (gamepadType) {
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
		return GamepadLayout::Nintendo;
	case SDL_CONTROLLER_TYPE_PS3:
	case SDL_CONTROLLER_TYPE_PS4:
#if SDL_VERSION_ATLEAST(2, 0, 14)
	case SDL_CONTROLLER_TYPE_PS5:
#endif
		return GamepadLayout::PlayStation;
	case SDL_CONTROLLER_TYPE_XBOXONE:
	case SDL_CONTROLLER_TYPE_XBOX360:
#if SDL_VERSION_ATLEAST(2, 0, 16)
	case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
	case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
#if SDL_VERSION_ATLEAST(2, 24, 0)
	case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD:
#endif
#endif
		return GamepadLayout::Xbox;
#if SDL_VERSION_ATLEAST(2, 0, 14)
	case SDL_CONTROLLER_TYPE_VIRTUAL:
#endif
	case SDL_CONTROLLER_TYPE_UNKNOWN:
#if SDL_VERSION_ATLEAST(2, 30, 0)
	case SDL_CONTROLLER_TYPE_MAX:
#endif
		break;
	}
#endif
	return GamepadLayout::Generic;
#endif // !defined(DEVILUTIONX_GAMEPAD_TYPE)
}

} // namespace devilution
