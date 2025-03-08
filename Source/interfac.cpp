/**
 * @file interfac.cpp
 *
 * Implementation of load screens.
 */

#include <cstdint>
#include <optional>
#include <string>

#include <SDL.h>
#include <expected.hpp>

#include "control.h"
#include "controls/input.h"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "game_mode.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "loadsave.h"
#include "multi.h"
#include "pfile.h"
#include "plrmsg.h"
#include "utils/log.hpp"
#include "utils/sdl_geometry.h"
#include "utils/sdl_thread.h"

#ifndef USE_SDL1
#include "controls/touch/renderers.h"
#endif

namespace devilution {

namespace {

#if (defined(__APPLE__) || defined(__3DS__)) && defined(USE_SDL1)
// On Tiger PPC, SDL_PushEvent from a background thread appears to do nothing.
#define SDL_PUSH_EVENT_BG_THREAD_WORKS 0
#else
#define SDL_PUSH_EVENT_BG_THREAD_WORKS 1
#endif

#if !SDL_PUSH_EVENT_BG_THREAD_WORKS
// This workaround is not completely thread-safe but the worst
// that can happen is we miss some WM_PROGRESS events,
// which is not a problem.
struct {
	std::atomic<int> type;
	std::string error;
} NextCustomEvent;
#endif

constexpr uint32_t MaxProgress = 534;
constexpr uint32_t ProgressStepSize = 23;

OptionalOwnedClxSpriteList sgpBackCel;

bool IsProgress;
uint32_t sgdwProgress;
int progress_id;

/** The color used for the progress bar as an index into the palette. */
const uint8_t BarColor[3] = { 138, 43, 254 };
/** The screen position of the top left corner of the progress bar. */
const int BarPos[3][2] = { { 53, 37 }, { 53, 421 }, { 53, 37 } };

OptionalOwnedClxSpriteList ArtCutsceneWidescreen;

SdlEventType CustomEventsBegin = SDL_USEREVENT;
constexpr uint16_t NumCustomEvents = WM_LAST - WM_FIRST + 1;

Cutscenes GetCutSceneFromLevelType(dungeon_type type)
{
	switch (type) {
	case DTYPE_TOWN:
		return CutTown;
	case DTYPE_CATHEDRAL:
		return CutLevel1;
	case DTYPE_CATACOMBS:
		return CutLevel2;
	case DTYPE_CAVES:
		return CutLevel3;
	case DTYPE_HELL:
		return CutLevel4;
	case DTYPE_NEST:
		return CutLevel6;
	case DTYPE_CRYPT:
		return CutLevel5;
	default:
		return CutLevel1;
	}
}

Cutscenes PickCutscene(interface_mode uMsg)
{
	switch (uMsg) {
	case WM_DIABLOADGAME:
	case WM_DIABNEWGAME:
		return CutStart;
	case WM_DIABRETOWN:
		return CutTown;
	case WM_DIABNEXTLVL:
	case WM_DIABPREVLVL:
	case WM_DIABTOWNWARP:
	case WM_DIABTWARPUP: {
		int lvl = MyPlayer->plrlevel;
		if (lvl == 1 && uMsg == WM_DIABNEXTLVL)
			return CutTown;
		if (lvl == 16 && uMsg == WM_DIABNEXTLVL)
			return CutGate;
		return GetCutSceneFromLevelType(GetLevelType(lvl));
	}
	case WM_DIABWARPLVL:
		return CutPortal;
	case WM_DIABSETLVL:
	case WM_DIABRTNLVL:
		if (setlvlnum == SL_BONECHAMB)
			return CutLevel2;
		if (setlvlnum == SL_VILEBETRAYER)
			return CutPortalRed;
		if (IsArenaLevel(setlvlnum)) {
			if (uMsg == WM_DIABSETLVL)
				return GetCutSceneFromLevelType(setlvltype);
			return CutTown;
		}
		return CutLevel1;
	default:
		app_fatal("Unknown progress mode");
	}
}

void LoadCutsceneBackground(interface_mode uMsg)
{
	const char *celPath;
	const char *palPath;

	switch (PickCutscene(uMsg)) {
	case CutStart:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutstartw.clx");
		celPath = "gendata\\cutstart";
		palPath = "gendata\\cutstart.pal";
		progress_id = 1;
		break;
	case CutTown:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutttw.clx");
		celPath = "gendata\\cuttt";
		palPath = "gendata\\cuttt.pal";
		progress_id = 1;
		break;
	case CutLevel1:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutl1dw.clx");
		celPath = "gendata\\cutl1d";
		palPath = "gendata\\cutl1d.pal";
		progress_id = 0;
		break;
	case CutLevel2:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut2w.clx");
		celPath = "gendata\\cut2";
		palPath = "gendata\\cut2.pal";
		progress_id = 2;
		break;
	case CutLevel3:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut3w.clx");
		celPath = "gendata\\cut3";
		palPath = "gendata\\cut3.pal";
		progress_id = 1;
		break;
	case CutLevel4:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cut4w.clx");
		celPath = "gendata\\cut4";
		palPath = "gendata\\cut4.pal";
		progress_id = 1;
		break;
	case CutLevel5:
		ArtCutsceneWidescreen = LoadOptionalClx("nlevels\\cutl5w.clx");
		celPath = "nlevels\\cutl5";
		palPath = "nlevels\\cutl5.pal";
		progress_id = 1;
		break;
	case CutLevel6:
		ArtCutsceneWidescreen = LoadOptionalClx("nlevels\\cutl6w.clx");
		celPath = "nlevels\\cutl6";
		palPath = "nlevels\\cutl6.pal";
		progress_id = 1;
		break;
	case CutPortal:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutportlw.clx");
		celPath = "gendata\\cutportl";
		palPath = "gendata\\cutportl.pal";
		progress_id = 1;
		break;
	case CutPortalRed:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutportrw.clx");
		celPath = "gendata\\cutportr";
		palPath = "gendata\\cutportr.pal";
		progress_id = 1;
		break;
	case CutGate:
		ArtCutsceneWidescreen = LoadOptionalClx("gendata\\cutgatew.clx");
		celPath = "gendata\\cutgate";
		palPath = "gendata\\cutgate.pal";
		progress_id = 1;
		break;
	}

	assert(!sgpBackCel);
	sgpBackCel = LoadCel(celPath, 640);
	LoadPalette(palPath);

	sgdwProgress = 0;
}

void FreeCutsceneBackground()
{
	sgpBackCel = std::nullopt;
	ArtCutsceneWidescreen = std::nullopt;
}

void DrawCutsceneBackground()
{
	const Rectangle &uiRectangle = GetUIRectangle();
	const Surface &out = GlobalBackBuffer();
	SDL_FillRect(out.surface, nullptr, 0x000000);
	if (ArtCutsceneWidescreen) {
		const ClxSprite sprite = (*ArtCutsceneWidescreen)[0];
		RenderClxSprite(out, sprite, { uiRectangle.position.x - (sprite.width() - uiRectangle.size.width) / 2, uiRectangle.position.y });
	}
	ClxDraw(out, { uiRectangle.position.x, 480 - 1 + uiRectangle.position.y }, (*sgpBackCel)[0]);
}

void DrawCutsceneForeground()
{
	const Rectangle &uiRectangle = GetUIRectangle();
	const Surface &out = GlobalBackBuffer();
	constexpr int ProgressHeight = 22;
	SDL_Rect rect = MakeSdlRect(
	    out.region.x + BarPos[progress_id][0] + uiRectangle.position.x,
	    out.region.y + BarPos[progress_id][1] + uiRectangle.position.y,
	    sgdwProgress,
	    ProgressHeight);
	SDL_FillRect(out.surface, &rect, BarColor[progress_id]);

	if (DiabloUiSurface() == PalSurface)
		BltFast(&rect, &rect);
	RenderPresent();
}

void DoLoad(interface_mode uMsg)
{
	IncProgress();
	sound_init();
	IncProgress();

	Player &myPlayer = *MyPlayer;
	tl::expected<void, std::string> loadResult;
	switch (uMsg) {
	case WM_DIABLOADGAME:
		IncProgress(2);
		loadResult = LoadGame(true);
		if (loadResult.has_value()) IncProgress(2);
		break;
	case WM_DIABNEWGAME:
		myPlayer.pOriginalCathedral = !gbIsHellfire;
		IncProgress();
		FreeGameMem();
		IncProgress();
		pfile_remove_temp_files();
		IncProgress();
		loadResult = LoadGameLevel(true, ENTRY_MAIN);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABNEXTLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_MAIN);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABPREVLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		currlevel--;
		leveltype = GetLevelType(currlevel);
		assert(myPlayer.isOnActiveLevel());
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_PREV);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABSETLVL:
		// Note: ReturnLevel, ReturnLevelType and ReturnLvlPosition is only set to ensure vanilla compatibility
		ReturnLevel = GetMapReturnLevel();
		ReturnLevelType = GetLevelType(ReturnLevel);
		ReturnLvlPosition = GetMapReturnPosition();
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		setlevel = true;
		leveltype = setlvltype;
		currlevel = static_cast<uint8_t>(setlvlnum);
		FreeGameMem();
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_SETLVL);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABRTNLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		setlevel = false;
		FreeGameMem();
		IncProgress();
		currlevel = GetMapReturnLevel();
		leveltype = GetLevelType(currlevel);
		loadResult = LoadGameLevel(false, ENTRY_RTNLVL);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABWARPLVL:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		GetPortalLevel();
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_WARPLVL);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABTOWNWARP:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_TWARPDN);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABTWARPUP:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_TWARPUP);
		if (loadResult.has_value()) IncProgress();
		break;
	case WM_DIABRETOWN:
		IncProgress();
		if (!gbIsMultiplayer) {
			pfile_save_level();
		} else {
			DeltaSaveLevel();
		}
		IncProgress();
		FreeGameMem();
		setlevel = false;
		currlevel = myPlayer.plrlevel;
		leveltype = GetLevelType(currlevel);
		IncProgress();
		loadResult = LoadGameLevel(false, ENTRY_MAIN);
		if (loadResult.has_value()) IncProgress();
		break;
	default:
		loadResult = tl::make_unexpected<std::string>("Unknown progress mode");
		break;
	}

	if (!loadResult.has_value()) {
#if SDL_PUSH_EVENT_BG_THREAD_WORKS
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_ERROR);
		event.user.data1 = new std::string(std::move(loadResult).error());
		if (SDL_PushEvent(&event) < 0) {
			LogError("Failed to send WM_ERROR {}", SDL_GetError());
			SDL_ClearError();
		}
