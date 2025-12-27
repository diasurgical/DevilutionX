/**
 * @file local_coop_constants.hpp
 *
 * Centralized constants for local co-op system.
 * Extracted from local_coop.cpp to improve maintainability.
 */
#pragma once

#include <cstdint>
#include <array>

namespace devilution {

namespace LocalCoopLayout {

/// Panel dimensions and layout
namespace Panel {
	constexpr int Height = 87;
	constexpr int EdgePadding = 0;
	constexpr int TopBorderPadding = 5;
	constexpr int LeftBorderPadding = 7;
	constexpr int RightBorderPadding = 8;
	constexpr int Padding = 4;
	constexpr int Border = 2;
}

/// Panel content layout
namespace PanelContent {
	constexpr int NameTopOffset = 4;
	constexpr int ElementSpacing = 1;
	constexpr int BarsExtraDownOffset = 1;
	constexpr int FieldHeight = 20;
	constexpr int FieldPaddingTop = 2;
	constexpr int FieldPaddingSide = 4;
}

/// Health and mana bar dimensions
namespace HealthBar {
	constexpr int BoxWidth = 234;
	constexpr int BoxHeight = 12;
	constexpr int Border = 3;
	constexpr int Width = BoxWidth - (Border * 2) - 2;
	constexpr int Height = BoxHeight - (Border * 2) - 2;
	constexpr int Spacing = 1;
}

/// Experience bar dimensions
namespace ExperienceBar {
	constexpr int Height = 7;
	constexpr int Spacing = 1;
}

/// Belt dimensions and layout
namespace Belt {
	constexpr int SlotSize = 28; // Same as INV_SLOT_SIZE_PX
	constexpr int SlotSpacing = 1;
	constexpr int SlotsPerRow = 8;
	constexpr int BorderWidth = 237;
	constexpr int PanelX = 205;
	constexpr int PanelY = 21;
	constexpr int SlotSpacingPx = 29; // Spacing between belt slots in pixels
}

/// Skill slot dimensions
namespace SkillSlot {
	constexpr int Count = 4; // Display first 4 hotkey slots
	constexpr int OriginalHintBoxSize = 39;
	constexpr int OriginalIconSize = 37;
	constexpr int OriginalIconHeight = 38;
	constexpr int IconBorderPadding = 2;
	constexpr int OriginalLabelOffset = 14;
	constexpr int OriginalLabelWidth = 12;
	constexpr int GridSpacingX = 2;
	constexpr int GridSpacingY = 1;
	constexpr int VerticalSpacing = 2;
	constexpr int VerticalTopOffset = 1;
}

/// Character selection UI
namespace CharacterSelect {
	constexpr int BoxWidth = 220;
	constexpr int BoxHeight = 55;
	constexpr int Padding = 10;
}

/// Durability icon display
namespace DurabilityIcon {
	constexpr int Height = 32;
	constexpr int Spacing = 4;
}

/// Panel width calculation
namespace PanelWidth {
	constexpr int LeftBorderPadding = 7;
	constexpr int RightBorderPadding = 7;
	constexpr int MinBeltWidth = 245;
	constexpr int BaseContentWidth = MinBeltWidth - 7;
	constexpr int BaseWidth = LeftBorderPadding + BaseContentWidth + RightBorderPadding;
}

} // namespace LocalCoopLayout

namespace LocalCoopInput {

/// Button to skill slot mapping
/// Maps controller buttons to skill slot indices: A=2, B=3, X=0, Y=1
constexpr std::array<int, 4> ButtonToSkillSlot = {
	2, // A (South) -> slot 2
	3, // B (East) -> slot 3
	0, // X (West) -> slot 0
	1  // Y (North) -> slot 1
};

/// Button to belt slot offset mapping (when shoulder held)
/// A=0, B=1, X=2, Y=3
constexpr std::array<int, 4> ButtonToBeltOffset = { 0, 1, 2, 3 };

/// Skill button hold duration in milliseconds
constexpr uint32_t SkillButtonHoldTime = 500;

/// D-pad repeat delay in milliseconds
constexpr uint32_t DpadRepeatDelay = 300;

/// Joystick deadzone threshold
constexpr float JoystickDeadzone = 0.25f;

/// Movement stick threshold
constexpr float MovementStickThreshold = 0.5f;

/// Trigger threshold for button press detection
constexpr int16_t TriggerThreshold = 8192;

} // namespace LocalCoopInput

namespace LocalCoopCamera {

/// Dead zone radius in screen pixels
constexpr int DeadZone = 32;

/// Camera smoothing factor (0.0 = no smoothing, 1.0 = instant)
constexpr float SmoothFactor = 0.25f;

/// Fixed-point scaling factor (256 = 1.0)
constexpr int64_t FixedPointScale = 256;

} // namespace LocalCoopCamera

namespace LocalCoopTargeting {

/// Maximum distance for melee targeting (in tiles)
constexpr int MaxMeleeDistance = 25;

/// Maximum distance for ranged targeting
constexpr int MaxRangedDistance = 0; // Uses exact distance

/// Maximum distance for trigger/portal detection
constexpr int MaxTriggerDistance = 2;

/// Maximum distance for towner interaction
constexpr int MaxTownerDistance = 2;

} // namespace LocalCoopTargeting

} // namespace devilution

