/**
 * @file local_coop.hpp
 *
 * Interface for local co-op multiplayer functionality.
 * Allows multiple players on the same screen using multiple controllers.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifndef USE_SDL1
#ifdef USE_SDL3
#include <SDL3/SDL_joystick.h>
#else
#include <SDL.h>
#endif
#endif

#include "DiabloUI/diabloui.h"
#include "controls/axis_direction.h"
#include "engine/surface.hpp"

namespace devilution {

/// Maximum number of local co-op players supported
constexpr size_t MaxLocalPlayers = 4;

/// State for a single local co-op player (players 2-4)
struct LocalCoopPlayer {
	bool active = false;
	bool initialized = false;

#ifndef USE_SDL1
	SDL_JoystickID controllerId = -1;
#endif

	// Stick values (scaled with deadzone)
	float leftStickX = 0;
	float leftStickY = 0;

	// Unscaled stick values for deadzone processing
	float leftStickXUnscaled = 0;
	float leftStickYUnscaled = 0;

	// Character selection state
	bool characterSelectActive = true;
	std::vector<_uiheroinfo> availableHeroes;
	int selectedHeroIndex = 0;

	// D-pad repeat timing for character selection
	uint32_t lastDpadPress = 0;

	/// Reset player state
	void Reset();

	/// Get the movement direction from stick input
	AxisDirection GetMoveDirection() const;
};

/// Global local co-op state
struct LocalCoopState {
	bool enabled = false;

	/// Local co-op players (index 0 = player 2, index 1 = player 3, etc.)
	/// Player 1 uses the existing input system
	std::array<LocalCoopPlayer, MaxLocalPlayers - 1> players;

	/// D-pad repeat delay in milliseconds
	static constexpr uint32_t DpadRepeatDelay = 300;

	/// Get number of active local co-op players (excluding player 1)
	[[nodiscard]] size_t GetActivePlayerCount() const;

	/// Get total number of players (including player 1)
	[[nodiscard]] size_t GetTotalPlayerCount() const;

	/// Check if any player has character selection active
	[[nodiscard]] bool IsAnyCharacterSelectActive() const;
};

extern LocalCoopState g_LocalCoop;

/**
 * @brief Initialize local co-op system.
 *
 * Detects connected controllers and assigns them to local players.
 * Should be called when starting a new game.
 */
void InitLocalCoop();

/**
 * @brief Shutdown local co-op system and reset all state.
 */
void ShutdownLocalCoop();

/**
 * @brief Check if local co-op is available (2+ controllers connected).
 */
bool IsLocalCoopAvailable();

/**
 * @brief Check if local co-op is currently enabled.
 */
bool IsLocalCoopEnabled();

#ifndef USE_SDL1
/**
 * @brief Get the controller ID from an SDL event.
 * @return The joystick ID, or -1 if not a controller event.
 */
SDL_JoystickID GetControllerIdFromEvent(const SDL_Event &event);

/**
 * @brief Check if an event belongs to a local co-op player (not player 1).
 * @return The local co-op player index (0-2), or -1 if it's player 1's controller.
 */
int GetLocalCoopPlayerIndex(const SDL_Event &event);

/**
 * @brief Get the game player ID for a local co-op player index.
 * @param localIndex Local co-op player index (0-2)
 * @return Game player ID (1-3)
 */
inline uint8_t LocalCoopIndexToPlayerId(int localIndex)
{
	return static_cast<uint8_t>(localIndex + 1);
}

/**
 * @brief Process SDL event for local co-op players.
 *
 * Handles axis motion and button presses for players 2-4.
 * @return true if the event was consumed by local co-op.
 */
bool ProcessLocalCoopInput(const SDL_Event &event);
#endif

/**
 * @brief Update movement for all local co-op players.
 *
 * Should be called in the game loop after player 1's movement.
 */
void UpdateLocalCoopMovement();

/**
 * @brief Load available heroes for local co-op player selection.
 *
 * Excludes heroes already selected by other local players.
 * @param localIndex Local co-op player index (0-2)
 */
void LoadAvailableHeroesForLocalPlayer(int localIndex);

/**
 * @brief Confirm character selection and spawn a local co-op player.
 * @param localIndex Local co-op player index (0-2)
 */
void ConfirmLocalCoopCharacter(int localIndex);

/**
 * @brief Draw local co-op character selection UI.
 *
 * Draws selection UI in the top-right area of the screen for each
 * local co-op player that hasn't confirmed their character yet.
 * @param out The surface to draw on.
 */
void DrawLocalCoopCharacterSelect(const Surface &out);

/**
 * @brief Handle a local co-op player controller being disconnected.
 * @param controllerId The disconnected controller's ID.
 */
void HandleLocalCoopControllerDisconnect(SDL_JoystickID controllerId);

/**
 * @brief Handle a new controller being connected.
 *
 * May assign the controller to a new local co-op player slot.
 * @param controllerId The connected controller's ID.
 */
void HandleLocalCoopControllerConnect(SDL_JoystickID controllerId);

/**
 * @brief Calculate the center view position for all active local co-op players.
 *
 * Returns the average position of all active local players to keep the camera
 * centered on all players. If only one player is active, returns their position.
 *
 * @return The tile position where the camera should be centered.
 */
Point CalculateLocalCoopViewPosition();

/**
 * @brief Update the view position to follow all local co-op players.
 *
 * Should be called during gameplay to keep the camera centered on all players.
 * Uses smooth interpolation to avoid jarring camera movements.
 */
void UpdateLocalCoopCamera();

/**
 * @brief Check if a tile position is within the visible screen boundaries.
 *
 * Used to prevent local co-op players from moving off-screen.
 * @param tilePos The tile position to check.
 * @return true if the position is within the visible screen area.
 */
bool IsLocalCoopPositionOnScreen(Point tilePos);

/**
 * @brief Attempt to join a new local co-op player mid-game.
 *
 * Called when a new controller connects during gameplay.
 * Spawns a new player near existing players if a slot is available.
 *
 * @param controllerId The connected controller's ID.
 * @return True if a new player was successfully joined, false otherwise.
 */
bool TryJoinLocalCoopMidGame(SDL_JoystickID controllerId);

/**
 * @brief Global toggle state for local co-op HUD visibility.
 *
 * Similar to PartySidePanelOpen for multiplayer party info.
 */
extern bool LocalCoopHUDOpen;

/**
 * @brief Draw HUD for all local co-op players.
 *
 * Shows player name with level, and HP/Mana bars in each corner:
 * - Bottom-left: Player 1
 * - Bottom-right: Player 2  
 * - Top-left: Player 3
 * - Top-right: Player 4
 *
 * @param out The surface to draw on.
 */
void DrawLocalCoopPlayerHUD(const Surface &out);

} // namespace devilution
