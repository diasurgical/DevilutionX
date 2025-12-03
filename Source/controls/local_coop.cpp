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
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "game_mode.hpp"
#include "init.hpp"
#include "inv.h"
#include "items.h"
#include "levels/gendung.h"
#include "menu.h"
#include "msg.h"
#include "multi.h"
#include "options.h"
#include "pfile.h"
#include "player.h"
#include "playerdat.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"

namespace devilution {

LocalCoopState g_LocalCoop;

namespace {

/// Face direction lookup table based on axis input
const Direction FaceDir[3][3] = {
	// AxisDirectionX_NONE, AxisDirectionX_LEFT, AxisDirectionX_RIGHT
	{ Direction::South, Direction::West, Direction::East },           // AxisDirectionY_NONE
	{ Direction::North, Direction::NorthWest, Direction::NorthEast }, // AxisDirectionY_UP
	{ Direction::South, Direction::SouthWest, Direction::SouthEast }, // AxisDirectionY_DOWN
};

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

/// Callback for collecting available heroes
int g_CollectHeroExcludeSaveNum = -1;
std::vector<uint32_t> g_CollectHeroExcludeSaves;
std::vector<_uiheroinfo> *g_CollectHeroTarget = nullptr;

bool CollectHeroInfo(_uiheroinfo *info)
{
	// Don't include heroes already selected by other players
	if (info->saveNumber == static_cast<uint32_t>(g_CollectHeroExcludeSaveNum))
		return true;

	for (uint32_t excludeSave : g_CollectHeroExcludeSaves) {
		if (info->saveNumber == excludeSave)
			return true;
	}

	if (g_CollectHeroTarget != nullptr) {
		g_CollectHeroTarget->push_back(*info);
	}
	return true;
}

/**
 * @brief Get the controller ID for a player slot.
 *
 * Controller 0 = Player 1 (existing system)
 * Controller 1 = Player 2 (first local co-op player)
 * etc.
 */
SDL_JoystickID GetControllerIdForSlot(size_t slot)
{
	const auto &controllers = GameController::All();
	if (slot < controllers.size()) {
		return controllers[slot].GetInstanceId();
	}
	return -1;
}

/**
 * @brief Process button input for a local co-op player.
 */
void ProcessLocalCoopButtonInput(int localIndex, const SDL_Event &event)
{
	if (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN)
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const uint8_t playerId = LocalCoopIndexToPlayerId(localIndex);

	const auto button = SDLC_EventGamepadButton(event).button;

	// Character selection mode
	if (coopPlayer.characterSelectActive) {
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

	// A button = Primary attack
	if (button == SDL_GAMEPAD_BUTTON_SOUTH) {
		Point target = player.position.future + Displacement(player._pdir);
		if (player.UsesRangedWeapon()) {
			NetSendCmdLoc(playerId, true, CMD_RATTACKXY, target);
		} else {
			NetSendCmdLoc(playerId, true, CMD_SATTACKXY, target);
		}
	}
	// X button = interact/operate
	else if (button == SDL_GAMEPAD_BUTTON_WEST) {
		Point pos = player.position.future;
		// Try to operate object at current position or in front of player
		Point front = pos + Displacement(player._pdir);
		NetSendCmdLoc(playerId, true, CMD_OPOBJXY, front);
	}
	// B button = pickup item
	else if (button == SDL_GAMEPAD_BUTTON_EAST) {
		Point pos = player.position.future;
		int8_t itemId = dItem[pos.x][pos.y] - 1;
		if (itemId >= 0) {
			NetSendCmdLocParam1(true, CMD_GOTOAGETITEM, pos, itemId);
		}
	}
}

/**
 * @brief Process axis motion for a local co-op player.
 */
void ProcessLocalCoopAxisMotion(int localIndex, const SDL_Event &event)
{
	if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION)
		return;

	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];
	const auto &axis = SDLC_EventGamepadAxis(event);

