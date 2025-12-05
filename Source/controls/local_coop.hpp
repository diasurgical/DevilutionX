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

struct Object;
struct Player;
struct Missile;

/// Per-player cursor/targeting state for local co-op
struct LocalCoopCursorState {
	int pcursmonst = -1;        ///< Monster under cursor (-1 = none)
	int8_t pcursitem = -1;      ///< Item under cursor (-1 = none)
	Object *objectUnderCursor = nullptr;  ///< Object under cursor
	const Player *playerUnderCursor = nullptr;  ///< Player under cursor
	Point cursPosition = { -1, -1 };  ///< Cursor tile position
	Missile *pcursmissile = nullptr;  ///< Missile/portal under cursor
	int pcurstrig = -1;         ///< Trigger under cursor
	
	/// Reset all cursor state
	void Reset();
};

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
	
	// Per-player cursor/targeting state
	LocalCoopCursorState cursor;
	
	// Action state (similar to ControllerActionHeld for player 1)
	uint8_t actionHeld = 0; // GameActionType
	
	// Save slot number for this player (used for saving progress)
	uint32_t saveNumber = 0;

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
	
	/// Panel ownership: which player controls the currently open panels
	/// -1 = no owner (player 1 by default), 0-2 = local coop player index
	int panelOwner = -1;
	
	/// Store ownership: which player is currently using a store/shop
	/// -1 = no owner (player 1 by default), 0-2 = local coop player index
	/// When set, MyPlayer is temporarily switched to the store owner
	int storeOwner = -1;
	
	/// Saved MyPlayer pointer when store ownership is active
	Player* savedMyPlayer = nullptr;
	
	/// Check if a local coop player owns the panels
	[[nodiscard]] bool HasPanelOwner() const { return panelOwner >= 0; }
	
	/// Get the game player ID that owns panels (0 for player 1, 1-3 for local coop)
	[[nodiscard]] uint8_t GetPanelOwnerPlayerId() const;
	
	/// Try to claim panel ownership for a local coop player
	/// Returns true if ownership was granted
	bool TryClaimPanelOwnership(int localIndex);
	
	/// Release panel ownership (call when all panels are closed)
	void ReleasePanelOwnership();

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
 * @brief Get the local co-op player index for a game player ID.
 * @param playerId Game player ID (0-3)
 * @return Local co-op player index (0-2), or -1 if it's player 1 (ID 0)
 */
inline int PlayerIdToLocalCoopIndex(uint8_t playerId)
{
	if (playerId == 0)
		return -1; // Player 1 is not a local co-op player
	return static_cast<int>(playerId - 1);
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

/**
 * @brief Update target selection for a local co-op player.
 *
 * Finds monsters, NPCs, items, objects and triggers near the player
 * and updates their cursor state accordingly. This is the local coop
 * equivalent of plrctrls_after_check_curs_move().
 *
 * @param localIndex Local co-op player index (0-2)
 */
void UpdateLocalCoopTargetSelection(int localIndex);

/**
 * @brief Process a game action for a local co-op player.
 *
 * Handles panel toggles, primary/secondary actions, spells, etc.
 * When a panel is opened, that player gains panel ownership.
 *
 * @param localIndex Local co-op player index (0-2)
 * @param actionType The game action type to process
 */
void ProcessLocalCoopGameAction(int localIndex, uint8_t actionType);

/**
 * @brief Perform primary action for a local co-op player.
 *
 * Attack monsters, talk to NPCs, interact with the world.
 * Uses the player's own cursor state for targeting.
 *
 * @param localIndex Local co-op player index (0-2)
 */
void PerformLocalCoopPrimaryAction(int localIndex);

/**
 * @brief Perform secondary action for a local co-op player.
 *
 * Pick up items, open chests/doors, interact with objects.
 *
 * @param localIndex Local co-op player index (0-2)
 */
void PerformLocalCoopSecondaryAction(int localIndex);

/**
 * @brief Check if panels are currently open and owned by anyone.
 */
bool AreLocalCoopPanelsOpen();

/**
 * @brief Get the player who currently owns the UI panels.
 * @return Pointer to the player who owns panels, or nullptr if player 1 owns them
 */
Player* GetLocalCoopPanelOwnerPlayer();

/**
 * @brief Save all local co-op players to their respective save files.
 *
 * Called when the game saves. Each local co-op player is saved to the
 * save slot they originally loaded from.
 *
 * @param writeGameData Whether to also save game/level data
 */
void SaveLocalCoopPlayers(bool writeGameData);

/**
 * @brief Set store ownership for a local co-op player.
 *
 * When a local co-op player talks to a shopkeeper, this function
 * temporarily sets MyPlayer to their player so the store UI shows
 * the correct inventory/gold.
 *
 * @param localIndex Local co-op player index (0-2), or -1 to clear
 */
void SetLocalCoopStoreOwner(int localIndex);

/**
 * @brief Clear store ownership and restore MyPlayer.
 *
 * Called when the store is closed.
 */
void ClearLocalCoopStoreOwner();

/**
 * @brief Check if a local co-op player owns the store.
 */
bool IsLocalCoopStoreActive();

/**
 * @brief Get the player ID that owns the store.
 * @return Player ID (0-3), or 0 if player 1 owns it
 */
uint8_t GetLocalCoopStoreOwnerPlayerId();

} // namespace devilution
