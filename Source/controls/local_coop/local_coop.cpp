/**
 * @file local_coop.cpp
 *
 * Implementation of local co-op multiplayer functionality.
 */
#include "controls/local_coop/local_coop.hpp"
#include "controls/local_coop/local_coop_assets.hpp"
#include "controls/local_coop/local_coop_constants.hpp"

#ifndef USE_SDL1

#include <algorithm>
#include <cmath>

#include "control/control.hpp"
#include "controls/control_mode.hpp"
#include "controls/controller_motion.h"
#include "controls/devices/game_controller.h"
#include "controls/game_controls.h"
#include "controls/modifier_hints.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo.h"
#include "doom.h"
#include "engine/load_clx.hpp"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/scrollrt.h"
#include "engine/render/text_render.hpp"
#include "game_mode.hpp"
#include "gamemenu.h"
#include "gmenu.h"
#include "headless_mode.hpp"
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
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "msg.h"
#include "multi.h"
#include "objects.h"
#include "options.h"
#include "pack.h"
#include "panels/charpanel.hpp"
#include "panels/quest_log.hpp"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "player.h"
#include "qol/stash.h"
#include "qol/visual_store.h"
#include "quests.h"
#include "spells.h"
#include "stores.h"
#include "tables/playerdat.hpp"
#include "towners.h"
#include "track.h"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

namespace devilution {

/**
 * @brief Reload available heroes for all other players who are still selecting.
 * Called when a player changes their character selection to update other players' lists dynamically.
 * @param excludePlayerId The player ID (0-3) of the player whose selection changed
 */
void ReloadHeroesForOtherPlayers(uint8_t excludePlayerId)
{
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (i != excludePlayerId && g_LocalCoop.players[i].active && g_LocalCoop.players[i].characterSelectActive) {
			LoadAvailableHeroesForCoopSlot(static_cast<uint8_t>(i));
		}
	}
}

// HUD drawing helpers (Phase 2: broken down from DrawLocalCoopPlayerHUD)
// Defined outside anonymous namespace so they can be used by public functions
struct PlayerHUDLayout {
	int panelWidth;
	int panelContentWidth;
	int barHeight;
	int barsHeight;
	int barsWidth;
	int nameFieldHeight;
	int playerNumWidth;
	int playerNumSpacing;
	int xpBarHeight;
	int xpBarSpacing;
	int barSpacing;
	int durabilityIconHeight;
	int durabilityIconSpacing;
};

// Asset management moved to local_coop_assets.hpp/cpp

void InitLocalCoopHUDAssets()
{
	LocalCoopAssets::Init();
}

void FreeLocalCoopHUDAssets()
{
	LocalCoopAssets::Free();
}

LocalCoopState g_LocalCoop;

ValidatedPlayer GetValidatedPlayer(uint8_t playerId, bool requireInitialized, bool requireAlive)
{
	ValidatedPlayer result = { nullptr, nullptr, false };

	if (playerId >= g_LocalCoop.players.size() || playerId >= Players.size())
		return result;

	result.coop = &g_LocalCoop.players[playerId];
	result.player = &Players[playerId];

	if (!result.coop->active)
		return result;
	if (requireInitialized && !result.coop->initialized)
		return result;
	if (requireInitialized && result.coop->characterSelectActive)
		return result;
	if (requireAlive && result.player->_pHitPoints <= 0)
		return result;

	result.isValid = true;
	return result;
}

LocalCoopPlayerContext::LocalCoopPlayerContext(uint8_t playerId)
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

LocalCoopPlayerContext::~LocalCoopPlayerContext()
{
	MyPlayer = savedMyPlayer_;
	MyPlayerId = savedMyPlayerId_;
	InspectPlayer = savedInspectPlayer_;
}

namespace {

// Forward declarations
void CastLocalCoopSpell(uint8_t playerId, SpellID spellID, SpellType spellType);
bool IsStartButtonHeldForPlayer(uint8_t playerId);
bool IsBackButtonHeldForPlayer(uint8_t playerId);
bool DoesPlayerOwnSpellMenu(uint8_t playerId);
void ReleaseLocalCoopSpellMenu();

// Button input handlers
void HandleCharacterSelectInput(uint8_t playerId, LocalCoopPlayer &coopPlayer, uint8_t button);
void HandleStoreInput(uint8_t playerId, uint8_t button);
void HandleGameplayButtonDown(uint8_t playerId, LocalCoopPlayer &coopPlayer, Player &player, uint8_t button);
void HandleGameplayButtonUp(uint8_t playerId, LocalCoopPlayer &coopPlayer, uint8_t button);

/**
 * @brief Get a valid LocalCoopPlayer pointer for the given player ID.
 * @param playerId Game player ID (0-3)
 * @return Pointer to LocalCoopPlayer if valid, nullptr otherwise
 */
LocalCoopPlayer *GetValidLocalCoopPlayer(uint8_t playerId)
{
	if (playerId >= g_LocalCoop.players.size())
		return nullptr;
	return &g_LocalCoop.players[playerId];
}

/**
 * @brief Resize the Players array and fix up MyPlayer pointer if needed.
 * This ensures MyPlayer remains valid after vector reallocation.
 * @param newSize The new size for the Players array
 */
void ResizePlayersArray(size_t newSize)
{
	if (Players.size() >= newSize)
		return;

	size_t oldSize = Players.size();
	Players.resize(newSize);
	for (size_t i = oldSize; i < newSize; ++i) {
		Players[i].plractive = false;
	}
	if (MyPlayer != nullptr && MyPlayerId < Players.size()) {
		MyPlayer = &Players[MyPlayerId];
		InspectPlayer = MyPlayer;
	}
}

/// Face direction lookup table based on axis input
const Direction FaceDir[3][3] = {
	// AxisDirectionX_NONE, AxisDirectionX_LEFT, AxisDirectionX_RIGHT
	{ Direction::South, Direction::West, Direction::East },           // AxisDirectionY_NONE
	{ Direction::North, Direction::NorthWest, Direction::NorthEast }, // AxisDirectionY_UP
	{ Direction::South, Direction::SouthWest, Direction::SouthEast }, // AxisDirectionY_DOWN
};

/**
 * @brief Panel position structure for local coop HUD.
 */
struct PanelPosition {
	int panelX;
	int panelY;
	bool isValid;
};

/**
 * @brief Calculate panel position for a player in local coop mode.
 * @param playerId Game player ID (0-3)
 * @param screenWidth Screen width in pixels
 * @param screenHeight Screen height in pixels
 * @param panelWidth Panel width in pixels
 * @return PanelPosition with coordinates and validity
 */
PanelPosition GetPlayerPanelPosition(uint8_t playerId, int screenWidth, int screenHeight, int panelWidth)
{
	using namespace LocalCoopLayout;
	PanelPosition pos = { 0, 0, false };

	constexpr int panelHeight = Panel::Height;
	constexpr int panelEdgePadding = Panel::EdgePadding;

	switch (playerId) {
	case 0:
		pos.panelX = panelEdgePadding;
		pos.panelY = screenHeight - panelHeight - panelEdgePadding;
		pos.isValid = true;
		break;
	case 1:
		pos.panelX = screenWidth - panelWidth - panelEdgePadding;
		pos.panelY = screenHeight - panelHeight - panelEdgePadding;
		pos.isValid = true;
		break;
	case 2:
		pos.panelX = panelEdgePadding;
		pos.panelY = panelEdgePadding;
		pos.isValid = true;
		break;
	case 3:
		pos.panelX = screenWidth - panelWidth - panelEdgePadding;
		pos.panelY = panelEdgePadding;
		pos.isValid = true;
		break;
	default:
		pos.isValid = false;
		break;
	}

	return pos;
}

/**
 * @brief Whether the active spell slot is on the left side of a player's panel.
 *
 * The spell slot faces the screen center: right side for P1/P3 (left screen edge),
 * left side for P2/P4 (right screen edge).
 */
bool IsSpellSlotOnLeft(uint8_t playerId)
{
	return playerId == 1 || playerId == 3;
}

/**
 * @brief Get the X position of the bars/belt section inside a player's panel.
 * @param playerId Game player ID (0-3)
 * @param panelX Panel X position (from GetPlayerPanelPosition)
 */
int GetPanelBaseSectionX(uint8_t playerId, int panelX)
{
	return IsSpellSlotOnLeft(playerId) ? panelX + LocalCoopLayout::SpellSlot::SectionWidth : panelX;
}

/**
 * @brief Belt position structure for local coop HUD.
 */
struct BeltPosition {
	int baseX;
	int baseY;
	int slotSpacing;
};

/**
 * @brief Calculate belt position for a player's panel.
 * @param panelX Panel X position
 * @param panelY Panel Y position
 * @param panelWidth Panel width in pixels (for centering belt)
 * @param barsHeight Height of the health/mana bars area (defaults to 29 for compatibility)
 * @return BeltPosition with base coordinates and slot spacing
 */
BeltPosition CalculateBeltPosition(int panelX, int panelY, int panelWidth, int barsHeight = 29)
{
	using namespace LocalCoopLayout;
	constexpr int topBorderPadding = Panel::TopBorderPadding;
	constexpr int leftBorderPadding = Panel::LeftBorderPadding;
	constexpr int rightBorderPadding = Panel::RightBorderPadding;
	constexpr int nameTopOffset = PanelContent::NameTopOffset;
	constexpr int elementSpacing = PanelContent::ElementSpacing;
	constexpr int barsExtraDownOffset = PanelContent::BarsExtraDownOffset;
	constexpr int beltBorderWidth = Belt::BorderWidth;

	const int contentX = panelX + leftBorderPadding;
	const int panelContentWidth = panelWidth - leftBorderPadding - rightBorderPadding;
	const int currentY = panelY + topBorderPadding + nameTopOffset + 4;
	const int middleY = currentY + barsHeight + elementSpacing + barsExtraDownOffset;

	BeltPosition pos;
	pos.baseX = contentX + (panelContentWidth - beltBorderWidth) / 2;
	pos.baseY = middleY - 4;
	pos.slotSpacing = Belt::SlotSpacingPx;

	return pos;
}

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
bool IsTilePositionOnScreen(Point tilePos, uint8_t excludePlayerId = LocalCoopLayout::SharedScreen::ExcludeNoPlayer)
{
	using namespace LocalCoopLayout::SharedScreen;

	const int screenWidth = static_cast<int>(GetScreenWidth());
	const int viewportHeight = static_cast<int>(GetViewportHeight());

	const int maxScreenDistanceX = screenWidth - MaxDistanceFromRightEdge;
	const int maxScreenDistanceY = (viewportHeight / 2) - MaxDistanceFromViewportHalfHeight;
	auto tileToScreen = [](Point tile) -> Point {
		using namespace LocalCoopLayout::SharedScreen;
		return {
			(tile.y - tile.x) * TileToScreenXMultiplier,
			(tile.y + tile.x) * TileToScreenYMultiplier
		};
	};

	Point newScreenPos = tileToScreen(tilePos);

	int otherPlayersChecked = 0;
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

		int distX = std::abs(newScreenPos.x - otherScreenPos.x);
		int distY = std::abs(newScreenPos.y - otherScreenPos.y);

		int64_t normalizedX = (static_cast<int64_t>(distX) * NormalizationScale) / maxScreenDistanceX;
		int64_t normalizedY = (static_cast<int64_t>(distY) * NormalizationScale) / maxScreenDistanceY;
		int64_t ellipseCheck = normalizedX * normalizedX + normalizedY * normalizedY;

		if (ellipseCheck > EllipseOutsideThresholdSq) {
			return false;
		}
	}

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

/**
 * @brief Load hero info from save files for local co-op character selection.
 *
 * pfile_ui_set_hero_infos unpacks each hero into Players[0] (InitPlayer needs a real
 * Players[] slot for getId()/vision). We snapshot Player 1 before and restore after.
 */
void LoadHeroInfosForLocalCoop(std::vector<_uiheroinfo> &heroList, const std::vector<uint32_t> &excludeSaves)
{
	heroList.clear();

	uint32_t savedSaveNumber = gSaveNumber;

	bool savedPlrActive = Players[0].plractive;
	Point savedPositionTile = Players[0].position.tile;
	Point savedPositionFuture = Players[0].position.future;
	Point savedPositionLast = Players[0].position.last;
	Point savedPositionOld = Players[0].position.old;
	Point savedPositionTemp = Players[0].position.temp;
	int savedPlrLevel = Players[0].plrlevel;
	bool savedPlrIsOnSetLevel = Players[0].plrIsOnSetLevel;
	bool savedIsWalking = Players[0].isWalking();

	uint32_t savedExperience = Players[0]._pExperience;
	uint8_t savedLevel = Players[0].getCharacterLevel();
	int savedStatPts = Players[0]._pStatPts;
	Fixed26_6 savedHitPoints = Players[0]._pHitPoints;
	Fixed26_6 savedMaxHP = Players[0]._pMaxHP;
	Fixed26_6 savedHPBase = Players[0]._pHPBase;
	Fixed26_6 savedMaxHPBase = Players[0]._pMaxHPBase;
	Fixed26_6 savedMana = Players[0]._pMana;
	Fixed26_6 savedMaxMana = Players[0]._pMaxMana;
	Fixed26_6 savedManaBase = Players[0]._pManaBase;
	Fixed26_6 savedMaxManaBase = Players[0]._pMaxManaBase;

	PLR_MODE savedPmode = Players[0]._pmode;
	Direction savedTempDirection = Players[0].tempDirection;
	action_id savedDestAction = Players[0].destAction;
	int savedDestParam1 = Players[0].destParam1;
	int savedDestParam2 = Players[0].destParam2;
	int savedDestParam3 = Players[0].destParam3;
	int savedDestParam4 = Players[0].destParam4;
	int savedLightId = Players[0].lightId;
	AnimationInfo savedAnimInfo = Players[0].AnimInfo;
	int8_t savedWalkpath[MaxPathLengthPlayer];
	for (size_t i = 0; i < MaxPathLengthPlayer; ++i) {
		savedWalkpath[i] = Players[0].walkpath[i];
	}

	pfile_ui_set_hero_infos([&heroList, &excludeSaves](_uiheroinfo *info) -> bool {
		for (uint32_t excludeSave : excludeSaves) {
			if (info->saveNumber == excludeSave)
				return true;
		}
		heroList.push_back(*info);
		return true;
	});

	if (savedSaveNumber < MAX_CHARACTERS) {
		pfile_read_player_from_save(savedSaveNumber, Players[0]);
		CalcPlrInv(Players[0], true);

		Players[0].plractive = savedPlrActive;
		Players[0].plrlevel = savedPlrLevel;
		Players[0].plrIsOnSetLevel = savedPlrIsOnSetLevel;

		Point validTile = savedPositionTile;
		if (!InDungeonBounds(validTile) || !PosOkPlayer(Players[0], validTile)) {
			validTile = Players[0].position.tile;
			if (!InDungeonBounds(validTile) || !PosOkPlayer(Players[0], validTile)) {
				if (InDungeonBounds(savedPositionTile)) {
					const WorldTileDisplacement offset[9] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 0 }, { 0, 2 }, { 1, 2 }, { 2, 1 }, { 2, 2 } };
					bool found = false;
					for (int i = 0; i < 9; i++) {
						Point testPos = savedPositionTile + offset[i];
						if (InDungeonBounds(testPos) && PosOkPlayer(Players[0], testPos)) {
							validTile = testPos;
							found = true;
							break;
						}
					}
					if (!found) {
						validTile = InDungeonBounds(ViewPosition) ? ViewPosition : Point { 0, 0 };
					}
				} else {
					validTile = InDungeonBounds(ViewPosition) ? ViewPosition : Point { 0, 0 };
				}
			}
		}

		Point validFuture = savedPositionFuture;
		bool needToStopWalking = false;
		if (!InDungeonBounds(validFuture) || (savedIsWalking && !PosOkPlayer(Players[0], validFuture))) {
			validFuture = validTile;
			needToStopWalking = true;
		}

		Point validLast = savedPositionLast;
		if (!InDungeonBounds(validLast)) {
			validLast = validTile;
		}

		Point validOld = savedPositionOld;
		if (!InDungeonBounds(validOld)) {
			validOld = validTile;
		}

		Point validTemp = savedPositionTemp;
		if (!InDungeonBounds(validTemp)) {
			validTemp = validTile;
		}

		if (Players[0].isOnActiveLevel()) {
			FixPlrWalkTags(Players[0]);
		}

		Players[0].position.tile = validTile;
		Players[0].position.future = validFuture;
		Players[0].position.last = validLast;
		Players[0].position.old = validOld;
		Players[0].position.temp = validTemp;

