/**
 * @file track.cpp
 *
 * Implementation of functionality tracking what the mouse cursor is pointing at.
 */
#include "track.h"

#include <SDL.h>

#include "cursor.h"
#include "engine/point.hpp"
#include "player.h"

namespace devilution {

namespace {

bool sgbIsScrolling;
Uint32 sgdwLastWalk;
bool sgbIsWalking;
bool destActionSet;

} // namespace

void track_process()
{
	if (blockClicks)
		return;
	if (cursmx < 0 || cursmx >= MAXDUNX - 1 || cursmy < 0 || cursmy >= MAXDUNY - 1)
		return;

	auto &player = Players[MyPlayerId];

	if (sgbMouseDown == CLICK_NONE) {
		if (destActionSet) {
			player.destAction = ACTION_NONE;
			NetSendCmdParam1(true, CMD_RESETACTION, pcursmonst);
			destActionSet = false;
		}
		return;
	}

	if (player.destAction == ACTION_NONE)
		destActionSet = true;

	int isShift = (SDL_GetModState() & KMOD_SHIFT) != 0 ? DVL_MK_SHIFT : 0;
	if (sgbMouseDown == CLICK_RIGHT) {
		RightMouseDown();
	}
	if (sgbMouseDown == CLICK_LEFT) {
		track_repeat_walk(LeftMouseDown(isShift));
		if (blockClicks) {
			sgbMouseDown = CLICK_NONE;
		} else {
			LeftMouseUp(isShift);
		}
	}
	return;

	if (player._pmode != PM_STAND && !(player.IsWalking() && player.AnimInfo.GetFrameToUseForRendering() > 6))
		return;

	const Point target = player.GetTargetPosition();
	if (cursmx != target.x || cursmy != target.y) {
		Uint32 tick = SDL_GetTicks();
		int tickMultiplier = 6;
		if (currlevel == 0 && sgGameInitInfo.bRunInTown != 0)
			tickMultiplier = 3;
		if ((int)(tick - sgdwLastWalk) >= gnTickDelay * tickMultiplier) {
			sgdwLastWalk = tick;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, { cursmx, cursmy });
			if (!sgbIsScrolling)
				sgbIsScrolling = true;
		}
	}
}

void track_repeat_walk(bool rep)
{
	if (rep) {
		sgbIsScrolling = false;
		sgdwLastWalk = SDL_GetTicks() - gnTickDelay;
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, { cursmx, cursmy });
	} else if (sgbIsScrolling) {
		sgbIsScrolling = false;
	}
}

bool track_isscrolling()
{
	return sgbIsScrolling;
}

} // namespace devilution
