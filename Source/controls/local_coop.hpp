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
#include "controls/controller_buttons.h"
#include "engine/displacement.hpp"
#include "engine/surface.hpp"

namespace devilution {

/// Maximum number of local co-op players supported
constexpr size_t MaxLocalPlayers = 4;

struct Object;
struct Player;
struct Missile;

/// Per-player cursor/targeting state for local co-op
struct LocalCoopCursorState {
	int pcursmonst = -1;                       ///< Monster under cursor (-1 = none)
	int8_t pcursitem = -1;                     ///< Item under cursor (-1 = none)
	Object *objectUnderCursor = nullptr;       ///< Object under cursor
	const Player *playerUnderCursor = nullptr; ///< Player under cursor
	Point cursPosition = { -1, -1 };           ///< Cursor tile position
	Missile *pcursmissile = nullptr;           ///< Missile/portal under cursor
	int pcurstrig = -1;                        ///< Trigger under cursor

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

	// D-pad values for movement (-1, 0, or 1)
	int dpadX = 0;
	int dpadY = 0;

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

	// Skill button hold state for long-press quick spell assignment
	// -1 = no button held, 0-3 = skill slot being held
	int skillButtonHeld = -1;
	uint32_t skillButtonPressTime = 0;
	bool skillMenuOpenedByHold = false; // Track if we opened the menu via long press

	// Trigger state for inventory/character screen (like player 1)
	bool leftTriggerPressed = false;  // Character screen
	bool rightTriggerPressed = false; // Inventory screen

	// Shoulder button hold state for belt item access
	bool leftShoulderHeld = false;  // When held, shows A/B/X/Y labels on belt slots 1-4
	bool rightShoulderHeld = false; // When held, shows A/B/X/Y labels on belt slots 5-8

	/// Reset player state
	void Reset();

	/// Get the movement direction from stick and D-pad input
	AxisDirection GetMoveDirection() const;
};

/// Global local co-op state
struct LocalCoopState {
	bool enabled = false;

	/// Unified player array (index 0 = player 1, index 1 = player 2, etc.)
	/// In local coop mode, all players use the same LocalCoopPlayer structure
	/// Player 1 (index 0) is always marked as active when local coop is enabled
	std::array<LocalCoopPlayer, MaxLocalPlayers> players;

	/// D-pad repeat delay in milliseconds
	static constexpr uint32_t DpadRepeatDelay = 300;

	/// Panel ownership: which game player ID controls the currently open panels
	/// 0 = player 1 (default), 1-3 = local coop players 2-4
	/// Use -1 to indicate no explicit owner (falls back to player 1)
	int panelOwnerPlayerId = -1;

	/// Store ownership: which game player ID is currently using a store/shop
	/// -1 = no owner (player 1 by default), 1-3 = local coop players 2-4
	/// When set, MyPlayer is temporarily switched to the store owner
	int storeOwnerPlayerId = -1;

	/// Spell menu ownership: which game player ID has the quick spell menu open
	/// -1 = no owner (falls back to player 1), 0-3 = game player ID
	/// While open, MyPlayer/InspectPlayer are set to the owning player
	int spellMenuOwnerPlayerId = -1;

	/// Saved MyPlayer pointer when store ownership is active
	Player *savedMyPlayer = nullptr;

	/// Camera offset for smooth scrolling (calculated by UpdateLocalCoopCamera)
	/// These store the averaged walking offset of all players in screen pixels
	int cameraOffsetX = 0;
	int cameraOffsetY = 0;

	/// Camera target position in screen space (scaled by 256 for precision)
	/// Used for dead zone calculations - camera only moves when target exceeds dead zone
	int64_t cameraTargetScreenX = 0;
	int64_t cameraTargetScreenY = 0;
	bool cameraInitialized = false;

	/// Smoothed camera position (what's actually rendered) in screen space (scaled by 256)
	/// The camera smoothly interpolates from current position to target position
	int64_t cameraSmoothScreenX = 0;
	int64_t cameraSmoothScreenY = 0;

	/// Dead zone radius in screen pixels - camera won't move if average player position
	/// is within this distance from the current camera target
	static constexpr int CameraDeadZone = 32;

	/// Camera smoothing factor (0.0 = no smoothing, 1.0 = instant)
	/// Higher values = faster camera response but potentially jerky movement
	static constexpr float CameraSmoothFactor = 0.25f;

	/// Check if a local coop player owns the panels (not player 1)
	[[nodiscard]] bool HasPanelOwner() const { return panelOwnerPlayerId > 0; }

	/// Get the game player ID that owns panels (0 for player 1, 1-3 for local coop)
	[[nodiscard]] uint8_t GetPanelOwnerPlayerId() const;

