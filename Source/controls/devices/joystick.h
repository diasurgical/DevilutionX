#pragma once
// Joystick mappings for SDL1 and additional buttons on SDL2.

#include <array>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "controls/controller.h"
#include "controls/controller_buttons.h"
#include "utils/static_vector.hpp"

namespace devilution {

class Joystick {
	static std::vector<Joystick> joysticks_;

public:
#ifdef USE_SDL3
	static void Add(SDL_JoystickID deviceIndex);
#else
	static void Add(int deviceIndex);
#endif
	static void Remove(SDL_JoystickID instanceId);
	static Joystick *Get(SDL_JoystickID instanceId);
	static Joystick *Get(const SDL_Event &event);
	static const std::vector<Joystick> &All();
	static bool IsPressedOnAnyJoystick(ControllerButton button);

	// Must be called exactly once at the start of each SDL input event.
	void UnlockHatState();

	static StaticVector<ControllerButtonEvent, 4> ToControllerButtonEvents(const SDL_Event &event);
	bool IsPressed(ControllerButton button) const;
	static bool ProcessAxisMotion(const SDL_Event &event);

	SDL_JoystickID instance_id() const
	{
		return instance_id_;
	}

private:
	struct HatState {
		bool pressed;
		bool didStateChange;
	};

	static int ToSdlJoyButton(ControllerButton button);

	bool IsHatButtonPressed(ControllerButton button) const;
	StaticVector<ControllerButtonEvent, 4> GetHatEvents();
	void UpdateHatState(const SDL_JoyHatEvent &event);

	SDL_Joystick *sdl_joystick_ = NULL;
	SDL_JoystickID instance_id_ = -1;
	std::array<HatState, 4> hatState_;
	bool lockHatState_ = false;
};

} // namespace devilution
