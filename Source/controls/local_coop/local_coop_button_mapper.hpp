/**
 * @file local_coop_button_mapper.hpp
 *
 * Button mapping utilities for local co-op controls.
 * Consolidates mapping logic between controller buttons, SDL buttons, skill slots, and belt slots.
 */
#pragma once

#include <cstdint>
#include <string_view>

#include "controls/controller_buttons.h"

namespace devilution {

/**
 * @brief Centralized button mapping utilities for local co-op.
 *
 * This class provides consistent mapping between:
 * - Controller buttons (ControllerButton enum)
 * - SDL gamepad buttons (uint8_t)
 * - Skill slots (0-3)
 * - Belt slots (0-7)
 * - Button labels ("A", "B", "X", "Y")
 */
class LocalCoopButtonMapper {
public:
	/**
	 * @brief Get skill slot index from ControllerButton.
	 * @param button The controller button
	 * @return Skill slot index (0-3), or -1 if not a skill button
	 *
	 * Mapping: A=2, B=3, X=0, Y=1
	 */
	static int GetSkillSlot(ControllerButton button);

	/**
	 * @brief Get skill slot index from SDL gamepad button.
	 * @param button The SDL gamepad button
	 * @return Skill slot index (0-3), or -1 if not a skill button
	 *
	 * Mapping: South/A=2, East/B=3, West/X=0, North/Y=1
	 */
	static int GetSkillSlot(uint8_t sdlButton);

	/**
	 * @brief Calculate belt slot index from button index and shoulder button state.
	 * @param buttonIndex Button index (0=A, 1=B, 2=X, 3=Y)
	 * @param leftShoulderHeld True if left shoulder is held (belt slots 0-3)
	 * @param rightShoulderHeld True if right shoulder is held (belt slots 4-7)
	 * @return Belt slot index (0-7), or -1 if no shoulder is held
	 */
	static int GetBeltSlot(int buttonIndex, bool leftShoulderHeld, bool rightShoulderHeld);

	/**
	 * @brief Get button index from SDL gamepad button.
	 * @param sdlButton The SDL gamepad button
	 * @return Button index (0-3), or -1 if not a face button
	 *
	 * Mapping: South/A=0, East/B=1, West/X=2, North/Y=3
	 */
	static int GetButtonIndex(uint8_t sdlButton);

	/**
	 * @brief Get button label string for skill slot display.
	 * @param slotIndex Skill slot index (0-3)
	 * @return Button label ("X", "Y", "A", "B"), or empty string if invalid
	 *
	 * Mapping: slot 0=X, slot 1=Y, slot 2=A, slot 3=B
	 */
	static std::string_view GetSkillSlotLabel(int slotIndex);

	/**
	 * @brief Get button label string for belt slot display.
	 * @param buttonIndex Button index (0-3)
	 * @return Button label ("A", "B", "X", "Y"), or empty string if invalid
	 *
	 * Mapping: index 0=A, index 1=B, index 2=X, index 3=Y
	 */
	static std::string_view GetBeltButtonLabel(int buttonIndex);

private:
	// Button to skill slot mapping: A=2, B=3, X=0, Y=1
	static constexpr int ButtonToSkillSlot[4] = { 2, 3, 0, 1 };

	// Button to belt offset mapping: A=0, B=1, X=2, Y=3
	static constexpr int ButtonToBeltOffset[4] = { 0, 1, 2, 3 };

	// Skill slot button labels: slot 0=X, slot 1=Y, slot 2=A, slot 3=B
	static constexpr std::string_view SkillSlotLabels[4] = { "X", "Y", "A", "B" };

	// Belt button labels: button 0=A, button 1=B, button 2=X, button 3=Y
	static constexpr std::string_view BeltButtonLabels[4] = { "A", "B", "X", "Y" };
};

} // namespace devilution