#else
		NextCustomEvent.error = std::move(loadResult).error();
		NextCustomEvent.type = static_cast<int>(WM_ERROR);
#endif
		return;
	}

#if SDL_PUSH_EVENT_BG_THREAD_WORKS
	SDL_Event event;
	event.type = CustomEventToSdlEvent(WM_DONE);
	if (SDL_PushEvent(&event) < 0) {
		LogError("Failed to send WM_DONE {}", SDL_GetError());
		SDL_ClearError();
	}
#else
	NextCustomEvent.type = static_cast<int>(WM_DONE);
#endif
}

struct {
	uint32_t loadStartedAt;
	EventHandler prevHandler;
	bool skipRendering;
	bool done;
	uint32_t drawnProgress;
	std::array<SDL_Color, 256> palette;
} ProgressEventHandlerState;

void InitRendering()
{
	// Blit the background once and then free it.
	DrawCutsceneBackground();
	if (RenderDirectlyToOutputSurface && PalSurface != nullptr) {
		// Render into all the backbuffers if there are multiple.
		const void *initialPixels = PalSurface->pixels;
		if (DiabloUiSurface() == PalSurface)
			BltFast(nullptr, nullptr);
		RenderPresent();
		while (PalSurface->pixels != initialPixels) {
			DrawCutsceneBackground();
			if (DiabloUiSurface() == PalSurface)
				BltFast(nullptr, nullptr);
			RenderPresent();
		}
	}
	FreeCutsceneBackground();

	// The loading thread sets `orig_palette`, so we make sure to use
	// our own palette for the fade-in.
	PaletteFadeIn(8, ProgressEventHandlerState.palette);
}

