#pragma once

// Processes and stores mouse and joystick motion.

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "controls/axis_direction.h"
#include "controls/controller.h"

namespace devilution {

// Whether we're currently simulating the mouse with SELECT + D-Pad.
extern bool SimulatingMouseWithPadmapper;

// Raw axis values.
extern float leftStickXUnscaled, leftStickYUnscaled, rightStickXUnscaled, rightStickYUnscaled;

// Axis values scaled to [-1, 1] range and clamped to a deadzone.
extern float leftStickX, leftStickY, rightStickX, rightStickY;

// Whether stick positions have been updated and need rescaling.
extern bool leftStickNeedsScaling, rightStickNeedsScaling;

// Minimum scaled stick magnitude to register a direction.
constexpr float StickDirectionThreshold = 0.4F;

// Returns the lower bound of the "turn-only" stick band, derived from the user-configured
// controller deadzone. Between this value and StickDirectionThreshold the player rotates in place
// to face the stick direction without walking, so that aiming (e.g. the Amazon's ranged attack)
// does not require taking a step.
float GetStickTurnThreshold();

// Updates motion state for mouse and joystick sticks.
void ProcessControllerMotion(const SDL_Event &event);

// Indicates whether the event represents movement of an analog thumbstick.
bool IsControllerMotion(const SDL_Event &event);

// Returns direction of the left thumb stick or DPad (if allow_dpad = true).
AxisDirection GetLeftStickOrDpadDirection(bool usePadmapper);

// Returns a lower-sensitivity direction used solely for turn-without-movement (aiming in place).
// Active when the scaled stick magnitude is in [GetStickTurnThreshold(), StickDirectionThreshold)
// so gentle stick nudges rotate the player without issuing a walk command.
AxisDirection GetLeftStickTurnDirection();

// Simulates right-stick movement based on input from padmapper mouse movement actions.
void SimulateRightStickWithPadmapper(ControllerButtonEvent ctrlEvent);

} // namespace devilution
