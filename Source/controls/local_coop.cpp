/**
 * @file local_coop.cpp
 *
 * Implementation of local co-op multiplayer functionality.
 */
#include "controls/local_coop.hpp"

#ifndef USE_SDL1

#include <algorithm>
#include <cmath>

#include "control.h"
#include "controls/controller_motion.h"
#include "controls/devices/game_controller.h"
#include "controls/game_controls.h"
#include "controls/modifier_hints.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "doom.h"
#include "engine/load_clx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/scrollrt.h"
#include "engine/render/text_render.hpp"
#include "game_mode.hpp"
#include "help.h"
#include "init.hpp"
#include "inv.h"
#include "items.h"
#include "levels/gendung.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "loadsave.h"
#include "menu.h"
#include "missiles.h"
#include "monster.h"
#include "msg.h"
#include "multi.h"
#include "objects.h"
#include "options.h"
#include "pack.h"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "player.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "quests.h"
#include "stores.h"
#include "towners.h"
#include "track.h"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"

namespace devilution {

// Local co-op HUD sprites
namespace {
OptionalOwnedClxSpriteList LocalCoopHealthBox;
OptionalOwnedClxSpriteList LocalCoopHealth;
OptionalOwnedClxSpriteList LocalCoopHealthBlue; // For mana shield
OptionalOwnedClxSpriteList LocalCoopBoxLeft;
OptionalOwnedClxSpriteList LocalCoopBoxMiddle;
OptionalOwnedClxSpriteList LocalCoopBoxRight;
OptionalOwnedClxSpriteList LocalCoopCharBg;
bool LocalCoopHUDAssetsLoaded = false;
} // namespace

void InitLocalCoopHUDAssets()
{
	if (LocalCoopHUDAssetsLoaded)
		return;

	// Load health bar sprites (same as monster health bar)
	LocalCoopHealthBox = LoadOptionalClx("data\\healthbox.clx");
	LocalCoopHealth = LoadOptionalClx("data\\health.clx");

	// Create blue version for mana (same transformation as monster health bar)
	if (LocalCoopHealth) {
		std::array<uint8_t, 256> healthBlueTrn = {};
		for (int i = 0; i < 256; i++)
			healthBlueTrn[i] = static_cast<uint8_t>(i);
		healthBlueTrn[234] = PAL16_BLUE + 5;
		healthBlueTrn[235] = PAL16_BLUE + 6;
		healthBlueTrn[236] = PAL16_BLUE + 7;
		LocalCoopHealthBlue = LocalCoopHealth->clone();
		ClxApplyTrans(*LocalCoopHealthBlue, healthBlueTrn.data());
	}

	// Load character panel box sprites for golden borders
	LocalCoopBoxLeft = LoadOptionalClx("data\\boxleftend.clx");
	LocalCoopBoxMiddle = LoadOptionalClx("data\\boxmiddle.clx");
	LocalCoopBoxRight = LoadOptionalClx("data\\boxrightend.clx");

	// Load character background for rock texture
	LocalCoopCharBg = LoadOptionalClx("data\\charbg.clx");

	LocalCoopHUDAssetsLoaded = true;
}

void FreeLocalCoopHUDAssets()
{
	LocalCoopCharBg = std::nullopt;
	LocalCoopBoxRight = std::nullopt;
	LocalCoopBoxMiddle = std::nullopt;
	LocalCoopBoxLeft = std::nullopt;
	LocalCoopHealthBlue = std::nullopt;
	LocalCoopHealth = std::nullopt;
	LocalCoopHealthBox = std::nullopt;
	LocalCoopHUDAssetsLoaded = false;
}

LocalCoopState g_LocalCoop;

namespace {

// Forward declarations
void CastLocalCoopHotkeySpell(int localIndex, int slotIndex);

/**
 * @brief RAII helper to temporarily swap MyPlayer, MyPlayerId, and InspectPlayer for local co-op actions.
 * 
 * This ensures network commands are sent with the correct player ID and that
 * player-specific state is properly managed during action execution.
 * InspectPlayer is also swapped because UI elements like the spell menu use it.
 */
class LocalCoopPlayerContext {
public:
	LocalCoopPlayerContext(uint8_t playerId)
	    : savedMyPlayer_(MyPlayer)
	    , savedMyPlayerId_(MyPlayerId)
	    , savedInspectPlayer_(InspectPlayer)
	{
		if (playerId < Players.size()) {
			MyPlayer = &Players[playerId];
			MyPlayerId = playerId;
			InspectPlayer = &Players[playerId];
		}
	}

	~LocalCoopPlayerContext()
	{
		MyPlayer = savedMyPlayer_;
		MyPlayerId = savedMyPlayerId_;
		InspectPlayer = savedInspectPlayer_;
	}

	// Non-copyable
	LocalCoopPlayerContext(const LocalCoopPlayerContext &) = delete;
	LocalCoopPlayerContext &operator=(const LocalCoopPlayerContext &) = delete;

private:
	Player *savedMyPlayer_;
	uint8_t savedMyPlayerId_;
	Player *savedInspectPlayer_;
};

/// Hold duration in milliseconds to open quick spell menu
constexpr uint32_t SkillButtonHoldTime = 500;

/// Face direction lookup table based on axis input
const Direction FaceDir[3][3] = {
	// AxisDirectionX_NONE, AxisDirectionX_LEFT, AxisDirectionX_RIGHT
	{ Direction::South, Direction::West, Direction::East },           // AxisDirectionY_NONE
	{ Direction::North, Direction::NorthWest, Direction::NorthEast }, // AxisDirectionY_UP
	{ Direction::South, Direction::SouthWest, Direction::SouthEast }, // AxisDirectionY_DOWN
};

/**
 * @brief Check if a tile position is within allowed distance from other players.
 * 
 * In local co-op, all players must stay close enough that they can all fit on screen.
 * This checks if moving to a new position would put this player too far from any other
 * active player, which would cause someone to be pushed off-screen.
 * 
 * The check uses tile distance in screen space (isometric projection) to account for
 * the actual visual distance on screen.
 * 
 * @param tilePos The tile position to check.
 * @param excludePlayerId Player ID to exclude from distance check (the player trying to move).
 * @return true if the position keeps all players within screen bounds.
 */
bool IsTilePositionOnScreen(Point tilePos, uint8_t excludePlayerId = 255)
{
	// Maximum allowed distance between any two players in screen pixels
	// This should be less than half the screen size to ensure both fit
	const int screenWidth = static_cast<int>(GetScreenWidth());
	const int viewportHeight = static_cast<int>(GetViewportHeight());
	
	// Use separate limits for X and Y, with margin for player sprites and UI
	// Note: viewportHeight is usually smaller than screenWidth on most displays
	// Horizontal margin is smaller to allow players to spread out more across the wider screen
	const int maxScreenDistanceX = (screenWidth / 2) - 50;   // Half screen width with small margin
	const int maxScreenDistanceY = (viewportHeight / 2) - 80; // Half viewport height with margin
	
	// Convert tile position to screen space for distance calculation
	// In isometric view: screenX = (tileY - tileX) * 32, screenY = (tileY + tileX) * 16
	auto tileToScreen = [](Point tile) -> Point {
		return {
			(tile.y - tile.x) * 32,
			(tile.y + tile.x) * 16
		};
	};
	
	Point newScreenPos = tileToScreen(tilePos);
	
	// Count how many other active players we check against
	int otherPlayersChecked = 0;
	
	// Check distance to all other active players
	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
	for (size_t i = 0; i < totalPlayers && i < Players.size(); ++i) {
		if (static_cast<uint8_t>(i) == excludePlayerId)
			continue;
			
		const Player &otherPlayer = Players[i];
		if (!otherPlayer.plractive || otherPlayer._pHitPoints <= 0)
			continue;
		if (otherPlayer.plrlevel != Players[0].plrlevel)
			continue;
		
		otherPlayersChecked++;
		Point otherScreenPos = tileToScreen(otherPlayer.position.future);
		
		// Calculate screen distance
		int distX = std::abs(newScreenPos.x - otherScreenPos.x);
		int distY = std::abs(newScreenPos.y - otherScreenPos.y);
		
		// Use an ellipse-based check to account for isometric projection properly.
		// In isometric view, horizontal tile movement affects both screen X and Y.
		// Using (distX/maxX)^2 + (distY/maxY)^2 <= 1 gives a more natural boundary.
		// This allows more movement along the wider screen axis while properly
		// constraining the narrower vertical axis.
		int64_t normalizedX = (static_cast<int64_t>(distX) * 1000) / maxScreenDistanceX;
		int64_t normalizedY = (static_cast<int64_t>(distY) * 1000) / maxScreenDistanceY;
		int64_t ellipseCheck = normalizedX * normalizedX + normalizedY * normalizedY;
		
		// Check if within ellipse (1000^2 = 1,000,000)
		if (ellipseCheck > 1000000) {
			return false; // Would be too far from this player
		}
	}
	
	// If there are no other players to check against, allow movement
	if (otherPlayersChecked == 0) {
		return true;
	}
	
	return true;
}

/**
 * @brief Apply deadzone scaling to joystick axes.
 */
void ScaleJoystickAxes(float *x, float *y, float deadzone)
{
	if (deadzone == 0)
		return;
	if (deadzone >= 1.0f) {
		*x = 0;
		*y = 0;
		return;
	}

	const float maximum = 32767.0f;
	float analogX = *x;
	float analogY = *y;
	const float deadZone = deadzone * maximum;

	const float magnitude = std::sqrt(analogX * analogX + analogY * analogY);
	if (magnitude >= deadZone) {
		const float scalingFactor = 1.f / magnitude * (magnitude - deadZone) / (maximum - deadZone);
		analogX *= scalingFactor;
		analogY *= scalingFactor;

		float clampingFactor = 1.f;
		const float absAnalogX = std::fabs(analogX);
		const float absAnalogY = std::fabs(analogY);
		if (absAnalogX > 1.0f || absAnalogY > 1.0f) {
			clampingFactor = 1.f / std::max(absAnalogX, absAnalogY);
		}
		*x = clampingFactor * analogX;
		*y = clampingFactor * analogY;
	} else {
		*x = 0;
		*y = 0;
	}
}

/// Static variables for hero collection callback
static std::vector<_uiheroinfo> *g_LocalCoopHeroList = nullptr;
static const std::vector<uint32_t> *g_LocalCoopExcludeSaves = nullptr;

/**
 * @brief Callback for collecting heroes in LoadHeroInfosForLocalCoop.
 */
bool CollectHeroForLocalCoop(_uiheroinfo *info)
{
	if (g_LocalCoopHeroList == nullptr || g_LocalCoopExcludeSaves == nullptr)
		return true;

	// Check if this hero should be excluded
	bool excluded = false;
	for (uint32_t excludeSave : *g_LocalCoopExcludeSaves) {
		if (info->saveNumber == excludeSave) {
			excluded = true;
			break;
		}
	}
	if (!excluded) {
		g_LocalCoopHeroList->push_back(*info);
	}
	return true; // Continue processing
}

/**
 * @brief Load hero info from save files.
 * Uses pfile_ui_set_hero_infos which temporarily modifies Players[0],
 * but we restore it by re-reading player 1's save file after.
 * Respects the current game mode (single-player vs multiplayer) to show
 * the correct character saves. pfile_ui_set_hero_infos uses gbIsMultiplayer
 * to determine which save directory to read from (single_ vs multi_).
 */
void LoadHeroInfosForLocalCoop(std::vector<_uiheroinfo> &heroList, const std::vector<uint32_t> &excludeSaves)
{
	heroList.clear();

	// Save player 1's save number so we can restore it
	uint32_t savedSaveNumber = gSaveNumber;

	// Set up static variables for callback
	g_LocalCoopHeroList = &heroList;
	g_LocalCoopExcludeSaves = &excludeSaves;

	// This will modify Players[0] for each hero
	// It uses gbIsMultiplayer to determine save directory (single_ vs multi_)
	// so it will automatically show the correct characters based on the current game mode
	pfile_ui_set_hero_infos(CollectHeroForLocalCoop);

	// Restore player 1 by re-reading their save file
	if (savedSaveNumber < MAX_CHARACTERS) {
		pfile_read_player_from_save(savedSaveNumber, Players[0]);
		// pfile_read_player_from_save calls CalcPlrInv with loadgfx=false,
		// so we need to recalculate to ensure graphics are properly set
		CalcPlrInv(Players[0], true);
	}

	// Clear static variables
	g_LocalCoopHeroList = nullptr;
	g_LocalCoopExcludeSaves = nullptr;
}

/**
 * @brief Check if there's a valid target for primary action (attack/talk/operate).
 */
bool HasLocalCoopPrimaryTarget(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return false;
	
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Monster to attack or towner to talk to
	if (cursor.pcursmonst != -1)
		return true;
	
	// Object to operate
	if (cursor.objectUnderCursor != nullptr)
		return true;
	
	return false;
}

/**
 * @brief Check if there's a valid target for secondary action (pickup/operate/portal).
 */
bool HasLocalCoopSecondaryTarget(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return false;
	
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Item to pick up
	if (cursor.pcursitem != -1)
		return true;
	
	// Object to operate
	if (cursor.objectUnderCursor != nullptr)
		return true;
	
	// Portal or trigger to walk to
	if (cursor.pcursmissile != nullptr || cursor.pcurstrig != -1)
		return true;
	
	return false;
}

/**
 * @brief Process D-pad input for local co-op player movement.
 * D-pad controls movement direction, similar to left stick.
 * Also handles navigation in quick spell menu when assigning skills.
 * Also handles panel navigation (inventory, character, etc.) when this player owns the panels.
 */
void ProcessLocalCoopDpadInput(int localIndex, const SDL_Event &event)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];

	// Don't process D-pad for movement when in character selection
	if (coopPlayer.characterSelectActive)
		return;
	
	// If quick spell menu is open for skill assignment, use D-pad for navigation
	if (SpellSelectFlag && coopPlayer.skillMenuOpenedByHold) {
		if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			const auto button = SDLC_EventGamepadButton(event).button;
			AxisDirection dir = { AxisDirectionX_NONE, AxisDirectionY_NONE };
			
			switch (button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				dir.y = AxisDirectionY_UP;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				dir.y = AxisDirectionY_DOWN;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
				dir.x = AxisDirectionX_LEFT;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				dir.x = AxisDirectionX_RIGHT;
				break;
			default:
				break;
			}
			
			if (dir.x != AxisDirectionX_NONE || dir.y != AxisDirectionY_NONE) {
				HotSpellMove(dir);
			}
		}
		return;  // Don't update movement when navigating spell menu
	}
	
	// If this coop player owns panels, use D-pad for panel navigation instead of movement
	// Panels: inventory, character sheet, quest log, spellbook
	if (g_LocalCoop.panelOwner == localIndex && (invflag || CharFlag || QuestLogIsOpen || SpellbookFlag)) {
		if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			const auto button = SDLC_EventGamepadButton(event).button;
			AxisDirection dir = { AxisDirectionX_NONE, AxisDirectionY_NONE };
			
			switch (button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				dir.y = AxisDirectionY_UP;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				dir.y = AxisDirectionY_DOWN;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
				dir.x = AxisDirectionX_LEFT;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				dir.x = AxisDirectionX_RIGHT;
				break;
			default:
				break;
			}
			
			if (dir.x != AxisDirectionX_NONE || dir.y != AxisDirectionY_NONE) {
				// Swap player context so panel navigation works for this coop player
				const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
				if (playerId < Players.size()) {
					LocalCoopPlayerContext context(playerId);
					// Navigate the panel using the navigation system
					ProcessGamePanelNavigation(dir);
				}
			}
		}
		return;  // Don't update movement when navigating panels
	}

	if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
		const auto button = SDLC_EventGamepadButton(event).button;
		switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			coopPlayer.dpadY = 1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			coopPlayer.dpadY = -1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			coopPlayer.dpadX = -1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			coopPlayer.dpadX = 1;
			break;
		default:
			break;
		}
	} else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
		const auto button = SDLC_EventGamepadButton(event).button;
		switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			if (coopPlayer.dpadY > 0) coopPlayer.dpadY = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			if (coopPlayer.dpadY < 0) coopPlayer.dpadY = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			if (coopPlayer.dpadX < 0) coopPlayer.dpadX = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			if (coopPlayer.dpadX > 0) coopPlayer.dpadX = 0;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Process button input for a local co-op player.
 * 
 * Button mapping:
 * - A (South): Primary action if target exists, otherwise cast skill slot A
 * - B (East): Secondary action if target exists, otherwise cast skill slot B
 * - X (West): Cast skill slot X
 * - Y (North): Cast skill slot Y
 * - LB: Use health potion
 * - RB: Use mana potion
 * - Select: Toggle character info
 * - Start: Toggle inventory
 * - D-pad: Movement
 */
