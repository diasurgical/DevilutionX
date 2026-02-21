/**
 * @file local_coop_button_mapper.cpp
 *
 * Implementation of button mapping utilities for local co-op.
 */
#include "controls/local_coop/local_coop_button_mapper.hpp"

#ifndef USE_SDL1
#ifdef USE_SDL3
#include <SDL3/SDL_gamepad.h>
#else
#include <SDL_gamecontroller.h>
#endif
#endif

namespace devilution {

int LocalCoopButtonMapper::GetSkillSlot(ControllerButton button)
{
	switch (button) {
	case ControllerButton_BUTTON_A:
		return ButtonToSkillSlot[0]; // A -> slot 2
	case ControllerButton_BUTTON_B:
		return ButtonToSkillSlot[1]; // B -> slot 3
	case ControllerButton_BUTTON_X:
		return ButtonToSkillSlot[2]; // X -> slot 0
	case ControllerButton_BUTTON_Y:
		return ButtonToSkillSlot[3]; // Y -> slot 1
	default:
		return -1;
	}
}

int LocalCoopButtonMapper::GetSkillSlot(uint8_t sdlButton)
{
#ifndef USE_SDL1
#ifdef USE_SDL3
	switch (sdlButton) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return ButtonToSkillSlot[0]; // A (South) -> slot 2
	case SDL_GAMEPAD_BUTTON_EAST:
		return ButtonToSkillSlot[1]; // B (East) -> slot 3
	case SDL_GAMEPAD_BUTTON_WEST:
		return ButtonToSkillSlot[2]; // X (West) -> slot 0
	case SDL_GAMEPAD_BUTTON_NORTH:
		return ButtonToSkillSlot[3]; // Y (North) -> slot 1
	default:
		return -1;
	}
#else
	// SDL2 uses SDL_GameControllerButton enum
	switch (sdlButton) {
	case SDL_CONTROLLER_BUTTON_A:
		return ButtonToSkillSlot[0]; // A -> slot 2
	case SDL_CONTROLLER_BUTTON_B:
		return ButtonToSkillSlot[1]; // B -> slot 3
	case SDL_CONTROLLER_BUTTON_X:
		return ButtonToSkillSlot[2]; // X -> slot 0
	case SDL_CONTROLLER_BUTTON_Y:
		return ButtonToSkillSlot[3]; // Y -> slot 1
	default:
		return -1;
	}
#endif
#else
	return -1; // SDL1 not supported
#endif
}

int LocalCoopButtonMapper::GetBeltSlot(int buttonIndex, bool leftShoulderHeld, bool rightShoulderHeld)
{
	if (!leftShoulderHeld && !rightShoulderHeld)
		return -1;

	if (buttonIndex < 0 || buttonIndex >= 4)
		return -1;

	const int baseSlot = leftShoulderHeld ? 0 : 4;
	return baseSlot + ButtonToBeltOffset[buttonIndex];
}

int LocalCoopButtonMapper::GetButtonIndex(uint8_t sdlButton)
{
#ifndef USE_SDL1
#ifdef USE_SDL3
	switch (sdlButton) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return 0; // A
	case SDL_GAMEPAD_BUTTON_EAST:
		return 1; // B
	case SDL_GAMEPAD_BUTTON_WEST:
		return 2; // X
	case SDL_GAMEPAD_BUTTON_NORTH:
		return 3; // Y
	default:
		return -1;
	}
#else
	// SDL2 uses SDL_GameControllerButton enum
	switch (sdlButton) {
	case SDL_CONTROLLER_BUTTON_A:
		return 0; // A
	case SDL_CONTROLLER_BUTTON_B:
		return 1; // B
	case SDL_CONTROLLER_BUTTON_X:
		return 2; // X
	case SDL_CONTROLLER_BUTTON_Y:
		return 3; // Y
	default:
		return -1;
	}
#endif
#else
	return -1; // SDL1 not supported
#endif
}

std::string_view LocalCoopButtonMapper::GetSkillSlotLabel(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= 4)
		return "";
	return SkillSlotLabels[slotIndex];
}

std::string_view LocalCoopButtonMapper::GetBeltButtonLabel(int buttonIndex)
{
	if (buttonIndex < 0 || buttonIndex >= 4)
		return "";
	return BeltButtonLabels[buttonIndex];
}

} // namespace devilution