void CheckShouldSkipRendering()
{
	if (!ProgressEventHandlerState.skipRendering) return;
	const bool shouldSkip = ProgressEventHandlerState.loadStartedAt + *GetOptions().Gameplay.skipLoadingScreenThresholdMs > SDL_GetTicks();
	if (shouldSkip) return;
	ProgressEventHandlerState.skipRendering = false;
	if (!HeadlessMode) InitRendering();
}

void ProgressEventHandler(const SDL_Event &event, uint16_t modState)
{
	DisableInputEventHandler(event, modState);
	if (!IsCustomEvent(event.type)) return;

	const interface_mode customEvent = GetCustomEvent(event.type);
	switch (customEvent) {
	case WM_PROGRESS:
		if (!HeadlessMode && ProgressEventHandlerState.drawnProgress != sgdwProgress && !ProgressEventHandlerState.skipRendering) {
			DrawCutsceneForeground();
			ProgressEventHandlerState.drawnProgress = sgdwProgress;
		}
		break;
	case WM_ERROR:
		app_fatal(*static_cast<std::string *>(event.user.data1));
		break;
	case WM_DONE: {
		if (!ProgressEventHandlerState.skipRendering) {
			NewCursor(CURSOR_HAND);

			if (!HeadlessMode) {
				assert(ghMainWnd);

				if (RenderDirectlyToOutputSurface && PalSurface != nullptr) {
					// The loading thread sets `orig_palette`, so we make sure to use
					// our own palette for drawing the foreground.
					ApplyToneMapping(logical_palette, ProgressEventHandlerState.palette, 256);

					// Ensure that all back buffers have the full progress bar.
					const void *initialPixels = PalSurface->pixels;
					do {
						DrawCutsceneForeground();
						if (DiabloUiSurface() == PalSurface)
							BltFast(nullptr, nullptr);
						RenderPresent();
					} while (PalSurface->pixels != initialPixels);
				}

				// The loading thread sets `orig_palette`, so we make sure to use
				// our own palette for the fade-out.
				PaletteFadeOut(8, ProgressEventHandlerState.palette);
			}
		}

		[[maybe_unused]] EventHandler prevHandler = SetEventHandler(ProgressEventHandlerState.prevHandler);
		assert(prevHandler == ProgressEventHandler);
		ProgressEventHandlerState.prevHandler = nullptr;
		IsProgress = false;

		Player &myPlayer = *MyPlayer;
		NetSendCmdLocParam2(true, CMD_PLAYER_JOINLEVEL, myPlayer.position.tile, myPlayer.plrlevel, myPlayer.plrIsOnSetLevel ? 1 : 0);
		DelayPlrMessages(SDL_GetTicks() - ProgressEventHandlerState.loadStartedAt);

		if (gbSomebodyWonGameKludge && myPlayer.isOnLevel(16)) {
			PrepDoEnding();
		}

		gbSomebodyWonGameKludge = false;
		ProgressEventHandlerState.done = true;

#if !defined(USE_SDL1) && !defined(__vita__)
		if (renderer != nullptr) {
			InitVirtualGamepadTextures(*renderer);
		}
#endif
	} break;
	default:
		app_fatal("Unknown progress mode");
		break;
	}
}

} // namespace

