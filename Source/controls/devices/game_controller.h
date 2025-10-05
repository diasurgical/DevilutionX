#pragma once

#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#else
#include <SDL.h>
#endif

#include "controls/controller_buttons.h"
#include "controls/game_controls.h"

namespace devilution {

class GameController {
	static std::vector<GameController> controllers_;

public:
#ifdef USE_SDL3
	static void Add(SDL_JoystickID joystickId);
#else
	static void Add(int joystickIndex);
#endif
	static void Remove(SDL_JoystickID instanceId);
	static GameController *Get(SDL_JoystickID instanceId);
	static GameController *Get(const SDL_Event &event);
	static const std::vector<GameController> &All();
	static bool IsPressedOnAnyController(ControllerButton button, SDL_JoystickID *which = nullptr);

	// Must be called exactly once at the start of each SDL input event.
	void UnlockTriggerState();

	ControllerButton ToControllerButton(const SDL_Event &event);

	bool IsPressed(ControllerButton button) const;
	static bool ProcessAxisMotion(const SDL_Event &event);

#ifdef USE_SDL3
	static SDL_GamepadButton ToSdlGameControllerButton(ControllerButton button);
#else
	static SDL_GameControllerButton ToSdlGameControllerButton(ControllerButton button);
#endif

	static GamepadLayout getLayout(const SDL_Event &event);

private:
#ifdef USE_SDL3
	SDL_Gamepad *sdl_game_controller_ = nullptr;
#else
	SDL_GameController *sdl_game_controller_ = nullptr;
#endif

	SDL_JoystickID instance_id_ = -1;

	ControllerButton trigger_left_state_ = ControllerButton_NONE;
	ControllerButton trigger_right_state_ = ControllerButton_NONE;
	bool trigger_left_is_down_ = false;
	bool trigger_right_is_down_ = false;
};

} // namespace devilution
