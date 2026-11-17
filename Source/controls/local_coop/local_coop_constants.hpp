/**
 * @file local_coop_constants.hpp
 *
 * Centralized constants for local co-op system.
 * Extracted from local_coop.cpp to improve maintainability.
 */
#pragma once

#include <cstdint>

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
} // namespace Panel

/// Panel content layout
namespace PanelContent {
constexpr int NameTopOffset = 4;
constexpr int ElementSpacing = 1;
constexpr int BarsExtraDownOffset = 1;
constexpr int FieldHeight = 20;
constexpr int FieldPaddingTop = 2;
constexpr int FieldPaddingSide = 4;
} // namespace PanelContent

/// Health and mana bar dimensions
namespace HealthBar {
constexpr int BoxWidth = 234;
constexpr int BoxHeight = 12;
constexpr int Border = 3;
constexpr int Width = BoxWidth - (Border * 2) - 2;
constexpr int Height = BoxHeight - (Border * 2) - 2;
constexpr int HeightXpEnabled = 11; // Reduced height when XP bar is enabled
constexpr int Spacing = 1;
} // namespace HealthBar

/// Experience bar dimensions
namespace ExperienceBar {
constexpr int Height = 7;
constexpr int Spacing = 1;
constexpr int MaxValue = 10000; // Used for percentage calculation (10000 = 100%)
} // namespace ExperienceBar

/// Belt dimensions and layout
namespace Belt {
constexpr int SlotSize = 28; // Same as INV_SLOT_SIZE_PX
constexpr int SlotSpacing = 1;
constexpr int SlotsPerRow = 8;
constexpr int BorderWidth = 237;
constexpr int PanelX = 205;
constexpr int PanelY = 21;
constexpr int SlotSpacingPx = 29; // Spacing between belt slots in pixels
} // namespace Belt

/// Active spell slot, drawn inside the panel next to the bars/belt section
namespace SpellSlot {
constexpr int IconSize = 56;         // Large spell icon, drawn unscaled (same as the main panel spell button)
constexpr int BorderOverhang = 3;    // Golden border extends 3px left/top of the icon (same as the belt)
constexpr int BorderExtraWidth = 5;  // Golden border is 5px wider than the icon (same as the belt)
constexpr int BorderExtraHeight = 6; // Golden border is 6px taller than the icon (same as the belt)
constexpr int BorderGap = 1;         // Gap between the belt border and the spell border
constexpr int SectionWidth = 62;     // Extra panel width taken by the spell slot
constexpr int VerticalOffset = 1;    // Offset from vertically centered in the panel
} // namespace SpellSlot

/// Character selection UI
namespace CharacterSelect {
constexpr int BoxWidth = 220;
constexpr int BoxHeight = 55;
constexpr int Padding = 10;
} // namespace CharacterSelect

/// Durability icon display
namespace DurabilityIcon {
constexpr int Height = 32;
constexpr int Spacing = 4;
} // namespace DurabilityIcon

/// Panel width calculation
namespace PanelWidth {
constexpr int LeftBorderPadding = 7;
constexpr int RightBorderPadding = 7;
constexpr int MinBeltWidth = 245;
constexpr int BaseContentWidth = MinBeltWidth - 7;
constexpr int BaseWidth = LeftBorderPadding + BaseContentWidth + RightBorderPadding;
} // namespace PanelWidth

/// Shared-screen bounds for local co-op (IsTilePositionOnScreen)
namespace SharedScreen {
constexpr int TileToScreenXMultiplier = 32;
constexpr int TileToScreenYMultiplier = 16;
constexpr int MaxDistanceFromRightEdge = 100;
constexpr int MaxDistanceFromViewportHalfHeight = 80;
constexpr int NormalizationScale = 1000;
constexpr int64_t EllipseOutsideThresholdSq = 1000000;
constexpr uint8_t ExcludeNoPlayer = 255;
} // namespace SharedScreen

} // namespace LocalCoopLayout

namespace LocalCoopInput {

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

/// Fixed-point scaling factor (256 = 1.0)
constexpr int64_t FixedPointScale = 256;

/// How long the camera takes to ease to its new target after a jump (player joined, died or teleported)
constexpr uint32_t TransitionDurationMs = 250;

/// A frame-to-frame target movement larger than this (in screen pixels) counts as a jump.
/// Walking moves far less than this per rendered frame.
constexpr int64_t JumpThreshold = 32;

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
