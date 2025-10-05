/**
 * @file nthread.cpp
 *
 * Implementation of functions for managing game ticks.
 */
#include "nthread.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "diablo.h"
#include "engine/animationinfo.h"
#include "engine/demomode.h"
#include "game_mode.hpp"
#include "gmenu.h"
#include "storm/storm_net.hpp"
#include "utils/sdl_mutex.h"
#include "utils/sdl_thread.h"

namespace devilution {

uint8_t sgbNetUpdateRate;
size_t gdwMsgLenTbl[MAX_PLRS];
uint32_t gdwTurnsInTransit;
uintptr_t glpMsgTbl[MAX_PLRS];
uint32_t gdwLargestMsgSize;
uint32_t gdwNormalMsgSize;
int last_tick;
uint8_t ProgressToNextGameTick = 0;

namespace {

SdlMutex MemCrit;
bool nthread_should_run;
int8_t sgbSyncCountdown;
uint32_t turn_upper_bit;
bool sgbTicsOutOfSync;
int8_t sgbPacketCountdown;
bool sgbThreadIsRunning;
SdlThread Thread;

void NthreadHandler()
{
	if (!nthread_should_run) {
		return;
	}

	while (true) {
		MemCrit.lock();
		if (!nthread_should_run) {
			MemCrit.unlock();
			break;
		}
		nthread_send_and_recv_turn(0, 0);
		int delta = gnTickDelay;
		if (nthread_recv_turns())
			delta = last_tick - SDL_GetTicks();
		MemCrit.unlock();
		if (delta > 0)
			SDL_Delay(delta);
		if (!nthread_should_run)
			return;
	}
}

} // namespace

void nthread_terminate_game(const char *pszFcn)
{
	app_fatal(pszFcn);
	gbGameDestroyed = true;
}

uint32_t nthread_send_and_recv_turn(uint32_t curTurn, int turnDelta)
{
	uint32_t curTurnsInTransit;
	if (!SNetGetTurnsInTransit(&curTurnsInTransit)) {
		nthread_terminate_game("SNetGetTurnsInTransit");
		return 0;
	}
	while (curTurnsInTransit++ < gdwTurnsInTransit) {

		const uint32_t turnTmp = turn_upper_bit | (curTurn & 0x7FFFFFFF);
		turn_upper_bit = 0;
		uint32_t turn = turnTmp;

		if (!SNetSendTurn((char *)&turn, sizeof(turn))) {
			nthread_terminate_game("SNetSendTurn");
			return 0;
		}

		curTurn += turnDelta;
		if (curTurn >= 0x7FFFFFFF)
			curTurn &= 0xFFFF;
	}
	return curTurn;
}

bool nthread_recv_turns(bool *pfSendAsync)
{
	if (pfSendAsync != nullptr)
		*pfSendAsync = false;
	sgbPacketCountdown--;
	if (sgbPacketCountdown > 0) {
		last_tick += gnTickDelay;
		return true;
	}
	sgbSyncCountdown--;
	sgbPacketCountdown = sgbNetUpdateRate;
	if (sgbSyncCountdown != 0) {
		if (pfSendAsync != nullptr)
			*pfSendAsync = true;
		last_tick += gnTickDelay;
		return true;
	}
	if (!SNetReceiveTurns(MAX_PLRS, (char **)glpMsgTbl, gdwMsgLenTbl, &player_state[0])) {
		sgbTicsOutOfSync = false;
		sgbSyncCountdown = 1;
		sgbPacketCountdown = 1;
		return false;
	}
	if (!sgbTicsOutOfSync) {
		sgbTicsOutOfSync = true;
		last_tick = SDL_GetTicks();
	}
	sgbSyncCountdown = 4;
	multi_msg_countdown();
	if (pfSendAsync != nullptr)
		*pfSendAsync = true;
	last_tick += gnTickDelay;
	return true;
}

void nthread_set_turn_upper_bit()
{
	turn_upper_bit = 0x80000000;
}

void nthread_start(bool setTurnUpperBit)
{
	last_tick = SDL_GetTicks();
	sgbPacketCountdown = 1;
	sgbSyncCountdown = 1;
	sgbTicsOutOfSync = true;
	if (setTurnUpperBit)
		nthread_set_turn_upper_bit();
	else
		turn_upper_bit = 0;
	_SNETCAPS caps;
	caps.size = 36;
	SNetGetProviderCaps(&caps);
	gdwTurnsInTransit = caps.defaultturnsintransit;
	if (gdwTurnsInTransit == 0)
		gdwTurnsInTransit = 1;
	if (caps.defaultturnssec <= 20 && caps.defaultturnssec != 0)
		sgbNetUpdateRate = 20 / caps.defaultturnssec;
	else
		sgbNetUpdateRate = 1;
	uint32_t largestMsgSize = 512;
	if (caps.maxmessagesize < 0x200)
		largestMsgSize = caps.maxmessagesize;
	gdwLargestMsgSize = largestMsgSize;
	gdwNormalMsgSize = caps.bytessec * sgbNetUpdateRate / 20;
	gdwNormalMsgSize *= 3;
	gdwNormalMsgSize >>= 2;
	if (caps.maxplayers > MAX_PLRS)
		caps.maxplayers = MAX_PLRS;
	gdwNormalMsgSize /= caps.maxplayers;
	while (gdwNormalMsgSize < 0x80) {
		gdwNormalMsgSize *= 2;
		sgbNetUpdateRate *= 2;
	}
	if (gdwNormalMsgSize > largestMsgSize)
		gdwNormalMsgSize = largestMsgSize;
	if (gbIsMultiplayer) {
		sgbThreadIsRunning = false;
		MemCrit.lock();
		nthread_should_run = true;
		Thread = SdlThread { NthreadHandler };
	}
}

void nthread_cleanup()
{
	nthread_should_run = false;
	gdwTurnsInTransit = 0;
	gdwNormalMsgSize = 0;
	gdwLargestMsgSize = 0;
	if (Thread.joinable() && Thread.get_id() != this_sdl_thread::get_id()) {
		if (!sgbThreadIsRunning)
			MemCrit.unlock();
		Thread.join();
	}
}

void nthread_ignore_mutex(bool bStart)
{
	if (!Thread.joinable())
		return;

	if (bStart)
		MemCrit.unlock();
	else
		MemCrit.lock();
	sgbThreadIsRunning = bStart;
}

bool nthread_has_500ms_passed(bool *drawGame /*= nullptr*/)
{
	const int currentTickCount = SDL_GetTicks();
	int ticksElapsed = currentTickCount - last_tick;
	// Check if we missed multiple game ticks (> 10)
	if (ticksElapsed > gnTickDelay * 10) {
		bool resetLastTick = true;
		if (gbIsMultiplayer) {
			for (size_t i = 0; i < Players.size(); i++) {
				if ((player_state[i] & PS_CONNECTED) != 0 && i != MyPlayerId) {
					// Reset last tick is not allowed when other players are connected, because the elapsed time is needed to sync the game ticks between the clients
					resetLastTick = false;
					break;
				}
			}
		}
		if (resetLastTick) {
			// Reset last tick to avoid catching up with all missed game ticks (game speed is dramatically increased for a short time)
			last_tick = currentTickCount;
			ticksElapsed = 0;
		}
	}
	if (drawGame != nullptr) {
		// Check if we missed a game tick.
		// This can happen when we run a low-end device that can't render fast enough (typically 20fps).
		// If this happens, try to speed-up the game by skipping the rendering.
		// This avoids desyncs and hourglasses when running multiplayer and slowdowns in singleplayer.
		*drawGame = ticksElapsed <= gnTickDelay;
	}
	return ticksElapsed >= 0;
}

void nthread_UpdateProgressToNextGameTick()
{
	if (!gbRunGame || PauseMode != 0 || (!gbIsMultiplayer && gmenu_is_active()) || !gbProcessPlayers || demo::IsRunning()) // if game is not running or paused there is no next gametick in the near future
		return;
	const int currentTickCount = SDL_GetTicks();
	const int ticksMissing = last_tick - currentTickCount;
	if (ticksMissing <= 0) {
		ProgressToNextGameTick = AnimationInfo::baseValueFraction; // game tick is due
		return;
	}
	const int ticksAdvanced = gnTickDelay - ticksMissing;
	int32_t fraction = ticksAdvanced * AnimationInfo::baseValueFraction / gnTickDelay;
	fraction = std::clamp<int32_t>(fraction, 0, AnimationInfo::baseValueFraction);
	ProgressToNextGameTick = static_cast<uint8_t>(fraction);
}

} // namespace devilution
