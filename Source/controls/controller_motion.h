#pragma once

// Processes and stores mouse and joystick motion.

#include <SDL.h>

#include "./axis_direction.h"
#include "./controller.h"

namespace devilution {
// Returns true if gamepad aiming is currently active (right stick or padmapped mouse).
bool IsGamepadAimActive();

// Raw axis values.
extern float leftStickXUnscaled, leftStickYUnscaled, rightStickXUnscaled, rightStickYUnscaled;

// Axis values scaled to [-1, 1] range and clamped to a deadzone.
extern float leftStickX, leftStickY, rightStickX, rightStickY;

// Whether stick positions have been updated and need rescaling.
extern bool leftStickNeedsScaling, rightStickNeedsScaling;

// Updates motion state for mouse and joystick sticks.
void ProcessControllerMotion(const SDL_Event &event);

// Indicates whether the event represents movement of an analog thumbstick.
bool IsControllerMotion(const SDL_Event &event);

// Returns direction of the left thumb stick or DPad (if allow_dpad = true).
AxisDirection GetLeftStickOrDpadDirection(bool usePadmapper);

// Simulates right-stick movement based on input from padmapper mouse movement actions.
void SimulateRightStickWithPadmapper(ControllerButtonEvent ctrlEvent);

} // namespace devilution
