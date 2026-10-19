/**
 * @file track.cpp
 *
 * Implementation of functionality tracking what the mouse cursor is pointing at.
 */
#include "track.h"

#include <cstdint>
#include <optional>

#include "controls/control_mode.hpp"
#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "levels/dun_tile_data.hpp"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "stores.h"

namespace devilution {

namespace {

/**
 * @brief A repeated request that needs the player to walk, together with everything the path search for it depends on.
 */
struct WalkingRequest {
	PlayerActionType action;
	/** Tile the path search starts from. */
	Point start;
	/** Tile the player needs to walk to. */
	Point destination;
	uint32_t walkabilityGeneration;
	/** Players without life don't block their tile. This isn't covered by the generation, because a player's life is changed in too many places. */
	uint32_t lifelessPlayers;

	bool operator==(const WalkingRequest &other) const = default;
};

std::optional<WalkingRequest> LastWalkingRequest;

uint32_t GetLifelessPlayersMask()
{
	uint32_t mask = 0;
	for (const Player &player : Players) {
		if (player.hasNoLife())
			mask |= 1U << player.getId();
	}
	return mask;
}

/**
 * @brief Checks whether repeating a request that needs the player to walk to the given tile is pointless.
 *
 * A request that finds a path makes the player leave their tile. So if the same request was already made from
 * the same tile and nothing that affects path finding has changed since, the path search failed and would fail
 * again.
 */
bool IsWalkingRequestRedundant(const Player &player, Point destination)
{
	const WalkingRequest request { LastPlayerAction, player.position.future, destination, WalkabilityGeneration, GetLifelessPlayersMask() };
	if (LastWalkingRequest == request)
		return true;

	LastWalkingRequest = request;
	return false;
}

/**
 * @brief Checks whether repeating a melee attack on a target at the given tile is pointless, @see IsWalkingRequestRedundant
 */
bool IsMeleeAttackRedundant(const Player &player, Point targetPosition)
{
	// The target is in reach, so the attack doesn't depend on path finding.
	if (player.position.future.WalkingDistance(targetPosition) <= 1)
		return false;

	return IsWalkingRequestRedundant(player, targetPosition);
}

void RepeatWalk(Player &player)
{
	if (!InDungeonBounds(cursPosition))
		return;

	if (player._pmode != PM_STAND && (!player.isWalking() || player.AnimInfo.getFrameToUseForRendering() <= 6))
		return;

	const Point target = player.GetTargetPosition();
	if (cursPosition == target)
		return;

	if (IsWalkingRequestRedundant(player, cursPosition))
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
}

} // namespace

void InvalidateTargets()
{
	if (pcursmonst != -1) {
		const Monster &monster = Monsters[pcursmonst];
		if (monster.isInvalid || monster.hasNoLife()
		    || (monster.flags & MFLAG_HIDDEN) != 0
		    || !IsTileLit(monster.position.tile)) {
			pcursmonst = -1;
		}
	}

	if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->canInteractWith())
		ObjectUnderCursor = nullptr;

	if (PlayerUnderCursor != nullptr) {
		const Player &targetPlayer = *PlayerUnderCursor;
		if (targetPlayer._pmode == PM_DEATH || targetPlayer._pmode == PM_QUIT || !targetPlayer.plractive
		    || !targetPlayer.isOnActiveLevel() || targetPlayer.hasNoLife()
		    || !IsTileLit(targetPlayer.position.tile))
			PlayerUnderCursor = nullptr;
	}
}

void RepeatPlayerAction()
{
	if (pcurs != CURSOR_HAND)
		return;

	if (sgbMouseDown == CLICK_NONE && ControllerActionHeld == GameActionType_NONE)
		return;

	if (IsPlayerInStore())
		return;

	if (LastPlayerAction == PlayerActionType::None)
		return;

	Player &myPlayer = *MyPlayer;
	if (myPlayer.destAction != ACTION_NONE)
		return;
	if (myPlayer._pInvincible)
		return;
	if (!myPlayer.CanChangeAction())
		return;

	const bool rangedAttack = myPlayer.UsesRangedWeapon();
	switch (LastPlayerAction) {
	case PlayerActionType::Attack:
		if (InDungeonBounds(cursPosition))
			NetSendCmdLoc(MyPlayerId, true, rangedAttack ? CMD_RATTACKXY : CMD_SATTACKXY, cursPosition);
		break;
	case PlayerActionType::AttackMonsterTarget:
		if (pcursmonst != -1) {
			if (!rangedAttack && IsMeleeAttackRedundant(myPlayer, Monsters[pcursmonst].position.future))
				break;
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKID : CMD_ATTACKID, pcursmonst);
		}
		break;
	case PlayerActionType::AttackPlayerTarget:
		if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode) {
			if (!rangedAttack && IsMeleeAttackRedundant(myPlayer, PlayerUnderCursor->position.future))
				break;
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKPID : CMD_ATTACKPID, PlayerUnderCursor->getId());
		}
		break;
	case PlayerActionType::Spell:
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			UpdateSpellTarget(MyPlayer->_pRSpell);
		}
		CheckPlrSpell(ControlMode == ControlTypes::KeyboardAndMouse);
		break;
	case PlayerActionType::SpellMonsterTarget:
		if (pcursmonst != -1)
			CheckPlrSpell(false);
		break;
	case PlayerActionType::SpellPlayerTarget:
		if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode)
			CheckPlrSpell(false);
		break;
	case PlayerActionType::OperateObject:
		if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->isDoor()) {
			if (!IsPlayerAdjacentToObject(myPlayer, *ObjectUnderCursor) && IsWalkingRequestRedundant(myPlayer, cursPosition))
				break;
			NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
		}
		break;
	case PlayerActionType::Walk:
		RepeatWalk(myPlayer);
		break;
	case PlayerActionType::None:
		break;
	}
}

bool track_isscrolling()
{
	return LastPlayerAction == PlayerActionType::Walk;
}

} // namespace devilution