	switch (axis.axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		coopPlayer.leftStickXUnscaled = static_cast<float>(axis.value);
		break;
	case SDL_GAMEPAD_AXIS_LEFTY:
		coopPlayer.leftStickYUnscaled = static_cast<float>(-axis.value);
		break;
	default:
		return;
	}

	// Apply deadzone scaling
	coopPlayer.leftStickX = coopPlayer.leftStickXUnscaled;
	coopPlayer.leftStickY = coopPlayer.leftStickYUnscaled;
	constexpr float deadzone = 0.25f;
	ScaleJoystickAxes(&coopPlayer.leftStickX, &coopPlayer.leftStickY, deadzone);
}

/**
 * @brief Update movement for a single local co-op player.
 */
void UpdateLocalCoopPlayerMovement(int localIndex)
{
	LocalCoopPlayer &coopPlayer = g_LocalCoop.players[localIndex];

	if (!coopPlayer.active || !coopPlayer.initialized)
		return;
	if (coopPlayer.characterSelectActive)
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
		return;
	}

	const Direction pdir = FaceDir[static_cast<size_t>(dir.y)][static_cast<size_t>(dir.x)];
	const Point delta = player.position.future + pdir;

	// Update facing direction
	if (!player.isWalking() && player.CanChangeAction()) {
		player._pdir = pdir;
	}

	// Try to walk to new position
	if (PosOkPlayer(player, delta)) {
		NetSendCmdLoc(playerId, true, CMD_WALKXY, delta);
	} else {
		// Just turn to face direction if can't move
		if (player._pmode == PM_STAND) {
			StartStand(player, pdir);
		}
	}
}

} // namespace

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
}

AxisDirection LocalCoopPlayer::GetMoveDirection() const
{
	AxisDirection dir;
	dir.x = AxisDirectionX_NONE;
	dir.y = AxisDirectionY_NONE;

	const float threshold = 0.5f;

	if (leftStickX <= -threshold)
		dir.x = AxisDirectionX_LEFT;
	else if (leftStickX >= threshold)
		dir.x = AxisDirectionX_RIGHT;

	if (leftStickY >= threshold)
		dir.y = AxisDirectionY_UP;
	else if (leftStickY <= -threshold)
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

void InitLocalCoop()
{
	g_LocalCoop = {};

	const auto &controllers = GameController::All();
	if (controllers.size() < 2) {
		Log("Local co-op: Not enough controllers ({} found, need at least 2)", controllers.size());
		return;
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
		Players.resize(totalPlayers);
	}
}

void ShutdownLocalCoop()
{
	g_LocalCoop = {};
}

bool IsLocalCoopAvailable()
{
	// Check if the option is enabled and we have at least 2 controllers
	return *GetOptions().Gameplay.enableLocalCoop && GameController::All().size() >= 2;
}

bool IsLocalCoopEnabled()
{
	return g_LocalCoop.enabled;
}

SDL_JoystickID GetControllerIdFromEvent(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		return SDLC_EventGamepadAxis(event).which;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		return SDLC_EventGamepadButton(event).which;
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
			return static_cast<int>(i);
		}
	}

	return -1;
}

bool ProcessLocalCoopInput(const SDL_Event &event)
{
	if (!g_LocalCoop.enabled)
		return false;

	int localIndex = GetLocalCoopPlayerIndex(event);
	if (localIndex < 0)
		return false;

	ProcessLocalCoopAxisMotion(localIndex, event);
	ProcessLocalCoopButtonInput(localIndex, event);

	return true;
}

