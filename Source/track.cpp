/**
 * @file track.cpp
 *
 * Implementation of functionality tracking what the mouse cursor is pointing at.
 */
#include "track.h"

#include <SDL.h>

#include "controls/control_mode.hpp"
#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "player.h"
#include "stores.h"

namespace devilution {

namespace {

void RepeatWalk(Player &player)
{
	if (!InDungeonBounds(cursPosition))
		return;

	if (player._pmode != PM_STAND && !(player.isWalking() && player.AnimInfo.getFrameToUseForRendering() > 6))
		return;

	const Point target = player.GetTargetPosition();
	if (cursPosition == target)
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
}

} // namespace

void InvalidateTargets()
{
	if (pcursmonst != -1) {
		const Monster &monster = Monsters[pcursmonst];
		if (monster.isInvalid || monster.hitPoints >> 6 <= 0
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
		    || !targetPlayer.isOnActiveLevel() || targetPlayer._pHitPoints >> 6 <= 0
		    || !IsTileLit(targetPlayer.position.tile))
			PlayerUnderCursor = nullptr;
	}
}

void RepeatMouseAction()
{
	if (pcurs != CURSOR_HAND)
		return;

	if (sgbMouseDown == CLICK_NONE && ControllerActionHeld == GameActionType_NONE)
		return;

	if (IsPlayerInStore())
		return;

	if (LastMouseButtonAction == MouseActionType::None)
		return;

	Player &myPlayer = *MyPlayer;
	if (myPlayer.destAction != ACTION_NONE)
		return;
	if (myPlayer._pInvincible)
		return;
	if (!myPlayer.CanChangeAction())
		return;

	bool rangedAttack = myPlayer.UsesRangedWeapon();
	switch (LastMouseButtonAction) {
	case MouseActionType::Attack:
		if (InDungeonBounds(cursPosition))
			NetSendCmdLoc(MyPlayerId, true, rangedAttack ? CMD_RATTACKXY : CMD_SATTACKXY, cursPosition);
		break;
	case MouseActionType::AttackMonsterTarget:
		if (pcursmonst != -1)
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKID : CMD_ATTACKID, pcursmonst);
		break;
	case MouseActionType::AttackPlayerTarget:
		if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode)
			NetSendCmdParam1(true, rangedAttack ? CMD_RATTACKPID : CMD_ATTACKPID, PlayerUnderCursor->getId());
		break;
	case MouseActionType::Spell:
		if (ControlMode != ControlTypes::KeyboardAndMouse) {
			UpdateSpellTarget(MyPlayer->_pRSpell);
		}
		CheckPlrSpell(ControlMode == ControlTypes::KeyboardAndMouse);
		break;
	case MouseActionType::SpellMonsterTarget:
		if (pcursmonst != -1)
			CheckPlrSpell(false);
		break;
	case MouseActionType::SpellPlayerTarget:
		if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode)
			CheckPlrSpell(false);
		break;
	case MouseActionType::OperateObject:
		if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->isDoor()) {
			NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
		}
		break;
	case MouseActionType::Walk:
		RepeatWalk(myPlayer);
		break;
	case MouseActionType::None:
		break;
	}
}

bool track_isscrolling()
{
	return LastMouseButtonAction == MouseActionType::Walk;
}

} // namespace devilution
