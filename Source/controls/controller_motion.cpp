#include "controls/controller_motion.h"

#include <cmath>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#else
#include <SDL.h>
#endif

#include "control.h"
#include "controls/control_mode.hpp"
#include "controls/controller.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "controls/game_controls.h"
#include "controls/padmapper.hpp"
#include "controls/plrctrls.h"
#include "controls/touch/gamepad.h"
#include "engine/demomode.h"
#include "options.h"
#include "utils/is_of.hpp"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"

namespace devilution {

bool SimulatingMouseWithPadmapper;

namespace {

void ScaleJoystickAxes(float *x, float *y, float deadzone)
{
	// radial and scaled dead_zone
	// https://web.archive.org/web/20200130014626/www.third-helix.com:80/2013/04/12/doing-thumbstick-dead-zones-right.html
	// input values go from -32767.0...+32767.0, output values are from -1.0 to 1.0;

	if (deadzone == 0) {
		return;
	}
	if (deadzone >= 1.0) {
		*x = 0;
		*y = 0;
		return;
	}

	const float maximum = 32767.0;
	float analogX = *x;
	float analogY = *y;
	const float deadZone = deadzone * maximum;

	const float magnitude = std::sqrt(analogX * analogX + analogY * analogY);
	if (magnitude >= deadZone) {
		// find scaled axis values with magnitudes between zero and maximum
		const float scalingFactor = 1.F / magnitude * (magnitude - deadZone) / (maximum - deadZone);
		analogX = (analogX * scalingFactor);
		analogY = (analogY * scalingFactor);

		// std::clamp to ensure results will never exceed the max_axis value
		float clampingFactor = 1.F;
		const float absAnalogX = std::fabs(analogX);
		const float absAnalogY = std::fabs(analogY);
		if (absAnalogX > 1.0 || absAnalogY > 1.0) {
			if (absAnalogX > absAnalogY) {
				clampingFactor = 1.F / absAnalogX;
			} else {
				clampingFactor = 1.F / absAnalogY;
			}
		}
		*x = (clampingFactor * analogX);
		*y = (clampingFactor * analogY);
	} else {
		*x = 0;
		*y = 0;
	}
}

bool IsMovementOverriddenByPadmapper(ControllerButton button)
{
	const ControllerButtonEvent releaseEvent { button, true };
	const std::string_view actionName = PadmapperActionNameTriggeredByButtonEvent(releaseEvent);
	const ControllerButtonCombo buttonCombo = GetOptions().Padmapper.ButtonComboForAction(actionName);
	return buttonCombo.modifier != ControllerButton_NONE;
}

bool TriggersQuickSpellAction(ControllerButton button)
{
	const ControllerButtonEvent releaseEvent { button, true };
	const std::string_view actionName = PadmapperActionNameTriggeredByButtonEvent(releaseEvent);

	const std::string_view prefix { "QuickSpell" };
	if (actionName.size() < prefix.size())
		return false;
	const std::string_view truncatedActionName { actionName.data(), prefix.size() };
	return truncatedActionName == prefix;
}

bool IsPressedForMovement(ControllerButton button)
{
	return !PadMenuNavigatorActive
	    && IsControllerButtonPressed(button)
	    && !IsMovementOverriddenByPadmapper(button)
	    && !(SpellSelectFlag && TriggersQuickSpellAction(button));
}

void SetSimulatingMouseWithPadmapper(bool value)
{
	if (SimulatingMouseWithPadmapper == value)
		return;
	SimulatingMouseWithPadmapper = value;
	if (value) {
		LogVerbose("Control: begin simulating mouse with D-Pad");
	} else {
		LogVerbose("Control: end simulating mouse with D-Pad");
	}
}

} // namespace

float leftStickX, leftStickY, rightStickX, rightStickY;
float leftStickXUnscaled, leftStickYUnscaled, rightStickXUnscaled, rightStickYUnscaled;
bool leftStickNeedsScaling, rightStickNeedsScaling;

namespace {

void ScaleJoysticks()
{
	const Options &options = GetOptions();
	const float rightDeadzone = options.Controller.fDeadzone;
	const float leftDeadzone = options.Controller.fDeadzone;

	if (leftStickNeedsScaling) {
		leftStickX = leftStickXUnscaled;
		leftStickY = leftStickYUnscaled;
		ScaleJoystickAxes(&leftStickX, &leftStickY, leftDeadzone);
		leftStickNeedsScaling = false;
	}

	if (rightStickNeedsScaling) {
		rightStickX = rightStickXUnscaled;
		rightStickY = rightStickYUnscaled;
		ScaleJoystickAxes(&rightStickX, &rightStickY, rightDeadzone);
		rightStickNeedsScaling = false;
	}
}

} // namespace

bool IsControllerMotion(const SDL_Event &event)
{
#ifndef USE_SDL1
	if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
		return IsAnyOf(SDLC_EventGamepadAxis(event).axis,
		    SDL_GAMEPAD_AXIS_LEFTX,
		    SDL_GAMEPAD_AXIS_LEFTY,
		    SDL_GAMEPAD_AXIS_RIGHTX,
		    SDL_GAMEPAD_AXIS_RIGHTY);
	}
#endif

#if defined(JOY_AXIS_LEFTX) || defined(JOY_AXIS_LEFTY) || defined(JOY_AXIS_RIGHTX) || defined(JOY_AXIS_RIGHTY)
	if (event.type == SDL_EVENT_JOYSTICK_AXIS_MOTION) {
		switch (event.jaxis.axis) {
#ifdef JOY_AXIS_LEFTX
		case JOY_AXIS_LEFTX:
			return true;
#endif
#ifdef JOY_AXIS_LEFTY
		case JOY_AXIS_LEFTY:
			return true;
#endif
#ifdef JOY_AXIS_RIGHTX
		case JOY_AXIS_RIGHTX:
			return true;
#endif
#ifdef JOY_AXIS_RIGHTY
		case JOY_AXIS_RIGHTY:
			return true;
#endif
		default:
			return false;
		}
	}
#endif