		if (needToStopWalking && savedIsWalking) {
			Players[0].walkpath[0] = WALK_NONE;
			Players[0].destAction = ACTION_NONE;
			if (Players[0]._pmode == PM_WALK_NORTHWARDS || Players[0]._pmode == PM_WALK_SOUTHWARDS || Players[0]._pmode == PM_WALK_SIDEWAYS) {
				Players[0]._pmode = PM_STAND;
			}
		}

		if (Players[0].isOnActiveLevel() && Players[0].plractive) {
			Players[0].occupyTile(validTile, false);
		}

		Players[0]._pExperience = savedExperience;
		Players[0].setCharacterLevel(savedLevel);
		Players[0]._pStatPts = savedStatPts;
		Players[0]._pHitPoints = savedHitPoints;
		Players[0]._pMaxHP = savedMaxHP;
		Players[0]._pHPBase = savedHPBase;
		Players[0]._pMaxHPBase = savedMaxHPBase;
		Players[0]._pMana = savedMana;
		Players[0]._pMaxMana = savedMaxMana;
		Players[0]._pManaBase = savedManaBase;
		Players[0]._pMaxManaBase = savedMaxManaBase;

		if (savedPmode <= PM_QUIT) {
			Players[0]._pmode = savedPmode;
		} else {
			Players[0]._pmode = PM_STAND;
		}
		if (savedTempDirection <= Direction::SouthWest) {
			Players[0].tempDirection = savedTempDirection;
		} else {
			Players[0].tempDirection = Direction::South;
		}
		Players[0].destAction = savedDestAction;
		Players[0].destParam1 = savedDestParam1;
		Players[0].destParam2 = savedDestParam2;
		Players[0].destParam3 = savedDestParam3;
		Players[0].destParam4 = savedDestParam4;
		if (savedLightId >= 0) {
			Players[0].lightId = savedLightId;
		}
		Players[0].AnimInfo = savedAnimInfo;
		Players[0].AnimInfo.sprites = std::nullopt;
		for (size_t i = 0; i < MaxPathLengthPlayer; ++i) {
			Players[0].walkpath[i] = savedWalkpath[i];
		}

		SetPlrAnims(Players[0]);

		if (Players[0].isOnActiveLevel()) {
			ResetPlayerGFX(Players[0]);

			if (!Players[0].hasNoLife()) {
				for (size_t i = 0; i < enum_size<player_graphic>::value; i++) {
					auto graphic = static_cast<player_graphic>(i);
					if (graphic != player_graphic::Death) {
						LoadPlrGFX(Players[0], graphic);
					}
				}
			} else {
				Players[0]._pgfxnum &= ~0xFU;
				LoadPlrGFX(Players[0], player_graphic::Death);
			}

			player_graphic currentGraphic = Players[0].getGraphic();
			if (!HeadlessMode) {
				Players[0].AnimInfo.sprites = Players[0].AnimationData[static_cast<size_t>(currentGraphic)].spritesForDirection(Players[0]._pdir);
			}
		}
	}
}

/**
 * @brief Check if there's a valid target for primary action (attack/talk/operate).
 */
bool HasLocalCoopPrimaryTarget(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return false;

	LocalCoopCursorState &cursor = validated.coop->cursor;

	if (cursor.pcursmonst != -1)
		return true;

	if (cursor.objectUnderCursor != nullptr)
		return true;

	return false;
}

/**
 * @brief Check if there's a valid target for secondary action (pickup/operate/portal).
 */
bool HasLocalCoopSecondaryTarget(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return false;

	LocalCoopCursorState &cursor = validated.coop->cursor;

	if (cursor.pcursitem != -1)
		return true;

	if (cursor.objectUnderCursor != nullptr)
		return true;

	if (cursor.pcursmissile != nullptr || cursor.pcurstrig != -1)
		return true;

	return false;
}

/**
 * @brief Map an SDL gamepad D-pad button to axis direction (spell menu / panel navigation).
 */
AxisDirection DpadGamepadButtonToAxisDirection(uint8_t button)
{
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
	return dir;
}

/**
 * @brief Process D-pad input for local co-op player movement.
 * D-pad controls movement direction, similar to left stick.
 * Also handles navigation in quick spell menu when assigning skills.
 * Also handles panel navigation (inventory, character, etc.) when this player owns the panels.
 */
void ProcessLocalCoopDpadInput(uint8_t playerId, const SDL_Event &event)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;

	if (coopPlayer->characterSelectActive)
		return;

	// If Start is held, D-pad is used for panel shortcuts (not navigation)
	// The shortcuts are handled in HandleGameplayButtonDown
	if (IsStartButtonHeldForPlayer(playerId))
		return;

	if (DoesPlayerOwnSpellMenu(playerId)) {
		if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			const auto button = SDLC_EventGamepadButton(event).button;
			AxisDirection dir = DpadGamepadButtonToAxisDirection(button);

			if (dir.x != AxisDirectionX_NONE || dir.y != AxisDirectionY_NONE) {
				HotSpellMove(dir);
			}
		}
		return;
	}

	if (g_LocalCoop.DoesPlayerOwnPanels(playerId) && (invflag || CharFlag || QuestLogIsOpen || SpellbookFlag)) {
		if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
			const auto button = SDLC_EventGamepadButton(event).button;
			AxisDirection dir = DpadGamepadButtonToAxisDirection(button);
			if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE) {
				// Not a D-pad button, let it fall through to normal processing
				return;
			}

			// Swap player context so panel navigation works for this coop player
			if (playerId < Players.size()) {
				LocalCoopPlayerContext context(playerId);
				ProcessGamePanelNavigation(dir);
				if (invflag) {
					ResetInvCursorPosition();
					pcursinvitem = CheckInvHLight();
				}
			}
		} else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
			// For button up events, check if it's a D-pad button
			// If so, return early to prevent movement D-pad state from being updated
			const auto button = SDLC_EventGamepadButton(event).button;
			if (button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
			    || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
				// D-pad button released while navigating panels - don't update movement state
				return;
			}
			// Not a D-pad button, let it fall through to normal processing
		}
		return; // Don't update movement when navigating panels
	}

	if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
		const auto button = SDLC_EventGamepadButton(event).button;
		switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			coopPlayer->dpadY = 1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			coopPlayer->dpadY = -1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			coopPlayer->dpadX = -1;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			coopPlayer->dpadX = 1;
			break;
		default:
			break;
		}
	} else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
		const auto button = SDLC_EventGamepadButton(event).button;
		switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			if (coopPlayer->dpadY > 0) coopPlayer->dpadY = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			if (coopPlayer->dpadY < 0) coopPlayer->dpadY = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			if (coopPlayer->dpadX < 0) coopPlayer->dpadX = 0;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			if (coopPlayer->dpadX > 0) coopPlayer->dpadX = 0;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Handle character selection input for a local co-op player.
 */
void HandleCharacterSelectInput(uint8_t playerId, LocalCoopPlayer &coopPlayer, uint8_t button)
{
	using namespace LocalCoopInput;
	uint32_t now = SDL_GetTicks();

	if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
		if (now - coopPlayer.lastDpadPress > DpadRepeatDelay) {
			if (!coopPlayer.availableHeroes.empty()) {
				coopPlayer.selectedHeroIndex--;
				if (coopPlayer.selectedHeroIndex < 0) {
					coopPlayer.selectedHeroIndex = static_cast<int>(coopPlayer.availableHeroes.size()) - 1;
				}
				ReloadHeroesForOtherPlayers(playerId);
			}
			coopPlayer.lastDpadPress = now;
		}
	} else if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
		if (now - coopPlayer.lastDpadPress > DpadRepeatDelay) {
			if (!coopPlayer.availableHeroes.empty()) {
				coopPlayer.selectedHeroIndex++;
				if (coopPlayer.selectedHeroIndex >= static_cast<int>(coopPlayer.availableHeroes.size())) {
					coopPlayer.selectedHeroIndex = 0;
				}
				ReloadHeroesForOtherPlayers(playerId);
			}
			coopPlayer.lastDpadPress = now;
		}
	} else if (button == SDL_GAMEPAD_BUTTON_SOUTH || button == SDL_GAMEPAD_BUTTON_START) {
		ConfirmLocalCoopCharacter(playerId);
	}
}

/**
 * @brief Handle store input for a local co-op player.
 */
void HandleStoreInput(uint8_t playerId, uint8_t button)
{
	switch (button) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
	case SDL_GAMEPAD_BUTTON_NORTH:
		StoreEnter();
		break;
	case SDL_GAMEPAD_BUTTON_EAST:
	case SDL_GAMEPAD_BUTTON_BACK:
	case SDL_GAMEPAD_BUTTON_START:
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

/**
 * @brief Check if a button is held on a local coop player's controller.
 */
bool IsButtonHeldForPlayer(uint8_t playerId, ControllerButton button)
{
	LocalCoopPlayer *coopPlayer = g_LocalCoop.GetPlayer(playerId);
	if (coopPlayer == nullptr || !coopPlayer->active)
		return false;

#ifndef USE_SDL1
	if (coopPlayer->controllerId == -1)
		return false;

	GameController *controller = GameController::Get(coopPlayer->controllerId);
	if (controller == nullptr)
		return false;

	return controller->IsPressed(button);
#else
	return false;
#endif
}

/**
 * @brief Check if Start button is held on a local coop player's controller.
 */
bool IsStartButtonHeldForPlayer(uint8_t playerId)
{
	return IsButtonHeldForPlayer(playerId, ControllerButton_BUTTON_START);
}

/**
 * @brief Check if Back/Select button is held on a local coop player's controller.
 */
bool IsBackButtonHeldForPlayer(uint8_t playerId)
{
	return IsButtonHeldForPlayer(playerId, ControllerButton_BUTTON_BACK);
}

/**
 * @brief Check if this player has the quick spell menu (speedbook) open.
 */
bool DoesPlayerOwnSpellMenu(uint8_t playerId)
{
	return SpellSelectFlag && g_LocalCoop.spellMenuOwnerPlayerId == static_cast<int>(playerId);
}

/**
 * @brief Restore Player 1's context after a local co-op player's spell menu closes.
 */
void ReleaseLocalCoopSpellMenu()
{
	if (g_LocalCoop.spellMenuOwnerPlayerId < 0)
		return;

	g_LocalCoop.spellMenuOwnerPlayerId = -1;

	// Keep panel owner context if a coop player still owns panels, otherwise restore Player 1
	const uint8_t contextPlayerId = g_LocalCoop.HasPanelOwner() ? g_LocalCoop.GetPanelOwnerPlayerId() : 0;
	MyPlayer = &Players[contextPlayerId];
	MyPlayerId = contextPlayerId;
	InspectPlayer = &Players[contextPlayerId];
}

/**
 * @brief Toggle the quick spell menu (speedbook) for a local co-op player.
 *
 * Same as Player 1's "Speedbook" action: the player picks the active spell
 * from the menu, which is then cast with the spell action button.
 */
void ToggleLocalCoopSpellMenu(uint8_t playerId)
{
	if (SpellSelectFlag) {
		// Only the player who opened the menu can close it with this button
		if (DoesPlayerOwnSpellMenu(playerId)) {
			SpellSelectFlag = false;
			ReleaseLocalCoopSpellMenu();
		}
		return;
	}

	// Don't steal the UI from another player who has panels open
	if ((IsLeftPanelOpen() || IsRightPanelOpen()) && !g_LocalCoop.DoesPlayerOwnPanels(playerId))
		return;

	// The spell menu owner keeps MyPlayer/InspectPlayer until the menu closes,
	// so the menu shows (and selects) this player's spells.
	g_LocalCoop.spellMenuOwnerPlayerId = static_cast<int>(playerId);
	MyPlayer = &Players[playerId];
	MyPlayerId = playerId;
	InspectPlayer = &Players[playerId];

	ProcessGameAction(GameAction { GameActionType_TOGGLE_QUICK_SPELL_MENU });

	if (!SpellSelectFlag)
		ReleaseLocalCoopSpellMenu();
}

/**
 * @brief Select the highlighted spell in the speedbook as the player's active spell.
 */
void SelectLocalCoopSpellFromMenu()
{
	// MyPlayer is already the spell menu owner
	SetSpell();
	ReleaseLocalCoopSpellMenu();
}

/**
 * @brief Close the panels owned by this player, one at a time, like Player 1's cancel action.
 * @return true if a panel was closed
 */
bool CloseLocalCoopPanel(uint8_t playerId)
{
	if (!g_LocalCoop.DoesPlayerOwnPanels(playerId))
		return false;

	if (invflag) {
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_INVENTORY);
		return true;
	}
	if (SpellbookFlag) {
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_SPELL_BOOK);
		return true;
	}
	if (QuestLogIsOpen) {
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_QUEST_LOG);
		return true;
	}
	if (CharFlag) {
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_CHARACTER_INFO);
		return true;
	}
	return false;
}

/**
 * @brief Check if this player owns open panels (inventory, character, quest log or spellbook).
 */
bool DoesPlayerHaveOpenPanels(uint8_t playerId)
{
	return g_LocalCoop.DoesPlayerOwnPanels(playerId) && (invflag || CharFlag || QuestLogIsOpen || SpellbookFlag);
}

/**
 * @brief Handle the Start button modifier (gamepad menu navigator), same as Player 1.
 * @return true if the button was consumed
 */
bool HandleMenuNavigatorButton(uint8_t playerId, uint8_t button)
{
	switch (button) {
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		PressEscKey();
		LastPlayerAction = PlayerActionType::None;
		PadHotspellMenuActive = false;
		gamemenu_on();
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		CycleAutomapType();
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_CHARACTER_INFO);
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_INVENTORY);
		return true;
	case SDL_GAMEPAD_BUTTON_SOUTH:
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_SPELL_BOOK);
		return true;
	case SDL_GAMEPAD_BUTTON_WEST:
		ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_QUEST_LOG);
		return true;
	case SDL_GAMEPAD_BUTTON_EAST:
		return true;
	case SDL_GAMEPAD_BUTTON_NORTH:
#ifdef __3DS__
		GetOptions().Graphics.zoom.SetValue(!*GetOptions().Graphics.zoom);
		CalcViewportGeometry();
#endif
		return true;
	case SDL_GAMEPAD_BUTTON_BACK:
		// Start + Back: toggle game menu
		{
			const bool inMenu = gmenu_is_active();
			PressEscKey();
			LastPlayerAction = PlayerActionType::None;
			PadHotspellMenuActive = false;
			if (!inMenu)
				gamemenu_on();
		}
		return true;
	default:
		return false;
	}
}

/**
 * @brief Handle gameplay button press (button down events).
 *
 * Uses the same default mapping as Player 1's gamepad:
 * - B (East): Primary action (attack, talk, lift/place inventory items, select speedbook spell)
 * - Y (North): Secondary action (pick up items, open chests/doors, use inventory items)
 * - X (West): Cast the active spell (select speedbook spell, drop/move inventory items)
 * - A (South): Open speedbook to choose the active spell, or close open menus
 * - LB / RB: Use health / mana potion
 * - LT / RT: Character / Inventory (Back + LT / RT: Quest log / Spellbook)
 * - Start + D-pad / face buttons: Menu navigator, Start + Back: Game menu
 * - Left stick click: Toggle automap
 */