void ProcessLocalCoopButtonInput(int localIndex, const SDL_Event &event)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);

	const bool isButtonDown = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
	const bool isButtonUp = (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP);
	
	if (!isButtonDown && !isButtonUp)
		return;
		
	const auto button = SDLC_EventGamepadButton(event).button;

	// Character selection mode
	if (coopPlayer.characterSelectActive) {
		if (!isButtonDown)
			return;
			
		uint32_t now = SDL_GetTicks();

		if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
			if (now - coopPlayer.lastDpadPress > LocalCoopState::DpadRepeatDelay) {
				if (!coopPlayer.availableHeroes.empty()) {
					coopPlayer.selectedHeroIndex--;
					if (coopPlayer.selectedHeroIndex < 0) {
						coopPlayer.selectedHeroIndex = static_cast<int>(coopPlayer.availableHeroes.size()) - 1;
					}
				}
				coopPlayer.lastDpadPress = now;
			}
		} else if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
			if (now - coopPlayer.lastDpadPress > LocalCoopState::DpadRepeatDelay) {
				if (!coopPlayer.availableHeroes.empty()) {
					coopPlayer.selectedHeroIndex++;
					if (coopPlayer.selectedHeroIndex >= static_cast<int>(coopPlayer.availableHeroes.size())) {
						coopPlayer.selectedHeroIndex = 0;
					}
				}
				coopPlayer.lastDpadPress = now;
			}
		} else if (button == SDL_GAMEPAD_BUTTON_SOUTH) { // A button - confirm
			ConfirmLocalCoopCharacter(localIndex);
		}
		return;
	}

	// Gameplay mode
	if (playerId >= Players.size())
		return;

	Player &player = Players[playerId];

	if (player._pHitPoints <= 0)
		return;
	
	// Store mode: this player owns the store
	if (g_LocalCoop.storeOwner == localIndex && IsPlayerInStore()) {
		if (isButtonDown) {
			switch (button) {
			case SDL_GAMEPAD_BUTTON_SOUTH: // A button = Confirm/Enter
				StoreEnter();
				break;
			case SDL_GAMEPAD_BUTTON_EAST: // B button = Back/ESC
				StoreESC();
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				StoreUp();
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				StoreDown();
				break;
			default:
				break;
			}
		}
		return;
	}

	// Handle button presses - map to game actions
	if (isButtonDown) {
		uint32_t now = SDL_GetTicks();
		
		// If quick spell menu is open and this player opened it, handle spell assignment
		// Use same slot mapping as player 1: A=2, B=3, X=0, Y=1
		if (SpellSelectFlag && coopPlayer.skillMenuOpenedByHold) {
			int assignSlot = -1;
			switch (button) {
			case SDL_GAMEPAD_BUTTON_SOUTH: assignSlot = 2; break;  // A
			case SDL_GAMEPAD_BUTTON_EAST: assignSlot = 3; break;   // B
			case SDL_GAMEPAD_BUTTON_WEST: assignSlot = 0; break;   // X
			case SDL_GAMEPAD_BUTTON_NORTH: assignSlot = 1; break;  // Y
			default: break;
			}
			
			if (assignSlot >= 0) {
				// Assign the currently selected spell to this slot
				AssignLocalCoopSpellToSlot(localIndex, assignSlot);
				coopPlayer.skillButtonHeld = -1;
				coopPlayer.skillButtonPressTime = 0;
				coopPlayer.skillMenuOpenedByHold = false;
				return;
			}
		}
		
		switch (button) {
		case SDL_GAMEPAD_BUTTON_SOUTH: // A button - Primary action or skill slot 2
			// If this player owns panels (inventory/character/etc.), always perform primary action for panel interaction
			if (g_LocalCoop.panelOwner == localIndex && (invflag || CharFlag || QuestLogIsOpen || SpellbookFlag)) {
				coopPlayer.actionHeld = GameActionType_PRIMARY_ACTION;
				ProcessLocalCoopGameAction(localIndex, GameActionType_PRIMARY_ACTION);
			}
			// If there's a target (monster/towner/object), perform primary action (attack/talk/operate)
			// Otherwise, use skill slot 2 (consistent with Player 1's behavior)
			else if (HasLocalCoopPrimaryTarget(localIndex)) {
				// Only perform action if player can change action (not already in an action)
				if (player.CanChangeAction() && player.destAction == ACTION_NONE) {
					coopPlayer.actionHeld = GameActionType_PRIMARY_ACTION;
					ProcessLocalCoopGameAction(localIndex, GameActionType_PRIMARY_ACTION);
				}
			} else {
				// No target - use skill slot 2 (A button slot)
				coopPlayer.skillButtonHeld = 2;
				coopPlayer.skillButtonPressTime = now;
				coopPlayer.skillMenuOpenedByHold = false;
			}
			break;
			
		case SDL_GAMEPAD_BUTTON_EAST: // B button - Secondary action (pickup) or skill slot 3
			if (HasLocalCoopSecondaryTarget(localIndex)) {
				// Only perform action if player can change action
				if (player.CanChangeAction() && player.destAction == ACTION_NONE) {
					coopPlayer.actionHeld = GameActionType_SECONDARY_ACTION;
					ProcessLocalCoopGameAction(localIndex, GameActionType_SECONDARY_ACTION);
				}
			} else {
				// Start tracking hold for skill slot assignment (B = slot 3)
				coopPlayer.skillButtonHeld = 3;
				coopPlayer.skillButtonPressTime = now;
				coopPlayer.skillMenuOpenedByHold = false;
			}
			break;
			
		case SDL_GAMEPAD_BUTTON_WEST: // X button - Skill slot 0
			// Only allow spell casting if not already in an action
			if (player.CanChangeAction() && player.destAction == ACTION_NONE) {
				// Start tracking hold for skill slot assignment (X = slot 0)
				coopPlayer.skillButtonHeld = 0;
				coopPlayer.skillButtonPressTime = now;
				coopPlayer.skillMenuOpenedByHold = false;
			}
			break;
			
		case SDL_GAMEPAD_BUTTON_NORTH: // Y button - Skill slot 1
			// Only allow spell casting if not already in an action
			if (player.CanChangeAction() && player.destAction == ACTION_NONE) {
				// Start tracking hold for skill slot assignment (Y = slot 1)
				coopPlayer.skillButtonHeld = 1;
				coopPlayer.skillButtonPressTime = now;
				coopPlayer.skillMenuOpenedByHold = false;
			}
			break;
			
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: // LB = Use health potion
			ProcessLocalCoopGameAction(localIndex, GameActionType_USE_HEALTH_POTION);
			break;
			
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: // RB = Use mana potion  
			ProcessLocalCoopGameAction(localIndex, GameActionType_USE_MANA_POTION);
			break;
			
		case SDL_GAMEPAD_BUTTON_BACK: // Select/Back = Toggle character info
			ProcessLocalCoopGameAction(localIndex, GameActionType_TOGGLE_CHARACTER_INFO);
			break;
			
		case SDL_GAMEPAD_BUTTON_START: // Start = Toggle inventory
			ProcessLocalCoopGameAction(localIndex, GameActionType_TOGGLE_INVENTORY);
			break;
			
		default:
			break;
		}
	} else if (isButtonUp) {
		// Handle skill button release - cast spell if it was a short press
		// Use same slot mapping as player 1: A=2, B=3, X=0, Y=1
		int releasedSlot = -1;
		switch (button) {
		case SDL_GAMEPAD_BUTTON_SOUTH:  // A = slot 2
			if (coopPlayer.actionHeld == GameActionType_PRIMARY_ACTION)
				coopPlayer.actionHeld = GameActionType_NONE;
			releasedSlot = 2;
			break;
		case SDL_GAMEPAD_BUTTON_EAST:   // B = slot 3
			if (coopPlayer.actionHeld == GameActionType_SECONDARY_ACTION)
				coopPlayer.actionHeld = GameActionType_NONE;
			releasedSlot = 3;
			break;
		case SDL_GAMEPAD_BUTTON_WEST:   // X = slot 0
			if (coopPlayer.actionHeld == GameActionType_CAST_SPELL)
				coopPlayer.actionHeld = GameActionType_NONE;
			releasedSlot = 0;
			break;
		case SDL_GAMEPAD_BUTTON_NORTH:  // Y = slot 1
			releasedSlot = 1;
			break;
		default:
			break;
		}
		
		// If we released a skill button that was being held
		if (releasedSlot >= 0 && coopPlayer.skillButtonHeld == releasedSlot) {
			// If the menu wasn't opened by this hold, it was a short press - cast the spell
			if (!coopPlayer.skillMenuOpenedByHold) {
				CastLocalCoopHotkeySpell(localIndex, releasedSlot);
			}
			// Reset hold tracking (but NOT skillMenuOpenedByHold if menu is open)
			// It will be reset when the menu closes or a spell is assigned
			coopPlayer.skillButtonHeld = -1;
			coopPlayer.skillButtonPressTime = 0;
			// Keep skillMenuOpenedByHold true if the spell menu is still open
		}
	}
}

/**
 * @brief Process axis motion for a local co-op player.
 * Handles left stick for movement and triggers for inventory/character screen.
 */
void ProcessLocalCoopAxisMotion(int localIndex, const SDL_Event &event)
{
	if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION)
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const auto &axis = SDLC_EventGamepadAxis(event);

	// Trigger threshold (same as player 1 - values are in range -32767 to 32767)
	constexpr int16_t TriggerThreshold = 8192;

	switch (axis.axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		coopPlayer.leftStickXUnscaled = static_cast<float>(axis.value);
		break;
	case SDL_GAMEPAD_AXIS_LEFTY:
		coopPlayer.leftStickYUnscaled = static_cast<float>(-axis.value); // Invert Y axis
		break;
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: {
		// Left trigger = Character screen (like player 1)
		bool nowPressed = axis.value > TriggerThreshold;
		if (nowPressed && !coopPlayer.leftTriggerPressed) {
			ProcessLocalCoopGameAction(localIndex, GameActionType_TOGGLE_CHARACTER_INFO);
		}
		coopPlayer.leftTriggerPressed = nowPressed;
		return;
	}
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
		// Right trigger = Inventory (like player 1)
		bool nowPressed = axis.value > TriggerThreshold;
		if (nowPressed && !coopPlayer.rightTriggerPressed) {
			ProcessLocalCoopGameAction(localIndex, GameActionType_TOGGLE_INVENTORY);
		}
		coopPlayer.rightTriggerPressed = nowPressed;
		return;
	}
	default:
		return;
	}

	// Apply deadzone scaling (values are in range -32767 to 32767)
	coopPlayer.leftStickX = coopPlayer.leftStickXUnscaled;
	coopPlayer.leftStickY = coopPlayer.leftStickYUnscaled;
	constexpr float deadzone = 0.25f;
	ScaleJoystickAxes(&coopPlayer.leftStickX, &coopPlayer.leftStickY, deadzone);
}

/**
 * @brief Update movement for a single local co-op player.
 * 
 * Sends walk commands through the network layer just like player 1.
 * 
 * Supports both left analog stick and D-pad for movement.
 */
void UpdateLocalCoopPlayerMovement(int localIndex)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];

	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	if (coopPlayer.characterSelectActive)
		return;

	// Don't move when this player owns a panel or store
	if (g_LocalCoop.panelOwner == localIndex || g_LocalCoop.storeOwner == localIndex)
		return;

	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;

	Player &player = Players[playerId];

	// Skip if player is not on the same level as player 1 or is dead
	if (player.plrlevel != Players[0].plrlevel)
		return;
	if (player._pHitPoints <= 0)
		return;

	AxisDirection dir = coopPlayer.GetMoveDirection();

	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE) {
		// If no input and player has a walk path, stop walking
		if (player.walkpath[0] != WALK_NONE && player.destAction == ACTION_NONE) {
			// Send stop walking command through network
			LocalCoopPlayerContext context(playerId);
			NetSendCmdLoc(playerId, true, CMD_WALKXY, player.position.future);
		}
		return;
	}

	const Direction pdir = FaceDir[static_cast<size_t>(dir.y)][static_cast<size_t>(dir.x)];
	const Point target = player.position.future + pdir;

	// Update facing direction when not walking
	if (!player.isWalking() && player.CanChangeAction()) {
		player._pdir = pdir;
	}

	// Check if the target position would keep all players on screen
	if (!IsTilePositionOnScreen(target, playerId)) {
		// Target is too far from other players, only allow turning
		if (player._pmode == PM_STAND) {
			StartStand(player, pdir);
		}
		return;
	}

	// Check if target is walkable and path is not blocked
	if (PosOkPlayer(player, target) && InDungeonBounds(target)) {
		// Send walk command through network layer (like player 1 does)
		LocalCoopPlayerContext context(playerId);
		NetSendCmdLoc(playerId, true, CMD_WALKXY, target);
	} else if (player._pmode == PM_STAND) {
		StartStand(player, pdir);
	}
}

} // namespace

void LocalCoopCursorState::Reset()
{
	pcursmonst = -1;
	pcursitem = -1;
	objectUnderCursor = nullptr;
	playerUnderCursor = nullptr;
	cursPosition = { -1, -1 };
	pcursmissile = nullptr;
	pcurstrig = -1;
}

void LocalCoopPlayer::Reset()
{
	active = false;
	initialized = false;
	controllerId = -1;
	leftStickX = 0;
	leftStickY = 0;
	leftStickXUnscaled = 0;
	leftStickYUnscaled = 0;
	characterSelectActive = true;
	availableHeroes.clear();
	selectedHeroIndex = 0;
	lastDpadPress = 0;
	dpadX = 0;
	dpadY = 0;
	cursor.Reset();
	actionHeld = 0;
	saveNumber = 0;
	skillButtonHeld = -1;
	skillButtonPressTime = 0;
	skillMenuOpenedByHold = false;
}

AxisDirection LocalCoopPlayer::GetMoveDirection() const
{
	AxisDirection dir;
	dir.x = AxisDirectionX_NONE;
	dir.y = AxisDirectionY_NONE;

	const float threshold = 0.5f;

	// Check left stick first
	if (leftStickX <= -threshold)
		dir.x = AxisDirectionX_LEFT;
	else if (leftStickX >= threshold)
		dir.x = AxisDirectionX_RIGHT;

	if (leftStickY >= threshold)
		dir.y = AxisDirectionY_UP;
	else if (leftStickY <= -threshold)
		dir.y = AxisDirectionY_DOWN;

	// D-pad overrides stick if pressed
	if (dpadX < 0)
		dir.x = AxisDirectionX_LEFT;
	else if (dpadX > 0)
		dir.x = AxisDirectionX_RIGHT;

	if (dpadY > 0)
		dir.y = AxisDirectionY_UP;
	else if (dpadY < 0)
		dir.y = AxisDirectionY_DOWN;

	return dir;
}

size_t LocalCoopState::GetActivePlayerCount() const
{
	size_t count = 0;
	for (const auto &player : players) {
		if (player.active)
			++count;
	}
	return count;
}