void UpdateLocalCoopMovement()
{
	if (!g_LocalCoop.enabled)
		return;

	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		UpdateLocalCoopPlayerMovement(static_cast<int>(i));
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
	g_CollectHeroExcludeSaves.clear();
	g_CollectHeroExcludeSaveNum = static_cast<int>(gSaveNumber); // Exclude player 1's hero

	// Exclude heroes already selected by other local co-op players
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (static_cast<int>(i) == localIndex)
			continue;
		const LocalCoopPlayer &other = g_LocalCoop.players[i];
		if (other.active && !other.characterSelectActive && !other.availableHeroes.empty()) {
			// This player has confirmed, exclude their hero
			if (other.selectedHeroIndex < static_cast<int>(other.availableHeroes.size())) {
				g_CollectHeroExcludeSaves.push_back(other.availableHeroes[other.selectedHeroIndex].saveNumber);
			}
		}
	}

	g_CollectHeroTarget = &coopPlayer.availableHeroes;
	pfile_ui_set_hero_infos(CollectHeroInfo);
	g_CollectHeroTarget = nullptr;

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

	if (coopPlayer.selectedHeroIndex < 0 ||
	    coopPlayer.selectedHeroIndex >= static_cast<int>(coopPlayer.availableHeroes.size())) {
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
		Players.resize(playerId + 1);
	}

	Player &player = Players[playerId];

	// Read hero from save file
	pfile_read_player_from_save(selectedHero.saveNumber, player);

	// Initialize player on the same level as Player 1
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

	player.position.tile = spawnPos;
	player.position.future = spawnPos;
	player.position.old = spawnPos;
	player._pdir = Direction::South;

	// Initialize graphics and state
	InitPlayerGFX(player);
	InitPlayer(player, true);

	coopPlayer.characterSelectActive = false;
	coopPlayer.initialized = true;

	Log("Local co-op: Player {} spawned at ({}, {})", playerId + 1, spawnPos.x, spawnPos.y);

	// Reload available heroes for remaining players who haven't selected yet
	for (size_t i = 0; i < g_LocalCoop.players.size(); ++i) {
		if (static_cast<int>(i) != localIndex && g_LocalCoop.players[i].active &&
		    g_LocalCoop.players[i].characterSelectActive) {
			LoadAvailableHeroesForLocalPlayer(static_cast<int>(i));
		}
	}
}

void DrawLocalCoopCharacterSelect(const Surface &out)
{
	if (!g_LocalCoop.enabled)
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

			// Show hero name
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

void DrawHUDBar(const Surface &out, int x, int y, int width, int height, int current, int max, uint8_t fillColor, uint8_t bgColor)
{
	// Draw background bar
	for (int i = 0; i < width; i++) {
		DrawVerticalLine(out, { x + i, y }, height, bgColor);
	}

	// Calculate fill width
	if (max > 0 && current > 0) {
		int fillWidth = (current * width) / max;
		if (fillWidth > width)
			fillWidth = width;
		for (int i = 0; i < fillWidth; i++) {
			DrawVerticalLine(out, { x + i, y }, height, fillColor);
		}
	}
}

constexpr int BeltSlotSize = 28; // Same as INV_SLOT_SIZE_PX
constexpr int BeltSlotSpacing = 1;
constexpr int BeltSlotsPerRow = 8; // Display belt as 8x1 grid (all slots side by side)

void DrawPlayerBeltSlot(const Surface &out, const Player &player, int slotIndex, Point position)
{
	const Item &item = player.SpdList[slotIndex];

	// Draw slot background
	FillRect(out, position.x, position.y, BeltSlotSize, BeltSlotSize, PAL16_GRAY + 12);

	// Draw border
	DrawHorizontalLine(out, position, BeltSlotSize, PAL16_GRAY + 8);
	DrawHorizontalLine(out, { position.x, position.y + BeltSlotSize - 1 }, BeltSlotSize, PAL16_GRAY + 8);
	DrawVerticalLine(out, position, BeltSlotSize, PAL16_GRAY + 8);
	DrawVerticalLine(out, { position.x + BeltSlotSize - 1, position.y }, BeltSlotSize, PAL16_GRAY + 8);

	if (item.isEmpty())
		return;

	// Draw item quality background
	InvDrawSlotBack(out, { position.x, position.y + BeltSlotSize }, { BeltSlotSize, BeltSlotSize }, item._iMagical);

	// Get item sprite
	const int cursId = item._iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);

	// Draw the item (position needs to be at bottom-left of slot for ClxDraw)
	Point itemPos = { position.x, position.y + BeltSlotSize };
	DrawItem(item, out, itemPos, sprite);
}