void HandleGameplayButtonDown(uint8_t playerId, LocalCoopPlayer &coopPlayer, Player &player, uint8_t button)
{
	if (IsStartButtonHeldForPlayer(playerId) && HandleMenuNavigatorButton(playerId, button))
		return;

	// Back is a modifier (Back + LT / RT); Back + Start opens the game menu
	if (button == SDL_GAMEPAD_BUTTON_START && IsBackButtonHeldForPlayer(playerId)) {
		HandleMenuNavigatorButton(playerId, SDL_GAMEPAD_BUTTON_BACK);
		return;
	}

	const bool ownsSpellMenu = DoesPlayerOwnSpellMenu(playerId);
	const bool hasOpenPanels = DoesPlayerHaveOpenPanels(playerId);

	switch (button) {
	case SDL_GAMEPAD_BUTTON_EAST: // B: Primary action
		if (ownsSpellMenu) {
			SelectLocalCoopSpellFromMenu();
			return;
		}
		if (hasOpenPanels && QuestLogIsOpen) {
			ProcessLocalCoopGameAction(playerId, GameActionType_TOGGLE_QUEST_LOG);
			return;
		}
		if (hasOpenPanels && (invflag || CharFlag)) {
			// Lift/place inventory items or press character panel buttons
			coopPlayer.actionHeld = GameActionType_PRIMARY_ACTION;
			ProcessLocalCoopGameAction(playerId, GameActionType_PRIMARY_ACTION);
		} else if (HasLocalCoopPrimaryTarget(playerId) && player.CanChangeAction() && player.destAction == ACTION_NONE) {
			coopPlayer.actionHeld = GameActionType_PRIMARY_ACTION;
			ProcessLocalCoopGameAction(playerId, GameActionType_PRIMARY_ACTION);
		}
		break;

	case SDL_GAMEPAD_BUTTON_NORTH: // Y: Secondary action
		if (ownsSpellMenu)
			return;
		if (hasOpenPanels && QuestLogIsOpen) {
			LocalCoopPlayerContext context(playerId);
			QuestlogEnter();
			return;
		}
		if (hasOpenPanels && invflag) {
			// Use/equip the highlighted inventory item, same as Player 1
			LocalCoopPlayerContext context(playerId);
			PerformSecondaryAction();
		} else if (HasLocalCoopSecondaryTarget(playerId) && player.CanChangeAction() && player.destAction == ACTION_NONE) {
			coopPlayer.actionHeld = GameActionType_SECONDARY_ACTION;
			ProcessLocalCoopGameAction(playerId, GameActionType_SECONDARY_ACTION);
		}
		break;

	case SDL_GAMEPAD_BUTTON_WEST: // X: Spell action
		if (ownsSpellMenu) {
			SelectLocalCoopSpellFromMenu();
			return;
		}
		if (hasOpenPanels && QuestLogIsOpen)
			return;
		if (hasOpenPanels && invflag) {
			// Drop held item / move highlighted item, same as Player 1
			LocalCoopPlayerContext context(playerId);
			PerformSpellAction();
			return;
		}
		coopPlayer.actionHeld = GameActionType_CAST_SPELL;
		ProcessLocalCoopGameAction(playerId, GameActionType_CAST_SPELL);
		break;

	case SDL_GAMEPAD_BUTTON_SOUTH: // A: Speedbook / cancel
		if (DoomFlag) {
			doom_close();
			return;
		}
		if (hasOpenPanels && QuestLogIsOpen) {
			LocalCoopPlayerContext context(playerId);
			QuestlogEnter();
			return;
		}
		if (ownsSpellMenu || !CloseLocalCoopPanel(playerId))
			ToggleLocalCoopSpellMenu(playerId);
		break;

	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		if (!ownsSpellMenu)
			ProcessLocalCoopGameAction(playerId, GameActionType_USE_HEALTH_POTION);
		break;

	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		if (!ownsSpellMenu)
			ProcessLocalCoopGameAction(playerId, GameActionType_USE_MANA_POTION);
		break;

	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		DoAutoMap();
		break;

	default:
		break;
	}
}

/**
 * @brief Handle gameplay button release (button up events).
 */
void HandleGameplayButtonUp(uint8_t playerId, LocalCoopPlayer &coopPlayer, uint8_t button)
{
	switch (button) {
	case SDL_GAMEPAD_BUTTON_EAST:
		if (coopPlayer.actionHeld == GameActionType_PRIMARY_ACTION) {
			if (g_LocalCoop.DoesPlayerOwnPanels(playerId) && CharFlag && CharPanelButtonActive) {
				LocalCoopPlayerContext context(playerId);
				if (pChrButtons.has_value()) {
					ReleaseChrBtns(false);
				}
			}
			coopPlayer.actionHeld = GameActionType_NONE;
		}
		break;
	case SDL_GAMEPAD_BUTTON_NORTH:
		if (coopPlayer.actionHeld == GameActionType_SECONDARY_ACTION)
			coopPlayer.actionHeld = GameActionType_NONE;
		break;
	case SDL_GAMEPAD_BUTTON_WEST:
		if (coopPlayer.actionHeld == GameActionType_CAST_SPELL)
			coopPlayer.actionHeld = GameActionType_NONE;
		break;
	default:
		break;
	}
}

/**
 * @brief Process button input for a local co-op player.
 *
 * Players 2-4 use the same default gamepad mapping as Player 1
 * (see HandleGameplayButtonDown). D-pad and left stick move the player.
 */
void ProcessLocalCoopButtonInput(uint8_t playerId, const SDL_Event &event)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer &coopPlayer = *validated.coop;

	const bool isButtonDown = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
	const bool isButtonUp = (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP);

	if (!isButtonDown && !isButtonUp)
		return;

	const auto button = SDLC_EventGamepadButton(event).button;

	// Handle character selection
	if (coopPlayer.characterSelectActive) {
		if (isButtonDown) {
			HandleCharacterSelectInput(playerId, coopPlayer, button);
		}
		return;
	}

	// Check if player is initialized and alive
	if (!validated.coop->initialized || validated.player->_pHitPoints <= 0)
		return;

	Player &player = *validated.player;

	// Handle store input
	if (g_LocalCoop.storeOwnerPlayerId == playerId && IsPlayerInStore()) {
		if (isButtonDown) {
			HandleStoreInput(playerId, button);
		}
		return;
	}

	if (isButtonDown) {
		// Handle quick text dismissal (any face button or Back/Start, like Player 1's menu keys)
		if (qtextflag && IsAnyOf(button, SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START)) {
			qtextflag = false;
			if (leveltype == DTYPE_TOWN)
				stream_stop();
			if (!IsPlayerInStore() && g_LocalCoop.storeOwnerPlayerId == playerId) {
				ClearLocalCoopStoreOwner();
			}
			return;
		}

		HandleGameplayButtonDown(playerId, coopPlayer, player, button);
	} else if (isButtonUp) {
		HandleGameplayButtonUp(playerId, coopPlayer, button);
	}
}

/**
 * @brief Process axis motion for a local co-op player.
 * Handles left stick for movement and triggers for inventory/character screen.
 */
void ProcessLocalCoopAxisMotion(uint8_t playerId, const SDL_Event &event)
{
	if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION)
		return;

	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	const auto &axis = SDLC_EventGamepadAxis(event);

	// Trigger threshold (same as player 1 - values are in range -32767 to 32767)
	using namespace LocalCoopInput;

	switch (axis.axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		coopPlayer->leftStickXUnscaled = static_cast<float>(axis.value);
		// Apply deadzone scaling
		coopPlayer->leftStickX = coopPlayer->leftStickXUnscaled;
		coopPlayer->leftStickY = coopPlayer->leftStickYUnscaled;
		ScaleJoystickAxes(&coopPlayer->leftStickX, &coopPlayer->leftStickY, JoystickDeadzone);
		break;
	case SDL_GAMEPAD_AXIS_LEFTY:
		coopPlayer->leftStickYUnscaled = static_cast<float>(-axis.value); // Invert Y axis
		// Apply deadzone scaling
		coopPlayer->leftStickX = coopPlayer->leftStickXUnscaled;
		coopPlayer->leftStickY = coopPlayer->leftStickYUnscaled;
		ScaleJoystickAxes(&coopPlayer->leftStickX, &coopPlayer->leftStickY, JoystickDeadzone);
		break;
	case SDL_GAMEPAD_AXIS_RIGHTX:
		coopPlayer->rightStickXUnscaled = static_cast<float>(axis.value);
		// Apply deadzone scaling
		coopPlayer->rightStickX = coopPlayer->rightStickXUnscaled;
		coopPlayer->rightStickY = coopPlayer->rightStickYUnscaled;
		ScaleJoystickAxes(&coopPlayer->rightStickX, &coopPlayer->rightStickY, JoystickDeadzone);
		break;
	case SDL_GAMEPAD_AXIS_RIGHTY:
		coopPlayer->rightStickYUnscaled = static_cast<float>(-axis.value); // Invert Y axis
		// Apply deadzone scaling
		coopPlayer->rightStickX = coopPlayer->rightStickXUnscaled;
		coopPlayer->rightStickY = coopPlayer->rightStickYUnscaled;
		ScaleJoystickAxes(&coopPlayer->rightStickX, &coopPlayer->rightStickY, JoystickDeadzone);
		break;
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: {
		// Left trigger = Character screen (like player 1)
		bool nowPressed = axis.value > TriggerThreshold;
		if (nowPressed && !coopPlayer->leftTriggerPressed && !IsStartButtonHeldForPlayer(playerId)) {
			// Back + Left trigger = Quest log (like player 1)
			ProcessLocalCoopGameAction(playerId, IsBackButtonHeldForPlayer(playerId) ? GameActionType_TOGGLE_QUEST_LOG : GameActionType_TOGGLE_CHARACTER_INFO);
		}
		coopPlayer->leftTriggerPressed = nowPressed;
		return;
	}
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
		// Right trigger = Inventory (like player 1)
		bool nowPressed = axis.value > TriggerThreshold;
		if (nowPressed && !coopPlayer->rightTriggerPressed && !IsStartButtonHeldForPlayer(playerId)) {
			// Back + Right trigger = Spellbook (like player 1)
			ProcessLocalCoopGameAction(playerId, IsBackButtonHeldForPlayer(playerId) ? GameActionType_TOGGLE_SPELL_BOOK : GameActionType_TOGGLE_INVENTORY);
		}
		coopPlayer->rightTriggerPressed = nowPressed;
		return;
	}
	default:
		return;
	}
}

/**
 * @brief Update movement for a single local co-op player.
 *
 * Sends walk commands through the network layer just like player 1.
 *
 * Supports both left analog stick and D-pad for movement.
 */
void UpdateLocalCoopPlayerMovement(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, true, true);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	Player &player = *validated.player;

	// Don't move when this player owns a panel, store or the speedbook
	if (g_LocalCoop.DoesPlayerOwnPanels(playerId) || g_LocalCoop.storeOwnerPlayerId == static_cast<int>(playerId) || DoesPlayerOwnSpellMenu(playerId))
		return;

	// Skip if player is not on the same level as player 1
	if (player.plrlevel != Players[0].plrlevel)
		return;

	AxisDirection dir = coopPlayer->GetMoveDirection();

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

// Forward declaration
void ReloadHeroesForOtherPlayers(uint8_t excludePlayerId);

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
	rightStickX = 0;
	rightStickY = 0;
	rightStickXUnscaled = 0;
	rightStickYUnscaled = 0;
	characterSelectActive = true;
	availableHeroes.clear();
	selectedHeroIndex = 0;
	lastDpadPress = 0;
	dpadX = 0;
	dpadY = 0;
	cursor.Reset();
	actionHeld = 0;
	g_LocalCoop.UpdateAnyCoopPlayerInitializedCache();
	saveNumber = 0;
	leftTriggerPressed = false;
	rightTriggerPressed = false;
}

AxisDirection LocalCoopPlayer::GetMoveDirection() const
{
	AxisDirection dir;
	dir.x = AxisDirectionX_NONE;
	dir.y = AxisDirectionY_NONE;

	using namespace LocalCoopInput;
	const float threshold = MovementStickThreshold;

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
	// Count active players excluding player 1 (for backward compatibility)
	size_t count = 0;
	for (size_t i = 1; i < players.size(); ++i) {
		if (players[i].active)
			++count;
	}
	return count;
}

size_t LocalCoopState::GetInitializedPlayerCount() const
{
	// Count initialized players excluding player 1 (for backward compatibility)
	size_t count = 0;
	for (size_t i = 1; i < players.size(); ++i) {
		if (players[i].active && players[i].initialized)
			++count;
	}
	return count;
}

void LocalCoopState::UpdateAnyCoopPlayerInitializedCache()
{
	anyCoopPlayerInitializedCache = false;
	for (size_t i = 1; i < players.size(); ++i) {
		if (players[i].active && players[i].initialized) {
			anyCoopPlayerInitializedCache = true;
			break;
		}
	}
}

size_t LocalCoopState::GetTotalPlayerCount() const
{
	// Count all active players including player 1
	size_t count = 0;
	for (const auto &player : players) {
		if (player.active)
			++count;
	}
	return count;
}

bool LocalCoopState::IsAnyCharacterSelectActive() const
{
	// Player 1 (index 0) never has character select active
	for (size_t i = 1; i < players.size(); ++i) {
		if (players[i].active && players[i].characterSelectActive)
			return true;
	}
	return false;
}

uint8_t LocalCoopState::GetPanelOwnerPlayerId() const
{
	if (panelOwnerPlayerId < 0)
		return 0; // Player 1
	return static_cast<uint8_t>(panelOwnerPlayerId);
}

bool LocalCoopState::DoesPlayerOwnPanels(uint8_t playerId) const
{
	// If no explicit owner, Player 1 owns the panels by default
	if (panelOwnerPlayerId < 0)
		return playerId == 0;
	return panelOwnerPlayerId == static_cast<int>(playerId);
}

bool LocalCoopState::TryClaimPanelOwnership(uint8_t playerId)
{
	// Check if panels are currently open
	bool panelsOpen = IsLeftPanelOpen() || IsRightPanelOpen();

	// If no panels are open, anyone can claim
	if (!panelsOpen) {
		panelOwnerPlayerId = static_cast<int>(playerId);
		return true;
	}
	// If this player already owns the panels, allow
	if (panelOwnerPlayerId == static_cast<int>(playerId))
		return true;
	// If panels are open but no explicit owner, Player 1 owns them
	if (panelOwnerPlayerId < 0 && playerId == 0)
		return true;
	// Panels are owned by another player
	return false;
}

void LocalCoopState::ReleasePanelOwnership()
{
	// Only release if no panels are open
	if (!IsLeftPanelOpen() && !IsRightPanelOpen()) {
		panelOwnerPlayerId = -1;
		// Restore MyPlayer and InspectPlayer to Player 1
		if (Players.size() > 0) {
			MyPlayer = &Players[0];
			MyPlayerId = 0;
			InspectPlayer = &Players[0];
		}
	}
}

/**
 * @brief Restore panel owner's context before drawing panels.
 * This ensures that when panels are drawn, they show the correct player's data.
 * Should be called before drawing inventory/character panels.
 */
void RestorePanelOwnerContext()
{
	if (!g_LocalCoop.enabled)
		return;

	// If a player owns the panels, restore their context
	// panelOwnerPlayerId can be 0 (Player 1), 1-3 (Players 2-4), or -1 (no owner)
	if (g_LocalCoop.panelOwnerPlayerId >= 0 && g_LocalCoop.panelOwnerPlayerId < static_cast<int>(Players.size())) {
		uint8_t ownerId = static_cast<uint8_t>(g_LocalCoop.panelOwnerPlayerId);
		MyPlayer = &Players[ownerId];
		MyPlayerId = ownerId;
		InspectPlayer = &Players[ownerId];
	} else if (g_LocalCoop.panelOwnerPlayerId < 0) {
		// No explicit owner, ensure Player 1's context is set
		if (Players.size() > 0) {
			MyPlayer = &Players[0];
			MyPlayerId = 0;
			InspectPlayer = &Players[0];
		}
	}
}

LocalCoopPlayer *LocalCoopState::GetPlayer(uint8_t playerId)
{
	if (playerId >= players.size())
		return nullptr;
	return &players[playerId];
}

const LocalCoopPlayer *LocalCoopState::GetPlayer(uint8_t playerId) const
{
	if (playerId >= players.size())
		return nullptr;
	return &players[playerId];
}

void InitLocalCoop()
{
	g_LocalCoop = {};

	const auto &controllers = GameController::All();
	if (controllers.size() < 2) {
		return;
	}

	size_t numPlayers = std::min(controllers.size(), MaxLocalPlayers);

	// Assign controllers to players
	// Player 1 (index 0) uses the normal input system (padmapper), not local coop
	// So we mark player 1 as active but don't assign a controller
	// Players 2-4 (indices 1-3) get controllers 1-3 assigned
	g_LocalCoop.players[0].active = true;
	g_LocalCoop.players[0].controllerId = -1; // No controller - uses normal input system
	g_LocalCoop.players[0].initialized = true;
	g_LocalCoop.players[0].characterSelectActive = false;

	for (size_t i = 1; i < numPlayers; ++i) {
		g_LocalCoop.players[i].active = true;
		g_LocalCoop.players[i].controllerId = controllers[i].GetInstanceId();
	}

	g_LocalCoop.enabled = true;
	g_LocalCoop.UpdateAnyCoopPlayerInitializedCache();

	// Make sure we have enough player slots
	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
	ResizePlayersArray(totalPlayers);
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
	// Also check if the option is enabled - if disabled, shut down local coop
	if (!*GetOptions().Gameplay.enableLocalCoop) {
		if (g_LocalCoop.enabled) {
			ShutdownLocalCoop();
		}
		return false;
	}
	return gbIsMultiplayer && g_LocalCoop.enabled;
}

bool IsAnyLocalCoopPlayerInitialized()
{
	if (!IsLocalCoopEnabled())
		return false;
	return g_LocalCoop.anyCoopPlayerInitializedCache;
}

size_t GetLocalCoopTotalPlayerCount()
{
	if (!IsLocalCoopEnabled())
		return 1; // Just player 1
	return g_LocalCoop.GetTotalPlayerCount();
}

bool ShouldHideMainPanelForLocalCoop()
{
	return IsLocalCoopEnabled();
}