size_t LocalCoopState::GetInitializedPlayerCount() const
{
	size_t count = 0;
	for (const auto &player : players) {
		if (player.active && player.initialized)
			++count;
	}
	return count;
}

size_t LocalCoopState::GetTotalPlayerCount() const
{
	return 1 + GetActivePlayerCount(); // Player 1 + local co-op players
}

bool LocalCoopState::IsAnyCharacterSelectActive() const
{
	for (const auto &player : players) {
		if (player.active && player.characterSelectActive)
			return true;
	}
	return false;
}

uint8_t LocalCoopState::GetPanelOwnerPlayerId() const
{
	if (panelOwner < 0)
		return 0; // Player 1
	return static_cast<uint8_t>(panelOwner + 1); // Local coop index 0 = player 2
}

bool LocalCoopState::TryClaimPanelOwnership(int localIndex)
{
	// Check if panels are currently open
	bool panelsOpen = IsLeftPanelOpen() || IsRightPanelOpen();
	
	// If no panels are open, anyone can claim
	if (!panelsOpen) {
		panelOwner = localIndex;
		return true;
	}
	// If this player already owns the panels, allow
	if (panelOwner == localIndex)
		return true;
	// If panels are open but panelOwner == -1, Player 1 owns them - block coop players
	if (panelOwner == -1)
		return false;
	// Otherwise, panels are owned by a different coop player
	return false;
}

void LocalCoopState::ReleasePanelOwnership()
{
	// Only release if no panels are open
	if (!IsLeftPanelOpen() && !IsRightPanelOpen()) {
		panelOwner = -1;
		// Restore MyPlayer and InspectPlayer to Player 1
		if (Players.size() > 0) {
			MyPlayer = &Players[0];
			MyPlayerId = 0;
			InspectPlayer = &Players[0];
		}
	}
}

void InitLocalCoop()
{
	g_LocalCoop = {};

	const auto &controllers = GameController::All();
	if (controllers.size() < 2) {
		Log("Local co-op: Not enough controllers ({} found, need at least 2)", controllers.size());
		return;
	}

	// Log all controller IDs for debugging
	Log("Local co-op: Listing all {} controllers:", controllers.size());
	for (size_t i = 0; i < controllers.size(); ++i) {
		Log("  Controller {}: instance ID = {}", i, controllers[i].GetInstanceId());
	}

	// Calculate how many local co-op players we can have
	size_t numCoopPlayers = std::min(controllers.size() - 1, MaxLocalPlayers - 1);

	Log("Local co-op: {} controllers detected, enabling {} co-op player slots",
	    controllers.size(), numCoopPlayers);

	// Assign controllers to local co-op players
	// Controller 0 stays with Player 1 (existing system)
	// Controllers 1+ go to local co-op players
	for (size_t i = 0; i < numCoopPlayers; ++i) {
		g_LocalCoop.players[i].active = true;
		g_LocalCoop.players[i].controllerId = controllers[i + 1].GetInstanceId();
		Log("Local co-op: Player {} assigned controller ID {}",
		    i + 2, g_LocalCoop.players[i].controllerId);
	}

	g_LocalCoop.enabled = true;

	// Make sure we have enough player slots
	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
	if (Players.size() < totalPlayers) {
		size_t oldSize = Players.size();
		Players.resize(totalPlayers);
		// Initialize new player slots to inactive
		for (size_t i = oldSize; i < totalPlayers; ++i) {
			Players[i].plractive = false;
		}
		// IMPORTANT: Resizing the vector may reallocate, invalidating MyPlayer pointer
		// We must update MyPlayer to point to the new location
		if (MyPlayer != nullptr && MyPlayerId < Players.size()) {
			MyPlayer = &Players[MyPlayerId];
			InspectPlayer = MyPlayer;
		}
	}
}

void ShutdownLocalCoop()
{
	FreeLocalCoopHUDAssets();
	g_LocalCoop = {};
}

bool IsLocalCoopAvailable()
{
	// Local coop only available in multiplayer games with 2+ controllers
	return gbIsMultiplayer && *GetOptions().Gameplay.enableLocalCoop && GameController::All().size() >= 2;
}

bool IsLocalCoopEnabled()
{
	// Must be in multiplayer mode for local coop to be active
	return gbIsMultiplayer && g_LocalCoop.enabled;
}

bool IsAnyLocalCoopPlayerInitialized()
{
	if (!g_LocalCoop.enabled)
		return false;
	
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].initialized)
			return true;
	}
	return false;
}

bool IsLocalCoopPlayer(const Player &player)
{
	if (!g_LocalCoop.enabled)
		return false;
	
	// Player 1 (index 0) is never a local co-op player
	uint8_t playerId = player.getId();
	if (playerId == 0)
		return false;
	
	// Check if this player ID corresponds to an active local co-op slot
	// We only check 'active' here, not 'initialized', because CalcPlrInv
	// may be called during character loading before initialization completes
	int localIndex = PlayerIdToLocalCoopIndex(playerId);
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return false;
	
	return g_LocalCoop.players[localIndex].active;
}

SDL_JoystickID GetControllerIdFromEvent(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		return SDLC_EventGamepadAxis(event).which;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		return SDLC_EventGamepadButton(event).which;
#ifndef USE_SDL1
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		return event.jaxis.which;
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		return event.jbutton.which;
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
		return event.jhat.which;
#endif
	default:
		return -1;
	}
}

int GetLocalCoopPlayerIndex(const SDL_Event &event)
{
	if (!g_LocalCoop.enabled)
		return -1;

	SDL_JoystickID eventControllerId = GetControllerIdFromEvent(event);
	if (eventControllerId == -1)
		return -1;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].controllerId == eventControllerId) {
			LogVerbose("Local co-op: Event from controller {} matched to player {} (local index {})",
			          eventControllerId, i + 2, i);
			return static_cast<int>(i);
		}
	}

	LogVerbose("Local co-op: Event from controller {} not matched to any local co-op player",
	          eventControllerId);
	return -1;
}

bool IsLocalCoopControllerId(SDL_JoystickID controllerId)
{
	if (!g_LocalCoop.enabled)
		return false;

	for (const LocalCoopPlayer &player : g_LocalCoop.players) {
		if (player.active && player.controllerId == controllerId) {
			LogVerbose("Local co-op: Controller {} is a local co-op controller", controllerId);
			return true;
		}
	}

	LogVerbose("Local co-op: Controller {} is NOT a local co-op controller", controllerId);
	return false;
}

bool ProcessLocalCoopInput(const SDL_Event &event)
{
	if (!g_LocalCoop.enabled)
		return false;

	int localIndex = GetLocalCoopPlayerIndex(event);
	if (localIndex < 0)
		return false;

	// Process all input for this coop player
	ProcessLocalCoopAxisMotion(localIndex, event);
	ProcessLocalCoopDpadInput(localIndex, event);
	ProcessLocalCoopButtonInput(localIndex, event);

	LogVerbose("Local co-op: Processed input for player {} (local index {}), dpad={},{}", 
	          localIndex + 2, localIndex,
	          g_LocalCoop.players[localIndex].dpadX,
	          g_LocalCoop.players[localIndex].dpadY);
	
	return true;
}

void UpdateLocalCoopMovement()
{
	if (!g_LocalCoop.enabled)
		return;

	// Check if a coop player owns panels and they've been closed
	// If so, release ownership and restore Player 1's context
	if (g_LocalCoop.panelOwner >= 0) {
		if (!IsLeftPanelOpen() && !IsRightPanelOpen()) {
			g_LocalCoop.ReleasePanelOwnership();
		}
	}

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		UpdateLocalCoopPlayerMovement(static_cast<int>(i));
		// Also update target selection for each player
		UpdateLocalCoopTargetSelection(static_cast<int>(i));
	}
}

void LoadAvailableHeroesForLocalPlayer(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	coopPlayer.availableHeroes.clear();
	coopPlayer.selectedHeroIndex = 0;

	// Build list of save numbers to exclude
	std::vector<uint32_t> excludeSaves;

	// Exclude player 1's currently selected hero
	excludeSaves.push_back(gSaveNumber);

	// Exclude heroes already selected by other local co-op players
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (static_cast<int>(i) == localIndex)
			continue;
		const LocalCoopPlayer &other = g_LocalCoop.players[i];
		if (other.active && !other.characterSelectActive && !other.availableHeroes.empty()) {
			// This player has confirmed, exclude their hero
			if (other.selectedHeroIndex >= 0 && other.selectedHeroIndex < static_cast<int>(other.availableHeroes.size())) {
				excludeSaves.push_back(other.availableHeroes[other.selectedHeroIndex].saveNumber);
			}
		}
	}

	// Load hero info without corrupting Players array
	LoadHeroInfosForLocalCoop(coopPlayer.availableHeroes, excludeSaves);

	Log("Local co-op: Player {} has {} available heroes",
	    localIndex + 2, coopPlayer.availableHeroes.size());
}

void ConfirmLocalCoopCharacter(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];

	if (coopPlayer.availableHeroes.empty()) {
		Log("Local co-op: Player {} has no heroes available", localIndex + 2);
		return;
	}

	if (coopPlayer.selectedHeroIndex < 0 || coopPlayer.selectedHeroIndex >= static_cast<int>(coopPlayer.availableHeroes.size())) {
		Log("Local co-op: Player {} has invalid hero index", localIndex + 2);
		return;
	}

	const _uiheroinfo &selectedHero = coopPlayer.availableHeroes[coopPlayer.selectedHeroIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);

	Log("Local co-op: Player {} selected {} (Lv {} {})",
	    playerId + 1,
	    selectedHero.name,
	    selectedHero.level,
	    GetPlayerDataForClass(selectedHero.heroclass).className);

	if (playerId >= Players.size()) {
		size_t oldSize = Players.size();
		Players.resize(playerId + 1);
		// Initialize new player slots to inactive
		for (size_t i = oldSize; i < Players.size(); ++i) {
			Players[i].plractive = false;
		}
		// IMPORTANT: Resizing the vector may reallocate, invalidating MyPlayer pointer
		if (MyPlayer != nullptr && MyPlayerId < Players.size()) {
			MyPlayer = &Players[MyPlayerId];
			InspectPlayer = MyPlayer;
		}
	}

	Player &player = Players[playerId];

	// Read hero from save file
	pfile_read_player_from_save(selectedHero.saveNumber, player);

	// Store the save number so we can save this player's progress later
	coopPlayer.saveNumber = selectedHero.saveNumber;

	// Initialize player on the same level as Player 1 BEFORE initializing graphics
	// so that isOnActiveLevel() returns true and graphics get loaded
	player.plrlevel = Players[0].plrlevel;
	player.plrIsOnSetLevel = Players[0].plrIsOnSetLevel;

	// Find a spawn position near Player 1
	Point spawnPos = Players[0].position.tile;

	// Try adjacent tiles
	static const Direction spawnDirs[] = {
		Direction::South, Direction::East, Direction::West, Direction::North,
		Direction::SouthEast, Direction::SouthWest, Direction::NorthEast, Direction::NorthWest
	};

	for (Direction dir : spawnDirs) {
		Point testPos = spawnPos + dir;
		if (PosOkPlayer(player, testPos)) {
			spawnPos = testPos;
			break;
		}
	}

	// Mark player as active first (needed for getId() to work correctly)
	player.plractive = true;
	gbActivePlayers++;

	// Set initial position before SyncInitPlr (which may adjust it)
	player.position.tile = spawnPos;
	player.position.future = spawnPos;
	player.position.old = spawnPos;
	player._pdir = Direction::South;

	// Initialize base graphics first with default settings
	// InitPlayerGFX calls ResetPlayerGFX and then loads all graphics based on _pgfxnum
	InitPlayerGFX(player);

	// Now recalculate inventory and graphics based on actual equipment
	// pfile_read_player_from_save called CalcPlrInv with loadgfx=false, so _pgfxnum was set
	// but graphics weren't loaded. By invalidating _pgfxnum, we force CalcPlrInv to
	// detect a change and reload graphics with the correct armor/weapon sprites.
	player._pgfxnum = 0xFF; // Invalid value to force graphics reload
	CalcPlrInv(player, true);

	// SyncInitPlr will set up animations and position (calls occupyTile)
	SyncInitPlr(player);
	// InitPlayer sets up player state and vision
	InitPlayer(player, true);

	// In local co-op, each player needs their own light source (unlike multiplayer where
	// other players are remote and don't need local lights). InitPlayer sets lightId to
	// NO_LIGHT for non-MyPlayer, so we need to add it ourselves.
	if (player.lightId == NO_LIGHT) {
		player.lightId = AddLight(player.position.tile, player._pLightRad);
		ChangeLightXY(player.lightId, player.position.tile);
	}

	coopPlayer.characterSelectActive = false;
	coopPlayer.initialized = true;

	Log("Local co-op: Player {} spawned at ({}, {})", playerId + 1, spawnPos.x, spawnPos.y);

	// Reload available heroes for remaining players who haven't selected yet
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (static_cast<int>(i) != localIndex && g_LocalCoop.players[i].active && g_LocalCoop.players[i].characterSelectActive) {
			LoadAvailableHeroesForLocalPlayer(static_cast<int>(i));
		}
	}
}

void DrawLocalCoopCharacterSelect(const Surface &out)
{
	// Only show character select in multiplayer with local coop enabled
	if (!gbIsMultiplayer || !g_LocalCoop.enabled)
		return;

	int yOffset = 10;
	constexpr int boxWidth = 220;
	constexpr int boxHeight = 55;
	constexpr int padding = 10;
	const int baseX = out.w() - boxWidth - padding;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];

		if (!coopPlayer.active)
			continue;
		if (!coopPlayer.characterSelectActive)
			continue;

		const int x = baseX;
		const int y = yOffset;

		// Draw title
		std::string title = StrCat(_("Player "), i + 2, " - ", _("Select Hero"));
		DrawString(out, title, { { x, y }, { boxWidth, 0 } },
		    { .flags = UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::FontSize12, .spacing = 1 });

		if (coopPlayer.availableHeroes.empty()) {
			DrawString(out, _("No heroes available"), { { x, y + 16 }, { boxWidth, 0 } },
			    { .flags = UiFlags::ColorRed | UiFlags::AlignCenter | UiFlags::FontSize12, .spacing = 1 });
			DrawString(out, _("Create another save first"), { { x, y + 30 }, { boxWidth, 0 } },
			    { .flags = UiFlags::ColorWhite | UiFlags::AlignCenter | UiFlags::FontSize12, .spacing = 1 });
		} else {
			const _uiheroinfo &hero = coopPlayer.availableHeroes[coopPlayer.selectedHeroIndex];

			// Show hero name (use the name from the hero info, which matches what will be loaded)
			DrawString(out, hero.name, { { x, y + 14 }, { boxWidth, 0 } },
			    { .flags = UiFlags::ColorWhite | UiFlags::AlignCenter, .spacing = 1 });

			// Show class and level
			const PlayerData &classData = GetPlayerDataForClass(hero.heroclass);
			std::string classInfo = StrCat(_("Lv "), hero.level, " ", classData.className);
			DrawString(out, classInfo, { { x, y + 30 }, { boxWidth, 0 } },
			    { .flags = UiFlags::ColorUiSilver | UiFlags::AlignCenter | UiFlags::FontSize12, .spacing = 1 });

			// Navigation hint
			std::string navHint = StrCat("< ", coopPlayer.selectedHeroIndex + 1, "/",
			    coopPlayer.availableHeroes.size(), " >  ", _("(A) Confirm"));
			DrawString(out, navHint, { { x, y + 44 }, { boxWidth, 0 } },
			    { .flags = UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::FontSize12, .spacing = 1 });
		}

		yOffset += boxHeight + padding;
	}
}

