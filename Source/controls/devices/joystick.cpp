#include "controls/devices/joystick.h"

#include <cstddef>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_joystick.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "controls/controller_motion.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/stubs.h"

namespace devilution {

std::vector<Joystick> Joystick::joysticks_;

StaticVector<ControllerButtonEvent, 4> Joystick::ToControllerButtonEvents(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP: {
#ifdef USE_SDL3
		const bool up = !event.jbutton.down;
#else
		const bool up = (event.jbutton.state == SDL_RELEASED);
#endif
#if defined(JOY_BUTTON_A) || defined(JOY_BUTTON_B) || defined(JOY_BUTTON_X) || defined(JOY_BUTTON_Y)                                            \
    || defined(JOY_BUTTON_LEFTSTICK) || defined(JOY_BUTTON_RIGHTSTICK) || defined(JOY_BUTTON_LEFTSHOULDER) || defined(JOY_BUTTON_RIGHTSHOULDER) \
    || defined(JOY_BUTTON_TRIGGERLEFT) || defined(JOY_BUTTON_TRIGGERRIGHT) || defined(JOY_BUTTON_START) || defined(JOY_BUTTON_BACK)             \
    || defined(JOY_BUTTON_DPAD_LEFT) || defined(JOY_BUTTON_DPAD_UP) || defined(JOY_BUTTON_DPAD_RIGHT) || defined(JOY_BUTTON_DPAD_DOWN)
		switch (event.jbutton.button) {
#ifdef JOY_BUTTON_A
		case JOY_BUTTON_A:
			return { ControllerButtonEvent { ControllerButton_BUTTON_A, up } };
#endif
#ifdef JOY_BUTTON_B
		case JOY_BUTTON_B:
			return { ControllerButtonEvent { ControllerButton_BUTTON_B, up } };
#endif
#ifdef JOY_BUTTON_X
		case JOY_BUTTON_X:
			return { ControllerButtonEvent { ControllerButton_BUTTON_X, up } };
#endif
#ifdef JOY_BUTTON_Y
		case JOY_BUTTON_Y:
			return { ControllerButtonEvent { ControllerButton_BUTTON_Y, up } };
#endif
#ifdef JOY_BUTTON_LEFTSTICK
		case JOY_BUTTON_LEFTSTICK:
			return { ControllerButtonEvent { ControllerButton_BUTTON_LEFTSTICK, up } };
#endif
#ifdef JOY_BUTTON_RIGHTSTICK
		case JOY_BUTTON_RIGHTSTICK:
			return { ControllerButtonEvent { ControllerButton_BUTTON_RIGHTSTICK, up } };
#endif
#ifdef JOY_BUTTON_LEFTSHOULDER
		case JOY_BUTTON_LEFTSHOULDER:
			return { ControllerButtonEvent { ControllerButton_BUTTON_LEFTSHOULDER, up } };
#endif
#ifdef JOY_BUTTON_RIGHTSHOULDER
		case JOY_BUTTON_RIGHTSHOULDER:
			return { ControllerButtonEvent { ControllerButton_BUTTON_RIGHTSHOULDER, up } };
#endif
#ifdef JOY_BUTTON_TRIGGERLEFT
		case JOY_BUTTON_TRIGGERLEFT:
			return { ControllerButtonEvent { ControllerButton_AXIS_TRIGGERLEFT, up } };
#endif
#ifdef JOY_BUTTON_TRIGGERRIGHT
		case JOY_BUTTON_TRIGGERRIGHT:
			return { ControllerButtonEvent { ControllerButton_AXIS_TRIGGERRIGHT, up } };
#endif
#ifdef JOY_BUTTON_START
		case JOY_BUTTON_START:
			return { ControllerButtonEvent { ControllerButton_BUTTON_START, up } };
#endif
#ifdef JOY_BUTTON_BACK
		case JOY_BUTTON_BACK:
			return { ControllerButtonEvent { ControllerButton_BUTTON_BACK, up } };
#endif
#ifdef JOY_BUTTON_DPAD_LEFT
		case JOY_BUTTON_DPAD_LEFT:
			return { ControllerButtonEvent { ControllerButton_BUTTON_DPAD_LEFT, up } };
#endif
#ifdef JOY_BUTTON_DPAD_UP
		case JOY_BUTTON_DPAD_UP:
			return { ControllerButtonEvent { ControllerButton_BUTTON_DPAD_UP, up } };
#endif
#ifdef JOY_BUTTON_DPAD_RIGHT
		case JOY_BUTTON_DPAD_RIGHT:
			return { ControllerButtonEvent { ControllerButton_BUTTON_DPAD_RIGHT, up } };
#endif
#ifdef JOY_BUTTON_DPAD_DOWN
		case JOY_BUTTON_DPAD_DOWN:
			return { ControllerButtonEvent { ControllerButton_BUTTON_DPAD_DOWN, up } };
#endif
		default:
			return { ControllerButtonEvent { ControllerButton_IGNORE, up } };
		}
#else
		return { ControllerButtonEvent { ControllerButton_IGNORE, up } };
#endif
	}
#ifdef USE_SDL3
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
#else
	case SDL_JOYHATMOTION:
#endif
	{
		Joystick *joystick = Get(event);
		if (joystick == nullptr)
			return { ControllerButtonEvent { ControllerButton_IGNORE, false } };
		joystick->UpdateHatState(event.jhat);
		return joystick->GetHatEvents();
	}
#ifdef USE_SDL3
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
	case SDL_EVENT_JOYSTICK_BALL_MOTION:
#else
	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
#endif
		// ProcessAxisMotion() requires a ControllerButtonEvent parameter
		// so provide one here using ControllerButton_NONE
		return { ControllerButtonEvent { ControllerButton_NONE, false } };
	default:
		return {};
	}
}