bool IsLocalCoopTargetObject(const Object *object)
{
	if (object == nullptr || !IsLocalCoopEnabled())
		return false;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		if (coopPlayer.active && coopPlayer.initialized) {
			if (coopPlayer.cursor.objectUnderCursor == object) {
				return true;
			}
		}
	}
	return false;
}

void LoadAvailableHeroesForAllLocalCoopPlayers()
{
	if (!IsLocalCoopEnabled())
		return;

	// Iterate over local coop players (skip Player 1 at index 0)
	// Player 1 is not in character select, so we only load heroes for players 2-4
	for (size_t i = 1; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active) {
			LoadAvailableHeroesForCoopSlot(static_cast<uint8_t>(i));
		}
	}
}

void SyncLocalCoopPlayersToLevel(interface_mode fom, int lvl)
{
	if (!IsLocalCoopEnabled())
		return;

	for (size_t playerId = 0; playerId < g_LocalCoop.players.size(); ++playerId) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[playerId];
		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;

		if (playerId >= Players.size())
			continue;

		Player &coopPlayerRef = Players[playerId];

		// Sync level for local coop player
		InitLevelChange(coopPlayerRef);

		// Set the same mode and invincibility as MyPlayer for level transition
		coopPlayerRef._pmode = PM_NEWLVL;
		coopPlayerRef._pInvincible = true;

		switch (fom) {
		case WM_DIABNEXTLVL:
		case WM_DIABPREVLVL:
		case WM_DIABRTNLVL:
		case WM_DIABTOWNWARP:
			coopPlayerRef.setLevel(lvl);
			break;
		case WM_DIABSETLVL:
			coopPlayerRef.setLevel(setlvlnum);
			break;
		case WM_DIABTWARPUP:
			coopPlayerRef.setLevel(lvl);
			break;
		case WM_DIABRETOWN:
			break;
		default:
			break;
		}
	}

	// Reset camera initialization so it re-centers on all players
	// at the new level's spawn point
	g_LocalCoop.cameraInitialized = false;
}

void ResetLocalCoopCamera()
{
	g_LocalCoop.cameraInitialized = false;
}

Player *FindLocalCoopPlayerOnTrigger(int &outTriggerIndex)
{
	if (!IsLocalCoopEnabled())
		return nullptr;

	for (size_t playerId = 0; playerId < g_LocalCoop.players.size(); ++playerId) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[playerId];
		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;

		if (playerId >= Players.size())
			continue;

		Player &player = Players[playerId];
		if (player._pmode != PM_STAND)
			continue;
		if (!player.isOnActiveLevel())
			continue;

		for (int i = 0; i < numtrigs; i++) {
			if (player.position.tile == trigs[i].position) {
				outTriggerIndex = i;
				return &player;
			}
		}
	}
	return nullptr;
}

bool IsAnyLocalPlayerWalking(Direction &outDirection)
{
	if (IsAnyLocalCoopPlayerInitialized()) {
		// Check all players for walking
		size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
		for (size_t i = 0; i < totalPlayers && i < Players.size(); ++i) {
			const Player &player = Players[i];
			if (player.plractive && !player.hasNoLife() && player.isWalking()) {
				outDirection = player._pdir;
				return true;
			}
		}
		return false;
	}

	// Fallback: just check MyPlayer
	if (MyPlayer->isWalking()) {
		outDirection = MyPlayer->_pdir;
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

	// Check if this player ID corresponds to an active slot in unified array
	// We only check 'active' here, not 'initialized', because CalcPlrInv
	// may be called during character loading before initialization completes
	LocalCoopPlayer *coopPlayer = GetValidLocalCoopPlayer(playerId);
	return coopPlayer != nullptr && coopPlayer->active;
}

bool IsLocalPlayer(const Player &player)
{
	// Check if this is MyPlayer (player 1)
	if (&player == MyPlayer)
		return true;

	// Check if this is a local co-op player (player 2-4 in local co-op mode)
	return IsLocalCoopPlayer(player);
}

SDL_JoystickID GetControllerIdFromEvent(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		return SDLC_EventGamepadAxis(event).which;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		return SDLC_EventGamepadButton(event).which;
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		return event.jaxis.which;
	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		return event.jbutton.which;
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
		return event.jhat.which;
	default:
		return -1;
	}
}

int GetLocalCoopPlayerIndex(const SDL_Event &event)
{
	if (!g_LocalCoop.enabled)
		return -1;

	SDL_JoystickID eventControllerId = GetControllerIdFromEvent(event);
	if (eventControllerId == static_cast<SDL_JoystickID>(-1))
		return -1;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].controllerId == eventControllerId) {
			return static_cast<int>(i);
		}
	}

	return -1;
}

bool IsLocalCoopControllerId(SDL_JoystickID controllerId)
{
	if (!g_LocalCoop.enabled)
		return false;

	for (size_t i = 1; i < g_LocalCoop.players.size(); ++i) {
		if (g_LocalCoop.players[i].active && g_LocalCoop.players[i].controllerId == controllerId) {
			return true;
		}
	}

	return false;
}

bool ProcessLocalCoopInput(const SDL_Event &event)
{
	if (!g_LocalCoop.enabled)
		return false;

	int playerId = GetLocalCoopPlayerIndex(event);
	if (playerId < 0)
		return false;

	// Player 1 (playerId 0) uses the normal input system, not local co-op
	// Only process input for local co-op players (players 2-4, playerId 1-3)
	if (playerId == 0)
		return false;

	// When the game menu is open, translate local coop controller input to keyboard keys
	// for menu navigation, same as player 1
	if (gmenu_is_active()) {
		// Only handle button events for menu navigation
		if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
			const auto button = SDLC_EventGamepadButton(event).button;
			ControllerButton controllerButton = ControllerButton_NONE;

			// Convert SDL gamepad button to ControllerButton
			switch (button) {
			case SDL_GAMEPAD_BUTTON_SOUTH:
				controllerButton = ControllerButton_BUTTON_A;
				break;
			case SDL_GAMEPAD_BUTTON_EAST:
				controllerButton = ControllerButton_BUTTON_B;
				break;
			case SDL_GAMEPAD_BUTTON_WEST:
				controllerButton = ControllerButton_BUTTON_X;
				break;
			case SDL_GAMEPAD_BUTTON_NORTH:
				controllerButton = ControllerButton_BUTTON_Y;
				break;
			case SDL_GAMEPAD_BUTTON_BACK:
				controllerButton = ControllerButton_BUTTON_BACK;
				break;
			case SDL_GAMEPAD_BUTTON_START:
				controllerButton = ControllerButton_BUTTON_START;
				break;
			case SDL_GAMEPAD_BUTTON_LEFT_STICK:
				controllerButton = ControllerButton_BUTTON_LEFTSTICK;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				controllerButton = ControllerButton_BUTTON_DPAD_UP;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				controllerButton = ControllerButton_BUTTON_DPAD_DOWN;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
				controllerButton = ControllerButton_BUTTON_DPAD_LEFT;
				break;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				controllerButton = ControllerButton_BUTTON_DPAD_RIGHT;
				break;
			default:
				break;
			}

			// Translate ControllerButton to keyboard key for menu navigation
			if (controllerButton != ControllerButton_NONE) {
				SDL_Keycode key = SDLK_UNKNOWN;
				// Use same translation logic as player 1
				// For game menu, use TranslateControllerButtonToGameMenuKey logic
				// but also include D-pad for navigation (like TranslateControllerButtonToMenuKey)
				switch (TranslateTo(GamepadType, controllerButton)) {
				case ControllerButton_BUTTON_A:
				case ControllerButton_BUTTON_Y:
					key = SDLK_RETURN;
					break;
				case ControllerButton_BUTTON_B:
				case ControllerButton_BUTTON_BACK:
				case ControllerButton_BUTTON_START:
					key = SDLK_ESCAPE;
					break;
				case ControllerButton_BUTTON_LEFTSTICK:
					key = SDLK_TAB; // Map
					break;
				case ControllerButton_BUTTON_DPAD_UP:
					key = SDLK_UP;
					break;
				case ControllerButton_BUTTON_DPAD_DOWN:
					key = SDLK_DOWN;
					break;
				case ControllerButton_BUTTON_DPAD_LEFT:
					key = SDLK_LEFT;
					break;
				case ControllerButton_BUTTON_DPAD_RIGHT:
					key = SDLK_RIGHT;
					break;
				default:
					break;
				}

				// Send key to menu system (only on button down)
				if (key != SDLK_UNKNOWN && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
					if (gmenu_presskeys(key)) {
						return true;
					}
				}
			}
		}
		// Don't process movement or other actions when menu is open
		return true;
	}

	// Process all input for this player
	ProcessLocalCoopAxisMotion(static_cast<uint8_t>(playerId), event);
	ProcessLocalCoopDpadInput(static_cast<uint8_t>(playerId), event);
	ProcessLocalCoopButtonInput(static_cast<uint8_t>(playerId), event);

	return true;
}

/**
 * @brief Handle right stick cursor movement for a local coop player.
 * Similar to HandleRightStickMotion() but for a specific player's right stick.
 */
void UpdateLocalCoopPlayerRightStick(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, true, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;

	// Deadzone is already handled in ProcessLocalCoopAxisMotion
	if (coopPlayer->rightStickX == 0 && coopPlayer->rightStickY == 0) {
		return;
	}

	// Only move cursor when this player owns panels (inventory/character/etc)
	if (!g_LocalCoop.DoesPlayerOwnPanels(playerId))
		return;

	// Set player context so cursor movement affects the right player
	LocalCoopPlayerContext context(playerId);

	// Move cursor (similar to HandleRightStickMotion but using player's stick values)
	InvalidateInventorySlot();
	int x = MousePosition.x;
	int y = MousePosition.y;

	// Apply stick motion to cursor (scaled by 2 for reasonable speed)
	const int dtc = 16; // Approximate frame time in ms
	x += static_cast<int>(coopPlayer->rightStickX * dtc / 2);
	y -= static_cast<int>(coopPlayer->rightStickY * dtc / 2);

	x = std::min(std::max(x, 0), gnScreenWidth - 1);
	y = std::min(std::max(y, 0), gnScreenHeight - 1);

	// Always set MousePosition so the game uses our cursor position for drawing/checks.
	// SetCursorPos only sets MousePosition when ControlDevice != KeyboardAndMouse,
	// so we must set it explicitly for coop players (who use gamepad).
	MousePosition = { x, y };
	SetCursorPos(MousePosition);

	// Update inventory highlight when cursor moves
	if (invflag) {
		pcursinvitem = CheckInvHLight();
	}
}

void UpdateLocalCoopMovement()
{
	if (!g_LocalCoop.enabled)
		return;

	// Check if a coop player owns panels and they've been closed
	// If so, release ownership and restore Player 1's context
	if (g_LocalCoop.panelOwnerPlayerId > 0) {
		if (!IsLeftPanelOpen() && !IsRightPanelOpen()) {
			g_LocalCoop.ReleasePanelOwnership();
		}
	}

	// Update movement and target selection for all players
	// Player 1's movement is handled by the existing input system, but we still update target selection
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		// Update movement for coop players (players 2-4)
		if (i > 0) {
			UpdateLocalCoopPlayerMovement(static_cast<uint8_t>(i));
		}
		// Update target selection for all players
		UpdateLocalCoopTargetSelection(static_cast<uint8_t>(i));
	}
}

void ProcessLocalCoopRightStickCursor()
{
	if (!g_LocalCoop.enabled)
		return;

	// Update right stick cursor for coop players (2-4) who own panels
	for (size_t i = 1; i < g_LocalCoop.players.size(); ++i) {
		UpdateLocalCoopPlayerRightStick(static_cast<uint8_t>(i));
	}
}

void ProcessLocalCoopLeftStickPanelNavigation()
{
	if (!g_LocalCoop.enabled)
		return;

	// When a coop player owns panels, use their left stick for panel navigation (inventory slots, etc.)
	// Same as player 1's ProcessLeftStickOrDPadGameUI() but using each coop player's left stick
	for (size_t i = 1; i < g_LocalCoop.players.size(); ++i) {
		const uint8_t playerId = static_cast<uint8_t>(i);
		const bool ownsSpellMenu = DoesPlayerOwnSpellMenu(playerId);
		if (!ownsSpellMenu && !g_LocalCoop.DoesPlayerOwnPanels(playerId))
			continue;

		LocalCoopPlayer *coopPlayer = g_LocalCoop.GetPlayer(playerId);
		if (coopPlayer == nullptr || !coopPlayer->active)
			continue;

		AxisDirection dir = coopPlayer->GetMoveDirection();
		if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
			continue;

		if (ownsSpellMenu) {
			// Navigate the speedbook with the left stick, like player 1
			HotSpellMove(dir);
			continue;
		}

		LocalCoopPlayerContext context(playerId);
		ProcessGamePanelNavigation(dir);
		if (invflag) {
			ResetInvCursorPosition();
			pcursinvitem = CheckInvHLight();
		}
	}
}

void LoadAvailableHeroesForCoopSlot(uint8_t playerIdx)
{
	auto validated = GetValidatedPlayer(playerIdx, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;

	// Save the currently selected hero's save number to try to preserve selection
	uint32_t previouslySelectedSave = 0;
	if (!coopPlayer->availableHeroes.empty() && coopPlayer->selectedHeroIndex >= 0 && coopPlayer->selectedHeroIndex < static_cast<int>(coopPlayer->availableHeroes.size())) {
		previouslySelectedSave = coopPlayer->availableHeroes[coopPlayer->selectedHeroIndex].saveNumber;
	}

	coopPlayer->availableHeroes.clear();
	coopPlayer->selectedHeroIndex = 0;

	// Build list of save numbers to exclude
	std::vector<uint32_t> excludeSaves;

	// Exclude player 1's currently selected hero
	excludeSaves.push_back(gSaveNumber);

	// Exclude heroes already selected by other local co-op players
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		// Skip the current player
		if (i == playerIdx)
			continue;
		const LocalCoopPlayer &other = g_LocalCoop.players[i];
		if (other.active) {
			// Exclude heroes currently selected by other players (even if not confirmed yet)
			if (!other.availableHeroes.empty() && other.selectedHeroIndex >= 0 && other.selectedHeroIndex < static_cast<int>(other.availableHeroes.size())) {
				excludeSaves.push_back(other.availableHeroes[other.selectedHeroIndex].saveNumber);
			}
			// Also exclude confirmed heroes using saveNumber (for players who have already confirmed)
			// This handles the case where availableHeroes might have been cleared after confirmation
			if (!other.characterSelectActive && other.saveNumber != 0) {
				excludeSaves.push_back(other.saveNumber);
			}
		}
	}

	// Load hero info without corrupting Players array
	LoadHeroInfosForLocalCoop(coopPlayer->availableHeroes, excludeSaves);

	// Try to preserve the previously selected hero if it's still available
	if (previouslySelectedSave != 0 && !coopPlayer->availableHeroes.empty()) {
		for (size_t i = 0; i < coopPlayer->availableHeroes.size(); ++i) {
			if (coopPlayer->availableHeroes[i].saveNumber == previouslySelectedSave) {
				coopPlayer->selectedHeroIndex = static_cast<int>(i);
				break;
			}
		}
		// If we didn't find it, selectedHeroIndex remains 0 (first hero)
	}
}

void ConfirmLocalCoopCharacter(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;

	if (coopPlayer->availableHeroes.empty()) {
		return;
	}

	if (coopPlayer->selectedHeroIndex < 0 || coopPlayer->selectedHeroIndex >= static_cast<int>(coopPlayer->availableHeroes.size())) {
		return;
	}

	const _uiheroinfo &selectedHero = coopPlayer->availableHeroes[coopPlayer->selectedHeroIndex];

	ResizePlayersArray(playerId + 1);

	Player &player = Players[playerId];

	// Read hero from save file
	pfile_read_player_from_save(selectedHero.saveNumber, player);

	// Store the save number so we can save this player's progress later
	coopPlayer->saveNumber = selectedHero.saveNumber;

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

	coopPlayer->characterSelectActive = false;
	coopPlayer->initialized = true;
	g_LocalCoop.UpdateAnyCoopPlayerInitializedCache();
	if (player.isOnActiveLevel()) {
		SpawnResurrectBeam(Players[0], player);
	}

	// Reload available heroes for remaining players who haven't selected yet
	ReloadHeroesForOtherPlayers(playerId);
}