namespace {

// Local coop HUD panel dimensions
constexpr int CoopPanelPadding = 4;      // Padding inside the panel
constexpr int CoopPanelBorder = 2;       // Golden border width

// Health/Mana bar dimensions (using healthbox.clx style)
constexpr int HealthBoxWidth = 234;   // Width of healthbox sprite
constexpr int HealthBoxHeight = 12;   // Height of healthbox sprite
constexpr int HealthBarBorder = 3;    // Border inside health box
constexpr int HealthBarWidth = HealthBoxWidth - (HealthBarBorder * 2) - 2;
constexpr int HealthBarHeight = HealthBoxHeight - (HealthBarBorder * 2) - 2;

constexpr int BeltSlotSize = 28; // Same as INV_SLOT_SIZE_PX
constexpr int BeltSlotSpacing = 1;
constexpr int BeltSlotsPerRow = 8; // Display belt as 8x1 grid (all slots side by side)

// Belt slot position in the main panel sprite (from DrawInvBelt)
// The belt area in the panel is at { 205, 21, 232, 28 }
constexpr int BeltPanelX = 205;
constexpr int BeltPanelY = 21; // Y position in the panel sprite (accounting for PanelPaddingHeight)

// Skill slot constants
constexpr int NumSkillSlots = 4; // Display first 4 hotkey slots

// Panel field dimensions (for name display, similar to character panel)
constexpr int PanelFieldHeight = 20;
constexpr int PanelFieldPaddingTop = 2;
constexpr int PanelFieldPaddingSide = 4;

/**
 * @brief Draw a golden border around a rectangular area using box sprites
 * Uses the same boxleftend/boxmiddle/boxrightend sprites as charpanel
 * Uses boxleftend/boxmiddle/boxrightend sprites with bottom portion cropped from top
 * and rendered as a fake bottom border to create borders with proper golden styling
 */
void DrawGoldenBorder(const Surface &out, Rectangle rect)
{
	if (!LocalCoopBoxLeft || !LocalCoopBoxMiddle || !LocalCoopBoxRight) {
		// Fallback to FillRect if sprites not loaded
		const uint8_t borderColor = PAL16_YELLOW + 2;
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, 1, borderColor); // Top
		FillRect(out, rect.position.x, rect.position.y + rect.size.height - 1, rect.size.width, 1, borderColor); // Bottom
		FillRect(out, rect.position.x, rect.position.y, 1, rect.size.height, borderColor); // Left
		FillRect(out, rect.position.x + rect.size.width - 1, rect.position.y, 1, rect.size.height, borderColor); // Right
		return;
	}

	const ClxSprite left = (*LocalCoopBoxLeft)[0];
	const ClxSprite middle = (*LocalCoopBoxMiddle)[0];
	const ClxSprite right = (*LocalCoopBoxRight)[0];

	// Sprites are 24px tall (PanelFieldHeight)
	const int spriteHeight = left.height(); // Should be 24
	const int width = rect.size.width;
	const int height = rect.size.height;

	// Calculate bottom border height: split region roughly in half if height > 20px
	// Otherwise use a smaller bottom border
	int bottomBorderHeight;
	if (height > 20) {
		bottomBorderHeight = height / 2; // Use half the height for bottom border
	} else {
		bottomBorderHeight = 4; // Use smaller border for small regions
	}

	// Render top portion of sprite (cropped from bottom)
	// The subregion clips what we render so only the top portion shows
	int topHeight = height - bottomBorderHeight;
	if (topHeight > 0) {
		// Left end - tile vertically if needed
		int leftY = rect.position.y;
		int leftRemaining = topHeight;
		while (leftRemaining > 0) {
			int leftChunkHeight = std::min(leftRemaining, spriteHeight);
			Surface leftRegion = out.subregion(rect.position.x, leftY, left.width(), leftChunkHeight);
			RenderClxSprite(leftRegion, left, { 0, 0 });
			leftY += leftChunkHeight;
			leftRemaining -= leftChunkHeight;
		}

		// Middle - tile across width and vertically if needed
		int middleStart = rect.position.x + left.width();
		int middleLen = width - left.width() - right.width();
		if (middleLen > 0) {
			int middleY = rect.position.y;
			int middleRemaining = topHeight;
			while (middleRemaining > 0) {
				int middleChunkHeight = std::min(middleRemaining, spriteHeight);
				Surface middleRegion = out.subregion(middleStart, middleY, middleLen, middleChunkHeight);
				RenderClxSprite(middleRegion, middle, { 0, 0 });
				middleY += middleChunkHeight;
				middleRemaining -= middleChunkHeight;
			}
		}

		// Right end - tile vertically if needed
		int rightY = rect.position.y;
		int rightRemaining = topHeight;
		while (rightRemaining > 0) {
			int rightChunkHeight = std::min(rightRemaining, spriteHeight);
			Surface rightRegion = out.subregion(rect.position.x + width - right.width(), rightY, right.width(), rightChunkHeight);
			RenderClxSprite(rightRegion, right, { 0, 0 });
			rightY += rightChunkHeight;
			rightRemaining -= rightChunkHeight;
		}
	}

	// Render bottom portion of sprite as the bottom border
	// We need to render the sprite shifted up so only the bottom portion is visible
	int bottomY = rect.position.y + height - bottomBorderHeight;
	int spriteYOffset = -(spriteHeight - bottomBorderHeight); // Negative offset to shift sprite up

	// Left end bottom
	Surface bottomLeftRegion = out.subregion(rect.position.x, bottomY, left.width(), bottomBorderHeight);
	RenderClxSprite(bottomLeftRegion, left, { 0, spriteYOffset });

	// Middle bottom
	int middleLen = width - left.width() - right.width();
	if (middleLen > 0) {
		Surface bottomMiddleRegion = out.subregion(rect.position.x + left.width(), bottomY, middleLen, bottomBorderHeight);
		RenderClxSprite(bottomMiddleRegion, middle, { 0, spriteYOffset });
	}

	// Right end bottom
	Surface bottomRightRegion = out.subregion(rect.position.x + width - right.width(), bottomY, right.width(), bottomBorderHeight);
	RenderClxSprite(bottomRightRegion, right, { 0, spriteYOffset });
}

/**
 * @brief Draw a golden-bordered field (like character panel fields)
 */
void DrawCoopPanelField(const Surface &out, Point pos, int len)
{
	if (!LocalCoopBoxLeft || !LocalCoopBoxMiddle || !LocalCoopBoxRight)
		return;

	const ClxSprite left = (*LocalCoopBoxLeft)[0];
	const ClxSprite middle = (*LocalCoopBoxMiddle)[0];
	const ClxSprite right = (*LocalCoopBoxRight)[0];

	RenderClxSprite(out, left, pos);
	pos.x += left.width();
	int middleLen = len - left.width() - right.width();
	if (middleLen > 0) {
		RenderClxSprite(out.subregion(pos.x, pos.y, middleLen, middle.height()), middle, { 0, 0 });
	}
	pos.x += middleLen;
	RenderClxSprite(out, right, pos);
}

/**
 * @brief Draw a health or mana bar with golden border using sprites
 * Uses boxleftend/boxmiddle/boxrightend sprites with bottom 2px cropped from top portion
 * and rendered as a fake bottom border to create shorter bars with proper golden borders
 */
void DrawCoopSmallBar(const Surface &out, Point position, int width, int height, int current, int max, bool isMana, bool hasManaShield, bool isXP = false)
{
	// Draw the golden border
	DrawGoldenBorder(out, Rectangle { position, Size { width, height } });

	// Calculate inner area for the bar fill (between top sprite and bottom border)
	const int borderSize = 3; // Thicker golden border for better visibility
	const int bottomBorderHeight = 4; // Height of fake bottom border from bottom of sprite
	int innerX = position.x + borderSize;
	int innerY = position.y + borderSize;
	int innerWidth = width - (borderSize * 2);
	int innerHeight = height - borderSize - bottomBorderHeight + 1;
	
	// Draw black/transparent background for the bar area
	FillRect(out, innerX, innerY, innerWidth, innerHeight, 0); // Black fill

	// Draw fill bar
	if (max > 0 && current > 0) {
		int fillWidth = ((innerWidth * current) / max);
		if (fillWidth > innerWidth) fillWidth = innerWidth;

		if (fillWidth > 0) {
			uint8_t fillColor;
			if (isXP) {
				fillColor = PAL16_GRAY + 13; // White-ish for XP
			} else if (isMana) {
				fillColor = PAL16_BLUE + 12; // Bright blue
			} else if (hasManaShield) {
				fillColor = PAL16_YELLOW + 8;
			} else {
				fillColor = PAL16_RED + 8;
			}
			FillRect(out, innerX, innerY, fillWidth, innerHeight, fillColor);
		}
	}
}

/**
 * @brief Draw a single skill slot with configurable size
 */
void DrawPlayerSkillSlotSmall(const Surface &out, const Player &player, int slotIndex, Point position, int slotSize)
{
	// Get spell from hotkey slot
	SpellID spell = player._pSplHotKey[slotIndex];
	SpellType spellType = player._pSplTHotKey[slotIndex];

	// Button labels
	static constexpr std::string_view ButtonLabels[] = { "X", "Y", "A", "B" };
	const char *label = slotIndex < 4 ? ButtonLabels[slotIndex].data() : "";

	// HintBox sprite is 39x39, icon is 37x37
	constexpr int HintBoxSize = 39;
	constexpr int IconSize = 37;
	constexpr int IconBorderPadding = 2;

	// Center the hintBox within the slotSize
	int borderOffset = (slotSize - HintBoxSize) / 2;
	Point borderPos = { position.x + borderOffset - 1, position.y + borderOffset - 2};
	
	// Position calculations for spell icon (bottom-left based)
	// Add extra padding to shrink the visible icon area for a thicker border
	// +1px additional padding for better visual separation
	const Displacement spellIconDisplacement = { IconBorderPadding, HintBoxSize - IconBorderPadding };
	Point iconPos = borderPos + spellIconDisplacement;

	const uint64_t spells = player._pAblSpells | player._pMemSpells | player._pScrlSpells | player._pISpells;

	// If no spell assigned or spell not available, draw empty gray slot
	if (spell == SpellID::Invalid || spellType == SpellType::Invalid || (spells & GetSpellBitmask(spell)) == 0) {
		// Draw empty icon with gray coloring
		SetSpellTrans(SpellType::Invalid);
		DrawSmallSpellIcon(out, iconPos, SpellID::Null);
	} else {
		// Determine spell validity for coloring
		SpellType transType = spellType;
		if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown()) {
			transType = SpellType::Invalid;
		}
		if (spellType == SpellType::Spell) {
			int spellLevel = player.GetSpellLevel(spell);
			if (spellLevel == 0)
				transType = SpellType::Invalid;
		}

		// Draw the spell icon
		SetSpellTrans(transType);
		DrawSmallSpellIcon(out, iconPos, spell);
	}

	// Draw border sprite
	if (GetHintBoxSprite()) {
		RenderClxSprite(out, (*GetHintBoxSprite())[0], borderPos);
	}

	// Draw button label AFTER icon (bottom-right inside slot)
	DrawString(out, label, { { borderPos.x + HintBoxSize - 14, borderPos.y + HintBoxSize - 14 }, { 12, 0 } },
	    { .flags = UiFlags::ColorWhite | UiFlags::Outlined | UiFlags::FontSize12, .spacing = 0 });
}

/**
 * @brief Draw 2x2 grid of skill slots with configurable size
 */
void DrawPlayerSkillSlots2x2Small(const Surface &out, const Player &player, Point basePosition, int slotSize)
{
	constexpr int spacing = 1; // Reduced spacing between slots
	// Grid layout: A B / X Y  ->  indices 2,3 / 0,1
	static constexpr int SlotGrid[2][2] = {
		{ 2, 3 }, // Top row: A, B
		{ 0, 1 }  // Bottom row: X, Y
	};
	
	for (int row = 0; row < 2; row++) {
		for (int col = 0; col < 2; col++) {
			int slotX = basePosition.x + col * (slotSize + spacing);
			int slotY = basePosition.y + row * (slotSize + spacing);
			DrawPlayerSkillSlotSmall(out, player, SlotGrid[row][col], { slotX, slotY }, slotSize);
		}
	}
}

/**
 * @brief Draw 4 skill slots in a vertical column (1x4 layout)
 * Order from top to bottom: A, B, X, Y (indices 2, 3, 0, 1)
 */
void DrawPlayerSkillSlotsVertical(const Surface &out, const Player &player, Point basePosition, int slotSize, int totalHeight)
{
	// 4 slots stacked vertically with spacing to fill totalHeight
	// Calculate spacing between slots
	const int totalSlotHeight = slotSize * 4;
	const int totalSpacing = totalHeight - totalSlotHeight;
	const int spacing = totalSpacing / 3; // 3 gaps between 4 slots
	
	// Order: A, B, X, Y (indices 2, 3, 0, 1)
	static constexpr int SlotOrder[] = { 2, 3, 0, 1 };
	
	for (int i = 0; i < 4; i++) {
		int slotY = basePosition.y + i * (slotSize + spacing);
		DrawPlayerSkillSlotSmall(out, player, SlotOrder[i], { basePosition.x, slotY }, slotSize);
	}
}

/**
 * @brief Draw the panel background using charbg.clx (character panel background)
 * The sprite is tiled to fill the panel area with rock texture.
 */
void DrawCoopPanelBackground(const Surface &out, Rectangle rect)
{
	// Use the charbg.clx sprite for panel background
	if (LocalCoopCharBg) {
		const ClxSprite bg = (*LocalCoopCharBg)[0];
		const int srcW = bg.width();
		const int srcH = bg.height();
		
		const int rightBorderWidth = 7;  // Right fake border width (2px larger)
		const int bottomBorderWidth = 11; // Bottom fake border width (6px larger)
		
		// Tile the background to fill the main panel area (excluding fake borders)
		const int mainAreaWidth = rect.size.width - rightBorderWidth;
		const int mainAreaHeight = rect.size.height - bottomBorderWidth;
		
		for (int y = 0; y < mainAreaHeight; y += srcH) {
			for (int x = 0; x < mainAreaWidth; x += srcW) {
				// Calculate how much to draw (clip at panel edges)
				int drawW = std::min(srcW, mainAreaWidth - x);
				int drawH = std::min(srcH, mainAreaHeight - y);
				
				// Use subregion to clip the sprite rendering
				Surface subSurf = out.subregion(rect.position.x + x, rect.position.y + y, drawW, drawH);
				RenderClxSprite(subSurf, bg, { 0, 0 });
			}
		}
		
		// Add fake right border: crop rightmost edge of background sprite
		for (int y = 0; y < rect.size.height; y += srcH) {
			int drawH = std::min(srcH, rect.size.height - y);
			int srcX = srcW - rightBorderWidth; // Take rightmost edge
			
			Surface borderSurf = out.subregion(rect.position.x + mainAreaWidth, rect.position.y + y, rightBorderWidth, drawH);
			// Render with offset to show only the right edge
			RenderClxSprite(borderSurf, bg, { -srcX, 0 });
		}
		
		// Add fake bottom border: crop bottom edge of background sprite  
		for (int x = 0; x < mainAreaWidth; x += srcW) {
			int drawW = std::min(srcW, mainAreaWidth - x);
			int srcY = srcH - bottomBorderWidth; // Take bottom edge
			
			Surface borderSurf = out.subregion(rect.position.x + x, rect.position.y + mainAreaHeight, drawW, bottomBorderWidth);
			// Render with offset to show only the bottom edge
			RenderClxSprite(borderSurf, bg, { 0, -srcY });
		}
		
		// Add fake corner: bottom-right corner piece
		{
			int srcX = srcW - rightBorderWidth;
			int srcY = srcH - bottomBorderWidth;
			Surface cornerSurf = out.subregion(rect.position.x + mainAreaWidth, rect.position.y + mainAreaHeight, rightBorderWidth, bottomBorderWidth);
			RenderClxSprite(cornerSurf, bg, { -srcX, -srcY });
		}
	} else {
		// Fallback: fill with dark color
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PAL16_GRAY + 1);
	}
}