	/// Try to claim panel ownership for a game player
	/// @param playerId Game player ID (0 = player 1, 1-3 = coop players)
	/// Returns true if ownership was granted
	bool TryClaimPanelOwnership(uint8_t playerId);

	/// Release panel ownership (call when all panels are closed)
	void ReleasePanelOwnership();

	/// Get number of active local co-op players (excluding player 1)
	/// Note: "active" means a controller is assigned, not necessarily spawned
	[[nodiscard]] size_t GetActivePlayerCount() const;

	/// Get number of initialized/spawned local co-op players (excluding player 1)
	/// Use this to check if any co-op player has actually joined the game
	[[nodiscard]] size_t GetInitializedPlayerCount() const;

	/// Get total number of players (including player 1)
	[[nodiscard]] size_t GetTotalPlayerCount() const;

	/// Check if any player has character selection active
	[[nodiscard]] bool IsAnyCharacterSelectActive() const;

	/// Get the LocalCoopPlayer for any player (including player 1)
	/// @param playerId Game player ID (0-3)
	/// @return Pointer to LocalCoopPlayer, or nullptr if invalid
	[[nodiscard]] LocalCoopPlayer *GetPlayer(uint8_t playerId);
	[[nodiscard]] const LocalCoopPlayer *GetPlayer(uint8_t playerId) const;