void DrawLocalCoopCharacterSelect(const Surface &out)
{
	using namespace LocalCoopLayout;
	// Only show character select in multiplayer with local coop enabled
	if (!gbIsMultiplayer || !g_LocalCoop.enabled)
		return;

	constexpr int boxWidth = CharacterSelect::BoxWidth;
	constexpr int boxHeight = CharacterSelect::BoxHeight;
	constexpr int padding = CharacterSelect::Padding;

	// Start from index 1 (Player 2) since Player 1 never has character select
	for (size_t i = 1; i < g_LocalCoop.players.size(); ++i) {
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];

		if (!coopPlayer.active)
			continue;
		if (!coopPlayer.characterSelectActive)
			continue;

		// Position each player's character selection in different corners:
		// P2 (i=1): bottom right
		// P3 (i=2): top left
		// P4 (i=3): top right
		int x, y;
		if (i == 1) {
			// Player 2: bottom right
			x = out.w() - boxWidth - padding;
			y = out.h() - boxHeight - padding;
		} else if (i == 2) {
			// Player 3: top left
			x = padding;
			y = padding;
		} else if (i == 3) {
			// Player 4: top right
			x = out.w() - boxWidth - padding;
			y = padding;
		} else {
			// Fallback (shouldn't happen, but just in case)
			x = out.w() - boxWidth - padding;
			y = padding;
		}

		// Draw title - i is unified index (1-3), display as Player 2-4
		std::string title = StrCat(_("Player "), i + 1, " - ", _("Select Hero"));
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
	}
}

namespace {

// Local coop HUD panel dimensions - moved to LocalCoopLayout namespace in local_coop_constants.hpp
using namespace LocalCoopLayout;

/**
 * @brief Draw a golden border around a rectangular area using box sprites
 * Uses the same boxleftend/boxmiddle/boxrightend sprites as charpanel
 * Uses boxleftend/boxmiddle/boxrightend sprites with bottom portion cropped from top
 * and rendered as a fake bottom border to create borders with proper golden styling
 */
void DrawGoldenBorder(const Surface &out, Rectangle rect)
{
	if (!LocalCoopAssets::GetBoxLeft() || !LocalCoopAssets::GetBoxMiddle() || !LocalCoopAssets::GetBoxRight()) {
		// Fallback to FillRect if sprites not loaded
		const uint8_t borderColor = PAL16_YELLOW + 2;
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, 1, borderColor);                        // Top
		FillRect(out, rect.position.x, rect.position.y + rect.size.height - 1, rect.size.width, 1, borderColor); // Bottom
		FillRect(out, rect.position.x, rect.position.y, 1, rect.size.height, borderColor);                       // Left
		FillRect(out, rect.position.x + rect.size.width - 1, rect.position.y, 1, rect.size.height, borderColor); // Right
		return;
	}

	const ClxSprite left = (*LocalCoopAssets::GetBoxLeft())[0];
	const ClxSprite middle = (*LocalCoopAssets::GetBoxMiddle())[0];
	const ClxSprite right = (*LocalCoopAssets::GetBoxRight())[0];

	// Sprites are 24px tall (PanelFieldHeight)
	using namespace LocalCoopLayout;
	const int spriteHeight = left.height(); // Should be PanelContent::FieldHeight
	const int width = rect.size.width;
	const int height = rect.size.height;

	// Boxes taller than two sprites (e.g. the spell slot): top half of the sprite, bottom half of the
	// sprite, and the sprite's middle row repeated in between so the side edges stay continuous.
	if (height > spriteHeight * 2) {
		const int fillX = rect.position.x + left.width();
		const int fillWidth = width - left.width() - right.width();
		const int rightX = rect.position.x + width - right.width();
		const auto drawSpriteRows = [&](int y, int rowCount, int spriteRow) {
			RenderClxSprite(out.subregion(rect.position.x, y, left.width(), rowCount), left, { 0, -spriteRow });
			if (fillWidth > 0)
				RenderClxSprite(out.subregion(fillX, y, fillWidth, rowCount), middle, { 0, -spriteRow });
			RenderClxSprite(out.subregion(rightX, y, right.width(), rowCount), right, { 0, -spriteRow });
		};

		const int topRows = spriteHeight / 2;
		const int bottomRows = spriteHeight - topRows;
		drawSpriteRows(rect.position.y, topRows, 0);
		for (int y = rect.position.y + topRows; y < rect.position.y + height - bottomRows; ++y)
			drawSpriteRows(y, 1, topRows);
		drawSpriteRows(rect.position.y + height - bottomRows, bottomRows, topRows);
		return;
	}

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
	if (!LocalCoopAssets::GetBoxLeft() || !LocalCoopAssets::GetBoxMiddle() || !LocalCoopAssets::GetBoxRight())
		return;

	const ClxSprite left = (*LocalCoopAssets::GetBoxLeft())[0];
	const ClxSprite middle = (*LocalCoopAssets::GetBoxMiddle())[0];
	const ClxSprite right = (*LocalCoopAssets::GetBoxRight())[0];

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
	const int borderSize = 3;         // Thicker golden border for better visibility
	const int bottomBorderHeight = 4; // Height of fake bottom border from bottom of sprite
	int innerX = position.x + borderSize;
	int innerY = position.y + borderSize;
	int innerWidth = width - (borderSize * 2);
	// For very thin bars (like XP bar), adjust the calculation to ensure innerHeight is at least 1
	int innerHeight = height - borderSize - bottomBorderHeight + 1;
	if (innerHeight < 1) {
		// For thin bars, use a simpler calculation: just subtract top border
		innerHeight = std::max(1, height - borderSize);
		innerY = position.y + borderSize;
	}

	// Draw black/transparent background for the bar area
	FillRect(out, innerX, innerY, innerWidth, innerHeight, 0); // Black fill

	// Draw fill bar
	if (max > 0 && current > 0) {
		int fillWidth = ((innerWidth * current) / max);
		if (fillWidth > innerWidth) fillWidth = innerWidth;

		if (fillWidth > 0) {
			// XP bar: simple white rectangle fill
			if (isXP) {
				FillRect(out, innerX, innerY, fillWidth, innerHeight, PAL16_GRAY + 15); // White fill
			} else {
				// Other bars: use colorized sprite
				// Select the appropriate colorized sprite
				const OptionalOwnedClxSpriteList *barSprite = nullptr;
				if (isMana) {
					barSprite = &LocalCoopAssets::GetBarSpriteBlue();
				} else if (hasManaShield) {
					barSprite = &LocalCoopAssets::GetBarSpriteYellow();
				} else {
					barSprite = &LocalCoopAssets::GetBarSpriteRed();
				}

				// Render tiled sprite to fill the bar area
				if (barSprite && *barSprite && (*barSprite)->numSprites() > 0) {
					ClxSprite sprite = (*barSprite)->operator[](0);
					const int spriteWidth = sprite.width();
					const int spriteHeight = sprite.height();

					// Tile the sprite horizontally and vertically to fill the fill area
					int spriteY = innerY;
					int remainingHeight = innerHeight;
					while (remainingHeight > 0) {
						int chunkHeight = std::min(remainingHeight, spriteHeight);
						int spriteX = innerX;
						int remainingWidth = fillWidth;
						while (remainingWidth > 0) {
							int chunkWidth = std::min(remainingWidth, spriteWidth);
							Surface region = out.subregion(spriteX, spriteY, chunkWidth, chunkHeight);
							RenderClxSprite(region, sprite, { 0, 0 });
							spriteX += chunkWidth;
							remainingWidth -= chunkWidth;
						}
						spriteY += chunkHeight;
						remainingHeight -= chunkHeight;
					}
				} else {
					// Fallback to solid color if sprite is not available
					uint8_t fillColor;
					if (isMana) {
						fillColor = PAL16_BLUE + 8;
					} else if (hasManaShield) {
						fillColor = PAL16_YELLOW + 8;
					} else {
						fillColor = PAL16_RED + 8;
					}
					FillRect(out, innerX, innerY, fillWidth, innerHeight, fillColor);
				}
			}
		}
	}
}

/**
 * @brief Draw the player's active spell slot (the spell cast with the spell action button).
 *
 * Players choose their active spell from the speedbook, like Player 1 on the main panel.
 * The large spell icon is drawn at its native size with a golden border, like the belt.
 *
 * @param iconPosition Top-left corner of the spell icon
 */
void DrawPlayerActiveSpellSlot(const Surface &out, const Player &player, Point iconPosition)
{
	using namespace LocalCoopLayout;

	SpellID spell = player._pRSpell;
	SpellType spellType = player._pRSplType;

	// Same availability coloring as the main panel spell button (DrawSpell)
	if (!IsValidSpell(spell)) {
		spellType = SpellType::Invalid;
		spell = SpellID::Null;
	}
	if (spellType == SpellType::Spell) {
		if (CheckSpell(player, spell, spellType, true) != SpellCheckResult::Success || player.GetSpellLevel(spell) <= 0)
			spellType = SpellType::Invalid;
	}
	if (leveltype == DTYPE_TOWN && spellType != SpellType::Invalid && !GetSpellData(spell).isAllowedInTown())
		spellType = SpellType::Invalid;

	// Border first; the icon covers its inner area
	DrawGoldenBorder(out, Rectangle {
	                          iconPosition - Displacement { SpellSlot::BorderOverhang, SpellSlot::BorderOverhang },
	                          Size { SpellSlot::IconSize + SpellSlot::BorderExtraWidth, SpellSlot::IconSize + SpellSlot::BorderExtraHeight } });

	// DrawLargeSpellIcon takes the bottom-left corner
	SetSpellTrans(spellType);
	DrawLargeSpellIcon(out, iconPosition + Displacement { 0, SpellSlot::IconSize - 1 }, spell);
}

/**
 * @brief Draw the panel background using charbg.clx (character panel background)
 * The sprite is tiled to fill the panel area with rock texture.
 */
void DrawCoopPanelBackground(const Surface &out, Rectangle rect)
{
	// Use the charbg.clx sprite for panel background
	if (LocalCoopAssets::GetCharBg()) {
		const ClxSprite bg = (*LocalCoopAssets::GetCharBg())[0];
		const int srcW = bg.width();
		const int srcH = bg.height();

		const int rightBorderWidth = 7;   // Right fake border width (2px larger)
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
 * @brief Cast a spell for a local coop player.
 *
 * Mirrors Player 1's spell action (CheckPlrSpell), using this player's own targeting state.
 *
 * @param playerId The game player ID (0 = player 1, 1-3 = coop players)
 * @param spellID The spell to cast
 * @param spellType The spell source (skill, spell, scroll, staff charges)
 */
void CastLocalCoopSpell(uint8_t playerId, SpellID spellID, SpellType spellType)
{
	auto validated = GetValidatedPlayer(playerId);
	if (!validated.isValid)
		return;

	const LocalCoopCursorState &cursor = validated.coop->cursor;
	Player &player = *validated.player;

	// Check if player is already performing an action - prevent duplicate casts
	if (!player.CanChangeAction() || player.destAction != ACTION_NONE)
		return;

	// NetSendCmd* uses MyPlayerId internally
	LocalCoopPlayerContext context(playerId);

	if (!IsValidSpell(spellID)) {
		player.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}

	if (leveltype == DTYPE_TOWN && !GetSpellData(spellID).isAllowedInTown()) {
		player.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	const Player *targetPlayer = cursor.playerUnderCursor != &player ? cursor.playerUnderCursor : nullptr;
	if ((targetPlayer == nullptr && IsAnyOf(spellID, SpellID::Resurrect, SpellID::HealOther))
	    || (cursor.objectUnderCursor == nullptr && spellID == SpellID::TrapDisarm)) {
		player.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	bool canCast = false;
	SpellCheckResult spellcheck = SpellCheckResult::Success;
	switch (spellType) {
	case SpellType::Skill:
	case SpellType::Spell:
		spellcheck = CheckSpell(player, spellID, spellType, false);
		canCast = spellcheck == SpellCheckResult::Success;
		break;
	case SpellType::Scroll:
		canCast = CanUseScroll(player, spellID);
		break;
	case SpellType::Charges:
		canCast = CanUseStaff(player, spellID);
		break;
	case SpellType::Invalid:
		return;
	}

	if (!canCast) {
		if (spellType == SpellType::Spell) {
			switch (spellcheck) {
			case SpellCheckResult::Fail_NoMana:
				player.Say(HeroSpeech::NotEnoughMana);
				break;
			case SpellCheckResult::Fail_Level0:
				player.Say(HeroSpeech::ICantCastThatYet);
				break;
			default:
				player.Say(HeroSpeech::ICantDoThat);
				break;
			}
		}
		return;
	}

	// Town Portal and Teleport never target actors (same as Player 1)
	const bool canTargetActor = spellID != SpellID::TownPortal && spellID != SpellID::Teleport;
	const int8_t spell = static_cast<int8_t>(spellID);
	const uint8_t type = static_cast<uint8_t>(spellType);
	constexpr int spellFrom = 0;

	Point target = player.position.future + Displacement(player._pdir) * (spellID == SpellID::Teleport ? 4 : 1);
	if (canTargetActor && cursor.pcursmonst != -1) {
		target = Monsters[cursor.pcursmonst].position.tile;
	} else if (canTargetActor && targetPlayer != nullptr) {
		target = targetPlayer->position.future;
	}

	if (IsWallSpell(spellID)) {
		const Direction sd = GetDirection(player.position.tile, target);
		NetSendCmdLocParam4(true, CMD_SPELLXYD, target, spell, type, static_cast<uint16_t>(sd), spellFrom);
	} else if (canTargetActor && cursor.pcursmonst != -1) {
		NetSendCmdParam4(true, CMD_SPELLID, cursor.pcursmonst, spell, type, spellFrom);
	} else if (canTargetActor && targetPlayer != nullptr && !player.friendlyMode) {
		NetSendCmdParam4(true, CMD_SPELLPID, targetPlayer->getId(), spell, type, spellFrom);
	} else {
		NetSendCmdLocParam3(true, CMD_SPELLXY, target, spell, type, spellFrom);
	}
}

[[maybe_unused]] void DrawPlayerBeltSlot(const Surface &out, const Player &player, int slotIndex, Point position)
{
	const Item &item = player.SpdList[slotIndex];

	if (item.isEmpty())
		return;

	using namespace LocalCoopLayout;
	// Draw black background for the slot
	constexpr int slotSize = Belt::SlotSize;
	FillRect(out, position.x, position.y, slotSize - 4, slotSize - 4, 0);

	// Draw item quality background (colorizes the existing background)
	InvDrawSlotBack(out, { position.x, position.y + slotSize - 4 }, { slotSize - 4, slotSize - 4 }, item._iMagical);

	// Get item sprite
	const int cursId = item._iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);

	// Draw golden outline around the item (sprite-based border)
	Point itemPos = { position.x, position.y + slotSize - 4 };
	ClxDrawOutline(out, GetOutlineColor(item, true), itemPos, sprite);

	// Draw the item (position needs to be at bottom-left of slot for ClxDraw)
	DrawItem(item, out, itemPos, sprite);
}

void DrawPlayerBelt(const Surface &out, const Player &player, uint8_t playerId, Point basePosition)
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
		const int slotX = basePosition.x + (i * 29) + 5;
		const int slotY = basePosition.y + 3;
		const Point position { slotX - 2, slotY + InventorySlotSizeInPixels.height };

		// Draw item quality background
		InvDrawSlotBack(out, position, InventorySlotSizeInPixels, player.SpdList[i]._iMagical);

		// Get item sprite
		const int cursId = player.SpdList[i]._iCurs + CURSOR_FIRSTITEM;
		const ClxSprite sprite = GetInvItemSprite(cursId);

		// Draw highlight if this belt slot is under the cursor (when inventory is open)
		// Check that the cursor is actually over THIS player's belt
		if (invflag && pcursinvitem == i + INVITEM_BELT_FIRST) {
			uint8_t cursorBeltOwner = 0;
			std::optional<inv_xy_slot> cursorBeltSlot = FindLocalCoopBeltSlotUnderCursor(MousePosition, cursorBeltOwner);
			if (cursorBeltSlot.has_value() && cursorBeltOwner == static_cast<uint8_t>(playerId)) {
				ClxDrawOutline(out, GetOutlineColor(player.SpdList[i], true, &player), position, sprite);
			}
		}

		// Draw the item
		DrawItem(player.SpdList[i], out, position, sprite);
	}
}

} // namespace

bool LocalCoopHUDOpen = true;

/**
 * @brief Calculate the local coop panel width.
 * @return Panel width in pixels
 */
int GetLocalCoopPanelWidth()
{
	using namespace LocalCoopLayout;
	// Bars/belt section plus the active spell slot
	return PanelWidth::BaseWidth + SpellSlot::SectionWidth;
}