void RegisterCustomEvents()
{
#ifndef USE_SDL1
	CustomEventsBegin = SDL_RegisterEvents(NumCustomEvents);
#endif
}

bool IsCustomEvent(SdlEventType eventType)
{
	return eventType >= CustomEventsBegin && eventType < CustomEventsBegin + NumCustomEvents;
}

interface_mode GetCustomEvent(SdlEventType eventType)
{
	return static_cast<interface_mode>(eventType - CustomEventsBegin);
}

SdlEventType CustomEventToSdlEvent(interface_mode eventType)
{
	return CustomEventsBegin + eventType;
}

void interface_msg_pump()
{
	SDL_Event event;
	uint16_t modState;
	while (FetchMessage(&event, &modState)) {
		if (event.type != SDL_QUIT) {
			HandleMessage(event, modState);
		}
	}
}

void IncProgress(uint32_t steps)
{
	if (!IsProgress)
		return;
	const uint32_t prevProgress = sgdwProgress;
	sgdwProgress += ProgressStepSize * steps;
	if (sgdwProgress > MaxProgress)
		sgdwProgress = MaxProgress;
	if (!HeadlessMode && sgdwProgress != prevProgress) {
#if SDL_PUSH_EVENT_BG_THREAD_WORKS
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_PROGRESS);
		if (SDL_PushEvent(&event) < 0) {
			LogError("Failed to send WM_PROGRESS {}", SDL_GetError());
			SDL_ClearError();
		}
#else
		NextCustomEvent.type = static_cast<int>(WM_PROGRESS);
#endif
	}
}