	/// Get the LocalCoopPlayer for a coop player (players 2-4) - deprecated, use GetPlayer instead
	/// @param playerId Game player ID (1-3 for coop players)
	/// @return Pointer to LocalCoopPlayer, or nullptr if invalid or player 1
	[[nodiscard]] LocalCoopPlayer *GetCoopPlayer(uint8_t playerId);
	[[nodiscard]] const LocalCoopPlayer *GetCoopPlayer(uint8_t playerId) const;
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
 * @brief Initialize local co-op HUD assets (sprites for panel, health bars, etc.)
 * Called automatically when drawing the HUD for the first time.
 */
void InitLocalCoopHUDAssets();

/**
 * @brief Free local co-op HUD assets.
 * Should be called during shutdown.
 */
void FreeLocalCoopHUDAssets();

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

/**
 * @brief Check if any local co-op player (other than player 1) has spawned/initialized.
 * When true, corner HUDs are shown and main panel is hidden.
 */
bool IsAnyLocalCoopPlayerInitialized();

/**
 * @brief Get total player count including player 1.
 * @return Total number of players (1 + active local coop players)
 */
size_t GetLocalCoopTotalPlayerCount();

/**
 * @brief Check if an object is being targeted by any local coop player.
 * @param object The object to check
 * @return true if any local coop player has this object under their cursor
 */
bool IsLocalCoopTargetObject(const Object *object);

/**
 * @brief Load available heroes for all active local coop players.
 * Should be called when game is initialized to populate character selection.
 */
void LoadAvailableHeroesForAllLocalCoopPlayers();

/**
 * @brief Sync all local coop players to a new level.
 * Called when any player triggers a level change.
 * @param fom The interface mode (level change type)
 * @param lvl The target level
 */
void SyncLocalCoopPlayersToLevel(interface_mode fom, int lvl);

/**
 * @brief Reset local coop camera state.
 * Should be called when entering a new level to re-center camera.
 */
void ResetLocalCoopCamera();

/**
 * @brief Find the first local coop player standing on a trigger.
 * @param callback Function to call with playerId and trigger index when found
 * @return Pointer to the triggering player, or nullptr if none
 */
Player *FindLocalCoopPlayerOnTrigger(int &outTriggerIndex);

/**
 * @brief Check if any local player (including Player 1) is walking.
 * @param outDirection If a player is walking, their direction is stored here
 * @return true if any player is walking
 */
bool IsAnyLocalPlayerWalking(Direction &outDirection);

/**
 * @brief Set a player's shoulder button held state.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param isLeft true for left shoulder, false for right shoulder
 * @param held Whether the button is held
 */
void SetPlayerShoulderHeld(uint8_t playerId, bool isLeft, bool held);

/**
 * @brief Check if a player's shoulder button is held.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param isLeft true for left shoulder, false for right shoulder
 * @return true if the shoulder button is held
 */
bool IsPlayerShoulderHeld(uint8_t playerId, bool isLeft);

/**
 * @brief Get belt slot from button when shoulder is held for a player.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param button The face button (A/B/X/Y)
 * @return Belt slot 0-7 if shoulder is held and valid button, or -1 if not applicable
 */
int GetPlayerBeltSlotFromButton(uint8_t playerId, ControllerButton button);

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
 * @brief Check if a controller ID belongs to a local co-op player.
 * @param controllerId The controller ID to check.
 * @return true if the controller is assigned to a local co-op player.
 */
bool IsLocalCoopControllerId(SDL_JoystickID controllerId);

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
 * @brief Check if a player is a local co-op player (player 2-4 in local co-op mode).
 * @param player The player to check
 * @return true if the player is a local co-op player
 */
bool IsLocalCoopPlayer(const Player &player);

/**
 * @brief Check if a player is controlled locally (MyPlayer or a local co-op player).
 *
 * This is the primary function to use when you need to check if a player should
 * receive local processing (e.g., item pickup, spell casting, damage application).
 * Use this instead of checking `&player == MyPlayer` when local co-op is supported.
 *
 * @param player The player to check
 * @return true if the player is MyPlayer or a local co-op player
 */
bool IsLocalPlayer(const Player &player);

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
 * @brief Update skill button hold states for quick spell menu.
 *
 * Checks if any skill button has been held long enough to open quick spell menu.
 * Should be called in the game loop. Handles both player 1 and local coop players.
 */
void UpdateLocalCoopSkillButtons();

/**
 * @brief Handle skill button press for any player when local coop is enabled.
 *
 * Tracks button hold state for long-press to open quick spell menu.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param slotIndex Skill slot index (0=X, 1=Y, 2=A, 3=B)
 * @return true if the press was handled (should not be processed further)
 */
bool HandlePlayerSkillButtonDown(uint8_t playerId, int slotIndex);

/**
 * @brief Handle skill button release for any player when local coop is enabled.
 *
 * If short press, casts the spell. If long press was used to open menu, does nothing.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param slotIndex Skill slot index (0=X, 1=Y, 2=A, 3=B)
 * @return true if the release was handled
 */
bool HandlePlayerSkillButtonUp(uint8_t playerId, int slotIndex);

/**
 * @brief Unified button press handler for any player in local coop mode.
 *
 * Handles all button presses (face buttons, shoulders, triggers) for any player
 * including Player 1. This consolidates the logic that was previously split
 * between game_controls.cpp and local_coop.cpp.
 *
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param button The button that was pressed
 * @return true if the button was handled and should not be processed further
 */
bool HandleLocalCoopButtonPress(uint8_t playerId, ControllerButton button);

/**
 * @brief Unified button release handler for any player in local coop mode.
 *
 * Handles all button releases for any player including Player 1.
 *
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param button The button that was released
 * @return true if the button was handled and should not be processed further
 */
bool HandleLocalCoopButtonRelease(uint8_t playerId, ControllerButton button);

/**
 * @brief Assign spell to a player's skill slot from quick spell menu.
 *
 * Called when a player selects a spell while the quick spell menu is open.
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param slotIndex Skill slot to assign (0-3)
 */
void AssignPlayerSpellToSlot(uint8_t playerId, int slotIndex);

/**
 * @brief Draw skill slots for player 1 on the main panel.
 *
 * Displays skill slots similar to local coop players, next to the belt.
 * @param out Output surface to draw to
 */
void DrawPlayer1SkillSlots(const Surface &out);

/**
 * @brief Handle spell selection for local co-op when quick spell menu is open.
 *
 * Called when a spell is selected from the quick spell menu while a local coop
 * player is assigning skills.
 * @param localIndex Local co-op player index (0-2)
 * @param slotIndex Skill slot to assign (0-3)
 * @deprecated Use AssignPlayerSpellToSlot with playerId instead
 */
void AssignLocalCoopSpellToSlot(int localIndex, int slotIndex);

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
 * @brief Calculate the camera offset for smooth scrolling in local co-op mode.
 *
 * Returns the average walking offset of all active players. This is used by the
 * rendering code to produce smooth camera movement that follows all players.
 *
 * @return The camera offset displacement in pixels, or zero if local co-op is not active.
 */
Displacement GetLocalCoopCameraOffset();

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
 * @brief Handle panel toggle actions for any player when local coop is enabled.
 *
 * Routes panel actions through the panel ownership system so all players
 * are treated the same way.
 *
 * @param playerId Game player ID (0 = player 1, 1-3 = coop players)
 * @param actionType The panel action type (TOGGLE_CHARACTER_INFO, TOGGLE_INVENTORY, etc.)
 * @return true if the action was handled (panel opened or ownership blocked), false if default processing should continue
 */
bool HandleLocalCoopPanelAction(uint8_t playerId, uint8_t actionType);

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
Player *GetLocalCoopPanelOwnerPlayer();

/**
 * @brief Check if a monster/towner should be highlighted for any local coop player.
 *
 * This is used by the rendering code to draw outlines around targeted entities
 * for local coop players in addition to player 1's global pcursmonst.
 *
 * @param monsterId The monster/towner ID to check
 * @return true if any local coop player is targeting this entity
 */
bool IsLocalCoopTargetMonster(int monsterId);

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