// HUD drawing helper functions (defined outside anonymous namespace to avoid ambiguity)
// These are in the devilution namespace (which started at line 77)
PlayerHUDLayout CalculatePlayerHUDLayout(bool experienceBarEnabled, int panelWidth)
{
	using namespace LocalCoopLayout;
	PlayerHUDLayout layout;
	layout.panelWidth = panelWidth;
	layout.panelContentWidth = panelWidth - Panel::LeftBorderPadding - Panel::RightBorderPadding;

	// Health/mana bar dimensions - adjust based on experience bar
	const int barHeight = experienceBarEnabled ? HealthBar::HeightXpEnabled : HealthBar::Height;
	const int barSpacing = HealthBar::Spacing;
	const int xpBarHeight = ExperienceBar::Height;
	const int xpBarSpacing = experienceBarEnabled ? 0 : ExperienceBar::Spacing;
	layout.barsHeight = experienceBarEnabled
	    ? (barHeight * 2 + barSpacing + xpBarHeight + xpBarSpacing)
	    : (barHeight * 2 + barSpacing);

	layout.barHeight = barHeight;
	layout.barSpacing = barSpacing;
	layout.xpBarHeight = xpBarHeight;
	layout.xpBarSpacing = xpBarSpacing;

	// Name field dimensions
	layout.nameFieldHeight = PanelContent::FieldHeight;
	layout.playerNumWidth = 28;
	layout.playerNumSpacing = 0;

	// Bar width = space between P# and Level
	layout.barsWidth = layout.panelContentWidth - layout.playerNumWidth - layout.playerNumSpacing - layout.playerNumWidth + 1;

	// Durability icon dimensions
	layout.durabilityIconHeight = DurabilityIcon::Height;
	layout.durabilityIconSpacing = DurabilityIcon::Spacing;

	return layout;
}

/**
 * @brief Draw the top row of player HUD (player number, level, attribute icon).
 */
void DrawPlayerHUDTopRow(const Surface &out, const Player &player, size_t playerId, const PlayerHUDLayout &layout, int contentX, int currentY, int panelContentWidth)
{
	using namespace LocalCoopLayout;
	constexpr int nameLeftOffset = 0;
	int nameRowX = contentX + nameLeftOffset;
	constexpr int extraOffset = 1;

	// Draw player number field on the left
	DrawCoopPanelField(out, { nameRowX, currentY }, layout.playerNumWidth - extraOffset);
	char playerNumStr[4];
	snprintf(playerNumStr, sizeof(playerNumStr), "P%zu", playerId + 1);
	DrawString(out, playerNumStr,
	    { { nameRowX + 2, currentY + 2 }, { layout.playerNumWidth - 4, layout.nameFieldHeight - 4 } },
	    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorWhite, .spacing = 0 });

	// Draw level field on the right
	int levelX = nameRowX + panelContentWidth - layout.playerNumWidth - nameLeftOffset - extraOffset + 1;
	DrawCoopPanelField(out, { levelX, currentY }, layout.playerNumWidth);
	char levelStr[4];
	snprintf(levelStr, sizeof(levelStr), "%d", player.getCharacterLevel());
	DrawString(out, levelStr,
	    { { levelX + 2, currentY + 2 }, { layout.playerNumWidth - 4, layout.nameFieldHeight - 4 } },
	    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorWhite, .spacing = 0 });

	// Draw attribute points available icon overlapping the level field
	if (player._pStatPts > 0 && pChrButtons.has_value()) {
		const ClxSprite &iconSprite = (*pChrButtons)[1];
		const int iconWidth = iconSprite.width();
		const int iconHeight = iconSprite.height();
		const int squareSize = std::min(iconWidth, iconHeight);

		OwnedSurface tempSurface(iconWidth, iconHeight);
		FillRect(tempSurface, 0, 0, iconWidth, iconHeight, 0);
		RenderClxSprite(tempSurface, iconSprite, { 0, 0 });

		const int cropX = (iconWidth - squareSize) / 2 + 1;
		SDL_Rect cropRect = { cropX, 0, squareSize - 2, squareSize };
		Point iconPos = {
			levelX + layout.playerNumWidth / 2 - squareSize / 2 + 1,
			currentY + layout.nameFieldHeight / 2 - squareSize / 2 + 3
		};
		out.BlitFrom(tempSurface, cropRect, iconPos);
	}
}

/**
 * @brief Draw health, mana, and experience bars for a player.
 */
void DrawPlayerHUDBars(const Surface &out, const Player &player, const PlayerHUDLayout &layout, int barX, int barY, bool experienceBarEnabled)
{
	using namespace LocalCoopLayout;
	int currentHP = player._pHitPoints.whole();
	int maxHP = player._pMaxHP.whole();
	int currentMana = player._pMana.whole();
	int maxMana = player._pMaxMana.whole();
	bool hasManaShield = player.pManaShield;

	// Health bar
	DrawCoopSmallBar(out, { barX, barY }, layout.barsWidth, layout.barHeight, currentHP, maxHP, false, hasManaShield);
	barY += layout.barHeight + layout.barSpacing;

	// Mana bar
	DrawCoopSmallBar(out, { barX, barY - 2 }, layout.barsWidth, layout.barHeight, currentMana, maxMana, true, false);
	barY += layout.barHeight;

	// Experience bar (if enabled)
	if (experienceBarEnabled) {
		barY += layout.xpBarSpacing;
		int xpCurrent = 0;
		using namespace LocalCoopLayout;
		constexpr int xpMax = ExperienceBar::MaxValue;

		if (player.isMaxCharacterLevel()) {
			xpCurrent = xpMax;
		} else {
			const uint8_t charLevel = player.getCharacterLevel();
			const uint32_t prevXp = GetNextExperienceThresholdForLevel(charLevel - 1);
			const uint32_t nextXp = GetNextExperienceThresholdForLevel(charLevel);

			if (player._pExperience >= prevXp && nextXp > prevXp) {
				const uint32_t prevXpDelta1 = player._pExperience - prevXp;
				const uint32_t prevXpDelta = nextXp - prevXp;
				const uint32_t clampedDelta1 = std::min(prevXpDelta1, prevXpDelta);
				xpCurrent = static_cast<int>((static_cast<uint64_t>(xpMax) * clampedDelta1) / prevXpDelta);
				xpCurrent = std::clamp(xpCurrent, 0, xpMax);
			}
		}
		DrawCoopSmallBar(out, { barX, barY - 3 }, layout.barsWidth, layout.xpBarHeight, xpCurrent, xpMax, false, false, true);
	}
}

/**
 * @brief Draw the active spell slot inside a player's panel, next to the bars/belt section.
 * @param baseSectionX X position of the bars/belt section
 * @param spellOnLeft true if the spell slot is on the left side of the panel
 */