StaticVector<ControllerButtonEvent, 4> Joystick::GetHatEvents()
{
	StaticVector<ControllerButtonEvent, 4> hatEvents;
	if (hatState_[0].didStateChange)
		hatEvents.emplace_back(ControllerButton_BUTTON_DPAD_UP, !hatState_[0].pressed);
	if (hatState_[1].didStateChange)
		hatEvents.emplace_back(ControllerButton_BUTTON_DPAD_DOWN, !hatState_[1].pressed);
	if (hatState_[2].didStateChange)
		hatEvents.emplace_back(ControllerButton_BUTTON_DPAD_LEFT, !hatState_[2].pressed);
	if (hatState_[3].didStateChange)
		hatEvents.emplace_back(ControllerButton_BUTTON_DPAD_RIGHT, !hatState_[3].pressed);
	if (hatEvents.size() == 0)
		hatEvents.emplace_back(ControllerButton_IGNORE, false);
	return hatEvents;
}

void Joystick::UpdateHatState(const SDL_JoyHatEvent &event)
{
	if (lockHatState_)
		return;
#if defined(JOY_HAT_DPAD_UP_HAT) && defined(JOY_HAT_DPAD_UP)
	if (event.hat == JOY_HAT_DPAD_UP_HAT) {
		HatState &hatState = hatState_[0];
		bool pressed = (event.value & JOY_HAT_DPAD_UP) != 0;
		hatState.didStateChange = (pressed != hatState.pressed);
		hatState.pressed = pressed;
	}
#endif
#if defined(JOY_HAT_DPAD_DOWN_HAT) && defined(JOY_HAT_DPAD_DOWN)
	if (event.hat == JOY_HAT_DPAD_DOWN_HAT) {
		HatState &hatState = hatState_[1];
		bool pressed = (event.value & JOY_HAT_DPAD_DOWN) != 0;
		hatState.didStateChange = (pressed != hatState.pressed);
		hatState.pressed = pressed;
	}
#endif
#if defined(JOY_HAT_DPAD_LEFT_HAT) && defined(JOY_HAT_DPAD_LEFT)
	if (event.hat == JOY_HAT_DPAD_LEFT_HAT) {
		HatState &hatState = hatState_[2];
		bool pressed = (event.value & JOY_HAT_DPAD_LEFT) != 0;
		hatState.didStateChange = (pressed != hatState.pressed);
		hatState.pressed = pressed;
	}
#endif
#if defined(JOY_HAT_DPAD_RIGHT_HAT) && defined(JOY_HAT_DPAD_RIGHT)
	if (event.hat == JOY_HAT_DPAD_RIGHT_HAT) {
		HatState &hatState = hatState_[3];
		bool pressed = (event.value & JOY_HAT_DPAD_RIGHT) != 0;
		hatState.didStateChange = (pressed != hatState.pressed);
		hatState.pressed = pressed;
	}
#endif
	lockHatState_ = true;
}

