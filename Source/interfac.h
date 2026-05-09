/**
 * @file interfac.h
 *
 * Interface of load screens.
 */
#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL.h>
#endif

#include "utils/ui_fwd.h"

namespace devilution {

/**
 * @brief Custom events.
 */
enum interface_mode : uint8_t {
	WM_DIABNEXTLVL = 0,
	WM_DIABPREVLVL,
	WM_DIABRTNLVL,
	WM_DIABSETLVL,
	WM_DIABWARPLVL,
	WM_DIABTOWNWARP,
	WM_DIABTWARPUP,
	WM_DIABRETOWN,
	WM_DIABNEWGAME,
	WM_DIABLOADGAME,

	// Asynchronous loading events.
	WM_PROGRESS,
	WM_ERROR,
	WM_DONE,

	WM_FIRST = WM_DIABNEXTLVL,
	WM_LAST = WM_DONE,
};

void RegisterCustomEvents();

#if SDL_VERSION_ATLEAST(2, 0, 0)
using SdlEventType = uint16_t;
#else
using SdlEventType = uint8_t;
#endif

bool IsCustomEvent(SdlEventType eventType);

interface_mode GetCustomEvent(const SDL_Event &event);

void CustomEventToSdlEvent(SDL_Event &event, interface_mode eventType);

enum Cutscenes : uint8_t {
	CutStart,
	CutTown,
	CutLevel1,
	CutLevel2,
	CutLevel3,
	CutLevel4,
	CutLevel5,
	CutLevel6,
	CutPortal,
	CutPortalRed,
	CutGate,
};

void interface_msg_pump();
void IncProgress(uint32_t steps = 1);
void CompleteProgress();
void ShowProgress(interface_mode uMsg);

} // namespace devilution