void DrawPlayerHUDSpell(const Surface &out, const Player &player, int panelX, int panelY, int baseSectionX, const PlayerHUDLayout &layout, bool spellOnLeft)
{
	using namespace LocalCoopLayout;

	int borderX;
	if (spellOnLeft) {
		// Same padding from the panel edge as the bars/belt section
		borderX = panelX + Panel::LeftBorderPadding;
	} else {
		// Right of the belt border
		const BeltPosition beltPos = CalculateBeltPosition(baseSectionX, panelY, PanelWidth::BaseWidth, layout.barsHeight);
		borderX = beltPos.baseX + Belt::BorderWidth + SpellSlot::BorderGap;
	}

	const Point iconPosition {
		borderX + SpellSlot::BorderOverhang,
		panelY + (Panel::Height - SpellSlot::IconSize) / 2 // Vertically centered
	};
	DrawPlayerActiveSpellSlot(out, player, iconPosition);
}

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

	// Check if experience bar is enabled
	const bool experienceBarEnabled = *GetOptions().Gameplay.experienceBar;

	// Calculate layout dimensions (bars/belt section; the spell slot adds to the full panel width)
	const int panelWidth = GetLocalCoopPanelWidth();
	PlayerHUDLayout layout = CalculatePlayerHUDLayout(experienceBarEnabled, PanelWidth::BaseWidth);

	// Panel layout constants
	using namespace LocalCoopLayout;
	constexpr int topBorderPadding = Panel::TopBorderPadding;
	constexpr int leftBorderPadding = Panel::LeftBorderPadding;
	constexpr int elementSpacing = PanelContent::ElementSpacing;
	constexpr int nameTopOffset = PanelContent::NameTopOffset;
	constexpr int panelHeight = Panel::Height;

	// In local coop mode (2+ gamepads), always show corner HUDs for all players
	// The main panel is hidden and replaced with corner HUDs
	// We check all player slots (up to MaxLocalPlayers) to see who should be drawn

	for (size_t playerId = 0; playerId < MaxLocalPlayers; ++playerId) {
		// Ensure Players array has this slot
		if (playerId >= Players.size())
			continue;

		const Player &player = Players[playerId];
		const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[playerId];

		// Determine if this player's HUD should be drawn
		bool shouldDrawHUD = false;

		if (playerId == 0) {
			// Player 1: Always show HUD in local coop if player is active
			shouldDrawHUD = player.plractive;
		} else {
			// Players 2-4: Show HUD only if they're active, not in character select, and initialized
			shouldDrawHUD = coopPlayer.active
			    && !coopPlayer.characterSelectActive
			    && coopPlayer.initialized
			    && player.plractive;
		}

		if (!shouldDrawHUD)
			continue;

		// Determine panel position based on player ID - with 1px edge padding
		PanelPosition panelPos = GetPlayerPanelPosition(static_cast<uint8_t>(playerId), out.w(), out.h(), panelWidth);
		if (!panelPos.isValid)
			continue;

		int panelX = panelPos.panelX;
		int panelY = panelPos.panelY;
		bool atBottom = (playerId == 0 || playerId == 1);
		const bool spellOnLeft = IsSpellSlotOnLeft(static_cast<uint8_t>(playerId));
		const int baseSectionX = GetPanelBaseSectionX(static_cast<uint8_t>(playerId), panelX);

		// Draw panel background (charbg has built-in 5px golden border)
		DrawCoopPanelBackground(out, { { panelX, panelY }, { panelWidth, panelHeight } });

		// Content area starts after the border padding + nameTopOffset
		int contentX = baseSectionX + leftBorderPadding;
		int currentY = panelY + topBorderPadding + nameTopOffset + 4; // Top row moved 3px down

		// Draw top row (player number, level, attribute icon)
		DrawPlayerHUDTopRow(out, player, playerId, layout, contentX, currentY, layout.panelContentWidth);

		// Calculate bar position
		constexpr int nameLeftOffset = 0;
		constexpr int extraOffset = 1;
		int nameRowX = contentX + nameLeftOffset;
		int barX = nameRowX + layout.playerNumWidth + layout.playerNumSpacing - extraOffset;
		int barY = currentY;

		// Draw health, mana, and experience bars
		DrawPlayerHUDBars(out, player, layout, barX, barY, experienceBarEnabled);

		// Move currentY down to account for the tallest element in the top row (bars)
		currentY += layout.barsHeight + elementSpacing;

		// Draw active spell slot inside the panel
		DrawPlayerHUDSpell(out, player, panelX, panelY, baseSectionX, layout, spellOnLeft);

		// === BELT: Centered horizontally within the bars/belt section ===
		// Use CalculateBeltPosition helper to get belt position (handles centering and Y calculation)
		BeltPosition beltPos = CalculateBeltPosition(baseSectionX, panelY, layout.panelWidth, layout.barsHeight);
		DrawPlayerBelt(out, player, static_cast<uint8_t>(playerId), { beltPos.baseX, beltPos.baseY });

		// Draw durability icons - above panel for bottom players, below panel for top players
		int durabilityY;
		if (atBottom) {
			// For bottom players, draw icons ABOVE the panel
			durabilityY = panelY - layout.durabilityIconSpacing - layout.durabilityIconHeight;
		} else {
			// For top players, draw icons BELOW the panel
			durabilityY = panelY + panelHeight + layout.durabilityIconSpacing;
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
					// Skip player 1 (index 0) - they don't need character selection
					if (i > 0) {
						LoadAvailableHeroesForCoopSlot(static_cast<uint8_t>(i));
					}
					return;
				}
			}
			// If we get here, the controller is controller 0 (player 1's controller), so we're done
			return;
		}
		// Not enough controllers yet, wait for more
		return;
	}

	const auto &controllers = GameController::All();
	if (!controllers.empty() && controllers[0].GetInstanceId() == controllerId) {
		return;
	}

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		LocalCoopPlayer &coopPlayer = g_LocalCoop.players[i];
		if (coopPlayer.active && coopPlayer.controllerId == controllerId) {
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

			size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();
			ResizePlayersArray(totalPlayers);

			LoadAvailableHeroesForCoopSlot(static_cast<uint8_t>(i));
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

	// Sum positions of all active players that have chosen a character (initialized)
	// Iterate through all possible player slots to handle gaps (e.g., player 2 not chosen yet)
	for (size_t i = 0; i < MaxLocalPlayers && i < Players.size(); ++i) {
		const Player &player = Players[i];
		// Check both that the Player is active AND that the LocalCoopPlayer state is initialized
		// This ensures we only include players who have actually chosen a character
		if (!player.plractive || player.hasNoLife())
			continue;
		if (i >= g_LocalCoop.players.size() || !g_LocalCoop.players[i].initialized)
			continue;

		totalX += player.position.tile.x;
		totalY += player.position.tile.y;
		activeCount++;
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
void SetViewPosition(Point position)
{
	if (IsAnyLocalCoopPlayerInitialized())
		return;
	ViewPosition = position;
}

void SetViewPositionForPlayer(const Player &player, Point position)
{
	if (!IsLocalPlayer(player))
		return;
	if (IsAnyLocalCoopPlayerInitialized())
		return;
	ViewPosition = position;
}

void UpdateLocalCoopCamera()
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return;

	if (!g_LocalCoop.anyCoopPlayerInitializedCache)
		return;

	// Calculate pixel-precise positions for all active players in screen space
	// Screen coordinates: screenX = (tileY - tileX) * 32, screenY = (tileY + tileX) * -16
	// We scale by 256 for precision before division
	int activeCount = 0;
	int walkingCount = 0;
	Direction firstWalkDir = Direction::South;
	bool allWalkingSameDirection = true;
	int64_t totalScreenX = 0;
	int64_t totalScreenY = 0;

	// Iterate through all possible player slots (not just GetTotalPlayerCount)
	// This ensures we include all initialized players regardless of gaps
	// e.g., if player 2 hasn't chosen yet, we still include players 1 and 3
	for (size_t i = 0; i < MaxLocalPlayers && i < Players.size(); ++i) {
		const Player &player = Players[i];
		// Check both that the Player is active AND that the LocalCoopPlayer state is initialized
		// This double-check is necessary because:
		// - player.plractive is false until character selection is confirmed
		// - g_LocalCoop.players[i].initialized tracks whether they've chosen a character
		// We need BOTH to be true to include a player in camera calculations
		if (!player.plractive || player.hasNoLife())
			continue;
		if (i >= g_LocalCoop.players.size() || !g_LocalCoop.players[i].initialized)
			continue;

		if (player.isWalking()) {
			walkingCount++;
			if (walkingCount == 1) {
				firstWalkDir = player._pdir;
			} else if (player._pdir != firstWalkDir) {
				allWalkingSameDirection = false;
			}
		}

		// Start with the current tile position
		// Convert tile position to screen space (scaled by 256)
		// worldToScreen: screenX = (tileY - tileX) * 32, screenY = (tileY + tileX) * -16
		int tileX = player.position.tile.x;
		int tileY = player.position.tile.y;
		int64_t screenX = static_cast<int64_t>(tileY - tileX) * 32 * 256;
		int64_t screenY = static_cast<int64_t>(tileY + tileX) * -16 * 256;

		// If player is walking add their walking offset (already in screen pixels)
		if (!player.hasNoLife() && player.isWalking()) {
			Displacement screenOffset = GetOffsetForWalking(player.AnimInfo, player._pdir, true);
			screenX += static_cast<int64_t>(screenOffset.deltaX) * 256;
			screenY += static_cast<int64_t>(screenOffset.deltaY) * 256;
		}

		totalScreenX += screenX;
		totalScreenY += screenY;
		activeCount++;
	}

	// Need at least 1 active player
	if (activeCount == 0)
		return;

	// Calculate average screen position (still scaled by 256)
	int64_t avgScreenX256 = totalScreenX / activeCount;
	int64_t avgScreenY256 = totalScreenY / activeCount;

	// Update camera target to follow average player position
	// No dead zone - always track the average position for smoothest movement
	// The smoothing layer (below) handles gradual camera movement
	using namespace LocalCoopCamera;

	if (!g_LocalCoop.cameraInitialized) {
		// First time - initialize camera to current average position
		g_LocalCoop.cameraTargetScreenX = avgScreenX256;
		g_LocalCoop.cameraTargetScreenY = avgScreenY256;
		g_LocalCoop.cameraSmoothScreenX = avgScreenX256;
		g_LocalCoop.cameraSmoothScreenY = avgScreenY256;
		g_LocalCoop.cameraInitialized = true;
	} else {
		// Always update target to exactly match average position
		g_LocalCoop.cameraTargetScreenX = avgScreenX256;
		g_LocalCoop.cameraTargetScreenY = avgScreenY256;
	}

	// When all players walk in the same direction, bypass smoothing so the camera moves in lockstep (no jitter/lag).
	const bool allWalkingSameDir = (activeCount >= 1 && walkingCount == activeCount && allWalkingSameDirection);
	if (allWalkingSameDir) {
		g_LocalCoop.cameraSmoothScreenX = avgScreenX256;
		g_LocalCoop.cameraSmoothScreenY = avgScreenY256;
	} else {
		// Apply smoothing: interpolate smoothed camera position towards target
		constexpr int64_t smoothFactor256 = static_cast<int64_t>(SmoothFactor * FixedPointScale);
		int64_t smoothDeltaX = g_LocalCoop.cameraTargetScreenX - g_LocalCoop.cameraSmoothScreenX;
		int64_t smoothDeltaY = g_LocalCoop.cameraTargetScreenY - g_LocalCoop.cameraSmoothScreenY;
		int64_t moveX = (smoothDeltaX * smoothFactor256) / 256;
		int64_t moveY = (smoothDeltaY * smoothFactor256) / 256;

		if (MaxCameraVelocity > 0) {
			int64_t moveDistSq = moveX * moveX + moveY * moveY;
			constexpr int64_t maxVelSq = MaxCameraVelocity * MaxCameraVelocity;
			if (moveDistSq > maxVelSq) {
				int64_t absMoveX = moveX >= 0 ? moveX : -moveX;
				int64_t absMoveY = moveY >= 0 ? moveY : -moveY;
				int64_t approxMoveDist = (absMoveX > absMoveY)
				    ? absMoveX + absMoveY / 2
				    : absMoveY + absMoveX / 2;
				if (approxMoveDist > 0) {
					moveX = (moveX * MaxCameraVelocity) / approxMoveDist;
					moveY = (moveY * MaxCameraVelocity) / approxMoveDist;
				}
			}
		}
		if (MinCameraMovement > 0) {
			int64_t absMoveX = moveX >= 0 ? moveX : -moveX;
			int64_t absMoveY = moveY >= 0 ? moveY : -moveY;
			if (absMoveX < MinCameraMovement && absMoveY < MinCameraMovement) {
				moveX = 0;
				moveY = 0;
			}
		}
		g_LocalCoop.cameraSmoothScreenX += moveX;
		g_LocalCoop.cameraSmoothScreenY += moveY;
	}

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
	if (fracX < 0) {
		fracX += 256;
		tileX--;
	}
	if (fracY < 0) {
		fracY += 256;
		tileY--;
	}

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
	if (!g_LocalCoop.anyCoopPlayerInitializedCache)
		return {};
	return { g_LocalCoop.cameraOffsetX, g_LocalCoop.cameraOffsetY };
}

bool HasCameraOffset()
{
	Displacement offset = GetLocalCoopCameraOffset();
	return offset.deltaX != 0 || offset.deltaY != 0;
}

bool IsLocalCoopPositionOnScreen(Point tilePos)
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return true; // No restriction if local co-op is not enabled

	if (g_LocalCoop.GetActivePlayerCount() == 0)
		return true;
	if (!g_LocalCoop.anyCoopPlayerInitializedCache)
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

	// Make sure we have enough player slots (emptySlot is the game player index)
	ResizePlayersArray(static_cast<size_t>(emptySlot) + 1);

	// Initialize the local co-op player slot
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[emptySlot];
	coopPlayer.Reset();
	coopPlayer.active = true;
	coopPlayer.controllerId = controllerId;
	coopPlayer.characterSelectActive = true; // Start character selection

	// Load available heroes for selection (player id matches g_LocalCoop.players index)
	// Player 1 (emptySlot == 0) shouldn't reach here, but check to be safe
	if (emptySlot > 0) {
		LoadAvailableHeroesForCoopSlot(static_cast<uint8_t>(emptySlot));
	}

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
	if (monster.hasNoLife()) // dead
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
void FindLocalCoopActors(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	const Player *player = validated.player;

	LocalCoopCursorState &cursor = coopPlayer->cursor;

	// Check for towners in town
	if (leveltype == DTYPE_TOWN) {
		for (size_t i = 0; i < GetNumTowners(); i++) {
			const int distance = GetLocalCoopDistance(*player, Towners[i].position, 2);
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
	if (player->UsesRangedWeapon()) {
		int distance = 0;
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			const int mi = ActiveMonsters[i];
			const Monster &monster = Monsters[mi];

			if (!CanLocalCoopTargetMonster(monster))
				continue;

			const bool newCanTalk = CanTalkToMonst(monster);
			if (cursor.pcursmonst != -1 && !canTalk && newCanTalk)
				continue;
			const int newDistance = player->position.future.ExactDistance(monster.position.future);
			const int newRotations = GetLocalCoopRotaryDistance(*player, monster.position.future);
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

			const int distance = GetLocalCoopDistance(*player, monster.position.tile, maxSteps);
			if (distance == 0)
				continue;

			const bool newCanTalk = CanTalkToMonst(monster);
			if (cursor.pcursmonst != -1 && !canTalk && newCanTalk)
				continue;
			const int newRotations = GetLocalCoopRotaryDistance(*player, monster.position.tile);
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
void FindLocalCoopItemsAndObjects(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	const Player *player = validated.player;

	LocalCoopCursorState &cursor = coopPlayer->cursor;
	const Point futurePosition = player->position.future;

	int rotations = 5;

	// Search 3x3 area around player
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			Point targetPos = { futurePosition.x + dx, futurePosition.y + dy };

			// Validate position before accessing arrays
			if (!InDungeonBounds(targetPos)) {
				continue;
			}

			// Check for items
			const int8_t itemId = dItem[targetPos.x][targetPos.y] - 1;
			if (itemId >= 0) {
				const Item &item = Items[itemId];
				if (!item.isEmpty() && item.selectionRegion != SelectionRegion::None) {
					const int newRotations = GetLocalCoopRotaryDistance(*player, targetPos);
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
						const int newRotations = GetLocalCoopRotaryDistance(*player, targetPos);
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
void FindLocalCoopTriggers(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId, false, false);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	const Player *player = validated.player;

	LocalCoopCursorState &cursor = coopPlayer->cursor;

	// Don't find triggers if we already have a target
	if (cursor.pcursitem != -1 || cursor.objectUnderCursor != nullptr)
		return;

	int rotations = 0;
	int distance = 0;

	// Check for portal missiles
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal || missile._mitype == MissileID::RedPortal) {
			const int newDistance = GetLocalCoopDistance(*player, missile.position.tile, 2);
			if (newDistance == 0)
				continue;
			if (cursor.pcursmissile != nullptr && distance < newDistance)
				continue;
			const int newRotations = GetLocalCoopRotaryDistance(*player, missile.position.tile);
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
			const int newDistance = GetLocalCoopDistance(*player, { tx, ty }, 2);
			if (newDistance == 0)
				continue;
			cursor.cursPosition = { tx, ty };
			cursor.pcurstrig = i;
		}
	}
}

} // namespace

void UpdateLocalCoopTargetSelection(uint8_t playerId)
{
	if (!g_LocalCoop.enabled)
		return;

	auto validated = GetValidatedPlayer(playerId, true, false); // Don't require alive for target selection
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	const Player *player = validated.player;
	if (player->_pInvincible || player->_pHitPoints <= 0)
		return;
	if (DoomFlag)
		return;

	// If action is held and we have a valid target, keep it
	if (coopPlayer->actionHeld != GameActionType_NONE) {
		// Invalidate targets that are no longer valid
		if (coopPlayer->cursor.pcursmonst != -1) {
			const Monster &monster = Monsters[coopPlayer->cursor.pcursmonst];
			if (!CanLocalCoopTargetMonster(monster)) {
				coopPlayer->cursor.pcursmonst = -1;
			}
		}
		return;
	}

	// Clear previous targets
	coopPlayer->cursor.Reset();

	// Find new targets
	FindLocalCoopActors(playerId);
	FindLocalCoopItemsAndObjects(playerId);
	FindLocalCoopTriggers(playerId);
}

void UpdateLocalCoopSpellMenuOwner()
{
	if (!g_LocalCoop.enabled)
		return;

	// If the speedbook was closed while a local coop player owned it (e.g. by selecting a spell
	// or by another input path), restore MyPlayer/InspectPlayer.
	if (g_LocalCoop.spellMenuOwnerPlayerId >= 0 && !SpellSelectFlag)
		ReleaseLocalCoopSpellMenu();
}

void PerformLocalCoopPrimaryAction(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	Player *player = validated.player;

	// Temporarily swap player context so network commands use correct player ID
	LocalCoopPlayerContext context(playerId);

	// If this player owns panels and character panel is open, handle character panel button clicks
	if (g_LocalCoop.DoesPlayerOwnPanels(playerId) && CharFlag) {
		// CheckChrBtns uses MyPlayer and MousePosition, so we need to ensure context is set
		// Make sure pChrButtons is loaded before checking buttons (prevents crash)
		if (pChrButtons.has_value()) {
			CheckChrBtns();
		}
		return;
	}

	// If this player owns panels and inventory is open, handle inventory interactions
	// Primary action in inventory should grab/place items, same as player 1's secondary action
	if (g_LocalCoop.DoesPlayerOwnPanels(playerId) && invflag) {
		// Check for spell cursors (Identify, Repair, Recharge, etc.) first
		// These need to be handled before normal inventory interactions
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
			if (pcurs == CURSOR_HOURGLASS)
				return;
			TryIconCurs();
			NewCursor(CURSOR_HAND);
			return;
		}
		// If player is holding an item, try to place it
		if (!player->HoldItem.isEmpty()) {
			CheckInvItem(true, false);
			return;
		}
		// If inventory item is highlighted (via D-Pad navigation), grab it to cursor
		if (pcursinvitem != -1) {
			Item &item = GetInventoryItem(*player, pcursinvitem);
			if (!item.isEmpty() && player->_pmode <= PM_WALK_SIDEWAYS && player->HoldItem.isEmpty()) {
				CloseGoldDrop();

				// Store the item location before removing
				const int8_t itemLocation = pcursinvitem;

				// Put item in HoldItem
				player->HoldItem = item;
				player->HoldItem._iStatFlag = player->CanUseItem(player->HoldItem);

				// Remove item from inventory based on location
				if (itemLocation < INVITEM_INV_FIRST) {
					// Equipment slot
					RemoveEquipment(*player, static_cast<inv_body_loc>(itemLocation), false);
					CalcPlrInv(*player, true);
				} else if (itemLocation <= INVITEM_INV_LAST) {
					// Inventory grid
					player->RemoveInvItem(itemLocation - INVITEM_INV_FIRST, false);
				} else {
					// Belt
					player->RemoveSpdBarItem(itemLocation - INVITEM_BELT_FIRST);
				}

				// Handle gold separately
				if (player->HoldItem._itype == ItemType::Gold) {
					player->_pGold = CalculateGold(*player);
				}

				// Update cursor to show the item (only for MyPlayer)
				if (MyPlayer == player) {
					NewCursor(player->HoldItem);
					PlaySFX(SfxID::GrabItem);
				}

				// Clear pcursinvitem since item is no longer in inventory
				pcursinvitem = -1;
			}
		}
		return;
	}

	LocalCoopCursorState &cursor = coopPlayer->cursor;

	// Talk to towner
	if (leveltype == DTYPE_TOWN && cursor.pcursmonst != -1) {
		NetSendCmdLocParam1(true, CMD_TALKXY, Towners[cursor.pcursmonst].position, cursor.pcursmonst);
		return;
	}

	// Attack monster
	if (cursor.pcursmonst != -1) {
		if (!player->UsesRangedWeapon() || CanTalkToMonst(Monsters[cursor.pcursmonst])) {
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
	Point target = player->position.future + Displacement(player->_pdir);
	if (player->UsesRangedWeapon()) {
		NetSendCmdLoc(playerId, true, CMD_RATTACKXY, target);
	} else {
		NetSendCmdLoc(playerId, true, CMD_SATTACKXY, target);
	}
}

void PerformLocalCoopSecondaryAction(uint8_t playerId)
{
	auto validated = GetValidatedPlayer(playerId);
	if (!validated.isValid)
		return;

	LocalCoopPlayer *coopPlayer = validated.coop;
	Player *player = validated.player;

	// Swap player context so network commands are sent for the correct player
	LocalCoopPlayerContext context(playerId);

	LocalCoopCursorState &cursor = coopPlayer->cursor;

	// If this player owns panels and inventory is open, handle inventory interactions
	if (g_LocalCoop.DoesPlayerOwnPanels(playerId) && invflag) {
		// If player is holding an item, try to place it
		if (!player->HoldItem.isEmpty()) {
			// Handle inventory interactions with auto-move behavior (isShiftHeld=true)
			// Belt items will automatically move to inventory
			// Inventory items will automatically move to belt (if possible)
			// CheckInvItem handles pasting (when holding item)
			CheckInvItem(true, false);
			return;
		}
		// Otherwise, check if we should grab highlighted item
		// Only process grab if cursor was moved via D-Pad (pcursinvitem is set) and secondary action is pressed
		if (pcursinvitem != -1) {
			// Get the highlighted inventory item
			Item &item = GetInventoryItem(*player, pcursinvitem);
			if (!item.isEmpty()) {
				// Check if player can grab items (not in certain actions)
				if (player->_pmode <= PM_WALK_SIDEWAYS && player->HoldItem.isEmpty()) {
					CloseGoldDrop();

					// Store the item location before removing
					const int8_t itemLocation = pcursinvitem;

					// Put item in HoldItem
					player->HoldItem = item;
					player->HoldItem._iStatFlag = player->CanUseItem(player->HoldItem);

					// Remove item from inventory based on location
					if (itemLocation < INVITEM_INV_FIRST) {
						// Equipment slot
						RemoveEquipment(*player, static_cast<inv_body_loc>(itemLocation), false);
						CalcPlrInv(*player, true);
					} else if (itemLocation <= INVITEM_INV_LAST) {
						// Inventory grid
						player->RemoveInvItem(itemLocation - INVITEM_INV_FIRST, false);
					} else {
						// Belt
						player->RemoveSpdBarItem(itemLocation - INVITEM_BELT_FIRST);
					}

					// Handle gold separately
					if (player->HoldItem._itype == ItemType::Gold) {
						player->_pGold = CalculateGold(*player);
					}

					// Update cursor to show the item (only for MyPlayer)
					if (MyPlayer == player) {
						NewCursor(player->HoldItem);
						PlaySFX(SfxID::GrabItem);
					}

					// Clear pcursinvitem since item is no longer in inventory
					// This prevents info box from trying to display info for removed item
					pcursinvitem = -1;

					// Successfully grabbed, return early
					return;
				}
			}
		}
	}

	// Pick up item
	if (cursor.pcursitem != -1) {
		// For local coop players, always auto-pickup to inventory (CMD_GOTOAGETITEM)
		// This makes items go directly to inventory if there's room, instead of to cursor
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
		MakePlrPath(*player, cursor.pcursmissile->position.tile, true);
		player->destAction = ACTION_WALK;
	} else if (cursor.pcurstrig != -1) {
		MakePlrPath(*player, trigs[cursor.pcurstrig].position, true);
		player->destAction = ACTION_WALK;
	}
}

void ProcessLocalCoopGameAction(uint8_t playerId, uint8_t actionType)
{
	if (!g_LocalCoop.enabled)
		return;

	if (!local_coop::IsPlayerValid(playerId))
		return;

	switch (actionType) {
	case GameActionType_NONE:
		break;

	case GameActionType_PRIMARY_ACTION:
		PerformLocalCoopPrimaryAction(playerId);
		break;

	case GameActionType_SECONDARY_ACTION:
		PerformLocalCoopSecondaryAction(playerId);
		break;

	case GameActionType_USE_HEALTH_POTION: {
		LocalCoopPlayerContext context(playerId);
		UseBeltItem(BeltItemType::Healing);
		break;
	}

	case GameActionType_USE_MANA_POTION: {
		LocalCoopPlayerContext context(playerId);
		UseBeltItem(BeltItemType::Mana);
		break;
	}

	case GameActionType_TOGGLE_CHARACTER_INFO:
	case GameActionType_TOGGLE_INVENTORY:
	case GameActionType_TOGGLE_SPELL_BOOK:
	case GameActionType_TOGGLE_QUEST_LOG:
		// Panel toggles - try to claim ownership using playerId
		if (g_LocalCoop.TryClaimPanelOwnership(playerId)) {
			// Switch to this player's context for all panel operations
			// This sets MyPlayer, MyPlayerId, and InspectPlayer to the coop player
			MyPlayer = &Players[playerId];
			MyPlayerId = playerId;
			InspectPlayer = &Players[playerId];

			// If closing inventory and player has item in HoldItem, drop it
			// MyPlayer is already set to Players[playerId] above, so we can use it directly
			if (actionType == GameActionType_TOGGLE_INVENTORY && invflag && !MyPlayer->HoldItem.isEmpty()) {
				// Try to drop the item before closing inventory
				std::optional<Point> itemTile = FindAdjacentPositionForItem(MyPlayer->position.future, MyPlayer->_pdir);
				if (itemTile) {
					NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, MyPlayer->HoldItem);
					MyPlayer->HoldItem.clear();
					NewCursor(CURSOR_HAND);
				} else {
					// Can't find a place to drop, keep item in HoldItem
					MyPlayer->Say(HeroSpeech::WhereWouldIPutThis);
				}
			}

			// If opening inventory, reload inventory image for this player's class
			// This ensures the inventory background matches the coop player's class
			if (actionType == GameActionType_TOGGLE_INVENTORY && !invflag) {
				InitInv();
			}

			ProcessGameAction(GameAction { static_cast<GameActionType>(actionType) });

			// CRITICAL: Restore context after ProcessGameAction
			// ProcessGameAction might have reset MyPlayer/InspectPlayer, so we need to restore it
			// This ensures the context persists while panels are open
			if (g_LocalCoop.DoesPlayerOwnPanels(playerId)) {
				MyPlayer = &Players[playerId];
				MyPlayerId = playerId;
				InspectPlayer = &Players[playerId];
			}

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
		CastLocalCoopSpell(playerId, Players[playerId]._pRSpell, Players[playerId]._pRSplType);
		break;

	default:
		break;
	}
}

bool HandleLocalCoopPanelAction(uint8_t playerId, uint8_t actionType)
{
	if (!g_LocalCoop.enabled)
		return false;

	// Only handle panel toggle actions
	if (actionType != GameActionType_TOGGLE_CHARACTER_INFO && actionType != GameActionType_TOGGLE_INVENTORY && actionType != GameActionType_TOGGLE_SPELL_BOOK && actionType != GameActionType_TOGGLE_QUEST_LOG) {
		return false;
	}

	// If another player owns panels, block this player from interfering
	if (g_LocalCoop.panelOwnerPlayerId >= 0 && g_LocalCoop.panelOwnerPlayerId != static_cast<int>(playerId)) {
		// Another player owns the panels, this player can't toggle
		return true; // Consume the action, do nothing
	}

	// This player can toggle panels - let the normal handler do it
	// Return false so ProcessGameAction continues with normal processing
	return false;
}

bool AreLocalCoopPanelsOpen()
{
	return IsLeftPanelOpen() || IsRightPanelOpen();
}

Player *GetLocalCoopPanelOwnerPlayer()
{
	if (!g_LocalCoop.enabled || g_LocalCoop.panelOwnerPlayerId < 0)
		return nullptr;

	uint8_t playerId = g_LocalCoop.GetPanelOwnerPlayerId();
	if (playerId < Players.size())
		return &Players[playerId];
	return nullptr;
}

std::optional<Point> GetLocalCoopBeltSlotPosition(uint8_t playerId, int beltSlot)
{
	if (!g_LocalCoop.enabled)
		return std::nullopt;

	if (playerId >= Players.size() || playerId >= g_LocalCoop.players.size())
		return std::nullopt;

	if (beltSlot < SLOTXY_BELT_FIRST || beltSlot > SLOTXY_BELT_LAST)
		return std::nullopt;

	// In local coop mode, use local panel positioning (main panel is hidden)

	// Get dynamic panel width (must match DrawLocalCoopPlayerHUD)
	const int panelWidth = GetLocalCoopPanelWidth();

	// Calculate panel position for this player (same logic as DrawLocalCoopPlayerHUD)
	const int screenWidth = gnScreenWidth;
	const int screenHeight = gnScreenHeight;

	PanelPosition panelPos = GetPlayerPanelPosition(playerId, screenWidth, screenHeight, panelWidth);
	if (!panelPos.isValid)
		return std::nullopt;

	// Calculate belt position (same as DrawPlayerBelt call in DrawLocalCoopPlayerHUD)
	BeltPosition beltPos = CalculateBeltPosition(GetPanelBaseSectionX(playerId, panelPos.panelX), panelPos.panelY, LocalCoopLayout::PanelWidth::BaseWidth);
	const int beltBaseX = beltPos.baseX;
	const int beltBaseY = beltPos.baseY;

	// Belt slots are positioned at 29px spacing starting at x=207 relative to belt area (from DrawPlayerBelt)
	// But we need to add the offset of 5px to the left (from slotX calculation in DrawPlayerBelt)
	const int slotIndex = beltSlot - SLOTXY_BELT_FIRST;
	const int slotX = beltBaseX + (slotIndex * 29) + 5;
	const int slotY = beltBaseY + 3;

	// Return the center of the slot for cursor positioning (matches GetSlotCoord behavior)
	// GetSlotCoord for regular inventory uses InvRect[slot].Center(), so we should return center too
	// Belt item is drawn at { slotX - 2, slotY + INV_SLOT_SIZE_PX }
	// Center would be at { slotX - 2 + INV_SLOT_SIZE_PX/2, slotY + INV_SLOT_SIZE_PX/2 }
	return Point { slotX - 2 + INV_SLOT_SIZE_PX / 2, slotY + INV_SLOT_SIZE_PX / 2 };
}

std::optional<inv_xy_slot> FindLocalCoopBeltSlotUnderCursor(Point cursorPosition, uint8_t &outPlayerId)
{
	if (!g_LocalCoop.enabled)
		return std::nullopt;

	// In local coop mode, check local belt panels (main panel is hidden)

	// Get dynamic panel width (must match DrawLocalCoopPlayerHUD)
	const int panelWidth = GetLocalCoopPanelWidth();

	const int screenWidth = gnScreenWidth;
	const int screenHeight = gnScreenHeight;

	// Check each player's belt area (up to 4 players in local co-op)
	for (size_t playerId = 0; playerId < MaxLocalPlayers && playerId < Players.size(); ++playerId) {
		const Player &player = Players[playerId];

		// Check if this player should have a local panel
		if (playerId == 0) {
			// Player 1 always shows local panel in local coop mode
			if (!player.plractive)
				continue;
		} else {
			// For local co-op players (2-4), check their state
			auto validated = GetValidatedPlayer(static_cast<uint8_t>(playerId), true, false);
			if (!validated.isValid)
				continue;
			if (!player.plractive)
				continue;
		}

		// Calculate panel position for this player
		PanelPosition panelPos = GetPlayerPanelPosition(static_cast<uint8_t>(playerId), screenWidth, screenHeight, panelWidth);
		if (!panelPos.isValid)
			continue;

		// Calculate belt position (same as DrawPlayerBelt call in DrawLocalCoopPlayerHUD)
		BeltPosition beltPos = CalculateBeltPosition(GetPanelBaseSectionX(static_cast<uint8_t>(playerId), panelPos.panelX), panelPos.panelY, LocalCoopLayout::PanelWidth::BaseWidth);
		const int beltBaseX = beltPos.baseX;
		const int beltBaseY = beltPos.baseY;

		// Check each belt slot for this player
		for (int slotIndex = 0; slotIndex < MaxBeltItems; ++slotIndex) {
			const int slotX = beltBaseX + (slotIndex * 29) + 5;
			const int slotY = beltBaseY + 3;

			// Create a rectangle for this belt slot (29x29 pixels, matching InvRect belt slots)
			const Rectangle slotRect = { { slotX - 2, slotY }, { INV_SLOT_SIZE_PX, INV_SLOT_SIZE_PX } };

			if (slotRect.contains(cursorPosition)) {
				outPlayerId = static_cast<uint8_t>(playerId);
				return static_cast<inv_xy_slot>(SLOTXY_BELT_FIRST + slotIndex);
			}
		}
	}

	return std::nullopt;
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

bool IsLocalCoopTargetItem(int8_t itemIndex)
{
	if (!g_LocalCoop.enabled || itemIndex < 0)
		return false;

	for (const LocalCoopPlayer &coopPlayer : g_LocalCoop.players) {
		if (!coopPlayer.active || !coopPlayer.initialized)
			continue;
		if (coopPlayer.cursor.pcursitem == itemIndex)
			return true;
	}
	return false;
}

void RestorePlayer1ContextForSave()
{
	if (!g_LocalCoop.enabled)
		return;

	// CRITICAL: Before saving, ensure MyPlayer points to Player 1
	// If a local coop player has the panel open, MyPlayer might point to them,
	// which would cause their data (including HoldItem) to be saved to Player 1's save file!
	// Force release panel ownership to restore MyPlayer to Player 1
	if (g_LocalCoop.panelOwnerPlayerId > 0) {
		g_LocalCoop.panelOwnerPlayerId = -1;
		if (Players.size() > 0) {
			MyPlayer = &Players[0];
			MyPlayerId = 0;
			InspectPlayer = &Players[0];
		}
	}
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

		if (i >= Players.size())
			continue;

		Player &player = Players[i];

		// Use the pfile function to save this player to their save slot
		pfile_write_player_to_save(coopPlayer.saveNumber, player);
	}
}

void SetLocalCoopStoreOwner(int playerId)
{
	if (!g_LocalCoop.enabled)
		return;

	if (playerId < 0 || playerId >= static_cast<int>(g_LocalCoop.players.size()))
		return;

	const LocalCoopPlayer &coopPlayer = g_LocalCoop.players[playerId];
	if (!coopPlayer.active || !coopPlayer.initialized)
		return;

	g_LocalCoop.storeOwnerPlayerId = playerId;
}

void ClearLocalCoopStoreOwner()
{
	if (!g_LocalCoop.enabled)
		return;

	if (g_LocalCoop.storeOwnerPlayerId > 0) {
		LocalCoopPlayer *coopPlayer = g_LocalCoop.GetPlayer(static_cast<uint8_t>(g_LocalCoop.storeOwnerPlayerId));
		if (coopPlayer != nullptr) {
			coopPlayer->leftStickX = 0.0f;
			coopPlayer->leftStickY = 0.0f;
		}
	}

	g_LocalCoop.storeOwnerPlayerId = -1;
}

void CloseLocalCoopStore()
{
	CloseVisualStore();
	ActiveStore = TalkID::None;
	ClearLocalCoopStoreOwner();
}

bool IsLocalCoopStoreActive()
{
	return g_LocalCoop.enabled && g_LocalCoop.storeOwnerPlayerId > 0;
}

uint8_t GetLocalCoopStoreOwnerPlayerId()
{
	if (!g_LocalCoop.enabled || g_LocalCoop.storeOwnerPlayerId < 0)
		return 0;

	return static_cast<uint8_t>(g_LocalCoop.storeOwnerPlayerId);
}

} // namespace devilution

#else // USE_SDL1

#include "qol/visual_store.h"
#include "stores.h"

namespace devilution {

LocalCoopState g_LocalCoop;
bool LocalCoopHUDOpen = false;

void InitLocalCoop() { }
void ShutdownLocalCoop() { }
bool IsLocalCoopAvailable() { return false; }
bool IsLocalCoopEnabled() { return false; }
bool IsAnyLocalCoopPlayerInitialized() { return false; }
bool ShouldHideMainPanelForLocalCoop() { return false; }
bool IsLocalCoopPlayer(const Player & /*player*/) { return false; }
bool IsLocalPlayer(const Player &player) { return &player == MyPlayer; }
void UpdateLocalCoopMovement() { }
void ProcessLocalCoopRightStickCursor() { }
void ProcessLocalCoopLeftStickPanelNavigation() { }
void UpdateLocalCoopSpellMenuOwner() { }
void LoadAvailableHeroesForCoopSlot(uint8_t /*playerIdx*/) { }
void ConfirmLocalCoopCharacter(uint8_t /*playerId*/) { }
void DrawLocalCoopCharacterSelect(const Surface & /*out*/) { }
void DrawLocalCoopPlayerHUD(const Surface & /*out*/) { }
void HandleLocalCoopControllerDisconnect(SDL_JoystickID /*controllerId*/) { }
void HandleLocalCoopControllerConnect(SDL_JoystickID /*controllerId*/) { }
Point CalculateLocalCoopViewPosition() { return {}; }
void UpdateLocalCoopCamera() { }
void SetViewPosition(Point position) { ViewPosition = position; }
void SetViewPositionForPlayer(const Player &player, Point position)
{
	if (&player == MyPlayer) ViewPosition = position;
}
Displacement GetLocalCoopCameraOffset() { return {}; }
bool HasCameraOffset() { return false; }
bool IsLocalCoopPositionOnScreen(Point /*tilePos*/) { return true; }
bool TryJoinLocalCoopMidGame(SDL_JoystickID /*controllerId*/) { return false; }
void UpdateLocalCoopTargetSelection(uint8_t /*playerId*/) { }
void ProcessLocalCoopGameAction(uint8_t /*playerId*/, uint8_t /*actionType*/) { }
void PerformLocalCoopPrimaryAction(uint8_t /*playerId*/) { }
void PerformLocalCoopSecondaryAction(uint8_t /*playerId*/) { }
bool AreLocalCoopPanelsOpen() { return false; }
Player *GetLocalCoopPanelOwnerPlayer() { return nullptr; }
std::optional<Point> GetLocalCoopBeltSlotPosition(uint8_t /*playerId*/, int /*beltSlot*/) { return std::nullopt; }
std::optional<inv_xy_slot> FindLocalCoopBeltSlotUnderCursor(Point /*cursorPosition*/, uint8_t & /*outPlayerId*/) { return std::nullopt; }
bool IsLocalCoopTargetMonster(int /*monsterId*/) { return false; }
bool IsLocalCoopTargetItem(int8_t /*itemIndex*/) { return false; }
void RestorePlayer1ContextForSave() { }
void SaveLocalCoopPlayers(bool /*writeGameData*/) { }
void SetLocalCoopStoreOwner(int /*playerId*/) { }
void ClearLocalCoopStoreOwner() { }
void CloseLocalCoopStore()
{
	CloseVisualStore();
	ActiveStore = TalkID::None;
}
bool IsLocalCoopStoreActive() { return false; }
uint8_t GetLocalCoopStoreOwnerPlayerId() { return 0; }
bool HandleLocalCoopPanelAction(uint8_t /*playerId*/, uint8_t /*actionType*/) { return false; }

size_t GetLocalCoopTotalPlayerCount() { return 1; }
bool IsLocalCoopTargetObject(const Object * /*object*/) { return false; }
void LoadAvailableHeroesForAllLocalCoopPlayers() { }
void SyncLocalCoopPlayersToLevel(interface_mode /*fom*/, int /*lvl*/) { }
void ResetLocalCoopCamera() { }
Player *FindLocalCoopPlayerOnTrigger(int & /*outTriggerIndex*/) { return nullptr; }

void LocalCoopCursorState::Reset() { }
void LocalCoopPlayer::Reset() { }
LocalCoopPlayerContext::LocalCoopPlayerContext(uint8_t /*playerId*/)
    : savedMyPlayer_(MyPlayer)
    , savedMyPlayerId_(MyPlayerId)
    , savedInspectPlayer_(InspectPlayer)
{
}
LocalCoopPlayerContext::~LocalCoopPlayerContext()
{
	MyPlayer = savedMyPlayer_;
	MyPlayerId = savedMyPlayerId_;
	InspectPlayer = savedInspectPlayer_;
}
AxisDirection LocalCoopPlayer::GetMoveDirection() const { return { AxisDirectionX_NONE, AxisDirectionY_NONE }; }
size_t LocalCoopState::GetActivePlayerCount() const { return 0; }
size_t LocalCoopState::GetInitializedPlayerCount() const { return 0; }
void LocalCoopState::UpdateAnyCoopPlayerInitializedCache() { anyCoopPlayerInitializedCache = false; }
size_t LocalCoopState::GetTotalPlayerCount() const { return 1; }
bool LocalCoopState::IsAnyCharacterSelectActive() const { return false; }
uint8_t LocalCoopState::GetPanelOwnerPlayerId() const { return 0; }
bool LocalCoopState::TryClaimPanelOwnership(uint8_t /*playerId*/) { return false; }
void LocalCoopState::ReleasePanelOwnership() { }
LocalCoopPlayer *LocalCoopState::GetPlayer(uint8_t /*playerId*/) { return nullptr; }
const LocalCoopPlayer *LocalCoopState::GetPlayer(uint8_t /*playerId*/) const { return nullptr; }

void RestorePanelOwnerContext() { }
bool IsAnyLocalPlayerWalking(Direction & /*outDirection*/) { return false; }

} // namespace devilution

#endif // USE_SDL1