	return false;
}

// Updates motion state for mouse and joystick sticks.
void ProcessControllerMotion(const SDL_Event &event)
{
#ifndef USE_SDL1
	GameController *const controller = GameController::Get(event);
	if (controller != nullptr && devilution::GameController::ProcessAxisMotion(event)) {
		ScaleJoysticks();
		SetSimulatingMouseWithPadmapper(false);
		return;
	}
#endif
	Joystick *const joystick = Joystick::Get(event);
	if (joystick != nullptr && devilution::Joystick::ProcessAxisMotion(event)) {
		ScaleJoysticks();
		SetSimulatingMouseWithPadmapper(false);
	}
}

AxisDirection GetLeftStickOrDpadDirection(bool usePadmapper)
{
	const float stickX = leftStickX;
	const float stickY = leftStickY;

	AxisDirection result { AxisDirectionX_NONE, AxisDirectionY_NONE };

	bool isUpPressed = stickY >= 0.5;
	bool isDownPressed = stickY <= -0.5;
	bool isLeftPressed = stickX <= -0.5;
	bool isRightPressed = stickX >= 0.5;

	if (usePadmapper) {
		isUpPressed |= PadmapperIsActionActive("MoveUp");
		isDownPressed |= PadmapperIsActionActive("MoveDown");
		isLeftPressed |= PadmapperIsActionActive("MoveLeft");
		isRightPressed |= PadmapperIsActionActive("MoveRight");
	} else if (!SimulatingMouseWithPadmapper) {
		isUpPressed |= IsPressedForMovement(ControllerButton_BUTTON_DPAD_UP);
		isDownPressed |= IsPressedForMovement(ControllerButton_BUTTON_DPAD_DOWN);
		isLeftPressed |= IsPressedForMovement(ControllerButton_BUTTON_DPAD_LEFT);
		isRightPressed |= IsPressedForMovement(ControllerButton_BUTTON_DPAD_RIGHT);
	}

#ifndef USE_SDL1
	if (ControlMode == ControlTypes::VirtualGamepad) {
		isUpPressed |= VirtualGamepadState.isActive && VirtualGamepadState.directionPad.isUpPressed;
		isDownPressed |= VirtualGamepadState.isActive && VirtualGamepadState.directionPad.isDownPressed;
		isLeftPressed |= VirtualGamepadState.isActive && VirtualGamepadState.directionPad.isLeftPressed;
		isRightPressed |= VirtualGamepadState.isActive && VirtualGamepadState.directionPad.isRightPressed;
	}
#endif

	if (isUpPressed) {
		result.y = AxisDirectionY_UP;
	} else if (isDownPressed) {
		result.y = AxisDirectionY_DOWN;
	}

	if (isLeftPressed) {
		result.x = AxisDirectionX_LEFT;
	} else if (isRightPressed) {
		result.x = AxisDirectionX_RIGHT;
	}

	return result;
}

void SimulateRightStickWithPadmapper(ControllerButtonEvent ctrlEvent)
{
	if (ctrlEvent.button == ControllerButton_NONE)
		return;
	if (!ctrlEvent.up && ctrlEvent.button == SuppressedButton)
		return;

	const std::string_view actionName = PadmapperActionNameTriggeredByButtonEvent(ctrlEvent);
	const bool upTriggered = actionName == "MouseUp";
	const bool downTriggered = actionName == "MouseDown";
	const bool leftTriggered = actionName == "MouseLeft";
	const bool rightTriggered = actionName == "MouseRight";
	if (!upTriggered && !downTriggered && !leftTriggered && !rightTriggered) {
		if (rightStickX == 0 && rightStickY == 0)
			SetSimulatingMouseWithPadmapper(false);
		return;
	}

	const bool upActive = (upTriggered && !ctrlEvent.up) || (!upTriggered && PadmapperIsActionActive("MouseUp"));
	const bool downActive = (downTriggered && !ctrlEvent.up) || (!downTriggered && PadmapperIsActionActive("MouseDown"));
	const bool leftActive = (leftTriggered && !ctrlEvent.up) || (!leftTriggered && PadmapperIsActionActive("MouseLeft"));
	const bool rightActive = (rightTriggered && !ctrlEvent.up) || (!rightTriggered && PadmapperIsActionActive("MouseRight"));

	rightStickX = 0;
	rightStickY = 0;
	if (upActive)
		rightStickY += 1.F;
	if (downActive)
		rightStickY -= 1.F;
	if (leftActive)
		rightStickX -= 1.F;
	if (rightActive)
		rightStickX += 1.F;
	SetSimulatingMouseWithPadmapper(true);
}

} // namespace devilution