void Joystick::UnlockHatState()
{
	lockHatState_ = false;
	for (HatState &hatState : hatState_)
		hatState.didStateChange = false;
}

int Joystick::ToSdlJoyButton(ControllerButton button)
{
#if defined(JOY_BUTTON_A) || defined(JOY_BUTTON_B) || defined(JOY_BUTTON_X) || defined(JOY_BUTTON_Y)                                                \
    || defined(JOY_BUTTON_BACK) || defined(JOY_BUTTON_START) || defined(JOY_BUTTON_LEFTSTICK) || defined(JOY_BUTTON_RIGHTSTICK)                     \
    || defined(JOY_BUTTON_LEFTSHOULDER) || defined(JOY_BUTTON_RIGHTSHOULDER) || defined(JOY_BUTTON_TRIGGERLEFT) || defined(JOY_BUTTON_TRIGGERRIGHT) \
    || defined(JOY_BUTTON_DPAD_LEFT) || defined(JOY_BUTTON_DPAD_UP) || defined(JOY_BUTTON_DPAD_RIGHT) || defined(JOY_BUTTON_DPAD_DOWN)
	switch (button) {
#ifdef JOY_BUTTON_A
	case ControllerButton_BUTTON_A:
		return JOY_BUTTON_A;
#endif
#ifdef JOY_BUTTON_B
	case ControllerButton_BUTTON_B:
		return JOY_BUTTON_B;
#endif
#ifdef JOY_BUTTON_X
	case ControllerButton_BUTTON_X:
		return JOY_BUTTON_X;
#endif
#ifdef JOY_BUTTON_Y
	case ControllerButton_BUTTON_Y:
		return JOY_BUTTON_Y;
#endif
#ifdef JOY_BUTTON_BACK
	case ControllerButton_BUTTON_BACK:
		return JOY_BUTTON_BACK;
#endif
#ifdef JOY_BUTTON_START
	case ControllerButton_BUTTON_START:
		return JOY_BUTTON_START;
#endif
#ifdef JOY_BUTTON_LEFTSTICK
	case ControllerButton_BUTTON_LEFTSTICK:
		return JOY_BUTTON_LEFTSTICK;
#endif
#ifdef JOY_BUTTON_RIGHTSTICK
	case ControllerButton_BUTTON_RIGHTSTICK:
		return JOY_BUTTON_RIGHTSTICK;
#endif
#ifdef JOY_BUTTON_LEFTSHOULDER
	case ControllerButton_BUTTON_LEFTSHOULDER:
		return JOY_BUTTON_LEFTSHOULDER;
#endif
#ifdef JOY_BUTTON_RIGHTSHOULDER
	case ControllerButton_BUTTON_RIGHTSHOULDER:
		return JOY_BUTTON_RIGHTSHOULDER;
#endif
#ifdef JOY_BUTTON_TRIGGERLEFT
	case ControllerButton_AXIS_TRIGGERLEFT:
		return JOY_BUTTON_TRIGGERLEFT;
#endif
#ifdef JOY_BUTTON_TRIGGERRIGHT
	case ControllerButton_AXIS_TRIGGERRIGHT:
		return JOY_BUTTON_TRIGGERRIGHT;
#endif
#ifdef JOY_BUTTON_DPAD_UP
	case ControllerButton_BUTTON_DPAD_UP:
		return JOY_BUTTON_DPAD_UP;
#endif
#ifdef JOY_BUTTON_DPAD_DOWN
	case ControllerButton_BUTTON_DPAD_DOWN:
		return JOY_BUTTON_DPAD_DOWN;
#endif
#ifdef JOY_BUTTON_DPAD_LEFT
	case ControllerButton_BUTTON_DPAD_LEFT:
		return JOY_BUTTON_DPAD_LEFT;
#endif
#ifdef JOY_BUTTON_DPAD_RIGHT
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return JOY_BUTTON_DPAD_RIGHT;
#endif
	default:
		return -1;
	}
#else
	return -1;
#endif
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): Not static if joystick mappings are defined.
bool Joystick::IsHatButtonPressed(ControllerButton button) const
{
#if (defined(JOY_HAT_DPAD_UP_HAT) && defined(JOY_HAT_DPAD_UP)) || (defined(JOY_HAT_DPAD_DOWN_HAT) && defined(JOY_HAT_DPAD_DOWN)) || (defined(JOY_HAT_DPAD_LEFT_HAT) && defined(JOY_HAT_DPAD_LEFT)) || (defined(JOY_HAT_DPAD_RIGHT_HAT) && defined(JOY_HAT_DPAD_RIGHT))
	switch (button) {
#if defined(JOY_HAT_DPAD_UP_HAT) && defined(JOY_HAT_DPAD_UP)
	case ControllerButton_BUTTON_DPAD_UP:
		return (SDL_JoystickGetHat(sdl_joystick_, JOY_HAT_DPAD_UP_HAT) & JOY_HAT_DPAD_UP) != 0;
#endif
#if defined(JOY_HAT_DPAD_DOWN_HAT) && defined(JOY_HAT_DPAD_DOWN)
	case ControllerButton_BUTTON_DPAD_DOWN:
		return (SDL_JoystickGetHat(sdl_joystick_, JOY_HAT_DPAD_DOWN_HAT) & JOY_HAT_DPAD_DOWN) != 0;
#endif
#if defined(JOY_HAT_DPAD_LEFT_HAT) && defined(JOY_HAT_DPAD_LEFT)
	case ControllerButton_BUTTON_DPAD_LEFT:
		return (SDL_JoystickGetHat(sdl_joystick_, JOY_HAT_DPAD_LEFT_HAT) & JOY_HAT_DPAD_LEFT) != 0;
#endif
#if defined(JOY_HAT_DPAD_RIGHT_HAT) && defined(JOY_HAT_DPAD_RIGHT)
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return (SDL_JoystickGetHat(sdl_joystick_, JOY_HAT_DPAD_RIGHT_HAT) & JOY_HAT_DPAD_RIGHT) != 0;
#endif
	default:
		return false;
	}
#else
	return false;
#endif
}