void CompleteProgress()
{
	if (HeadlessMode)
		return;
	if (!IsProgress)
		return;
	if (sgdwProgress < MaxProgress) {
		IncProgress((MaxProgress - sgdwProgress) / ProgressStepSize);
	}
}

void ShowProgress(interface_mode uMsg)
{
	IsProgress = true;
	gbSomebodyWonGameKludge = false;

	ProgressEventHandlerState.loadStartedAt = SDL_GetTicks();
	ProgressEventHandlerState.prevHandler = SetEventHandler(ProgressEventHandler);
	ProgressEventHandlerState.skipRendering = true;
	ProgressEventHandlerState.done = false;
	ProgressEventHandlerState.drawnProgress = 0;

#if !SDL_PUSH_EVENT_BG_THREAD_WORKS
	NextCustomEvent.type = -1;
#endif

#ifndef USE_SDL1
	DeactivateVirtualGamepad();
	FreeVirtualGamepadTextures();
#endif

	if (!HeadlessMode) {
		assert(ghMainWnd);

		interface_msg_pump();
		ClearScreenBuffer();
		scrollrt_draw_game_screen();

		if (IsHardwareCursor())
			SetHardwareCursorVisible(false);

		BlackPalette();

		// Always load the background (even if we end up skipping rendering it).
		// This is because the MPQ archive can only be read by a single thread at a time.
		LoadCutsceneBackground(uMsg);

		// Save the palette at this point because the loading process may replace it.
		ProgressEventHandlerState.palette = orig_palette;
	}

	// Begin loading
	static interface_mode loadTarget;
	loadTarget = uMsg;
	SdlThread loadThread = SdlThread([]() {
		const uint32_t start = SDL_GetTicks();
		DoLoad(loadTarget);
		LogVerbose("Load thread finished in {}ms", SDL_GetTicks() - start);
	});

	const auto processEvent = [&](const SDL_Event &event) {
		CheckShouldSkipRendering();
		if (event.type != SDL_QUIT) {
			HandleMessage(event, SDL_GetModState());
		}
		if (ProgressEventHandlerState.done) {
			loadThread.join();
			return false;
		}
		return true;
	};

	while (true) {
		CheckShouldSkipRendering();
		SDL_Event event;
		// We use the real `PollEvent` here instead of `FetchMessage`
		// to process real events rather than the recorded ones in demo mode.
		while (PollEvent(&event)) {
			if (!processEvent(event)) return;
		}
#if !SDL_PUSH_EVENT_BG_THREAD_WORKS
		if (const int customEventType = NextCustomEvent.type.exchange(-1); customEventType != -1) {
			event.type = CustomEventToSdlEvent(static_cast<interface_mode>(customEventType));
			if (static_cast<interface_mode>(customEventType) == static_cast<int>(WM_ERROR)) {
				event.user.data1 = &NextCustomEvent.error;
			}
			if (!processEvent(event)) return;
		}
#endif
	}
}

} // namespace devilution