/**
 * @brief Cast a spell from a specific hotkey slot for a local coop player.
 * 
 * @param localIndex The local player index (0-2 for players 2-4)
 * @param slotIndex The hotkey slot index (0-3 corresponding to D-pad Up/Down/Left/Right)
 */
void CastLocalCoopHotkeySpell(int localIndex, int slotIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;

	const size_t playerId = static_cast<size_t>(localIndex) + 1;
	if (playerId >= Players.size())
		return;

	Player &player = Players[playerId];
	if (player._pHitPoints <= 0)
		return;

	// Check if player is already performing an action - prevent duplicate casts
	if (!player.CanChangeAction() || player.destAction != ACTION_NONE)
		return;

	// Get spell from hotkey slot
	SpellID spell = player._pSplHotKey[slotIndex];
	SpellType spellType = player._pSplTHotKey[slotIndex];

	// If no spell assigned, perform a regular attack instead
	if (spell == SpellID::Invalid || spellType == SpellType::Invalid) {
		// Perform primary action (attack) just like pressing A button
		PerformLocalCoopPrimaryAction(localIndex);
		return;
	}

	// Check if spell can be cast in town
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown())
		return;

	// Check if player has the spell available
	uint64_t spells = 0;
	switch (spellType) {
	case SpellType::Skill:
		spells = player._pAblSpells;
		break;
	case SpellType::Spell:
		spells = player._pMemSpells;
		break;
	case SpellType::Scroll:
		spells = player._pScrlSpells;
		break;
	case SpellType::Charges:
		spells = player._pISpells;
		break;
	default:
		return;
	}

	if ((spells & GetSpellBitmask(spell)) == 0)
		return;

	// For spell type, check spell level
	if (spellType == SpellType::Spell && player.GetSpellLevel(spell) == 0)
		return;

	// Calculate target position for the spell
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	Point spellTarget = player.position.future + Displacement(player._pdir);

	if (cursor.pcursmonst != -1) {
		spellTarget = Monsters[cursor.pcursmonst].position.tile;
	} else if (cursor.playerUnderCursor != nullptr && cursor.playerUnderCursor != &player) {
		spellTarget = cursor.playerUnderCursor->position.future;
	}

	// Cast the spell
	NetSendCmdLocParam3(true, CMD_SPELLXY, spellTarget,
		static_cast<int16_t>(spell),
		static_cast<int8_t>(spellType), playerId);
}

void DrawPlayerBeltSlot(const Surface &out, const Player &player, int slotIndex, Point position)
{
	const Item &item = player.SpdList[slotIndex];

	if (item.isEmpty())
		return;

	// Draw black background for the slot
	FillRect(out, position.x, position.y, BeltSlotSize - 4, BeltSlotSize - 4, 0);

	// Draw item quality background (colorizes the existing background)
	InvDrawSlotBack(out, { position.x, position.y + BeltSlotSize - 4 }, { BeltSlotSize - 4, BeltSlotSize - 4 }, item._iMagical);

	// Get item sprite
	const int cursId = item._iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);

	// Draw golden outline around the item (sprite-based border)
	Point itemPos = { position.x, position.y + BeltSlotSize - 4 };
	ClxDrawOutline(out, GetOutlineColor(item, true), itemPos, sprite);

	// Draw the item (position needs to be at bottom-left of slot for ClxDraw)
	DrawItem(item, out, itemPos, sprite);
}

void DrawPlayerBelt(const Surface &out, const Player &player, Point basePosition, bool alignRight)
{
	// Draw golden border around the belt (same style as health/mana bars)
	// Belt region increased by 4 pixels (right and bottom), width reduced by 1px
	DrawGoldenBorder(out, Rectangle { basePosition, Size { 237, 34 } });

	// Belt area in the main panel sprite: { 205, 21, 232, 28 }
	// Offset panel box by 2 pixels to the right and 2 pixels to the bottom
	DrawPanelBox(out, { 205, 21, 232, 28 }, basePosition + Displacement { 3, 3 });

	// Draw belt items using the same positioning as the main panel
	for (int i = 0; i < MaxBeltItems; i++) {
		if (player.SpdList[i].isEmpty()) {
			continue;
		}

		// Calculate slot position (29px spacing between slots, starting at x=207 relative to belt area)
		// Offset slots by 2px to the right and 2px to the bottom
		const int slotX = basePosition.x + (i * 29) + 4;
		const int slotY = basePosition.y + 2;
		const Point position { slotX, slotY + InventorySlotSizeInPixels.height };
		
		// Draw item quality background
		InvDrawSlotBack(out, position, InventorySlotSizeInPixels, player.SpdList[i]._iMagical);
		
		// Get item sprite
		const int cursId = player.SpdList[i]._iCurs + CURSOR_FIRSTITEM;
		const ClxSprite sprite = GetInvItemSprite(cursId);

		// Draw the item
		DrawItem(player.SpdList[i], out, position, sprite);
	}
}

} // namespace - end anonymous namespace for drawing functions below to be accessible

void DrawPlayerSkillSlot(const Surface &out, const Player &player, int slotIndex, Point position)
{
	// Get spell from hotkey slot
	SpellID spell = player._pSplHotKey[slotIndex];
	SpellType spellType = player._pSplTHotKey[slotIndex];

	// Button labels for each slot
	// Slots are mapped as: A=2, B=3, X=0, Y=1 (to match gamepad buttons)
	static constexpr std::string_view ButtonLabels[] = { "X", "Y", "A", "B" };
	const char *label = slotIndex < 4 ? ButtonLabels[slotIndex].data() : "";

	// HintBox sprite is 39x39, spell icon is 37x38
	// RenderClxSprite uses TOP-LEFT position
	// DrawSmallSpellIcon uses BOTTOM-LEFT position
	// spellIconDisplacement converts from top-left border pos to bottom-left icon pos
	constexpr int HintBoxSize = 39;
	constexpr int IconSize = 37;
	
	// Position is the TOP-LEFT of where we want the slot to appear
	// borderPos is TOP-LEFT for RenderClxSprite (hintBox)
	// iconPos is BOTTOM-LEFT for DrawSmallSpellIcon
	// From DrawSpellsCircleMenuHint: spellIconDisplacement = { (39-37)/2 + 1, 39 - (39-37)/2 - 1 } = { 2, 37 }
	const Displacement spellIconDisplacement = { (HintBoxSize - IconSize) / 2 + 1, HintBoxSize - (HintBoxSize - IconSize) / 2 - 1 };
	Point borderPos = position; // TOP-LEFT for RenderClxSprite
	Point iconPos = position + spellIconDisplacement; // BOTTOM-LEFT for DrawSmallSpellIcon

	// Check if player has this spell available
	const uint64_t spells = player._pAblSpells | player._pMemSpells | player._pScrlSpells | player._pISpells;
	
	// If no spell assigned or spell not available, draw empty gray slot
	if (spell == SpellID::Invalid || spellType == SpellType::Invalid || (spells & GetSpellBitmask(spell)) == 0) {
		// Draw border sprite FIRST (top-left position) so icon renders on top
		if (GetHintBoxSprite()) {
			RenderClxSprite(out, (*GetHintBoxSprite())[0], borderPos);
		}
		// Draw empty icon with gray coloring (matches DrawSpellsCircleMenuHint behavior)
		SetSpellTrans(SpellType::Invalid);
		DrawSmallSpellIcon(out, iconPos, SpellID::Null);
		// Draw button label in bottom-right corner of slot
		DrawString(out, label, { { position.x + HintBoxSize - 14, position.y + HintBoxSize - 14 }, { 12, 0 } },
		    { .flags = UiFlags::ColorWhite | UiFlags::Outlined | UiFlags::FontSize12, .spacing = 0 });
		return;
	}

	// Determine spell validity for coloring
	SpellType transType = spellType;
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown()) {
		transType = SpellType::Invalid;
	}
	if (spellType == SpellType::Spell) {
		int spellLevel = player.GetSpellLevel(spell);
		if (spellLevel == 0)
			transType = SpellType::Invalid;
	}

	// Draw golden border sprite FIRST so spell icon renders on top
	if (GetHintBoxSprite()) {
		RenderClxSprite(out, (*GetHintBoxSprite())[0], borderPos);
	}
	
	// Draw the spell icon (37x38) - bottom-left position
	SetSpellTrans(transType);
	DrawSmallSpellIcon(out, iconPos, spell);

	// Draw button label in bottom-right corner (over the icon)
	DrawString(out, label, { { position.x + HintBoxSize - 14, position.y + HintBoxSize - 14 }, { 12, 0 } },
	    { .flags = UiFlags::ColorWhite | UiFlags::Outlined | UiFlags::FontSize12, .spacing = 0 });
}

void DrawPlayerSkillSlots2x2(const Surface &out, const Player &player, Point basePosition)
{
	// HintBox (border) is 39x39, use this for layout spacing
	constexpr int HintBoxSize = 39;
	constexpr int SlotSpacing = 2;
	
	// Skills are displayed as 2x2 grid:
	// Top row: A (index 2), B (index 3)
	// Bottom row: X (index 0), Y (index 1)
	static constexpr int SlotGrid[2][2] = {
		{ 2, 3 }, // Top row: A, B
		{ 0, 1 }  // Bottom row: X, Y
	};
	
	for (int row = 0; row < 2; row++) {
		for (int col = 0; col < 2; col++) {
			int slotX = basePosition.x + col * (HintBoxSize + SlotSpacing);
			int slotY = basePosition.y + row * (HintBoxSize + SlotSpacing);
			DrawPlayerSkillSlot(out, player, SlotGrid[row][col], { slotX, slotY });
		}
	}
}

bool LocalCoopHUDOpen = true;

void DrawLocalCoopPlayerHUD(const Surface &out)
{
	// Show HUD only in multiplayer when local co-op is actually enabled (2+ controllers)
	if (!gbIsMultiplayer || !IsLocalCoopEnabled())
		return;

	// Allow HUD to be toggled off
	if (!LocalCoopHUDOpen)
		return;

	// Ensure HUD assets are loaded
	InitLocalCoopHUDAssets();

	// Panel layout constants - charbg sprite borders
	constexpr int topBorderPadding = 5;      // Top border in charbg.clx
	constexpr int leftBorderPadding = 7;     // Left border (1px more to shift elements right)
	constexpr int rightBorderPadding = 8;    // Right border (content padding, not visual border) - reduced by 1px
	constexpr int bottomBorderPadding = 19;  // Bottom border (content padding, not visual border)
	constexpr int panelEdgePadding = 0;      // No padding from screen edge - panels touch edges
	constexpr int elementSpacing = 1;        // Reduced spacing between elements
	constexpr int nameTopOffset = 8;         // Offset for name row (1px more down)
	constexpr int nameLeftOffset = 0;        // No extra offset (elements shifted via leftBorderPadding)
	constexpr int barsToNameSpacing = 3;     // 3px spacing between name and bars
	constexpr int barsExtraDownOffset = 7;   // Extra 7px down for bars and belt (1px more)
	constexpr int barsExtraRightOffset = 0;  // No extra offset - bars and belt aligned with left edge
	constexpr int barsToBeltSpacing = 1;     // 1px spacing between bars and belt
	
	// Health/mana bar dimensions - with golden border
	constexpr int barHeight = 11; // Height increased by 1px for taller golden border sprite
	constexpr int barSpacing = 2;
	// Bars: health, mana (full height only, no XP bar)
	constexpr int barsHeight = barHeight * 2 + barSpacing; // = 10+10+2 = 22px
	
	// Skill slot dimensions - 2x2 grid OUTSIDE panel
	// Use 41px slots (39px hintBox + 1px padding each side)
	constexpr int SkillSlotSize = 41; // Slightly larger than hintBox for better fit
	constexpr int skillSlotSpacing = 1; // Reduced spacing between skill slots
	constexpr int skillColumnSpacing = 2; // Gap between panel and skill column
	constexpr int skillGridWidth = SkillSlotSize * 2 + skillSlotSpacing; // Width of 2x2 grid
	constexpr int skillGridHeight = SkillSlotSize * 2 + skillSlotSpacing; // Height of 2x2 grid
	
	// Belt dimensions - use actual sprite dimensions from main panel
	constexpr int beltWidth = 232; // Full belt sprite width
	constexpr int beltHeight = 28; // Belt sprite height from main panel
	
	// Name field height  
	constexpr int nameFieldHeight = 20;
	constexpr int playerNumWidth = 28;  // P# and level field width (increased by 3px)
	constexpr int playerNumSpacing = 1; // 1px spacing between P#/name/level
	
	// Content area width (belt width) - skills are now outside the panel
	const int contentWidth = beltWidth;
	// Bar width = full content width + 5px for wider bars (reduced by 1px)
	const int barsWidth = contentWidth + 5;
	
	// Panel content height: name row + middle (bars/belt stacked) 
	// Middle section: bars + spacing + belt
	constexpr int middleContentHeight = barsHeight + barsToBeltSpacing + beltHeight;
	
	// Panel dimensions with proper borders
	// Layout: topBorder + nameTopOffset + [name row] + spacing + [middle: bars+belt | skills] + bottomBorder
	const int panelContentWidth = contentWidth + 6; // 6px wider (increased by 1px to compensate for reduced right padding)
	const int panelHeight = topBorderPadding + nameTopOffset + nameFieldHeight + elementSpacing + middleContentHeight + bottomBorderPadding + 2; // 2px taller
	const int panelWidth = leftBorderPadding + panelContentWidth + rightBorderPadding;
	
	constexpr int durabilityIconHeight = 32;
	constexpr int durabilityIconSpacing = 4;

	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();

	// Check if any local co-op player has actually spawned (initialized)
	bool anyCoopPlayerInitialized = false;
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].initialized) {
			anyCoopPlayerInitialized = true;
			break;
		}
	}

	for (size_t playerId = 0; playerId < totalPlayers && playerId < Players.size(); ++playerId) {
		const Player &player = Players[playerId];

		// Skip if player is not active in the game
		if (!player.plractive)
			continue;

		// Player 1 only shows corner HUD when main panel is hidden
		if (playerId == 0 && !anyCoopPlayerInitialized)
			continue;

		// For local co-op players, check their state
		if (playerId > 0) {
			if (playerId - 1 >= g_LocalCoop.players.size())
				continue;
			const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[playerId - 1];
			if (!coopPlayer.active || coopPlayer.characterSelectActive || !coopPlayer.initialized)
				continue;
		}

		// Determine panel position based on player ID - with 1px edge padding
		int panelX, panelY;
		bool atBottom = false;
		bool skillsOnLeft = false; // For P2, skills go on left side

		switch (playerId) {
		case 0: // Player 1 - Bottom-left
			panelX = panelEdgePadding;
			panelY = out.h() - panelHeight - panelEdgePadding;
			atBottom = true;
			break;
		case 1: // Player 2 - Bottom-right (skills on LEFT)
			panelX = out.w() - panelWidth - panelEdgePadding;
			panelY = out.h() - panelHeight - panelEdgePadding;
			atBottom = true;
			skillsOnLeft = true;
			break;
		case 2: // Player 3 - Top-left
			panelX = panelEdgePadding;
			panelY = panelEdgePadding;
			break;
		case 3: // Player 4 - Top-right (skills on LEFT)
			panelX = out.w() - panelWidth - panelEdgePadding;
			panelY = panelEdgePadding;
			skillsOnLeft = true;
			break;
		default:
			continue;
		}

		// Get player stats
		int currentHP = player._pHitPoints >> 6;
		int maxHP = player._pMaxHP >> 6;
		int currentMana = player._pMana >> 6;
		int maxMana = player._pMaxMana >> 6;
		bool hasManaShield = player.pManaShield;

		// Draw panel background (charbg has built-in 5px golden border)
		DrawCoopPanelBackground(out, { { panelX, panelY }, { panelWidth, panelHeight } });

		// Content area starts after the border padding + nameTopOffset
		int contentX = panelX + leftBorderPadding;
		int currentY = panelY + topBorderPadding + nameTopOffset;

		// === TOP ROW: Player number + Name + Level ===
		// Apply nameLeftOffset (1px extra right)
		int nameRowX = contentX + nameLeftOffset;
		int extraOffset = 1;
		
		// Draw player number field on the left
		DrawCoopPanelField(out, { nameRowX, currentY }, playerNumWidth - extraOffset);
		char playerNumStr[4];
		snprintf(playerNumStr, sizeof(playerNumStr), "P%zu", playerId + 1);
		DrawString(out, playerNumStr, 
		    { { nameRowX + 2, currentY + 2 }, { playerNumWidth - 4, nameFieldHeight - 4 } },
		    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorWhite, .spacing = 0 });

		// Draw player name in middle (reduced width to make room for level)
		int nameX = nameRowX + playerNumWidth + playerNumSpacing - extraOffset;
		int nameWidth = panelContentWidth - playerNumWidth - playerNumSpacing - playerNumSpacing - playerNumWidth - nameLeftOffset;
		DrawCoopPanelField(out, { nameX, currentY }, nameWidth);
		DrawString(out, player._pName, 
		    { { nameX + 2, currentY + 2 }, { nameWidth - 4, nameFieldHeight - 4 } },
		    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorWhite, .spacing = 1 });

		// Draw level field on the right (same size as P#)
		int levelX = nameRowX + panelContentWidth - playerNumWidth - nameLeftOffset - extraOffset;
		DrawCoopPanelField(out, { levelX, currentY }, playerNumWidth);
		char levelStr[4];
		snprintf(levelStr, sizeof(levelStr), "%d", player.getCharacterLevel());
		DrawString(out, levelStr, 
		    { { levelX + 2, currentY + 2 }, { playerNumWidth - 4, nameFieldHeight - 4 } },
		    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorWhite, .spacing = 0 });

		currentY += nameFieldHeight + elementSpacing;

		// === MIDDLE ROW: Bars then Belt ===
		int middleY = currentY + barsExtraDownOffset;
		
		// Bars: Health, Mana (stacked vertically) - with extra right offset
		int barX = contentX + barsExtraRightOffset;
		int barY = middleY; // Start bars right after name row + extra offset
		
		// Health bar
		DrawCoopSmallBar(out, { barX, barY - 1 }, barsWidth, barHeight, currentHP, maxHP, false, hasManaShield);
		barY += barHeight + barSpacing;
		
		// Mana bar
		DrawCoopSmallBar(out, { barX, barY - 3 }, barsWidth, barHeight, currentMana, maxMana, true, false);
		barY += barHeight + barsToBeltSpacing;

		// Skill grid: OUTSIDE panel, 2x2 layout
		// For P2/P4 (right side of screen), skills go on LEFT of panel
		int skillX;
		if (skillsOnLeft) {
			skillX = panelX - skillColumnSpacing - skillGridWidth + 1; // Moved 1px to the right
		} else {
			skillX = panelX + panelWidth + skillColumnSpacing + 1; // Moved 1px to the right
		}
		// Center the skill grid vertically on the panel, moved 1px up
		int skillY = panelY + (panelHeight - skillGridHeight) / 2 - 1;
		DrawPlayerSkillSlots2x2Small(out, player, { skillX, skillY }, SkillSlotSize);

		// === BELT: Right after XP bar ===
		DrawPlayerBelt(out, player, { contentX + barsExtraRightOffset, barY - 4 }, false);

		// Draw durability icons - above panel for bottom players, below panel for top players
		int durabilityY;
		if (atBottom) {
			// For bottom players, draw icons ABOVE the panel
			durabilityY = panelY - durabilityIconSpacing - durabilityIconHeight;
		} else {
			// For top players, draw icons BELOW the panel
			durabilityY = panelY + panelHeight + durabilityIconSpacing;
		}
		DrawPlayerDurabilityIcons(out, player, { panelX + leftBorderPadding, durabilityY }, false);
	}
}