bool Joystick::IsPressed(ControllerButton button) const
{
	if (sdl_joystick_ == nullptr)
		return false;
	if (IsHatButtonPressed(button))
		return true;
	const int joyButton = ToSdlJoyButton(button);
	if (joyButton == -1)
		return false;
#ifdef USE_SDL3
	const int numButtons = SDL_GetNumJoystickButtons(sdl_joystick_);
	return joyButton < numButtons && SDL_GetJoystickButton(sdl_joystick_, joyButton);
#else
	const int numButtons = SDL_JoystickNumButtons(sdl_joystick_);
	return joyButton < numButtons && SDL_JoystickGetButton(sdl_joystick_, joyButton) != 0;
#endif
}

bool Joystick::ProcessAxisMotion(const SDL_Event &event)
{
	if (event.type != SDL_EVENT_JOYSTICK_AXIS_MOTION) return false;

#if defined(JOY_AXIS_LEFTX) || defined(JOY_AXIS_LEFTY) || defined(JOY_AXIS_RIGHTX) || defined(JOY_AXIS_RIGHTY)
	switch (event.jaxis.axis) {
#ifdef JOY_AXIS_LEFTX
	case JOY_AXIS_LEFTX:
		leftStickXUnscaled = event.jaxis.value;
		leftStickNeedsScaling = true;
		return true;
#endif
#ifdef JOY_AXIS_LEFTY
	case JOY_AXIS_LEFTY:
		leftStickYUnscaled = -event.jaxis.value;
		leftStickNeedsScaling = true;
		return true;
#endif
#ifdef JOY_AXIS_RIGHTX
	case JOY_AXIS_RIGHTX:
		rightStickXUnscaled = event.jaxis.value;
		rightStickNeedsScaling = true;
		return true;
#endif
#ifdef JOY_AXIS_RIGHTY
	case JOY_AXIS_RIGHTY:
		rightStickYUnscaled = -event.jaxis.value;
		rightStickNeedsScaling = true;
		return true;
#endif
	default:
		return false;
	}
#else
	return false;
#endif
}