void DrawPlayerBelt(const Surface &out, const Player &player, Point basePosition, bool alignRight)
{
	// Belt is displayed as an 8x1 grid (all slots side by side)
	// If alignRight, slots go right-to-left
	constexpr int beltWidth = BeltSlotsPerRow * (BeltSlotSize + BeltSlotSpacing) - BeltSlotSpacing;

	int startX = alignRight ? basePosition.x + beltWidth - BeltSlotSize : basePosition.x;
	int xDirection = alignRight ? -1 : 1;

	for (int i = 0; i < MaxBeltItems; i++) {
		int col = i % BeltSlotsPerRow;

		int slotX = startX + (col * (BeltSlotSize + BeltSlotSpacing) * xDirection);
		int slotY = basePosition.y;

		DrawPlayerBeltSlot(out, player, i, { slotX, slotY });
	}
}

} // namespace

bool LocalCoopHUDOpen = true;

void DrawLocalCoopPlayerHUD(const Surface &out)
{
	// Show HUD when local co-op option is enabled (even if only one player)
	if (!*GetOptions().Gameplay.enableLocalCoop)
		return;

	// Only draw HUD when not in character selection mode
	if (g_LocalCoop.enabled && g_LocalCoop.IsAnyCharacterSelectActive())
		return;

	constexpr int padding = 8;
	constexpr int barWidth = 240;
	constexpr int barHeight = 6;
	constexpr int barSpacing = 2;
	constexpr int textHeight = 14;
	constexpr int beltHeight = BeltSlotSize; // Single row
	constexpr int hudHeight = textHeight + (barHeight + barSpacing) * 2 + 4 + beltHeight + 4;

	// Draw HUD for each active player
	// Player 1: Bottom-left
	// Player 2: Bottom-right
	// Player 3: Top-left
	// Player 4: Top-right

	size_t totalPlayers = g_LocalCoop.GetTotalPlayerCount();

	for (size_t playerId = 0; playerId < totalPlayers && playerId < Players.size(); ++playerId) {
		const Player &player = Players[playerId];

		// Skip if player is not active
		if (!player.plractive)
			continue;

		// Determine position based on player ID
		int x, y;
		bool alignRight = false;
		bool atBottom = false;

		switch (playerId) {
		case 0: // Player 1 - Bottom-left
			x = padding;
			y = out.h() - padding - hudHeight;
			atBottom = true;
			break;
		case 1: // Player 2 - Bottom-right
			x = out.w() - padding - barWidth;
			y = out.h() - padding - hudHeight;
			alignRight = true;
			atBottom = true;
			break;
		case 2: // Player 3 - Top-left
			x = padding;
			y = padding;
			break;
		case 3: // Player 4 - Top-right
			x = out.w() - padding - barWidth;
			y = padding;
			alignRight = true;
			break;
		default:
			continue;
		}

		// Get player stats
		int currentHP = player._pHitPoints >> 6;
		int maxHP = player._pMaxHP >> 6;
		int currentMana = player._pMana >> 6;
		int maxMana = player._pMaxMana >> 6;
		int level = player.getCharacterLevel();

		// Determine HP bar color based on health percentage
		uint8_t hpBarColor = PAL8_RED + 4;
		if (player.pManaShield)
			hpBarColor = PAL8_YELLOW + 5;

		// For bottom players, draw belt first (at bottom), then bars, then name
		// For top players, draw name first, then bars, then belt
		int currentY = y;

		if (atBottom) {
			// Bottom players: Belt at very bottom
			int beltY = y + hudHeight - beltHeight;
			DrawPlayerBelt(out, player, { x, beltY }, alignRight);

			// Bars above belt
			int barsY = beltY - 4 - (barHeight + barSpacing) * 2;

			// Draw HP bar
			DrawHUDBar(out, x, barsY, barWidth, barHeight, currentHP, maxHP, hpBarColor, PAL16_GRAY + 10);

			// Draw Mana bar
			DrawHUDBar(out, x, barsY + barHeight + barSpacing, barWidth, barHeight, currentMana, maxMana, PAL8_BLUE + 3, PAL16_GRAY + 10);

			// Name at top
			std::string nameStr = StrCat("P", playerId + 1, ": ", player._pName, " Lv", level);
			DrawString(out, nameStr, { { x, y }, { barWidth, 0 } },
			    { .flags = UiFlags::ColorGold | UiFlags::Outlined | UiFlags::FontSize12, .spacing = 1 });
		} else {
			// Top players: Name at top
			std::string nameStr = StrCat("P", playerId + 1, ": ", player._pName, " Lv", level);
			DrawString(out, nameStr, { { x, currentY }, { barWidth, 0 } },
			    { .flags = UiFlags::ColorGold | UiFlags::Outlined | UiFlags::FontSize12, .spacing = 1 });
			currentY += textHeight;

			// Draw HP bar
			DrawHUDBar(out, x, currentY, barWidth, barHeight, currentHP, maxHP, hpBarColor, PAL16_GRAY + 10);
			currentY += barHeight + barSpacing;

			// Draw Mana bar
			DrawHUDBar(out, x, currentY, barWidth, barHeight, currentMana, maxMana, PAL8_BLUE + 3, PAL16_GRAY + 10);
			currentY += barHeight + 4;

			// Belt below bars
			DrawPlayerBelt(out, player, { x, currentY }, alignRight);
		}
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
	if (!g_LocalCoop.enabled)
		return;

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
				Players.resize(totalPlayers);
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

void UpdateLocalCoopCamera()
{
	if (!g_LocalCoop.enabled || !IsLocalCoopEnabled())
		return;

	// Only update camera if any character selection is complete
	if (g_LocalCoop.IsAnyCharacterSelectActive())
		return;

	// Calculate target position (center of all players)
	Point targetPos = CalculateLocalCoopViewPosition();

	// Smoothly move camera towards target (simple lerp)
	// This prevents jarring camera movements
	int dx = targetPos.x - ViewPosition.x;
	int dy = targetPos.y - ViewPosition.y;

	// Move camera one tile at a time towards target
	if (dx > 0)
		ViewPosition.x++;
	else if (dx < 0)
		ViewPosition.x--;

	if (dy > 0)
		ViewPosition.y++;
	else if (dy < 0)
		ViewPosition.y--;
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
		Players.resize(playerId + 1);
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

} // namespace devilution

#else // USE_SDL1

namespace devilution {

LocalCoopState g_LocalCoop;

void InitLocalCoop() { }
void ShutdownLocalCoop() { }
bool IsLocalCoopAvailable() { return false; }
bool IsLocalCoopEnabled() { return false; }
void UpdateLocalCoopMovement() { }
void LoadAvailableHeroesForLocalPlayer(int /*localIndex*/) { }
void ConfirmLocalCoopCharacter(int /*localIndex*/) { }
void DrawLocalCoopCharacterSelect(const Surface & /*out*/) { }
void DrawLocalCoopPlayerHUD(const Surface & /*out*/) { }
void HandleLocalCoopControllerDisconnect(SDL_JoystickID /*controllerId*/) { }
void HandleLocalCoopControllerConnect(SDL_JoystickID /*controllerId*/) { }
Point CalculateLocalCoopViewPosition() { return {}; }
void UpdateLocalCoopCamera() { }
bool TryJoinLocalCoopMidGame(SDL_JoystickID /*controllerId*/) { return false; }

void LocalCoopPlayer::Reset() { }
AxisDirection LocalCoopPlayer::GetMoveDirection() const { return { AxisDirectionX_NONE, AxisDirectionY_NONE }; }
size_t LocalCoopState::GetActivePlayerCount() const { return 0; }
size_t LocalCoopState::GetTotalPlayerCount() const { return 1; }
bool LocalCoopState::IsAnyCharacterSelectActive() const { return false; }

} // namespace devilution

#endif // USE_SDL1