void HandleLocalCoopControllerDisconnect(SDL_JoystickID controllerId)
{
	if (!g_LocalCoop.enabled)
		return;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		if (coopPlayer.active && coopPlayer.controllerId == controllerId) {
			Log("Local co-op: Player {} controller disconnected", i + 2);
			// Don't fully deactivate - player might reconnect
			// Just stop processing input
			coopPlayer.leftStickX = 0;
			coopPlayer.leftStickY = 0;
			coopPlayer.leftStickXUnscaled = 0;
			coopPlayer.leftStickYUnscaled = 0;
			break;
		}
	}
}

void HandleLocalCoopControllerConnect(SDL_JoystickID controllerId)
{
	// Check if local co-op option is enabled
	if (!*GetOptions().Gameplay.enableLocalCoop)
		return;

	// If local co-op isn't initialized yet, try to initialize it now
	if (!g_LocalCoop.enabled) {
		const auto &controllers = GameController::All();
		if (controllers.size() >= 2) {
			// We now have enough controllers, initialize local co-op
			InitLocalCoop();
			// After initialization, check if this controller was assigned
			// (it might be controller 0 which goes to player 1, or controller 1+ which goes to local co-op)
			for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
				if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].controllerId == controllerId) {
					// This controller was assigned to a local co-op player during InitLocalCoop
					// Load available heroes for character selection
					LoadAvailableHeroesForLocalPlayer(static_cast<int>(i));
					return;
				}
			}
			// If we get here, the controller is controller 0 (player 1's controller), so we're done
			return;
		}
		// Not enough controllers yet, wait for more
		return;
	}

	// Check if this controller is controller 0 (player 1's controller)
	// Controller 0 should not be assigned to local co-op players
	const auto &controllers = GameController::All();
	if (!controllers.empty() && controllers[0].GetInstanceId() == controllerId) {
		Log("Local co-op: Controller {} is player 1's controller, ignoring", controllerId);
		return;
	}

	// Check if this controller was previously assigned
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		if (coopPlayer.active && coopPlayer.controllerId == controllerId) {
			Log("Local co-op: Player {} controller reconnected", i + 2);
			return;
		}
	}

	// If game is running, try mid-game join
	if (gbRunGame) {
		TryJoinLocalCoopMidGame(controllerId);
		return;
	}

	// Try to assign to an empty slot (pre-game)
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		if (!coopPlayer.active) {
			coopPlayer.Reset();
			coopPlayer.active = true;
			coopPlayer.controllerId = controllerId;
			Log("Local co-op: New controller assigned to Player {}", i + 2);

			// Make sure we have enough player slots
			size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
			if (Players.size() < totalPlayers) {
				size_t oldSize = Players.size();
				Players.resize(totalPlayers);
				// Initialize new player slots to inactive
				for (size_t j = oldSize; j < totalPlayers; ++j) {
					Players[j].plractive = false;
				}
				// IMPORTANT: Resizing the vector may reallocate, invalidating MyPlayer pointer
				if (MyPlayer != nullptr && MyPlayerId < Players.size()) {
					MyPlayer = &Players[MyPlayerId];
					InspectPlayer = MyPlayer;
				}
			}

			// Load available heroes
			LoadAvailableHeroesForLocalPlayer(static_cast<int>(i));
			break;
		}
	}
}

Point CalculateLocalCoopViewPosition()
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return MyPlayer->position.tile;

	int totalX = 0;
	int totalY = 0;
	int activeCount = 0;

	// Sum positions of all active players
	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
	for (size_t i = 0; i < totalPlayers && i < Players.size(); ++i) {
		const Player &player = Players[i];
		if (player.plractive && player._pHitPoints > 0) {
			totalX += player.position.tile.x;
			totalY += player.position.tile.y;
			activeCount++;
		}
	}

	// If no active players, fall back to MyPlayer
	if (activeCount == 0)
		return MyPlayer->position.tile;

	// Return average position
	return { totalX / activeCount, totalY / activeCount };
}

/**
 * @brief Calculate pixel-precise camera position for smooth local co-op scrolling.
 * 
 * For smooth camera movement with multiple players, we need to work in pixel space
 * rather than tile space. Each player's position is converted to screen pixels
 * (including their walking offset if they're moving), then we average all positions
 * and convert back to tile + offset for the camera.
 * 
 * A dead zone is applied around the camera target to prevent constant small
 * adjustments when players are close together. The camera only moves when the
 * average player position exceeds the dead zone threshold.
 * 
 * This ensures the camera moves smoothly even when:
 * - Some players are walking and others are standing
 * - Players are walking in different directions
 * - Players finish walking at different times
 * 
 * The key insight is that screen coordinates are derived from world coordinates
 * using an isometric transformation: screenX = (tileY - tileX) * 32
 *                                     screenY = (tileY + tileX) * -16
 * 
 * By working in screen space, we can average player positions with sub-tile
 * precision and convert back to a tile position + pixel offset.
 */
void UpdateLocalCoopCamera()
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return;

	// Only update camera if character selection is complete for all players
	if (g_LocalCoop.IsAnyCharacterSelectActive())
		return;

	// Check if any local co-op player has been initialized (spawned)
	bool anyInitialized = false;
	for (const auto &player : g_LocalCoop.players) {
		if (player.active && player.initialized) {
			anyInitialized = true;
			break;
		}
	}

	// Don't take over camera control until at least one local co-op player has spawned
	if (!anyInitialized)
		return;

	// Calculate pixel-precise positions for all active players in screen space
	// Screen coordinates: screenX = (tileY - tileX) * 32, screenY = (tileY + tileX) * -16
	// We scale by 256 for precision before division
	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
	int activeCount = 0;
	
	// Accumulate screen-space positions (scaled by 256 for precision)
	int64_t totalScreenX = 0;
	int64_t totalScreenY = 0;

	for (size_t i = 0; i < totalPlayers && i < Players.size(); ++i) {
		const Player &player = Players[i];
		if (player.plractive && player._pHitPoints > 0) {
			// Convert tile position to screen space (scaled by 256)
			// worldToScreen: screenX = (tileY - tileX) * 32, screenY = (tileY + tileX) * -16
			int tileX = player.position.tile.x;
			int tileY = player.position.tile.y;
			int64_t screenX = static_cast<int64_t>(tileY - tileX) * 32 * 256;
			int64_t screenY = static_cast<int64_t>(tileY + tileX) * -16 * 256;
			
			// If player is walking, add their walking offset (already in screen pixels)
			// Scale it up by 256 to match our precision
			if (player.isWalking()) {
				Displacement screenOffset = GetOffsetForWalking(player.AnimInfo, player._pdir, true);
				screenX += static_cast<int64_t>(screenOffset.deltaX) * 256;
				screenY += static_cast<int64_t>(screenOffset.deltaY) * 256;
			}
			
			totalScreenX += screenX;
			totalScreenY += screenY;
			activeCount++;
		}
	}

	// Need at least 1 active player
	if (activeCount == 0)
		return;

	// Calculate average screen position (still scaled by 256)
	int64_t avgScreenX256 = totalScreenX / activeCount;
	int64_t avgScreenY256 = totalScreenY / activeCount;
	
	// Apply dead zone: only update camera target if average position moved outside the dead zone
	// Dead zone is in screen pixels, so scale by 256 to match our precision
	const int64_t deadZone256 = static_cast<int64_t>(LocalCoopState::CameraDeadZone) * 256;
	
	if (!g_LocalCoop.cameraInitialized) {
		// First time - initialize camera to current average position
		g_LocalCoop.cameraTargetScreenX = avgScreenX256;
		g_LocalCoop.cameraTargetScreenY = avgScreenY256;
		g_LocalCoop.cameraSmoothScreenX = avgScreenX256;
		g_LocalCoop.cameraSmoothScreenY = avgScreenY256;
		g_LocalCoop.cameraInitialized = true;
	} else {
		// Calculate distance from current camera target to new average position
		int64_t deltaX = avgScreenX256 - g_LocalCoop.cameraTargetScreenX;
		int64_t deltaY = avgScreenY256 - g_LocalCoop.cameraTargetScreenY;
		
		// Check if we're outside the dead zone (using squared distance to avoid sqrt)
		// If outside, move camera target to keep average position at edge of dead zone
		int64_t distSq = deltaX * deltaX + deltaY * deltaY;
		int64_t deadZoneSq = deadZone256 * deadZone256;
		
		if (distSq > deadZoneSq) {
			// Move camera target towards average position, keeping it at dead zone distance
			// Using integer approximation: move by (delta - deadZone * delta/dist)
			// Simplified: new_target = avg - deadZone * (delta / dist)
			// We approximate dist using the larger of |deltaX| and |deltaY| + half the smaller
			int64_t absDeltaX = deltaX >= 0 ? deltaX : -deltaX;
			int64_t absDeltaY = deltaY >= 0 ? deltaY : -deltaY;
			int64_t approxDist = (absDeltaX > absDeltaY) 
				? absDeltaX + absDeltaY / 2 
				: absDeltaY + absDeltaX / 2;
			
			if (approxDist > 0) {
				// Move camera so that average position is at edge of dead zone
				g_LocalCoop.cameraTargetScreenX = avgScreenX256 - (deltaX * deadZone256) / approxDist;
				g_LocalCoop.cameraTargetScreenY = avgScreenY256 - (deltaY * deadZone256) / approxDist;
			}
		}
		// If inside dead zone, don't move camera target
	}
	
	// Apply smoothing: interpolate smoothed camera position towards target
	// This prevents jerky camera movement by gradually moving towards the target
	// Using fixed-point arithmetic: smoothFactor is represented as an integer out of 256
	constexpr int64_t smoothFactor256 = static_cast<int64_t>(LocalCoopState::CameraSmoothFactor * 256);
	
	int64_t smoothDeltaX = g_LocalCoop.cameraTargetScreenX - g_LocalCoop.cameraSmoothScreenX;
	int64_t smoothDeltaY = g_LocalCoop.cameraTargetScreenY - g_LocalCoop.cameraSmoothScreenY;
	
	g_LocalCoop.cameraSmoothScreenX += (smoothDeltaX * smoothFactor256) / 256;
	g_LocalCoop.cameraSmoothScreenY += (smoothDeltaY * smoothFactor256) / 256;
	
	// Use smoothed camera position for rendering
	int64_t cameraScreenX256 = g_LocalCoop.cameraSmoothScreenX;
	int64_t cameraScreenY256 = g_LocalCoop.cameraSmoothScreenY;
	
	// Convert back to world coordinates
	// screenToWorld: tileX = (2*screenY + screenX) / -64, tileY = (2*screenY - screenX) / -64
	// Since our values are scaled by 256, we need: tileX = (2*screenY256 + screenX256) / (-64 * 256)
	int64_t worldX256 = (2 * cameraScreenY256 + cameraScreenX256) / -64;
	int64_t worldY256 = (2 * cameraScreenY256 - cameraScreenX256) / -64;
	
	// Extract integer tile position and fractional part
	int tileX = static_cast<int>(worldX256 / 256);
	int tileY = static_cast<int>(worldY256 / 256);
	int fracX = static_cast<int>(worldX256 % 256);
	int fracY = static_cast<int>(worldY256 % 256);
	
	// Handle negative remainders
	if (fracX < 0) { fracX += 256; tileX--; }
	if (fracY < 0) { fracY += 256; tileY--; }
	
	// Set ViewPosition to the integer tile position
	ViewPosition = { tileX, tileY };
	
	// Convert the fractional world position back to screen offset
	// worldToScreen: screenX = (fracY - fracX) * 32 / 256, screenY = (fracY + fracX) * -16 / 256
	g_LocalCoop.cameraOffsetX = ((fracY - fracX) * 32) / 256;
	g_LocalCoop.cameraOffsetY = ((fracY + fracX) * -16) / 256;
}