#ifdef USE_SDL3
void Joystick::Add(SDL_JoystickID id)
{
	Joystick result;
	const char *name = SDL_GetJoystickNameForID(id);
	if (name == nullptr) {
		LogWarn("Error getting name for joystick {}", id, SDL_GetError());
		SDL_ClearError();
	} else {
		Log("Adding joystick {}: {}", id, name);
	}
	result.sdl_joystick_ = SDL_OpenJoystick(id);
	if (result.sdl_joystick_ == nullptr) {
		LogError("{}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	result.instance_id_ = id;
	joysticks_.push_back(result);
}
#else
void Joystick::Add(int deviceIndex)
{
	if (SDL_NumJoysticks() <= deviceIndex)
		return;
	Joystick result;
	Log("Adding joystick {}: {}", deviceIndex,
	    SDL_JoystickNameForIndex(deviceIndex));
	result.sdl_joystick_ = SDL_JoystickOpen(deviceIndex);
	if (result.sdl_joystick_ == nullptr) {
		LogError("{}", SDL_GetError());
		SDL_ClearError();
		return;
	}
#ifndef USE_SDL1
	result.instance_id_ = SDL_JoystickInstanceID(result.sdl_joystick_);
#endif
	joysticks_.push_back(result);
}
#endif

void Joystick::Remove(SDL_JoystickID instanceId)
{
#ifndef USE_SDL1
	Log("Removing joystick (instance id: {})", instanceId);
	for (std::size_t i = 0; i < joysticks_.size(); ++i) {
		const Joystick &joystick = joysticks_[i];
		if (joystick.instance_id_ != instanceId)
			continue;
		joysticks_.erase(joysticks_.begin() + i);
		return;
	}
	Log("Joystick not found with instance id: {}", instanceId);
#endif
}

const std::vector<Joystick> &Joystick::All()
{
	return joysticks_;
}

Joystick *Joystick::Get(SDL_JoystickID instanceId)
{
	for (auto &joystick : joysticks_) {
		if (joystick.instance_id_ == instanceId)
			return &joystick;
	}
	return nullptr;
}

Joystick *Joystick::Get(const SDL_Event &event)
{
	switch (event.type) {
#ifndef USE_SDL1
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		return Get(event.jaxis.which);
	case SDL_EVENT_JOYSTICK_BALL_MOTION:
		return Get(event.jball.which);
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
		return Get(event.jhat.which);
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		return Get(event.jbutton.which);
	default:
		return nullptr;
#else
	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
	case SDL_JOYHATMOTION:
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		return joysticks_.empty() ? nullptr : &joysticks_[0];
	default:
		return nullptr;
#endif
	}
}

bool Joystick::IsPressedOnAnyJoystick(ControllerButton button)
{
	for (auto &joystick : joysticks_)
		if (joystick.IsPressed(button))
			return true;
	return false;
}

} // namespace devilution
