#pragma once

#include <cstdint>

#include "controls/controller_buttons.h"
#include "utils/attributes.h"

namespace devilution {

enum class ControlTypes : uint8_t {
	None,
	KeyboardAndMouse,
	Gamepad,
	VirtualGamepad,
};

extern DVL_API_FOR_TEST ControlTypes ControlMode;

/**
 * @brief Controlling device type.
 *
 * While simulating a mouse, `ControlMode` is set to `KeyboardAndMouse`,
 * even though a gamepad is used to control it.
 *
 * This value is always set to the actual active device type.
 */
extern ControlTypes ControlDevice;

extern GamepadLayout GamepadType;

} // namespace devilution