Displacement GetLocalCoopCameraOffset()
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return {};

	// Check if any local co-op player has been initialized (spawned)
	bool anyInitialized = false;
	for (const auto &player : g_LocalCoop.players) {
		if (player.active && player.initialized) {
			anyInitialized = true;
			break;
		}
	}

	// Don't provide camera offset until at least one local co-op player has spawned
	if (!anyInitialized)
		return {};

	// Return the pre-calculated average offset from UpdateLocalCoopCamera
	return { g_LocalCoop.cameraOffsetX, g_LocalCoop.cameraOffsetY };
}

bool IsLocalCoopPositionOnScreen(Point tilePos)
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return true; // No restriction if local co-op is not enabled

	// If no local co-op players have joined yet, don't restrict movement
	// This allows Player 1 to move freely until other players join
	if (g_LocalCoop.GetActivePlayerCount() == 0)
		return true;

	// Check if any local co-op player has been initialized (spawned)
	bool anyInitialized = false;
	for (const auto &player : g_LocalCoop.players) {
		if (player.active && player.initialized) {
			anyInitialized = true;
			break;
		}
	}

	// If no local co-op players are initialized, don't restrict movement
	if (!anyInitialized)
		return true;

	// Check if this position keeps Player 1 within range of all other players
	// Pass 0 as the excludePlayerId since this is for Player 1
	return IsTilePositionOnScreen(tilePos, 0);
}

bool TryJoinLocalCoopMidGame(SDL_JoystickID controllerId)
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return false;

	// Check if we're in gameplay (not in menus)
	if (gbIsMultiplayer && !gbIsSpawn)
		return false; // Don't allow mid-game join in network multiplayer

	// Check if this controller is already assigned
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].controllerId == controllerId) {
			return false; // Already assigned
		}
	}

	// Find an empty slot
	int emptySlot = -1;
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (!g_LocalCoop.players[i].active) {
			emptySlot = static_cast<int>(i);
			break;
		}
	}

	if (emptySlot < 0)
		return false; // No empty slots

	// Calculate the player ID (slot 0 = player 2, etc.)
	size_t playerId = static_cast<size_t>(emptySlot) + 1;

	// Make sure we have enough player slots
	if (Players.size() <= playerId) {
		size_t oldSize = Players.size();
		Players.resize(playerId + 1);
		// Initialize new player slots to inactive
		for (size_t i = oldSize; i < Players.size(); ++i) {
			Players[i].plractive = false;
		}
		// IMPORTANT: Resizing the vector may reallocate, invalidating MyPlayer pointer
		if (MyPlayer != nullptr && MyPlayerId < Players.size()) {
			MyPlayer = &Players[MyPlayerId];
			InspectPlayer = MyPlayer;
		}
	}

	// Initialize the local co-op player slot
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[emptySlot];
	coopPlayer.Reset();
	coopPlayer.active = true;
	coopPlayer.controllerId = controllerId;
	coopPlayer.characterSelectActive = true; // Start character selection

	Log("Local co-op: Player {} joined mid-game", playerId + 1);

	// Load available heroes for selection
	LoadAvailableHeroesForLocalPlayer(emptySlot);

	return true;
}

namespace {

/**
 * @brief Check if a monster can be targeted by a local coop player.
 */
bool CanLocalCoopTargetMonster(const Monster &monster)
{
	if ((monster.flags & MFLAG_HIDDEN) != 0)
		return false;
	if (monster.isPlayerMinion())
		return false;
	if (monster.hitPoints >> 6 <= 0) // dead
		return false;
	if (!IsTileLit(monster.position.tile)) // not visible
		return false;

	const int mx = monster.position.tile.x;
	const int my = monster.position.tile.y;
	if (dMonster[mx][my] == 0)
		return false;

	return true;
}

/**
 * @brief Get walking distance from player to position.
 */
int GetLocalCoopDistance(const Player &player, Point position, int maxDistance)
{
	int minDist = player.position.future.WalkingDistance(position);
	if (minDist > maxDistance)
		return 0;
	return minDist; // Simplified - just use walking distance
}

/**
 * @brief Get rotary distance (number of turns to face) for a player.
 */
int GetLocalCoopRotaryDistance(const Player &player, Point destination)
{
	if (player.position.future == destination)
		return -1;

	const int d1 = static_cast<int>(player._pdir);
	const int d2 = static_cast<int>(GetDirection(player.position.future, destination));

	const int d = std::abs(d1 - d2);
	if (d > 4)
		return 4 - (d % 4);

	return d;
}

/**
 * @brief Find actors (monsters/towners/players) for a local coop player.
 */
void FindLocalCoopActors(int localIndex)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	
	if (playerId >= Players.size())
		return;
		
	const Player &player = Players[playerId];
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Check for towners in town
	if (leveltype == DTYPE_TOWN) {
		for (size_t i = 0; i < GetNumTowners(); i++) {
			const int distance = GetLocalCoopDistance(player, Towners[i].position, 2);
			if (distance == 0)
				continue;
			if (!IsTownerPresent(Towners[i]._ttype))
				continue;
			cursor.pcursmonst = static_cast<int>(i);
		}
		return;
	}
	
	// Check for monsters
	int rotations = 0;
	bool canTalk = false;
	
	// Use ranged check if player has ranged weapon
	if (player.UsesRangedWeapon()) {
		int distance = 0;
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			const int mi = ActiveMonsters[i];
			const Monster &monster = Monsters[mi];
			
			if (!CanLocalCoopTargetMonster(monster))
				continue;
				
			const bool newCanTalk = CanTalkToMonst(monster);
			if (cursor.pcursmonst != -1 && !canTalk && newCanTalk)
				continue;
			const int newDistance = player.position.future.ExactDistance(monster.position.future);
			const int newRotations = GetLocalCoopRotaryDistance(player, monster.position.future);
			if (cursor.pcursmonst != -1 && canTalk == newCanTalk) {
				if (distance < newDistance)
					continue;
				if (distance == newDistance && rotations < newRotations)
					continue;
			}
			distance = newDistance;
			rotations = newRotations;
			canTalk = newCanTalk;
			cursor.pcursmonst = mi;
		}
	} else {
		// Melee targeting - find closest monster
		int maxSteps = 25;
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			const int mi = ActiveMonsters[i];
			const Monster &monster = Monsters[mi];
			
			if (!CanLocalCoopTargetMonster(monster))
				continue;
				
			const int distance = GetLocalCoopDistance(player, monster.position.tile, maxSteps);
			if (distance == 0)
				continue;
				
			const bool newCanTalk = CanTalkToMonst(monster);
			if (cursor.pcursmonst != -1 && !canTalk && newCanTalk)
				continue;
			const int newRotations = GetLocalCoopRotaryDistance(player, monster.position.tile);
			if (cursor.pcursmonst != -1 && canTalk == newCanTalk && rotations < newRotations)
				continue;
			rotations = newRotations;
			canTalk = newCanTalk;
			cursor.pcursmonst = mi;
			if (!canTalk)
				maxSteps = distance;
		}
	}
}

/**
 * @brief Find items and objects for a local coop player.
 */
void FindLocalCoopItemsAndObjects(int localIndex)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	
	if (playerId >= Players.size())
		return;
		
	const Player &player = Players[playerId];
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	const Point futurePosition = player.position.future;
	
	int rotations = 5;
	
	// Search 3x3 area around player
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			Point targetPos = { futurePosition.x + dx, futurePosition.y + dy };
			
			// Check for items
			const int8_t itemId = dItem[targetPos.x][targetPos.y] - 1;
			if (itemId >= 0) {
				const Item &item = Items[itemId];
				if (!item.isEmpty() && item.selectionRegion != SelectionRegion::None) {
					const int newRotations = GetLocalCoopRotaryDistance(player, targetPos);
					if (newRotations < rotations) {
						rotations = newRotations;
						cursor.pcursitem = itemId;
						cursor.cursPosition = targetPos;
					}
				}
			}
			
			// Check for objects (not in town)
			if (leveltype != DTYPE_TOWN && cursor.pcursitem == -1) {
				Object *object = FindObjectAtPosition(targetPos);
				if (object != nullptr && object->canInteractWith() && !object->IsDisabled()) {
					if (targetPos != futurePosition || !object->_oDoorFlag) {
						const int newRotations = GetLocalCoopRotaryDistance(player, targetPos);
						if (newRotations < rotations) {
							rotations = newRotations;
							cursor.objectUnderCursor = object;
							cursor.cursPosition = targetPos;
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Find triggers (portals, level transitions) for a local coop player.
 */
void FindLocalCoopTriggers(int localIndex)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	
	if (playerId >= Players.size())
		return;
		
	const Player &player = Players[playerId];
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Don't find triggers if we already have a target
	if (cursor.pcursitem != -1 || cursor.objectUnderCursor != nullptr)
		return;
	
	int rotations = 0;
	int distance = 0;
	
	// Check for portal missiles
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal || missile._mitype == MissileID::RedPortal) {
			const int newDistance = GetLocalCoopDistance(player, missile.position.tile, 2);
			if (newDistance == 0)
				continue;
			if (cursor.pcursmissile != nullptr && distance < newDistance)
				continue;
			const int newRotations = GetLocalCoopRotaryDistance(player, missile.position.tile);
			if (cursor.pcursmissile != nullptr && distance == newDistance && rotations < newRotations)
				continue;
			cursor.cursPosition = missile.position.tile;
			cursor.pcursmissile = &missile;
			distance = newDistance;
			rotations = newRotations;
		}
	}
	
	// Check for level triggers
	if (cursor.pcursmissile == nullptr) {
		for (int i = 0; i < numtrigs; i++) {
			const int tx = trigs[i].position.x;
			int ty = trigs[i].position.y;
			if (trigs[i]._tlvl == 13)
				ty -= 1;
			const int newDistance = GetLocalCoopDistance(player, { tx, ty }, 2);
			if (newDistance == 0)
				continue;
			cursor.cursPosition = { tx, ty };
			cursor.pcurstrig = i;
		}
	}
}

} // namespace

void UpdateLocalCoopTargetSelection(int localIndex)
{
	if (!g_LocalCoop.enabled)
		return;
		
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
		
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	if (coopPlayer.characterSelectActive)
		return;
		
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
		
	const Player &player = Players[playerId];
	if (player._pInvincible || player._pHitPoints <= 0)
		return;
	if (DoomFlag)
		return;
		
	// If action is held and we have a valid target, keep it
	if (coopPlayer.actionHeld != GameActionType_NONE) {
		// Invalidate targets that are no longer valid
		if (coopPlayer.cursor.pcursmonst != -1) {
			const Monster &monster = Monsters[coopPlayer.cursor.pcursmonst];
			if (!CanLocalCoopTargetMonster(monster)) {
				coopPlayer.cursor.pcursmonst = -1;
			}
		}
		return;
	}
	
	// Clear previous targets
	coopPlayer.cursor.Reset();
	
	// Find new targets
	FindLocalCoopActors(localIndex);
	FindLocalCoopItemsAndObjects(localIndex);
	FindLocalCoopTriggers(localIndex);
}

/**
 * @brief Open quick spell menu for a local co-op player to assign a skill slot.
 */
void OpenLocalCoopQuickSpellMenu(int localIndex, int slotIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
	
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
	
	// Set spell menu owner - this player will own the menu until it closes
	// We switch MyPlayer/InspectPlayer to this player and keep it that way
	g_LocalCoop.spellMenuOwner = localIndex;
	
	// Switch to this player's context for the spell menu
	MyPlayer = &Players[playerId];
	MyPlayerId = playerId;
	InspectPlayer = &Players[playerId];
	
	// Open the quick spell menu
	DoSpeedBook();
	
	// Mark that we opened the menu via hold
	coopPlayer.skillMenuOpenedByHold = true;
}

void UpdateLocalCoopSkillButtons()
{
	if (!g_LocalCoop.enabled)
		return;
	
	uint32_t now = SDL_GetTicks();
	
	// Check if spell menu was closed while a local coop player owned it
	// If so, restore MyPlayer/InspectPlayer back to Player 1
	if (g_LocalCoop.spellMenuOwner >= 0 && !SpellSelectFlag) {
		// Menu was closed, restore context
		MyPlayer = &Players[0];
		MyPlayerId = 0;
		InspectPlayer = &Players[0];
		
		// Clear ownership
		int ownerIndex = g_LocalCoop.spellMenuOwner;
		g_LocalCoop.spellMenuOwner = -1;
		
		// Clear the hold state for the owner
		if (ownerIndex >= 0 && ownerIndex < static_cast<int>(g_LocalCoop.players.size())) {
			g_LocalCoop.players[ownerIndex].skillMenuOpenedByHold = false;
		}
	}
	
	// Check player 1's skill button hold
	if (g_LocalCoop.player1SkillButtonHeld >= 0 && !g_LocalCoop.player1SkillMenuOpenedByHold) {
		uint32_t holdDuration = now - g_LocalCoop.player1SkillButtonPressTime;
		if (holdDuration >= SkillButtonHoldTime) {
			// Held long enough - open quick spell menu for player 1
			DoSpeedBook();
			g_LocalCoop.player1SkillMenuOpenedByHold = true;
		}
	}
	
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		
		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;
		if (coopPlayer.characterSelectActive)
			continue;
		
		// Check if a skill button is being held
		if (coopPlayer.skillButtonHeld >= 0 && !coopPlayer.skillMenuOpenedByHold) {
			uint32_t holdDuration = now - coopPlayer.skillButtonPressTime;
			if (holdDuration >= SkillButtonHoldTime) {
				// Held long enough - open quick spell menu for this slot
				OpenLocalCoopQuickSpellMenu(static_cast<int>(i), coopPlayer.skillButtonHeld);
			}
		}
	}
}

bool HandlePlayer1SkillButtonDown(int slotIndex)
{
	if (!g_LocalCoop.enabled)
		return false;
	
	if (slotIndex < 0 || slotIndex >= NumSkillSlots)
		return false;
	
	// If quick spell menu is open and player 1 opened it, assign the spell
	if (SpellSelectFlag && g_LocalCoop.player1SkillMenuOpenedByHold) {
		AssignPlayer1SpellToSlot(slotIndex);
		return true;
	}
	
	// Start tracking hold for skill slot assignment
	g_LocalCoop.player1SkillButtonHeld = slotIndex;
	g_LocalCoop.player1SkillButtonPressTime = SDL_GetTicks();
	g_LocalCoop.player1SkillMenuOpenedByHold = false;
	
	return false; // Don't consume - let normal processing happen if not held long enough
}

bool HandlePlayer1SkillButtonUp(int slotIndex)
{
	if (!g_LocalCoop.enabled)
		return false;
	
	if (slotIndex < 0 || slotIndex >= NumSkillSlots)
		return false;
	
	// If we released a skill button that was being held
	if (g_LocalCoop.player1SkillButtonHeld == slotIndex) {
		bool wasLongPress = g_LocalCoop.player1SkillMenuOpenedByHold;
		
		// Reset hold tracking (but NOT player1SkillMenuOpenedByHold if menu is open)
		g_LocalCoop.player1SkillButtonHeld = -1;
		g_LocalCoop.player1SkillButtonPressTime = 0;
		// Keep player1SkillMenuOpenedByHold true if the spell menu is still open
		// It will be reset when the menu closes or a spell is assigned
		
		// If it was a long press that opened the menu, don't cast
		if (wasLongPress) {
			return true;
		}
		
		// Short press - cast the spell (return false to let normal processing handle it)
		return false;
	}
	
	// Tracking wasn't started for this slot (e.g., A button with target did primary action on press)
	// Return true to indicate we should NOT process spell cast on release
	return true;
}

