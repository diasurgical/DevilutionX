/**
 * @file nthread.h
 *
 * Interface of functions for managing game ticks.
 */
#pragma once

#include <cstdint>

#include "player.h"
#include "utils/attributes.h"

namespace devilution {

extern uint8_t sgbNetUpdateRate;
extern size_t gdwMsgLenTbl[MaxPlayers];
extern uint32_t gdwTurnsInTransit;
extern uintptr_t glpMsgTbl[MaxPlayers];
extern uint32_t gdwLargestMsgSize;
extern uint32_t gdwNormalMsgSize;
/** @brief the progress as a fraction (see AnimationInfo::baseValueFraction) in time to the next game tick */
extern DVL_API_FOR_TEST uint8_t ProgressToNextGameTick;
extern int last_tick;

void nthread_terminate_game(const char *pszFcn);
uint32_t nthread_send_and_recv_turn(uint32_t curTurn, int turnDelta);
bool nthread_recv_turns(bool *pfSendAsync = nullptr);
void nthread_set_turn_upper_bit();
void nthread_start(bool setTurnUpperBit);
void nthread_cleanup();
void nthread_ignore_mutex(bool bStart);

/**
 * @brief Checks if it's time for the logic to advance
 * @return True if the engine should tick
 */
bool nthread_has_500ms_passed(bool *drawGame = nullptr);
/**
 * @brief Updates the progress in time to the next game tick
 */
void nthread_UpdateProgressToNextGameTick();

} // namespace devilution