void AssignPlayer1SpellToSlot(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= NumSkillSlots)
		return;
	
	// Use the standard SetSpeedSpell function
	SetSpeedSpell(static_cast<size_t>(slotIndex));
	
	// Close the spell menu
	SpellSelectFlag = false;
	
	// Reset hold state
	g_LocalCoop.player1SkillButtonHeld = -1;
	g_LocalCoop.player1SkillButtonPressTime = 0;
	g_LocalCoop.player1SkillMenuOpenedByHold = false;
}

void DrawPlayer1SkillSlots(const Surface &out)
{
	if (!g_LocalCoop.enabled)
		return;
	
	// Player 1 skill slots are only drawn on the corner HUD when local coop is active.
	// When no coop player has joined yet, don't draw skill slots on the main panel.
	// The skill slots will be drawn by DrawLocalCoopHUD() for all players including P1
	// once a coop player has spawned.
	// This function is essentially a no-op now - all drawing happens in the corner HUD.
	return;
}

void AssignLocalCoopSpellToSlot(int localIndex, int slotIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
	
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
	
	// MyPlayer should already be set to the spell menu owner by OpenLocalCoopQuickSpellMenu
	// Use the standard SetSpeedSpell function
	SetSpeedSpell(static_cast<size_t>(slotIndex));
	
	// Close the spell menu and restore context
	SpellSelectFlag = false;
	
	// Restore MyPlayer/InspectPlayer back to Player 1
	MyPlayer = &Players[0];
	MyPlayerId = 0;
	InspectPlayer = &Players[0];
	
	// Clear spell menu ownership
	g_LocalCoop.spellMenuOwner = -1;
	coopPlayer.skillMenuOpenedByHold = false;
}

void PerformLocalCoopPrimaryAction(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
		
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
		
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
		
	Player &player = Players[playerId];
	if (player._pHitPoints <= 0)
		return;

	// Temporarily swap player context so network commands use correct player ID
	LocalCoopPlayerContext context(playerId);
	
	// If this player owns panels and inventory is open, use the standard PerformPrimaryAction
	// which handles inventory item interaction
	if (g_LocalCoop.panelOwner == localIndex && invflag) {
		PerformPrimaryAction();
		return;
	}
		
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Talk to towner
	if (leveltype == DTYPE_TOWN && cursor.pcursmonst != -1) {
		NetSendCmdLocParam1(true, CMD_TALKXY, Towners[cursor.pcursmonst].position, cursor.pcursmonst);
		return;
	}
	
	// Attack monster
	if (cursor.pcursmonst != -1) {
		if (!player.UsesRangedWeapon() || CanTalkToMonst(Monsters[cursor.pcursmonst])) {
			NetSendCmdParam1(true, CMD_ATTACKID, cursor.pcursmonst);
		} else {
			NetSendCmdParam1(true, CMD_RATTACKID, cursor.pcursmonst);
		}
		return;
	}
	
	// Operate object
	if (cursor.objectUnderCursor != nullptr) {
		NetSendCmdLoc(playerId, true, CMD_OPOBJXY, cursor.cursPosition);
		return;
	}
	
	// Default: attack in facing direction
	Point target = player.position.future + Displacement(player._pdir);
	if (player.UsesRangedWeapon()) {
		NetSendCmdLoc(playerId, true, CMD_RATTACKXY, target);
	} else {
		NetSendCmdLoc(playerId, true, CMD_SATTACKXY, target);
	}
}

void PerformLocalCoopSecondaryAction(int localIndex)
{
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
		
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
		
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
		
	Player &player = Players[playerId];
	if (player._pHitPoints <= 0)
		return;
	
	// Swap player context so network commands are sent for the correct player
	LocalCoopPlayerContext context(playerId);
		
	LocalCoopCursorState &cursor = coopPlayer.cursor;
	
	// Pick up item
	if (cursor.pcursitem != -1) {
		NetSendCmdLocParam1(true, CMD_GOTOAGETITEM, cursor.cursPosition, cursor.pcursitem);
		return;
	}
	
	// Operate object
	if (cursor.objectUnderCursor != nullptr) {
		NetSendCmdLoc(playerId, true, CMD_OPOBJXY, cursor.cursPosition);
		return;
	}
	
	// Walk to portal/trigger
	if (cursor.pcursmissile != nullptr) {
		MakePlrPath(player, cursor.pcursmissile->position.tile, true);
		player.destAction = ACTION_WALK;
	} else if (cursor.pcurstrig != -1) {
		MakePlrPath(player, trigs[cursor.pcurstrig].position, true);
		player.destAction = ACTION_WALK;
	}
}

void ProcessLocalCoopGameAction(int localIndex, uint8_t actionType)
{
	if (!g_LocalCoop.enabled)
		return;
		
	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;
		
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	if (coopPlayer.characterSelectActive)
		return;
		
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);
	if (playerId >= Players.size())
		return;
		
	switch (actionType) {
	case GameActionType_NONE:
		break;
		
	case GameActionType_PRIMARY_ACTION:
		PerformLocalCoopPrimaryAction(localIndex);
		break;
		
	case GameActionType_SECONDARY_ACTION:
		PerformLocalCoopSecondaryAction(localIndex);
		break;
		
	case GameActionType_USE_HEALTH_POTION:
		// Use health potion from belt
		{
			LocalCoopPlayerContext context(playerId);
			for (int i = 0; i < MaxBeltItems; i++) {
				Item &item = Players[playerId].SpdList[i];
				if (item.isEmpty())
					continue;
				const bool isHealing = IsAnyOf(item._iMiscId, IMISC_HEAL, IMISC_FULLHEAL, IMISC_REJUV, IMISC_FULLREJUV);
				if (isHealing) {
					UseInvItem(INVITEM_BELT_FIRST + i);
					break;
				}
			}
		}
		break;
		
	case GameActionType_USE_MANA_POTION:
		// Use mana potion from belt
		{
			LocalCoopPlayerContext context(playerId);
			for (int i = 0; i < MaxBeltItems; i++) {
				Item &item = Players[playerId].SpdList[i];
				if (item.isEmpty())
					continue;
				const bool isMana = IsAnyOf(item._iMiscId, IMISC_MANA, IMISC_FULLMANA, IMISC_REJUV, IMISC_FULLREJUV);
				if (isMana) {
					UseInvItem(INVITEM_BELT_FIRST + i);
					break;
				}
			}
		}
		break;
		
	case GameActionType_TOGGLE_CHARACTER_INFO:
	case GameActionType_TOGGLE_INVENTORY:
	case GameActionType_TOGGLE_SPELL_BOOK:
	case GameActionType_TOGGLE_QUEST_LOG:
		// Panel toggles - try to claim ownership
		if (g_LocalCoop.TryClaimPanelOwnership(localIndex)) {
			// Switch to this player's context for all panel operations
			// This sets MyPlayer, MyPlayerId, and InspectPlayer to the coop player
			MyPlayer = &Players[playerId];
			MyPlayerId = playerId;
			InspectPlayer = &Players[playerId];
			
			ProcessGameAction(GameAction { static_cast<GameActionType>(actionType) });
			
			// Recalculate player inventory stats when opening inventory panel
			// This ensures item tooltips show correct values for the coop player
			if (actionType == GameActionType_TOGGLE_INVENTORY && invflag) {
				CalcPlrInv(Players[playerId], true);
			}
			
			// If panels were closed immediately (e.g., toggling them off), release ownership
			// UpdateLocalCoopMovement will also check this every frame
			if (!IsLeftPanelOpen() && !IsRightPanelOpen()) {
				g_LocalCoop.ReleasePanelOwnership();
			}
		}
		break;
		
	case GameActionType_TOGGLE_QUICK_SPELL_MENU:
		// Quick spell menu - no ownership needed, just switch player context
		{
			LocalCoopPlayerContext context(playerId);
			ProcessGameAction(GameAction { GameActionType_TOGGLE_QUICK_SPELL_MENU });
		}
		break;
		
	case GameActionType_CAST_SPELL:
		// Cast currently selected spell
		{
			LocalCoopPlayerContext context(playerId);
			Player &player = Players[playerId];
			if (player._pRSpell != SpellID::Invalid) {
				// Update target position for the spell
				LocalCoopCursorState &cursor = coopPlayer.cursor;
				Point spellTarget = player.position.future + Displacement(player._pdir);
				
				if (cursor.pcursmonst != -1) {
					spellTarget = Monsters[cursor.pcursmonst].position.tile;
				}
				
				// Cast spell at target
				NetSendCmdLocParam3(true, CMD_SPELLXY, spellTarget,
					static_cast<int16_t>(player._pRSpell),
					static_cast<int8_t>(player._pRSplType), playerId);
			}
		}
		break;
		
	default:
		break;
	}
}

bool HandleLocalCoopPlayer1PanelAction(uint8_t actionType)
{
	if (!g_LocalCoop.enabled)
		return false;
	
	// Only handle panel toggle actions
	if (actionType != GameActionType_TOGGLE_CHARACTER_INFO &&
	    actionType != GameActionType_TOGGLE_INVENTORY &&
	    actionType != GameActionType_TOGGLE_SPELL_BOOK &&
	    actionType != GameActionType_TOGGLE_QUEST_LOG) {
		return false;
	}
	
	// If a coop player owns panels, block Player 1 from interfering
	if (g_LocalCoop.panelOwner >= 0) {
		// A coop player owns the panels, Player 1 can't toggle
		return true; // Consume the action, do nothing
	}
	
	// Player 1 can toggle panels - let the normal handler do it
	// Return false so ProcessGameAction continues with normal processing
	return false;
}

bool AreLocalCoopPanelsOpen()
{
	return IsLeftPanelOpen() || IsRightPanelOpen();
}

Player* GetLocalCoopPanelOwnerPlayer()
{
	if (!g_LocalCoop.enabled || g_LocalCoop.panelOwner < 0)
		return nullptr;
		
	uint8_t playerId = g_LocalCoop.GetPanelOwnerPlayerId();
	if (playerId < Players.size())
		return &Players[playerId];
	return nullptr;
}

bool IsLocalCoopTargetMonster(int monsterId)
{
	if (!g_LocalCoop.enabled || monsterId < 0)
		return false;
	
	for (const LocalCoopPlayer &coopPlayer : g_LocalCoop.players) {
		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;
		if (coopPlayer.cursor.pcursmonst == monsterId)
			return true;
	}
	return false;
}

void SaveLocalCoopPlayers(bool /*writeGameData*/)
{
	if (!g_LocalCoop.enabled)
		return;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];

		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;

		if (coopPlayer.saveNumber == 0)
			continue; // No valid save number assigned

		const uint8_t playerId = LocalCoopIndexToPlayerId(static_cast<int>(i));
		if (playerId >= Players.size())
			continue;

		Player &player = Players[playerId];

		Log("Local co-op: Saving player {} (index {}) to save slot {}", playerId + 1, i, coopPlayer.saveNumber);

		// Use the pfile function to save this player to their save slot
		pfile_write_player_to_save(coopPlayer.saveNumber, player);
	}
}

void SetLocalCoopStoreOwner(int localIndex)
{
	if (!g_LocalCoop.enabled)
		return;

	if (localIndex < 0 || localIndex >= static_cast<int>(g_LocalCoop.players.size()))
		return;

	const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;

	g_LocalCoop.storeOwner = localIndex;

	Log("Local co-op: Setting store owner to player index {}", localIndex);
}

void ClearLocalCoopStoreOwner()
{
	if (!g_LocalCoop.enabled)
		return;

	if (g_LocalCoop.storeOwner >= 0) {
		Log("Local co-op: Clearing store owner (was player index {})", g_LocalCoop.storeOwner);
		
		// Reset stick state to prevent residual movement after exiting store
		if (g_LocalCoop.storeOwner < static_cast<int>(g_LocalCoop.players.size())) {
			LocalCoopPlayer &coopPlayer = g_LocalCoop.players[g_LocalCoop.storeOwner];
			coopPlayer.leftStickX = 0.0f;
			coopPlayer.leftStickY = 0.0f;
		}
	}

	g_LocalCoop.storeOwner = -1;
}

bool IsLocalCoopStoreActive()
{
	return g_LocalCoop.enabled && g_LocalCoop.storeOwner >= 0;
}

uint8_t GetLocalCoopStoreOwnerPlayerId()
{
	if (!g_LocalCoop.enabled || g_LocalCoop.storeOwner < 0)
		return 0;

	return LocalCoopIndexToPlayerId(g_LocalCoop.storeOwner);
}

} // namespace devilution

#else // USE_SDL1

namespace devilution {

LocalCoopState g_LocalCoop;
bool LocalCoopHUDOpen = false;

void InitLocalCoop() { }
void ShutdownLocalCoop() { }
bool IsLocalCoopAvailable() { return false; }
bool IsLocalCoopEnabled() { return false; }
bool IsAnyLocalCoopPlayerInitialized() { return false; }
bool IsLocalCoopPlayer(const Player & /*player*/) { return false; }
void UpdateLocalCoopMovement() { }
void UpdateLocalCoopSkillButtons() { }
bool HandlePlayer1SkillButtonDown(int /*slotIndex*/) { return false; }
bool HandlePlayer1SkillButtonUp(int /*slotIndex*/) { return false; }
void AssignPlayer1SpellToSlot(int /*slotIndex*/) { }
void DrawPlayer1SkillSlots(const Surface & /*out*/) { }
void AssignLocalCoopSpellToSlot(int /*localIndex*/, int /*slotIndex*/) { }
void LoadAvailableHeroesForLocalPlayer(int /*localIndex*/) { }
void ConfirmLocalCoopCharacter(int /*localIndex*/) { }
void DrawLocalCoopCharacterSelect(const Surface & /*out*/) { }
void DrawLocalCoopPlayerHUD(const Surface & /*out*/) { }
void HandleLocalCoopControllerDisconnect(SDL_JoystickID /*controllerId*/) { }
void HandleLocalCoopControllerConnect(SDL_JoystickID /*controllerId*/) { }
Point CalculateLocalCoopViewPosition() { return {}; }
void UpdateLocalCoopCamera() { }
Displacement GetLocalCoopCameraOffset() { return {}; }
bool IsLocalCoopPositionOnScreen(Point /*tilePos*/) { return true; }
bool TryJoinLocalCoopMidGame(SDL_JoystickID /*controllerId*/) { return false; }
void UpdateLocalCoopTargetSelection(int /*localIndex*/) { }
void ProcessLocalCoopGameAction(int /*localIndex*/, uint8_t /*actionType*/) { }
void PerformLocalCoopPrimaryAction(int /*localIndex*/) { }
void PerformLocalCoopSecondaryAction(int /*localIndex*/) { }
bool AreLocalCoopPanelsOpen() { return false; }
Player* GetLocalCoopPanelOwnerPlayer() { return nullptr; }
bool IsLocalCoopTargetMonster(int /*monsterId*/) { return false; }
void SaveLocalCoopPlayers(bool /*writeGameData*/) { }
void SetLocalCoopStoreOwner(int /*localIndex*/) { }
void ClearLocalCoopStoreOwner() { }
bool IsLocalCoopStoreActive() { return false; }
uint8_t GetLocalCoopStoreOwnerPlayerId() { return 0; }

void LocalCoopCursorState::Reset() { }
void LocalCoopPlayer::Reset() { }
AxisDirection LocalCoopPlayer::GetMoveDirection() const { return { AxisDirectionX_NONE, AxisDirectionY_NONE }; }
size_t LocalCoopState::GetActivePlayerCount() const { return 0; }
size_t LocalCoopState::GetInitializedPlayerCount() const { return 0; }
size_t LocalCoopState::GetTotalPlayerCount() const { return 1; }
bool LocalCoopState::IsAnyCharacterSelectActive() const { return false; }
uint8_t LocalCoopState::GetPanelOwnerPlayerId() const { return 0; }
bool LocalCoopState::TryClaimPanelOwnership(int /*localIndex*/) { return false; }
void LocalCoopState::ReleasePanelOwnership() { }

} // namespace devilution

#endif // USE_SDL1
