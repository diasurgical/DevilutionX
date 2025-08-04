/**
 * @file diablo.cpp
 *
 * Implementation of the main game initialization functions.
 */
#include <array>
#include <cstdint>
#include <string_view>

#include <fmt/format.h>

#include <config.h>

#include "DiabloUI/selstart.h"
#include "appfat.h"
#include "automap.h"
#include "capture.h"
#include "control.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "DiabloUI/diabloui.h"
#include "controls/control_mode.hpp"
#include "controls/keymapper.hpp"
#include "controls/plrctrls.h"
#include "controls/remap_keyboard.h"
#include "diablo.h"
#include "diablo_msg.hpp"
#include "discord/discord.h"
#include "doom.h"
#include "encrypt.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/demomode.h"
#include "engine/dx.h"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/sound.h"
#include "game_mode.hpp"
#include "gamemenu.h"
#include "gmenu.h"
#include "headless_mode.hpp"
#include "help.h"
#include "hwcursor.hpp"
#include "init.hpp"
#include "inv.h"
#include "levels/drlg_l1.h"
#include "levels/drlg_l2.h"
#include "levels/drlg_l3.h"
#include "levels/drlg_l4.h"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "levels/themes.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "loadsave.h"
#include "lua/lua_global.hpp"
#include "menu.h"
#include "minitext.h"
#include "missiles.h"
#include "monstdat.h"
#include "movie.h"
#include "multi.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "panels/console.hpp"
#include "panels/info_box.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "playerdat.hpp"
#include "plrmsg.h"
#include "qol/chatlog.h"
#include "qol/floatingnumbers.h"
#include "qol/itemlabels.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "quick_messages.hpp"
#include "restrict.h"
#include "stores.h"
#include "storm/storm_net.hpp"
#include "storm/storm_svid.h"
#include "towners.h"
#include "track.h"
#include "utils/console.h"
#include "utils/display.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/parse_int.hpp"
#include "utils/paths.h"
#include "utils/screen_reader.hpp"
#include "utils/sdl_thread.h"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

#ifndef USE_SDL1
#include "controls/touch/gamepad.h"
#include "controls/touch/renderers.h"
#endif

#ifdef __vita__
#include "platform/vita/touch.h"
#endif

#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
#include <gperftools/heap-profiler.h>
#endif

namespace devilution {

uint32_t DungeonSeeds[NUMLEVELS];
std::optional<uint32_t> LevelSeeds[NUMLEVELS];
Point MousePosition;
bool gbRunGameResult;
bool ReturnToMainMenu;
/** Enable updating of player character, set to false once Diablo dies */
bool gbProcessPlayers;
bool gbLoadGame;
bool cineflag;
int PauseMode;
clicktype sgbMouseDown;
uint16_t gnTickDelay = 50;
char gszProductName[64] = "DevilutionX vUnknown";

#ifdef _DEBUG
bool DebugDisableNetworkTimeout = false;
std::vector<std::string> DebugCmdsFromCommandLine;
#endif
GameLogicStep gGameLogicStep = GameLogicStep::None;

/** This and the following mouse variables are for handling in-game click-and-hold actions */
MouseActionType LastMouseButtonAction = MouseActionType::None;

// Controller support: Actions to run after updating the cursor state.
// Defined in SourceX/controls/plctrls.cpp.
extern void plrctrls_after_check_curs_move();
extern void plrctrls_every_frame();
extern void plrctrls_after_game_logic();

namespace {

char gszVersionNumber[64] = "internal version unknown";

bool gbGameLoopStartup;
bool forceSpawn;
bool forceDiablo;
int sgnTimeoutCurs;
bool gbShowIntro = true;
/** To know if these things have been done when we get to the diablo_deinit() function */
bool was_archives_init = false;
/** To know if surfaces have been initialized or not */
bool was_window_init = false;
bool was_ui_init = false;

void StartGame(interface_mode uMsg)
{
	CalcViewportGeometry();
	cineflag = false;
	InitCursor();
#ifdef _DEBUG
	LoadDebugGFX();
#endif
	assert(HeadlessMode || ghMainWnd);
	music_stop();
	InitMonsterHealthBar();
	InitXPBar();
	ShowProgress(uMsg);
	gmenu_init_menu();
	InitLevelCursor();
	sgnTimeoutCurs = CURSOR_NONE;
	sgbMouseDown = CLICK_NONE;
	LastMouseButtonAction = MouseActionType::None;
}

void FreeGame()
{
	FreeMonsterHealthBar();
	FreeXPBar();
	FreeControlPan();
	FreeInvGFX();
	FreeGMenu();
	FreeQuestText();
	FreeInfoBoxGfx();
	FreeStoreMem();

	for (Player &player : Players)
		ResetPlayerGFX(player);

	FreeCursor();
#ifdef _DEBUG
	FreeDebugGFX();
#endif
	FreeGameMem();
	stream_stop();
	music_stop();
}

bool ProcessInput()
{
	if (PauseMode == 2) {
		return false;
	}

	plrctrls_every_frame();

	if (!gbIsMultiplayer && gmenu_is_active()) {
		RedrawViewport();
		return false;
	}

	if (!gmenu_is_active() && sgnTimeoutCurs == CURSOR_NONE) {
#ifdef __vita__
		FinishSimulatedMouseClicks(MousePosition);
#endif
		CheckCursMove();
		plrctrls_after_check_curs_move();
		RepeatMouseAction();
	}

	return true;
}

void LeftMouseCmd(bool bShift)
{
	bool bNear;

	assert(!GetMainPanel().contains(MousePosition));

	if (leveltype == DTYPE_TOWN) {
		CloseGoldWithdraw();
		CloseStash();
		if (pcursitem != -1 && pcurs == CURSOR_HAND)
			NetSendCmdLocParam1(true, invflag ? CMD_GOTOGETITEM : CMD_GOTOAGETITEM, cursPosition, pcursitem);
		if (pcursmonst != -1)
			NetSendCmdLocParam1(true, CMD_TALKXY, cursPosition, pcursmonst);
		if (pcursitem == -1 && pcursmonst == -1 && PlayerUnderCursor == nullptr) {
			LastMouseButtonAction = MouseActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
		}
		return;
	}

	Player &myPlayer = *MyPlayer;
	bNear = myPlayer.position.tile.WalkingDistance(cursPosition) < 2;
	if (pcursitem != -1 && pcurs == CURSOR_HAND && !bShift) {
		NetSendCmdLocParam1(true, invflag ? CMD_GOTOGETITEM : CMD_GOTOAGETITEM, cursPosition, pcursitem);
	} else if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled() && (!bShift || (bNear && ObjectUnderCursor->_oBreak == 1))) {
		LastMouseButtonAction = MouseActionType::OperateObject;
		NetSendCmdLoc(MyPlayerId, true, pcurs == CURSOR_DISARM ? CMD_DISARMXY : CMD_OPOBJXY, cursPosition);
	} else if (myPlayer.UsesRangedWeapon()) {
		if (bShift) {
			LastMouseButtonAction = MouseActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_RATTACKXY, cursPosition);
		} else if (pcursmonst != -1) {
			if (CanTalkToMonst(Monsters[pcursmonst])) {
				NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
			} else {
				LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
				NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
			}
		} else if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode) {
			LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_RATTACKPID, PlayerUnderCursor->getId());
		}
	} else {
		if (bShift) {
			if (pcursmonst != -1) {
				if (CanTalkToMonst(Monsters[pcursmonst])) {
					NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
				} else {
					LastMouseButtonAction = MouseActionType::Attack;
					NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
				}
			} else {
				LastMouseButtonAction = MouseActionType::Attack;
				NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			}
		} else if (pcursmonst != -1) {
			LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode) {
			LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_ATTACKPID, PlayerUnderCursor->getId());
		}
	}
	if (!bShift && pcursitem == -1 && ObjectUnderCursor == nullptr && pcursmonst == -1 && PlayerUnderCursor == nullptr) {
		LastMouseButtonAction = MouseActionType::Walk;
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
	}
}

bool TryOpenDungeonWithMouse()
{
	if (leveltype != DTYPE_TOWN)
		return false;

	Item &holdItem = MyPlayer->HoldItem;
	if (holdItem.IDidx == IDI_RUNEBOMB && OpensHive(cursPosition))
		OpenHive();
	else if (holdItem.IDidx == IDI_MAPOFDOOM && OpensGrave(cursPosition))
		OpenGrave();
	else
		return false;

	NewCursor(CURSOR_HAND);
	return true;
}

void LeftMouseDown(uint16_t modState)
{
	LastMouseButtonAction = MouseActionType::None;

	if (gmenu_left_mouse(true))
		return;

	if (CheckMuteButton())
		return;

	if (sgnTimeoutCurs != CURSOR_NONE)
		return;

	if (MyPlayerIsDead) {
		CheckMainPanelButtonDead();
		return;
	}

	if (PauseMode == 2) {
		return;
	}
	if (DoomFlag) {
		doom_close();
		return;
	}

	if (SpellSelectFlag) {
		SetSpell();
		return;
	}

	if (IsPlayerInStore()) {
		CheckStoreBtn();
		return;
	}

	const bool isShiftHeld = (modState & KMOD_SHIFT) != 0;
	const bool isCtrlHeld = (modState & KMOD_CTRL) != 0;

	if (!GetMainPanel().contains(MousePosition)) {
		if (!gmenu_is_active() && !TryIconCurs()) {
			if (QuestLogIsOpen && GetLeftPanel().contains(MousePosition)) {
				QuestlogESC();
			} else if (qtextflag) {
				qtextflag = false;
				stream_stop();
			} else if (CharFlag && GetLeftPanel().contains(MousePosition)) {
				CheckChrBtns();
			} else if (invflag && GetRightPanel().contains(MousePosition)) {
				if (!DropGoldFlag)
					CheckInvItem(isShiftHeld, isCtrlHeld);
			} else if (IsStashOpen && GetLeftPanel().contains(MousePosition)) {
				if (!IsWithdrawGoldOpen)
					CheckStashItem(MousePosition, isShiftHeld, isCtrlHeld);
				CheckStashButtonPress(MousePosition);
			} else if (SpellbookFlag && GetRightPanel().contains(MousePosition)) {
				CheckSBook();
			} else if (!MyPlayer->HoldItem.isEmpty()) {
				if (!TryOpenDungeonWithMouse()) {
					Point currentPosition = MyPlayer->position.tile;
					std::optional<Point> itemTile = FindAdjacentPositionForItem(currentPosition, GetDirection(currentPosition, cursPosition));
					if (itemTile) {
						NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, MyPlayer->HoldItem);
						NewCursor(CURSOR_HAND);
					}
				}
			} else {
				CheckLevelButton();
				if (!LevelButtonDown)
					LeftMouseCmd(isShiftHeld);
			}
		}
	} else {
		if (!ChatFlag && !DropGoldFlag && !IsWithdrawGoldOpen && !gmenu_is_active())
			CheckInvScrn(isShiftHeld, isCtrlHeld);
		CheckMainPanelButton();
		CheckStashButtonPress(MousePosition);
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM)
			NewCursor(CURSOR_HAND);
	}
}

void LeftMouseUp(uint16_t modState)
{
	gmenu_left_mouse(false);
	CheckMuteButtonUp();
	if (MainPanelButtonDown)
		CheckMainPanelButtonUp();
	CheckStashButtonRelease(MousePosition);
	if (CharPanelButtonActive) {
		const bool isShiftHeld = (modState & KMOD_SHIFT) != 0;
		ReleaseChrBtns(isShiftHeld);
	}
	if (LevelButtonDown)
		CheckLevelButtonUp();
	if (IsPlayerInStore())
		ReleaseStoreBtn();
}

void RightMouseDown(bool isShiftHeld)
{
	LastMouseButtonAction = MouseActionType::None;

	if (gmenu_is_active() || sgnTimeoutCurs != CURSOR_NONE || PauseMode == 2 || MyPlayer->_pInvincible) {
		return;
	}

	if (qtextflag) {
		qtextflag = false;
		stream_stop();
		return;
	}

	if (DoomFlag) {
		doom_close();
		return;
	}
	if (IsPlayerInStore())
		return;
	if (SpellSelectFlag) {
		SetSpell();
		return;
	}
	if (SpellbookFlag && GetRightPanel().contains(MousePosition))
		return;
	if (TryIconCurs())
		return;
	if (pcursinvitem != -1 && UseInvItem(pcursinvitem))
		return;
	if (pcursstashitem != StashStruct::EmptyCell && UseStashItem(pcursstashitem))
		return;
	if (pcurs == CURSOR_HAND) {
		CheckPlrSpell(isShiftHeld);
	} else if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
		NewCursor(CURSOR_HAND);
	}
}

void ReleaseKey(SDL_Keycode vkey)
{
	remap_keyboard_key(&vkey);
	if (sgnTimeoutCurs != CURSOR_NONE)
		return;
	KeymapperRelease(vkey);
}

void ClosePanels()
{
	if (CanPanelsCoverView()) {
		if (!IsLeftPanelOpen() && IsRightPanelOpen() && MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
			SetCursorPos(MousePosition + Displacement { 160, 0 });
		} else if (!IsRightPanelOpen() && IsLeftPanelOpen() && MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
			SetCursorPos(MousePosition - Displacement { 160, 0 });
		}
	}
	CloseInventory();
	CloseCharPanel();
	SpellbookFlag = false;
	QuestLogIsOpen = false;
}

void PressKey(SDL_Keycode vkey, uint16_t modState)
{
	Options &options = GetOptions();
	remap_keyboard_key(&vkey);

	if (vkey == SDLK_UNKNOWN)
		return;

	if (gmenu_presskeys(vkey) || CheckKeypress(vkey)) {
		return;
	}

	if (MyPlayerIsDead) {
		if (vkey == SDLK_ESCAPE) {
			if (!gbIsMultiplayer) {
				if (gbValidSaveFile)
					gamemenu_load_game(false);
				else
					gamemenu_exit_game(false);
			} else {
				NetSendCmd(true, CMD_RETOWN);
			}
			return;
		}
		if (sgnTimeoutCurs != CURSOR_NONE) {
			return;
		}
		KeymapperPress(vkey);
		if (vkey == SDLK_RETURN || vkey == SDLK_KP_ENTER) {
			if ((modState & KMOD_ALT) != 0) {
				options.Graphics.fullscreen.SetValue(!IsFullScreen());
				if (!demo::IsRunning()) SaveOptions();
			} else {
				TypeChatMessage();
			}
		}
		if (vkey != SDLK_ESCAPE) {
			return;
		}
	}
	// Disallow player from accessing escape menu during the frames before the death message appears
	if (vkey == SDLK_ESCAPE && MyPlayer->_pHitPoints > 0) {
		if (!PressEscKey()) {
			LastMouseButtonAction = MouseActionType::None;
			gamemenu_on();
		}
		return;
	}

	if (DropGoldFlag) {
		control_drop_gold(vkey);
		return;
	}
	if (IsWithdrawGoldOpen) {
		WithdrawGoldKeyPress(vkey);
		return;
	}

	if (sgnTimeoutCurs != CURSOR_NONE) {
		return;
	}

	KeymapperPress(vkey);

	if (PauseMode == 2) {
		if ((vkey == SDLK_RETURN || vkey == SDLK_KP_ENTER) && (modState & KMOD_ALT) != 0) {
			options.Graphics.fullscreen.SetValue(!IsFullScreen());
			if (!demo::IsRunning()) SaveOptions();
		}
		return;
	}

	if (DoomFlag) {
		doom_close();
		return;
	}

	switch (vkey) {
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
	case SDLK_EQUALS:
	case SDLK_KP_EQUALS:
		if (AutomapActive) {
			AutomapZoomIn();
		}
		return;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
	case SDLK_UNDERSCORE:
		if (AutomapActive) {
			AutomapZoomOut();
		}
		return;
#ifdef _DEBUG
	case SDLK_v:
		if ((modState & KMOD_SHIFT) != 0)
			NextDebugMonster();
		else
			GetDebugMonster();
		return;
#endif
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if ((modState & KMOD_ALT) != 0) {
			options.Graphics.fullscreen.SetValue(!IsFullScreen());
			if (!demo::IsRunning()) SaveOptions();
		} else if (IsPlayerInStore()) {
			StoreEnter();
		} else if (QuestLogIsOpen) {
			QuestlogEnter();
		} else {
			TypeChatMessage();
		}
		return;
	case SDLK_UP:
		if (IsPlayerInStore()) {
			StoreUp();
		} else if (QuestLogIsOpen) {
			QuestlogUp();
		} else if (HelpFlag) {
			HelpScrollUp();
		} else if (ChatLogFlag) {
			ChatLogScrollUp();
		} else if (AutomapActive) {
			AutomapUp();
		} else if (IsStashOpen) {
			Stash.PreviousPage();
		}
		return;
	case SDLK_DOWN:
		if (IsPlayerInStore()) {
			StoreDown();
		} else if (QuestLogIsOpen) {
			QuestlogDown();
		} else if (HelpFlag) {
			HelpScrollDown();
		} else if (ChatLogFlag) {
			ChatLogScrollDown();
		} else if (AutomapActive) {
			AutomapDown();
		} else if (IsStashOpen) {
			Stash.NextPage();
		}
		return;
	case SDLK_PAGEUP:
		if (IsPlayerInStore()) {
			StorePrior();
		} else if (ChatLogFlag) {
			ChatLogScrollTop();
		}
		return;
	case SDLK_PAGEDOWN:
		if (IsPlayerInStore()) {
			StoreNext();
		} else if (ChatLogFlag) {
			ChatLogScrollBottom();
		}
		return;
	case SDLK_LEFT:
		if (AutomapActive && !ChatFlag)
			AutomapLeft();
		return;
	case SDLK_RIGHT:
		if (AutomapActive && !ChatFlag)
			AutomapRight();
		return;
	default:
		break;
	}
}

void HandleMouseButtonDown(Uint8 button, uint16_t modState)
{
	if (IsPlayerInStore() && (button == SDL_BUTTON_X1
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	        || button == 8
#endif
	        )) {
		StoreESC();
		return;
	}

	switch (button) {
	case SDL_BUTTON_LEFT:
		if (sgbMouseDown == CLICK_NONE) {
			sgbMouseDown = CLICK_LEFT;
			LeftMouseDown(modState);
		}
		break;
	case SDL_BUTTON_RIGHT:
		if (sgbMouseDown == CLICK_NONE) {
			sgbMouseDown = CLICK_RIGHT;
			RightMouseDown((modState & KMOD_SHIFT) != 0);
		}
		break;
	default:
		KeymapperPress(static_cast<SDL_Keycode>(button | KeymapperMouseButtonMask));
		break;
	}
}

void HandleMouseButtonUp(Uint8 button, uint16_t modState)
{
	if (sgbMouseDown == CLICK_LEFT && button == SDL_BUTTON_LEFT) {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(modState);
	} else if (sgbMouseDown == CLICK_RIGHT && button == SDL_BUTTON_RIGHT) {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
	} else {
		KeymapperRelease(static_cast<SDL_Keycode>(button | KeymapperMouseButtonMask));
	}
}

[[maybe_unused]] void LogUnhandledEvent(const char *name, int value)
{
	LogVerbose("Unhandled SDL event: {} {}", name, value);
}

void PrepareForFadeIn()
{
	if (HeadlessMode) return;
	BlackPalette();

	// Render the game to the buffer(s) with a fully black palette.
	// Palette fade-in will gradually make it visible.
	RedrawEverything();
	while (IsRedrawEverything()) {
		DrawAndBlit();
	}
}

void GameEventHandler(const SDL_Event &event, uint16_t modState)
{
	[[maybe_unused]] Options &options = GetOptions();
	StaticVector<ControllerButtonEvent, 4> ctrlEvents = ToControllerButtonEvents(event);
	for (ControllerButtonEvent ctrlEvent : ctrlEvents) {
		GameAction action;
		if (HandleControllerButtonEvent(event, ctrlEvent, action) && action.type == GameActionType_SEND_KEY) {
			if ((action.send_key.vk_code & KeymapperMouseButtonMask) != 0) {
				const unsigned button = action.send_key.vk_code & ~KeymapperMouseButtonMask;
				if (!action.send_key.up)
					HandleMouseButtonDown(static_cast<Uint8>(button), modState);
				else
					HandleMouseButtonUp(static_cast<Uint8>(button), modState);
			} else {
				if (!action.send_key.up)
					PressKey(static_cast<SDL_Keycode>(action.send_key.vk_code), modState);
				else
					ReleaseKey(static_cast<SDL_Keycode>(action.send_key.vk_code));
			}
		}
	}
	if (ctrlEvents.size() > 0 && ctrlEvents[0].button != ControllerButton_NONE) {
		return;
	}

#ifdef _DEBUG
	if (ConsoleHandleEvent(event)) {
		return;
	}
#endif

	if (IsChatActive() && HandleTalkTextInputEvent(event)) {
		return;
	}
	if (DropGoldFlag && HandleGoldDropTextInputEvent(event)) {
		return;
	}
	if (IsWithdrawGoldOpen && HandleGoldWithdrawTextInputEvent(event)) {
		return;
	}

	switch (event.type) {
	case SDL_KEYDOWN:
		PressKey(event.key.keysym.sym, modState);
		return;
	case SDL_KEYUP:
		ReleaseKey(event.key.keysym.sym);
		return;
	case SDL_MOUSEMOTION:
		if (ControlMode == ControlTypes::KeyboardAndMouse && invflag)
			InvalidateInventorySlot();
		MousePosition = { event.motion.x, event.motion.y };
		gmenu_on_mouse_move();
		return;
	case SDL_MOUSEBUTTONDOWN:
		MousePosition = { event.button.x, event.button.y };
		HandleMouseButtonDown(event.button.button, modState);
		return;
	case SDL_MOUSEBUTTONUP:
		MousePosition = { event.button.x, event.button.y };
		HandleMouseButtonUp(event.button.button, modState);
		return;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_MOUSEWHEEL:
		if (event.wheel.y > 0) { // Up
			if (IsPlayerInStore()) {
				StoreUp();
			} else if (QuestLogIsOpen) {
				QuestlogUp();
			} else if (HelpFlag) {
				HelpScrollUp();
			} else if (ChatLogFlag) {
				ChatLogScrollUp();
			} else if (IsStashOpen) {
				Stash.PreviousPage();
			} else if (SDL_GetModState() & KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomIn();
				}
			} else {
				KeymapperPress(MouseScrollUpButton);
			}
		} else if (event.wheel.y < 0) { // down
			if (IsPlayerInStore()) {
				StoreDown();
			} else if (QuestLogIsOpen) {
				QuestlogDown();
			} else if (HelpFlag) {
				HelpScrollDown();
			} else if (ChatLogFlag) {
				ChatLogScrollDown();
			} else if (IsStashOpen) {
				Stash.NextPage();
			} else if (SDL_GetModState() & KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomOut();
				}
			} else {
				KeymapperPress(MouseScrollDownButton);
			}
		} else if (event.wheel.x > 0) { // left
			KeymapperPress(MouseScrollLeftButton);
		} else if (event.wheel.x < 0) { // right
			KeymapperPress(MouseScrollRightButton);
		}
		break;
#endif
	default:
		if (IsCustomEvent(event.type)) {
			if (gbIsMultiplayer)
				pfile_write_hero();
			nthread_ignore_mutex(true);
			PaletteFadeOut(8);
			sound_stop();
			ShowProgress(GetCustomEvent(event));

			PrepareForFadeIn();
			LoadPWaterPalette();
			if (gbRunGame)
				PaletteFadeIn(8);
			nthread_ignore_mutex(false);
			gbGameLoopStartup = true;
			return;
		}
		MainWndProc(event);
		break;
	}
}

void RunGameLoop(interface_mode uMsg)
{
	demo::NotifyGameLoopStart();

	nthread_ignore_mutex(true);
	StartGame(uMsg);
	assert(HeadlessMode || ghMainWnd);
	EventHandler previousHandler = SetEventHandler(GameEventHandler);
	run_delta_info();
	gbRunGame = true;
	gbProcessPlayers = IsDiabloAlive(true);
	gbRunGameResult = true;

	PrepareForFadeIn();
	LoadPWaterPalette();
	PaletteFadeIn(8);
	InitBackbufferState();
	RedrawEverything();
	gbGameLoopStartup = true;
	nthread_ignore_mutex(false);

	discord_manager::StartGame();
	LuaEvent("GameStart");
#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
	unsigned run_game_iteration = 0;
#endif

	while (gbRunGame) {

#ifdef _DEBUG
		if (!gbGameLoopStartup && !DebugCmdsFromCommandLine.empty()) {
			InitConsole();
			for (const std::string &cmd : DebugCmdsFromCommandLine) {
				RunInConsole(cmd);
			}
			DebugCmdsFromCommandLine.clear();
		}
#endif

		SDL_Event event;
		uint16_t modState;
		while (FetchMessage(&event, &modState)) {
			if (event.type == SDL_QUIT) {
				gbRunGameResult = false;
				gbRunGame = false;
				break;
			}
			HandleMessage(event, modState);
		}
		if (!gbRunGame)
			break;

		bool drawGame = true;
		bool processInput = true;
		bool runGameLoop = demo::IsRunning() ? demo::GetRunGameLoop(drawGame, processInput) : nthread_has_500ms_passed(&drawGame);
		if (demo::IsRecording())
			demo::RecordGameLoopResult(runGameLoop);

		discord_manager::UpdateGame();

		if (!runGameLoop) {
			if (processInput)
				ProcessInput();
			if (!drawGame)
				continue;
			RedrawViewport();
			DrawAndBlit();
			continue;
		}

		multi_process_network_packets();
		if (game_loop(gbGameLoopStartup))
			diablo_color_cyc_logic();
		gbGameLoopStartup = false;
		if (drawGame)
			DrawAndBlit();
#ifdef GPERF_HEAP_FIRST_GAME_ITERATION
		if (run_game_iteration++ == 0)
			HeapProfilerDump("first_game_iteration");
#endif
	}

	demo::NotifyGameLoopEnd();

	if (gbIsMultiplayer) {
		pfile_write_hero(/*writeGameData=*/false);
		sfile_write_stash();
	}

	PaletteFadeOut(8);
	NewCursor(CURSOR_NONE);
	ClearScreenBuffer();
	RedrawEverything();
	scrollrt_draw_game_screen();
	previousHandler = SetEventHandler(previousHandler);
	assert(HeadlessMode || previousHandler == GameEventHandler);
	FreeGame();

	if (cineflag) {
		cineflag = false;
		DoEnding();
	}
}

void PrintWithRightPadding(std::string_view str, size_t width)
{
	printInConsole(str);
	if (str.size() >= width)
		return;
	printInConsole(std::string(width - str.size(), ' '));
}

void PrintHelpOption(std::string_view flags, std::string_view description)
{
	printInConsole("    ");
	PrintWithRightPadding(flags, 20);
	printInConsole(" ");
	PrintWithRightPadding(description, 30);
	printNewlineInConsole();
}

[[noreturn]] void PrintHelpAndExit()
{
	printInConsole((/* TRANSLATORS: Commandline Option */ "Options:"));
	printNewlineInConsole();
	PrintHelpOption("-h, --help", _(/* TRANSLATORS: Commandline Option */ "Print this message and exit"));
	PrintHelpOption("--version", _(/* TRANSLATORS: Commandline Option */ "Print the version and exit"));
	PrintHelpOption("--data-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the folder of diabdat.mpq"));
	PrintHelpOption("--save-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the folder of save files"));
	PrintHelpOption("--config-dir", _(/* TRANSLATORS: Commandline Option */ "Specify the location of diablo.ini"));
	PrintHelpOption("--lang", _(/* TRANSLATORS: Commandline Option */ "Specify the language code (e.g. en or pt_BR)"));
	PrintHelpOption("-n", _(/* TRANSLATORS: Commandline Option */ "Skip startup videos"));
	PrintHelpOption("-f", _(/* TRANSLATORS: Commandline Option */ "Display frames per second"));
	PrintHelpOption("--verbose", _(/* TRANSLATORS: Commandline Option */ "Enable verbose logging"));
#ifndef DISABLE_DEMOMODE
	PrintHelpOption("--record <#>", _(/* TRANSLATORS: Commandline Option */ "Record a demo file"));
	PrintHelpOption("--demo <#>", _(/* TRANSLATORS: Commandline Option */ "Play a demo file"));
	PrintHelpOption("--timedemo", _(/* TRANSLATORS: Commandline Option */ "Disable all frame limiting during demo playback"));
#endif
	printNewlineInConsole();
	printInConsole(_(/* TRANSLATORS: Commandline Option */ "Game selection:"));
	printNewlineInConsole();
	PrintHelpOption("--spawn", _(/* TRANSLATORS: Commandline Option */ "Force Shareware mode"));
	PrintHelpOption("--diablo", _(/* TRANSLATORS: Commandline Option */ "Force Diablo mode"));
	PrintHelpOption("--hellfire", _(/* TRANSLATORS: Commandline Option */ "Force Hellfire mode"));
	printInConsole(_(/* TRANSLATORS: Commandline Option */ "Hellfire options:"));
	printNewlineInConsole();
#ifdef _DEBUG
	printNewlineInConsole();
	printInConsole("Debug options:");
	printNewlineInConsole();
	PrintHelpOption("-i", "Ignore network timeout");
	PrintHelpOption("+<internal command>", "Pass commands to the engine");
#endif
	printNewlineInConsole();
	printInConsole(_("Report bugs at https://github.com/diasurgical/devilutionX/"));
	printNewlineInConsole();
	diablo_quit(0);
}

void PrintFlagMessage(std::string_view flag, std::string_view message)
{
	printInConsole(flag);
	printInConsole(message);
	printNewlineInConsole();
}

void PrintFlagRequiresArgument(std::string_view flag)
{
	PrintFlagMessage(flag, " requires an argument");
}

void DiabloParseFlags(int argc, char **argv)
{
#ifdef _DEBUG
	int argumentIndexOfLastCommandPart = -1;
	std::string currentCommand;
#endif
#ifndef DISABLE_DEMOMODE
	bool timedemo = false;
	int demoNumber = -1;
	int recordNumber = -1;
	bool createDemoReference = false;
#endif
	for (int i = 1; i < argc; i++) {
		const std::string_view arg = argv[i];
		if (arg == "-h" || arg == "--help") {
			PrintHelpAndExit();
		} else if (arg == "--version") {
			printInConsole(PROJECT_NAME);
			printInConsole(" v");
			printInConsole(PROJECT_VERSION);
			printNewlineInConsole();
			diablo_quit(0);
		} else if (arg == "--data-dir") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--data-dir");
				diablo_quit(64);
			}
			paths::SetBasePath(argv[++i]);
		} else if (arg == "--save-dir") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--save-dir");
				diablo_quit(64);
			}
			paths::SetPrefPath(argv[++i]);
		} else if (arg == "--config-dir") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--config-dir");
				diablo_quit(64);
			}
			paths::SetConfigPath(argv[++i]);
		} else if (arg == "--lang") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--lang");
				diablo_quit(64);
			}
			forceLocale = argv[++i];
#ifndef DISABLE_DEMOMODE
		} else if (arg == "--demo") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--demo");
				diablo_quit(64);
			}
			ParseIntResult<int> parsedParam = ParseInt<int>(argv[++i]);
			if (!parsedParam.has_value()) {
				PrintFlagMessage("--demo", " must be a number");
				diablo_quit(64);
			}
			demoNumber = parsedParam.value();
			gbShowIntro = false;
		} else if (arg == "--timedemo") {
			timedemo = true;
		} else if (arg == "--record") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--record");
				diablo_quit(64);
			}
			ParseIntResult<int> parsedParam = ParseInt<int>(argv[++i]);
			if (!parsedParam.has_value()) {
				PrintFlagMessage("--record", " must be a number");
				diablo_quit(64);
			}
			recordNumber = parsedParam.value();
		} else if (arg == "--create-reference") {
			createDemoReference = true;
#else
		} else if (arg == "--demo" || arg == "--timedemo" || arg == "--record" || arg == "--create-reference") {
			printInConsole("Binary compiled without demo mode support.");
			printNewlineInConsole();
			diablo_quit(1);
#endif
		} else if (arg == "-n") {
			gbShowIntro = false;
		} else if (arg == "-f") {
			EnableFrameCount();
		} else if (arg == "--spawn") {
			forceSpawn = true;
		} else if (arg == "--diablo") {
			forceDiablo = true;
		} else if (arg == "--hellfire") {
			forceHellfire = true;
		} else if (arg == "--vanilla") {
			gbVanilla = true;
		} else if (arg == "--verbose") {
			SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#ifdef _DEBUG
		} else if (arg == "-i") {
			DebugDisableNetworkTimeout = true;
		} else if (arg[0] == '+') {
			if (!currentCommand.empty())
				DebugCmdsFromCommandLine.push_back(currentCommand);
			argumentIndexOfLastCommandPart = i;
			currentCommand = arg.substr(1);
		} else if (arg[0] != '-' && (argumentIndexOfLastCommandPart + 1) == i) {
			currentCommand.append(" ");
			currentCommand.append(arg);
			argumentIndexOfLastCommandPart = i;
#endif
		} else {
			printInConsole("unrecognized option '");
			printInConsole(argv[i]);
			printInConsole("'");
			printNewlineInConsole();
			PrintHelpAndExit();
		}
	}

#ifdef _DEBUG
	if (!currentCommand.empty())
		DebugCmdsFromCommandLine.push_back(currentCommand);
#endif

#ifndef DISABLE_DEMOMODE
	if (demoNumber != -1)
		demo::InitPlayBack(demoNumber, timedemo);
	if (recordNumber != -1)
		demo::InitRecording(recordNumber, createDemoReference);
#endif
}

void DiabloInitScreen()
{
	MousePosition = { gnScreenWidth / 2, gnScreenHeight / 2 };
	if (ControlMode == ControlTypes::KeyboardAndMouse)
		SetCursorPos(MousePosition);

	ClrDiabloMsg();
}

void SetApplicationVersions()
{
	*BufCopy(gszProductName, PROJECT_NAME, " v", PROJECT_VERSION) = '\0';
	*BufCopy(gszVersionNumber, "version ", PROJECT_VERSION) = '\0';
}

void CheckArchivesUpToDate()
{
	const bool devilutionxMpqOutOfDate = IsDevilutionXMpqOutOfDate();
	const bool fontsMpqOutOfDate = AreExtraFontsOutOfDate();

	if (devilutionxMpqOutOfDate && fontsMpqOutOfDate) {
		app_fatal(_("Please update devilutionx.mpq and fonts.mpq to the latest version"));
	} else if (devilutionxMpqOutOfDate) {
		app_fatal(_("Failed to load UI resources.\n"
		            "\n"
		            "Make sure devilutionx.mpq is in the game folder and that it is up to date."));
	} else if (fontsMpqOutOfDate) {
		app_fatal(_("Please update fonts.mpq to the latest version"));
	}
}

void ApplicationInit()
{
	if (*GetOptions().Graphics.showFPS)
		EnableFrameCount();

	init_create_window();
	was_window_init = true;

	InitializeScreenReader();
	LanguageInitialize();

	SetApplicationVersions();

	ReadOnlyTest();
}

void DiabloInit()
{
	if (forceSpawn || *GetOptions().GameMode.shareware)
		gbIsSpawn = true;

	bool wasHellfireDiscovered = false;
	if (!forceDiablo && !forceHellfire)
		wasHellfireDiscovered = (HaveHellfire() && *GetOptions().GameMode.gameMode == StartUpGameMode::Ask);
	bool enableHellfire = forceHellfire || wasHellfireDiscovered;
	if (!forceDiablo && *GetOptions().GameMode.gameMode == StartUpGameMode::Hellfire) { // Migrate legacy options
		GetOptions().GameMode.gameMode.SetValue(StartUpGameMode::Diablo);
		enableHellfire = true;
	}
	if (forceDiablo || enableHellfire) {
		GetOptions().Mods.SetHellfireEnabled(enableHellfire);
	}

	gbIsHellfireSaveGame = gbIsHellfire;

	for (size_t i = 0; i < QuickMessages.size(); i++) {
		auto &messages = GetOptions().Chat.szHotKeyMsgs[i];
		if (messages.empty()) {
			messages.emplace_back(_(QuickMessages[i].message));
		}
	}

#ifndef USE_SDL1
	InitializeVirtualGamepad();
#endif

	UiInitialize();
	was_ui_init = true;

	if (wasHellfireDiscovered) {
		UiSelStartUpGameOption();
		if (!gbIsHellfire) {
			// Reinitialize the UI Elements because we changed the game
			UnloadUiGFX();
			UiInitialize();
			if (IsHardwareCursor())
				SetHardwareCursor(CursorInfo::UnknownCursor());
		}
	}

	DiabloInitScreen();

	snd_init();

	ui_sound_init();

	// Item graphics are loaded early, they already get touched during hero selection.
	InitItemGFX();

	// Always available.
	LoadSmallSelectionSpinner();

	CheckArchivesUpToDate();
}

void DiabloSplash()
{
	if (!gbShowIntro)
		return;

	if (*GetOptions().StartUp.splash == StartUpSplash::LogoAndTitleDialog)
		play_movie("gendata\\logo.smk", true);

	auto &intro = gbIsHellfire ? GetOptions().StartUp.hellfireIntro : GetOptions().StartUp.diabloIntro;

	if (*intro != StartUpIntro::Off) {
		if (gbIsHellfire)
			play_movie("gendata\\Hellfire.smk", true);
		else
			play_movie("gendata\\diablo1.smk", true);
		if (*intro == StartUpIntro::Once) {
			intro.SetValue(StartUpIntro::Off);
			if (!demo::IsRunning()) SaveOptions();
		}
	}

	if (IsAnyOf(*GetOptions().StartUp.splash, StartUpSplash::TitleDialog, StartUpSplash::LogoAndTitleDialog))
		UiTitleDialog();
}

void DiabloDeinit()
{
	FreeItemGFX();

	LuaShutdown();
	ShutDownScreenReader();

	if (gbSndInited)
		effects_cleanup_sfx();
	snd_deinit();
	if (was_ui_init)
		UiDestroy();
	if (was_archives_init)
		init_cleanup();
	if (was_window_init)
		dx_cleanup(); // Cleanup SDL surfaces stuff, so we have to do it before SDL_Quit().
	UnloadFonts();
	if (SDL_WasInit(SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC) != 0)
		SDL_Quit();
}

tl::expected<void, std::string> LoadLvlGFX()
{
	assert(pDungeonCels == nullptr);
	constexpr int SpecialCelWidth = 64;

	const auto loadAll = [](const char *cel, const char *til, const char *special) -> tl::expected<void, std::string> {
		ASSIGN_OR_RETURN(pDungeonCels, LoadFileInMemWithStatus(cel));
		ASSIGN_OR_RETURN(pMegaTiles, LoadFileInMemWithStatus<MegaTile>(til));
		ASSIGN_OR_RETURN(pSpecialCels, LoadCelWithStatus(special, SpecialCelWidth));
		return {};
	};

	switch (leveltype) {
	case DTYPE_TOWN: {
		auto cel = LoadFileInMemWithStatus("nlevels\\towndata\\town.cel");
		if (!cel.has_value()) {
			ASSIGN_OR_RETURN(pDungeonCels, LoadFileInMemWithStatus("levels\\towndata\\town.cel"));
		} else {
			pDungeonCels = std::move(*cel);
		}
		auto til = LoadFileInMemWithStatus<MegaTile>("nlevels\\towndata\\town.til");
		if (!til.has_value()) {
			ASSIGN_OR_RETURN(pMegaTiles, LoadFileInMemWithStatus<MegaTile>("levels\\towndata\\town.til"));
		} else {
			pMegaTiles = std::move(*til);
		}
		ASSIGN_OR_RETURN(pSpecialCels, LoadCelWithStatus("levels\\towndata\\towns", SpecialCelWidth));
		return {};
	}
	case DTYPE_CATHEDRAL:
		return loadAll(
		    "levels\\l1data\\l1.cel",
		    "levels\\l1data\\l1.til",
		    "levels\\l1data\\l1s");
	case DTYPE_CATACOMBS:
		return loadAll(
		    "levels\\l2data\\l2.cel",
		    "levels\\l2data\\l2.til",
		    "levels\\l2data\\l2s");
	case DTYPE_CAVES:
		return loadAll(
		    "levels\\l3data\\l3.cel",
		    "levels\\l3data\\l3.til",
		    "levels\\l1data\\l1s");
	case DTYPE_HELL:
		return loadAll(
		    "levels\\l4data\\l4.cel",
		    "levels\\l4data\\l4.til",
		    "levels\\l2data\\l2s");
	case DTYPE_NEST:
		return loadAll(
		    "nlevels\\l6data\\l6.cel",
		    "nlevels\\l6data\\l6.til",
		    "levels\\l1data\\l1s");
	case DTYPE_CRYPT:
		return loadAll(
		    "nlevels\\l5data\\l5.cel",
		    "nlevels\\l5data\\l5.til",
		    "nlevels\\l5data\\l5s");
	default:
		return tl::make_unexpected("LoadLvlGFX");
	}
}

tl::expected<void, std::string> LoadAllGFX()
{
	IncProgress();
#if !defined(USE_SDL1) && !defined(__vita__)
	InitVirtualGamepadGFX();
#endif
	IncProgress();
	RETURN_IF_ERROR(InitObjectGFX());
	IncProgress();
	RETURN_IF_ERROR(InitMissileGFX());
	IncProgress();
	return {};
}

/**
 * @param entry Where is the player entering from
 */
void CreateLevel(lvl_entry entry)
{
	CreateDungeon(DungeonSeeds[currlevel], entry);

	switch (leveltype) {
	case DTYPE_TOWN:
		InitTownTriggers();
		break;
	case DTYPE_CATHEDRAL:
		InitL1Triggers();
		break;
	case DTYPE_CATACOMBS:
		InitL2Triggers();
		break;
	case DTYPE_CAVES:
		InitL3Triggers();
		break;
	case DTYPE_HELL:
		InitL4Triggers();
		break;
	case DTYPE_NEST:
		InitHiveTriggers();
		break;
	case DTYPE_CRYPT:
		InitCryptTriggers();
		break;
	default:
		app_fatal("CreateLevel");
	}

	if (leveltype != DTYPE_TOWN) {
		Freeupstairs();
	}
	LoadRndLvlPal(leveltype);
}

void UnstuckChargers()
{
	if (gbIsMultiplayer) {
		for (Player &player : Players) {
			if (!player.plractive)
				continue;
			if (player._pLvlChanging)
				continue;
			if (!player.isOnActiveLevel())
				continue;
			if (&player == MyPlayer)
				continue;
			return;
		}
	}
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.mode == MonsterMode::Charge)
			monster.mode = MonsterMode::Stand;
	}
}

void UpdateMonsterLights()
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];

		if ((monster.flags & MFLAG_BERSERK) != 0) {
			int lightRadius = leveltype == DTYPE_NEST ? 9 : 3;
			monster.lightId = AddLight(monster.position.tile, lightRadius);
		}

		if (monster.lightId != NO_LIGHT) {
			if (monster.lightId == MyPlayer->lightId) { // Fix old saves where some monsters had 0 instead of NO_LIGHT
				monster.lightId = NO_LIGHT;
				continue;
			}

			Light &light = Lights[monster.lightId];
			if (monster.position.tile != light.position.tile) {
				ChangeLightXY(monster.lightId, monster.position.tile);
			}
		}
	}
}

void GameLogic()
{
	if (!ProcessInput()) {
		return;
	}
	if (gbProcessPlayers) {
		gGameLogicStep = GameLogicStep::ProcessPlayers;
		ProcessPlayers();
	}
	if (leveltype != DTYPE_TOWN) {
		gGameLogicStep = GameLogicStep::ProcessMonsters;
#ifdef _DEBUG
		if (!DebugInvisible)
#endif
			ProcessMonsters();
		gGameLogicStep = GameLogicStep::ProcessObjects;
		ProcessObjects();
		gGameLogicStep = GameLogicStep::ProcessMissiles;
		ProcessMissiles();
		gGameLogicStep = GameLogicStep::ProcessItems;
		ProcessItems();
		ProcessLightList();
		ProcessVisionList();
	} else {
		gGameLogicStep = GameLogicStep::ProcessTowners;
		ProcessTowners();
		gGameLogicStep = GameLogicStep::ProcessItemsTown;
		ProcessItems();
		gGameLogicStep = GameLogicStep::ProcessMissilesTown;
		ProcessMissiles();
	}
	gGameLogicStep = GameLogicStep::None;

#ifdef _DEBUG
	if (DebugScrollViewEnabled && (SDL_GetModState() & KMOD_SHIFT) != 0) {
		ScrollView();
	}
#endif

	sound_update();
	CheckTriggers();
	CheckQuests();
	RedrawViewport();
	pfile_update(false);

	plrctrls_after_game_logic();
}

void TimeoutCursor(bool bTimeout)
{
	if (bTimeout) {
		if (sgnTimeoutCurs == CURSOR_NONE && sgbMouseDown == CLICK_NONE) {
			sgnTimeoutCurs = pcurs;
			multi_net_ping();
			InfoString = StringOrView {};
			AddInfoBoxString(_("-- Network timeout --"));
			AddInfoBoxString(_("-- Waiting for players --"));
			NewCursor(CURSOR_HOURGLASS);
			RedrawEverything();
		}
		scrollrt_draw_game_screen();
	} else if (sgnTimeoutCurs != CURSOR_NONE) {
		// Timeout is gone, we should restore the previous cursor.
		// But the timeout cursor could already be changed by the now processed messages (for example item cursor from CMD_GETITEM).
		// Changing the item cursor back to the previous (hand) cursor could result in deleted items, because this resets Player.HoldItem (see NewCursor).
		if (pcurs == CURSOR_HOURGLASS)
			NewCursor(sgnTimeoutCurs);
		sgnTimeoutCurs = CURSOR_NONE;
		InfoString = StringOrView {};
		RedrawEverything();
	}
}

void HelpKeyPressed()
{
	if (HelpFlag) {
		HelpFlag = false;
	} else if (IsPlayerInStore()) {
		InfoString = StringOrView {};
		AddInfoBoxString(_("No help available")); /// BUGFIX: message isn't displayed
		AddInfoBoxString(_("while in stores"));
		LastMouseButtonAction = MouseActionType::None;
	} else {
		CloseInventory();
		CloseCharPanel();
		SpellbookFlag = false;
		SpellSelectFlag = false;
		if (qtextflag && leveltype == DTYPE_TOWN) {
			qtextflag = false;
			stream_stop();
		}
		QuestLogIsOpen = false;
		CancelCurrentDiabloMsg();
		gamemenu_off();
		DisplayHelp();
		doom_close();
	}
}

void InventoryKeyPressed()
{
	if (IsPlayerInStore())
		return;
	invflag = !invflag;
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!invflag) { // We closed the inventory
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!SpellbookFlag) { // We opened the inventory
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	SpellbookFlag = false;
	CloseGoldWithdraw();
	CloseStash();
}

void CharacterSheetKeyPressed()
{
	if (IsPlayerInStore())
		return;
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (CharFlag) { // We are closing the character sheet
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!QuestLogIsOpen) { // We opened the character sheet
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	ToggleCharPanel();
}

void QuestLogKeyPressed()
{
	if (IsPlayerInStore())
		return;
	if (!QuestLogIsOpen) {
		StartQuestlog();
	} else {
		QuestLogIsOpen = false;
	}
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (!QuestLogIsOpen) { // We closed the quest log
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!CharFlag) { // We opened the character quest log
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	CloseCharPanel();
	CloseGoldWithdraw();
	CloseStash();
}

void DisplaySpellsKeyPressed()
{
	if (IsPlayerInStore())
		return;
	CloseCharPanel();
	QuestLogIsOpen = false;
	CloseInventory();
	SpellbookFlag = false;
	if (!SpellSelectFlag) {
		DoSpeedBook();
	} else {
		SpellSelectFlag = false;
	}
	LastMouseButtonAction = MouseActionType::None;
}

void SpellBookKeyPressed()
{
	if (IsPlayerInStore())
		return;
	SpellbookFlag = !SpellbookFlag;
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!SpellbookFlag) { // We closed the inventory
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!invflag) { // We opened the inventory
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	CloseInventory();
}

void CycleSpellHotkeys(bool next)
{
	StaticVector<size_t, NumHotkeys> validHotKeyIndexes;
	std::optional<size_t> currentIndex;
	for (size_t slot = 0; slot < NumHotkeys; slot++) {
		if (!IsValidSpeedSpell(slot))
			continue;
		if (MyPlayer->_pRSpell == MyPlayer->_pSplHotKey[slot] && MyPlayer->_pRSplType == MyPlayer->_pSplTHotKey[slot]) {
			// found current
			currentIndex = validHotKeyIndexes.size();
		}
		validHotKeyIndexes.emplace_back(slot);
	}
	if (validHotKeyIndexes.size() == 0)
		return;

	size_t newIndex;
	if (!currentIndex) {
		newIndex = next ? 0 : (validHotKeyIndexes.size() - 1);
	} else if (next) {
		newIndex = (*currentIndex == validHotKeyIndexes.size() - 1) ? 0 : (*currentIndex + 1);
	} else {
		newIndex = *currentIndex == 0 ? (validHotKeyIndexes.size() - 1) : (*currentIndex - 1);
	}
	ToggleSpell(validHotKeyIndexes[newIndex]);
}

bool IsPlayerDead()
{
	return MyPlayer->_pmode == PM_DEATH || MyPlayerIsDead;
}

bool IsGameRunning()
{
	return PauseMode != 2;
}

bool CanPlayerTakeAction()
{
	return !IsPlayerDead() && IsGameRunning();
}

bool CanAutomapBeToggledOff()
{
	// check if every window is closed - if yes, automap can be toggled off
	if (!QuestLogIsOpen && !IsWithdrawGoldOpen && !IsStashOpen && !CharFlag
	    && !SpellbookFlag && !invflag && !isGameMenuOpen && !qtextflag && !SpellSelectFlag
	    && !ChatLogFlag && !HelpFlag)
		return true;

	return false;
}

void OptionLanguageCodeChanged()
{
	UnloadFonts();
	LanguageInitialize();
	LoadLanguageArchive();
	effects_cleanup_sfx();
	if (gbRunGame)
		sound_init();
	else
		ui_sound_init();
}

const auto OptionChangeHandlerLanguage = (GetOptions().Language.code.SetValueChangedCallback(OptionLanguageCodeChanged), true);

} // namespace

void InitKeymapActions()
{
	Options &options = GetOptions();
	for (uint32_t i = 0; i < 8; ++i) {
		options.Keymapper.AddAction(
		    "BeltItem{}",
		    N_("Belt item {}"),
		    N_("Use Belt item."),
		    '1' + i,
		    [i] {
			    Player &myPlayer = *MyPlayer;
			    if (!myPlayer.SpdList[i].isEmpty() && myPlayer.SpdList[i]._itype != ItemType::Gold) {
				    UseInvItem(INVITEM_BELT_FIRST + i);
			    }
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i + 1);
	}
	for (uint32_t i = 0; i < NumHotkeys; ++i) {
		options.Keymapper.AddAction(
		    "QuickSpell{}",
		    N_("Quick spell {}"),
		    N_("Hotkey for skill or spell."),
		    i < 4 ? static_cast<uint32_t>(SDLK_F5) + i : static_cast<uint32_t>(SDLK_UNKNOWN),
		    [i]() {
			    if (SpellSelectFlag) {
				    SetSpeedSpell(i);
				    return;
			    }
			    if (!*GetOptions().Gameplay.quickCast)
				    ToggleSpell(i);
			    else
				    QuickCast(i);
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i + 1);
	}
	options.Keymapper.AddAction(
	    "QuickSpellPrevious",
	    N_("Previous quick spell"),
	    N_("Selects the previous quick spell (cycles)."),
	    MouseScrollUpButton,
	    [] { CycleSpellHotkeys(false); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "QuickSpellNext",
	    N_("Next quick spell"),
	    N_("Selects the next quick spell (cycles)."),
	    MouseScrollDownButton,
	    [] { CycleSpellHotkeys(true); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "UseHealthPotion",
	    N_("Use health potion"),
	    N_("Use health potions from belt."),
	    SDLK_UNKNOWN,
	    [] { UseBeltItem(BeltItemType::Healing); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "UseManaPotion",
	    N_("Use mana potion"),
	    N_("Use mana potions from belt."),
	    SDLK_UNKNOWN,
	    [] { UseBeltItem(BeltItemType::Mana); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "DisplaySpells",
	    N_("Speedbook"),
	    N_("Open Speedbook."),
	    'S',
	    DisplaySpellsKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "QuickSave",
	    N_("Quick save"),
	    N_("Saves the game."),
	    SDLK_F2,
	    [] { gamemenu_save_game(false); },
	    nullptr,
	    [&]() { return !gbIsMultiplayer && CanPlayerTakeAction(); });
	options.Keymapper.AddAction(
	    "QuickLoad",
	    N_("Quick load"),
	    N_("Loads the game."),
	    SDLK_F3,
	    [] { gamemenu_load_game(false); },
	    nullptr,
	    [&]() { return !gbIsMultiplayer && gbValidSaveFile && !IsPlayerInStore() && IsGameRunning(); });
#ifndef NOEXIT
	options.Keymapper.AddAction(
	    "QuitGame",
	    N_("Quit game"),
	    N_("Closes the game."),
	    SDLK_UNKNOWN,
	    [] { gamemenu_quit_game(false); });
#endif
	options.Keymapper.AddAction(
	    "StopHero",
	    N_("Stop hero"),
	    N_("Stops walking and cancel pending actions."),
	    SDLK_UNKNOWN,
	    [] { MyPlayer->Stop(); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "Item Highlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    SDLK_LALT,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	options.Keymapper.AddAction(
	    "Toggle Item Highlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    SDLK_RCTRL,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	options.Keymapper.AddAction(
	    "Toggle Automap",
	    N_("Toggle automap"),
	    N_("Toggles if automap is displayed."),
	    SDLK_TAB,
	    DoAutoMap,
	    nullptr,
	    IsGameRunning);
	options.Keymapper.AddAction(
	    "CycleAutomapType",
	    N_("Cycle map type"),
	    N_("Opaque -> Transparent -> Minimap -> None"),
	    SDLK_m,
	    CycleAutomapType,
	    nullptr,
	    IsGameRunning);

	options.Keymapper.AddAction(
	    "Inventory",
	    N_("Inventory"),
	    N_("Open Inventory screen."),
	    'I',
	    InventoryKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "Character",
	    N_("Character"),
	    N_("Open Character screen."),
	    'C',
	    CharacterSheetKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "QuestLog",
	    N_("Quest log"),
	    N_("Open Quest log."),
	    'Q',
	    QuestLogKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "SpellBook",
	    N_("Spellbook"),
	    N_("Open Spellbook."),
	    'B',
	    SpellBookKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	for (uint32_t i = 0; i < QuickMessages.size(); ++i) {
		options.Keymapper.AddAction(
		    "QuickMessage{}",
		    N_("Quick Message {}"),
		    N_("Use Quick Message in chat."),
		    (i < 4) ? static_cast<uint32_t>(SDLK_F9) + i : static_cast<uint32_t>(SDLK_UNKNOWN),
		    [i]() { DiabloHotkeyMsg(i); },
		    nullptr,
		    nullptr,
		    i + 1);
	}
	options.Keymapper.AddAction(
	    "Hide Info Screens",
	    N_("Hide Info Screens"),
	    N_("Hide all info screens."),
	    SDLK_SPACE,
	    [] {
		    if (CanAutomapBeToggledOff())
			    AutomapActive = false;

		    ClosePanels();
		    HelpFlag = false;
		    ChatLogFlag = false;
		    SpellSelectFlag = false;
		    if (qtextflag && leveltype == DTYPE_TOWN) {
			    qtextflag = false;
			    stream_stop();
		    }

		    CancelCurrentDiabloMsg();
		    gamemenu_off();
		    doom_close();
	    },
	    nullptr,
	    IsGameRunning);
	options.Keymapper.AddAction(
	    "Zoom",
	    N_("Zoom"),
	    N_("Zoom Game Screen."),
	    'Z',
	    [] {
		    GetOptions().Graphics.zoom.SetValue(!*GetOptions().Graphics.zoom);
		    CalcViewportGeometry();
	    },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "Pause Game",
	    N_("Pause Game"),
	    N_("Pauses the game."),
	    'P',
	    diablo_pause_game);
	options.Keymapper.AddAction(
	    "Pause Game (Alternate)",
	    N_("Pause Game (Alternate)"),
	    N_("Pauses the game."),
	    SDLK_PAUSE,
	    diablo_pause_game);
	options.Keymapper.AddAction(
	    "DecreaseBrightness",
	    N_("Decrease Brightness"),
	    N_("Reduce screen brightness."),
	    'F',
	    DecreaseBrightness,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "IncreaseBrightness",
	    N_("Increase Brightness"),
	    N_("Increase screen brightness."),
	    'G',
	    IncreaseBrightness,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "Help",
	    N_("Help"),
	    N_("Open Help Screen."),
	    SDLK_F1,
	    HelpKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "Screenshot",
	    N_("Screenshot"),
	    N_("Takes a screenshot."),
	    SDLK_PRINTSCREEN,
	    nullptr,
	    CaptureScreen);
	options.Keymapper.AddAction(
	    "GameInfo",
	    N_("Game info"),
	    N_("Displays game infos."),
	    'V',
	    [] {
		    EventPlrMsg(fmt::format(
		                    fmt::runtime(_(/* TRANSLATORS: {:s} means: Character Name, Game Version, Game Difficulty. */ "{:s} {:s}")),
		                    PROJECT_NAME,
		                    PROJECT_VERSION),
		        UiFlags::ColorWhite);
	    },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "ChatLog",
	    N_("Chat Log"),
	    N_("Displays chat log."),
	    'L',
	    [] {
		    ToggleChatLog();
	    });
	options.Keymapper.AddAction(
	    "SortInv",
	    N_("Sort Inventory"),
	    N_("Sorts the inventory."),
	    'R',
	    [] {
		    ReorganizeInventory(*MyPlayer);
	    });
#ifdef _DEBUG
	options.Keymapper.AddAction(
	    "OpenConsole",
	    N_("Console"),
	    N_("Opens Lua console."),
	    SDLK_BACKQUOTE,
	    OpenConsole);
	options.Keymapper.AddAction(
	    "DebugToggle",
	    "Debug toggle",
	    "Programming is like magic.",
	    'X',
	    [] {
		    DebugToggle = !DebugToggle;
	    });
#endif
	options.Keymapper.CommitActions();
}

void InitPadmapActions()
{
	Options &options = GetOptions();
	for (int i = 0; i < 8; ++i) {
		options.Padmapper.AddAction(
		    "BeltItem{}",
		    N_("Belt item {}"),
		    N_("Use Belt item."),
		    ControllerButton_NONE,
		    [i] {
			    Player &myPlayer = *MyPlayer;
			    if (!myPlayer.SpdList[i].isEmpty() && myPlayer.SpdList[i]._itype != ItemType::Gold) {
				    UseInvItem(INVITEM_BELT_FIRST + i);
			    }
		    },
		    nullptr,
		    CanPlayerTakeAction,
		    i + 1);
	}
	for (uint32_t i = 0; i < NumHotkeys; ++i) {
		options.Padmapper.AddAction(
		    "QuickSpell{}",
		    N_("Quick spell {}"),
		    N_("Hotkey for skill or spell."),
		    ControllerButton_NONE,
		    [i]() {
			    if (SpellSelectFlag) {
				    SetSpeedSpell(i);
				    return;
			    }
			    if (!*GetOptions().Gameplay.quickCast)
				    ToggleSpell(i);
			    else
				    QuickCast(i);
		    },
		    nullptr,
		    []() { return CanPlayerTakeAction() && !InGameMenu(); },
		    i + 1);
	}
	options.Padmapper.AddAction(
	    "PrimaryAction",
	    N_("Primary action"),
	    N_("Attack monsters, talk to towners, lift and place inventory items."),
	    ControllerButton_BUTTON_B,
	    [] {
		    ControllerActionHeld = GameActionType_PRIMARY_ACTION;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformPrimaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "SecondaryAction",
	    N_("Secondary action"),
	    N_("Open chests, interact with doors, pick up items."),
	    ControllerButton_BUTTON_Y,
	    [] {
		    ControllerActionHeld = GameActionType_SECONDARY_ACTION;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformSecondaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "SpellAction",
	    N_("Spell action"),
	    N_("Cast the active spell."),
	    ControllerButton_BUTTON_X,
	    [] {
		    ControllerActionHeld = GameActionType_CAST_SPELL;
		    LastMouseButtonAction = MouseActionType::None;
		    PerformSpellAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastMouseButtonAction = MouseActionType::None;
	    },
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "CancelAction",
	    N_("Cancel action"),
	    N_("Close menus."),
	    ControllerButton_BUTTON_A,
	    [] {
		    if (DoomFlag) {
			    doom_close();
			    return;
		    }

		    GameAction action;
		    if (SpellSelectFlag)
			    action = GameAction(GameActionType_TOGGLE_QUICK_SPELL_MENU);
		    else if (invflag)
			    action = GameAction(GameActionType_TOGGLE_INVENTORY);
		    else if (SpellbookFlag)
			    action = GameAction(GameActionType_TOGGLE_SPELL_BOOK);
		    else if (QuestLogIsOpen)
			    action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
		    else if (CharFlag)
			    action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
		    ProcessGameAction(action);
	    },
	    nullptr,
	    [] { return DoomFlag || SpellSelectFlag || invflag || SpellbookFlag || QuestLogIsOpen || CharFlag; });
	options.Padmapper.AddAction(
	    "MoveUp",
	    N_("Move up"),
	    N_("Moves the player character up."),
	    ControllerButton_BUTTON_DPAD_UP,
	    [] {});
	options.Padmapper.AddAction(
	    "MoveDown",
	    N_("Move down"),
	    N_("Moves the player character down."),
	    ControllerButton_BUTTON_DPAD_DOWN,
	    [] {});
	options.Padmapper.AddAction(
	    "MoveLeft",
	    N_("Move left"),
	    N_("Moves the player character left."),
	    ControllerButton_BUTTON_DPAD_LEFT,
	    [] {});
	options.Padmapper.AddAction(
	    "MoveRight",
	    N_("Move right"),
	    N_("Moves the player character right."),
	    ControllerButton_BUTTON_DPAD_RIGHT,
	    [] {});
	options.Padmapper.AddAction(
	    "StandGround",
	    N_("Stand ground"),
	    N_("Hold to prevent the player from moving."),
	    ControllerButton_NONE,
	    [] {});
	options.Padmapper.AddAction(
	    "ToggleStandGround",
	    N_("Toggle stand ground"),
	    N_("Toggle whether the player moves."),
	    ControllerButton_NONE,
	    [] { StandToggle = !StandToggle; },
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "UseHealthPotion",
	    N_("Use health potion"),
	    N_("Use health potions from belt."),
	    ControllerButton_BUTTON_LEFTSHOULDER,
	    [] { UseBeltItem(BeltItemType::Healing); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "UseManaPotion",
	    N_("Use mana potion"),
	    N_("Use mana potions from belt."),
	    ControllerButton_BUTTON_RIGHTSHOULDER,
	    [] { UseBeltItem(BeltItemType::Mana); },
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "Character",
	    N_("Character"),
	    N_("Open Character screen."),
	    ControllerButton_AXIS_TRIGGERLEFT,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_CHARACTER_INFO });
	    },
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "Inventory",
	    N_("Inventory"),
	    N_("Open Inventory screen."),
	    ControllerButton_AXIS_TRIGGERRIGHT,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_INVENTORY });
	    },
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "QuestLog",
	    N_("Quest log"),
	    N_("Open Quest log."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_AXIS_TRIGGERLEFT },
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_QUEST_LOG });
	    },
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "SpellBook",
	    N_("Spellbook"),
	    N_("Open Spellbook."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_AXIS_TRIGGERRIGHT },
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_SPELL_BOOK });
	    },
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "DisplaySpells",
	    N_("Speedbook"),
	    N_("Open Speedbook."),
	    ControllerButton_BUTTON_A,
	    [] {
		    ProcessGameAction(GameAction { GameActionType_TOGGLE_QUICK_SPELL_MENU });
	    },
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Padmapper.AddAction(
	    "Toggle Automap",
	    N_("Toggle automap"),
	    N_("Toggles if automap is displayed."),
	    ControllerButton_BUTTON_LEFTSTICK,
	    DoAutoMap);
	options.Padmapper.AddAction(
	    "MouseUp",
	    N_("Move mouse up"),
	    N_("Simulates upward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_UP },
	    [] {});
	options.Padmapper.AddAction(
	    "MouseDown",
	    N_("Move mouse down"),
	    N_("Simulates downward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_DOWN },
	    [] {});
	options.Padmapper.AddAction(
	    "MouseLeft",
	    N_("Move mouse left"),
	    N_("Simulates leftward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_LEFT },
	    [] {});
	options.Padmapper.AddAction(
	    "MouseRight",
	    N_("Move mouse right"),
	    N_("Simulates rightward mouse movement."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_DPAD_RIGHT },
	    [] {});
	auto leftMouseDown = [] {
		ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		sgbMouseDown = CLICK_LEFT;
		LeftMouseDown(standGround ? KMOD_SHIFT : KMOD_NONE);
	};
	auto leftMouseUp = [] {
		ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(standGround ? KMOD_SHIFT : KMOD_NONE);
	};
	options.Padmapper.AddAction(
	    "LeftMouseClick1",
	    N_("Left mouse click"),
	    N_("Simulates the left mouse button."),
	    ControllerButton_BUTTON_RIGHTSTICK,
	    leftMouseDown,
	    leftMouseUp);
	options.Padmapper.AddAction(
	    "LeftMouseClick2",
	    N_("Left mouse click"),
	    N_("Simulates the left mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_LEFTSHOULDER },
	    leftMouseDown,
	    leftMouseUp);
	auto rightMouseDown = [] {
		ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_RIGHT;
		RightMouseDown(standGround);
	};
	auto rightMouseUp = [] {
		LastMouseButtonAction = MouseActionType::None;
		sgbMouseDown = CLICK_NONE;
	};
	options.Padmapper.AddAction(
	    "RightMouseClick1",
	    N_("Right mouse click"),
	    N_("Simulates the right mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_RIGHTSTICK },
	    rightMouseDown,
	    rightMouseUp);
	options.Padmapper.AddAction(
	    "RightMouseClick2",
	    N_("Right mouse click"),
	    N_("Simulates the right mouse button."),
	    { ControllerButton_BUTTON_BACK, ControllerButton_BUTTON_RIGHTSHOULDER },
	    rightMouseDown,
	    rightMouseUp);
	options.Padmapper.AddAction(
	    "PadHotspellMenu",
	    N_("Gamepad hotspell menu"),
	    N_("Hold to set or use spell hotkeys."),
	    ControllerButton_BUTTON_BACK,
	    [] { PadHotspellMenuActive = true; },
	    [] { PadHotspellMenuActive = false; });
	options.Padmapper.AddAction(
	    "PadMenuNavigator",
	    N_("Gamepad menu navigator"),
	    N_("Hold to access gamepad menu navigation."),
	    ControllerButton_BUTTON_START,
	    [] { PadMenuNavigatorActive = true; },
	    [] { PadMenuNavigatorActive = false; });
	auto toggleGameMenu = [] {
		bool inMenu = gmenu_is_active();
		PressEscKey();
		LastMouseButtonAction = MouseActionType::None;
		PadHotspellMenuActive = false;
		PadMenuNavigatorActive = false;
		if (!inMenu)
			gamemenu_on();
	};
	options.Padmapper.AddAction(
	    "ToggleGameMenu1",
	    N_("Toggle game menu"),
	    N_("Opens the game menu."),
	    {
	        ControllerButton_BUTTON_BACK,
	        ControllerButton_BUTTON_START,
	    },
	    toggleGameMenu);
	options.Padmapper.AddAction(
	    "ToggleGameMenu2",
	    N_("Toggle game menu"),
	    N_("Opens the game menu."),
	    {
	        ControllerButton_BUTTON_START,
	        ControllerButton_BUTTON_BACK,
	    },
	    toggleGameMenu);
	options.Padmapper.AddAction(
	    "QuickSave",
	    N_("Quick save"),
	    N_("Saves the game."),
	    ControllerButton_NONE,
	    [] { gamemenu_save_game(false); },
	    nullptr,
	    [&]() { return !gbIsMultiplayer && CanPlayerTakeAction(); });
	options.Padmapper.AddAction(
	    "QuickLoad",
	    N_("Quick load"),
	    N_("Loads the game."),
	    ControllerButton_NONE,
	    [] { gamemenu_load_game(false); },
	    nullptr,
	    [&]() { return !gbIsMultiplayer && gbValidSaveFile && !IsPlayerInStore() && IsGameRunning(); });
	options.Padmapper.AddAction(
	    "Item Highlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    ControllerButton_NONE,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	options.Padmapper.AddAction(
	    "Toggle Item Highlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    ControllerButton_NONE,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	options.Padmapper.AddAction(
	    "Hide Info Screens",
	    N_("Hide Info Screens"),
	    N_("Hide all info screens."),
	    ControllerButton_NONE,
	    [] {
		    if (CanAutomapBeToggledOff())
			    AutomapActive = false;

		    ClosePanels();
		    HelpFlag = false;
		    ChatLogFlag = false;
		    SpellSelectFlag = false;
		    if (qtextflag && leveltype == DTYPE_TOWN) {
			    qtextflag = false;
			    stream_stop();
		    }

		    CancelCurrentDiabloMsg();
		    gamemenu_off();
		    doom_close();
	    },
	    nullptr,
	    IsGameRunning);
	options.Padmapper.AddAction(
	    "Zoom",
	    N_("Zoom"),
	    N_("Zoom Game Screen."),
	    ControllerButton_NONE,
	    [] {
		    GetOptions().Graphics.zoom.SetValue(!*GetOptions().Graphics.zoom);
		    CalcViewportGeometry();
	    },
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "Pause Game",
	    N_("Pause Game"),
	    N_("Pauses the game."),
	    ControllerButton_NONE,
	    diablo_pause_game);
	options.Padmapper.AddAction(
	    "DecreaseBrightness",
	    N_("Decrease Brightness"),
	    N_("Reduce screen brightness."),
	    ControllerButton_NONE,
	    DecreaseBrightness,
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "IncreaseBrightness",
	    N_("Increase Brightness"),
	    N_("Increase screen brightness."),
	    ControllerButton_NONE,
	    IncreaseBrightness,
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "Help",
	    N_("Help"),
	    N_("Open Help Screen."),
	    ControllerButton_NONE,
	    HelpKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "Screenshot",
	    N_("Screenshot"),
	    N_("Takes a screenshot."),
	    ControllerButton_NONE,
	    nullptr,
	    CaptureScreen);
	options.Padmapper.AddAction(
	    "GameInfo",
	    N_("Game info"),
	    N_("Displays game infos."),
	    ControllerButton_NONE,
	    [] {
		    EventPlrMsg(fmt::format(
		                    fmt::runtime(_(/* TRANSLATORS: {:s} means: Character Name, Game Version, Game Difficulty. */ "{:s} {:s}")),
		                    PROJECT_NAME,
		                    PROJECT_VERSION),
		        UiFlags::ColorWhite);
	    },
	    nullptr,
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "SortInv",
	    N_("Sort Inventory"),
	    N_("Sorts the inventory."),
	    ControllerButton_NONE,
	    [] {
		    ReorganizeInventory(*MyPlayer);
	    });
	options.Padmapper.AddAction(
	    "ChatLog",
	    N_("Chat Log"),
	    N_("Displays chat log."),
	    ControllerButton_NONE,
	    [] {
		    ToggleChatLog();
	    });
	options.Padmapper.CommitActions();
}

void SetCursorPos(Point position)
{
	if (ControlDevice != ControlTypes::KeyboardAndMouse) {
		MousePosition = position;
		return;
	}

	LogicalToOutput(&position.x, &position.y);
	if (!demo::IsRunning())
		SDL_WarpMouseInWindow(ghMainWnd, position.x, position.y);
}

void FreeGameMem()
{
	pDungeonCels = nullptr;
	pMegaTiles = nullptr;
	pSpecialCels = std::nullopt;

	FreeMonsters();
	FreeMissileGFX();
	FreeObjectGFX();
	FreeTownerGFX();
	FreeStashGFX();
#ifndef USE_SDL1
	DeactivateVirtualGamepad();
	FreeVirtualGamepadGFX();
#endif
}

bool StartGame(bool bNewGame, bool bSinglePlayer)
{
	gbSelectProvider = true;
	ReturnToMainMenu = false;

	do {
		gbLoadGame = false;

		if (!NetInit(bSinglePlayer)) {
			gbRunGameResult = true;
			break;
		}

		// Save 2.8 MiB of RAM by freeing all main menu resources
		// before starting the game.
		UiDestroy();

		gbSelectProvider = false;

		if (bNewGame || !gbValidSaveFile) {
			InitLevels();
			InitQuests();
			InitPortals();
			InitDungMsgs(*MyPlayer);
			DeltaSyncJunk();
		}
		giNumberOfLevels = gbIsHellfire ? 25 : 17;
		interface_mode uMsg = WM_DIABNEWGAME;
		if (gbValidSaveFile && gbLoadGame) {
			uMsg = WM_DIABLOADGAME;
		}
		RunGameLoop(uMsg);
		NetClose();
		UnloadFonts();

		// If the player left the game into the main menu,
		// initialize main menu resources.
		if (gbRunGameResult)
			UiInitialize();
		if (ReturnToMainMenu)
			return true;
	} while (gbRunGameResult);

	SNetDestroy();
	return gbRunGameResult;
}

void diablo_quit(int exitStatus)
{
	FreeGameMem();
	music_stop();
	DiabloDeinit();
	exit(exitStatus);
}

#ifdef __UWP__
void (*onInitialized)() = NULL;

void setOnInitialized(void (*callback)())
{
	onInitialized = callback;
}
#endif

int DiabloMain(int argc, char **argv)
{
#ifdef _DEBUG
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

	DiabloParseFlags(argc, argv);
	InitKeymapActions();
	InitPadmapActions();

	// Need to ensure devilutionx.mpq (and fonts.mpq if available) are loaded before attempting to read translation settings
	LoadCoreArchives();
	was_archives_init = true;

	// Read settings including translation next. This will use the presence of fonts.mpq and look for assets in devilutionx.mpq
	LoadOptions();
	if (demo::IsRunning()) demo::OverrideOptions();

	// Then look for a voice pack file based on the selected translation
	LoadLanguageArchive();

	ApplicationInit();
	LuaInitialize();
	if (!demo::IsRunning()) SaveOptions();

	// Finally load game data
	LoadGameArchives();

	// Load dynamic data before we go into the menu as we need to initialise player characters in memory pretty early.
	LoadPlayerDataFiles();

	// TODO: We can probably load this much later (when the game is starting).
	LoadSpellData();
	LoadMissileData();
	LoadMonsterData();
	LoadItemData();
	LoadObjectData();

	DiabloInit();
#ifdef __UWP__
	onInitialized();
#endif
	if (!demo::IsRunning()) SaveOptions();

	DiabloSplash();
	mainmenu_loop();
	DiabloDeinit();

	return 0;
}

bool TryIconCurs()
{
	if (pcurs == CURSOR_RESURRECT) {
		if (PlayerUnderCursor != nullptr) {
			NetSendCmdParam1(true, CMD_RESURRECT, PlayerUnderCursor->getId());
			NewCursor(CURSOR_HAND);
			return true;
		}

		return false;
	}

	if (pcurs == CURSOR_HEALOTHER) {
		if (PlayerUnderCursor != nullptr) {
			NetSendCmdParam1(true, CMD_HEALOTHER, PlayerUnderCursor->getId());
			NewCursor(CURSOR_HAND);
			return true;
		}

		return false;
	}

	if (pcurs == CURSOR_TELEKINESIS) {
		DoTelekinesis();
		return true;
	}

	Player &myPlayer = *MyPlayer;

	if (pcurs == CURSOR_IDENTIFY) {
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			CheckIdentify(myPlayer, pcursinvitem);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			item._iIdentified = true;
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_REPAIR) {
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			DoRepair(myPlayer, pcursinvitem);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			RepairItem(item, myPlayer.getCharacterLevel());
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_RECHARGE) {
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			DoRecharge(myPlayer, pcursinvitem);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			RechargeItem(item, myPlayer);
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_OIL) {
		bool changeCursor = true;
		if (pcursinvitem != -1 && !IsInspectingPlayer())
			changeCursor = DoOil(myPlayer, pcursinvitem);
		else if (pcursstashitem != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[pcursstashitem];
			changeCursor = ApplyOilToItem(item, myPlayer);
		}
		if (changeCursor)
			NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_TELEPORT) {
		const SpellID spellID = myPlayer.inventorySpell;
		const SpellType spellType = SpellType::Scroll;
		const int spellFrom = myPlayer.spellFrom;
		if (IsWallSpell(spellID)) {
			Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
			NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), static_cast<uint16_t>(sd), spellFrom);
		} else if (pcursmonst != -1 && leveltype != DTYPE_TOWN) {
			NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		} else if (PlayerUnderCursor != nullptr && !myPlayer.friendlyMode) {
			NetSendCmdParam4(true, CMD_SPELLPID, PlayerUnderCursor->getId(), static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		} else {
			NetSendCmdLocParam3(true, CMD_SPELLXY, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		}
		NewCursor(CURSOR_HAND);
		return true;
	}

	if (pcurs == CURSOR_DISARM && ObjectUnderCursor == nullptr) {
		NewCursor(CURSOR_HAND);
		return true;
	}

	return false;
}

void diablo_pause_game()
{
	if (!gbIsMultiplayer) {
		if (PauseMode != 0) {
			PauseMode = 0;
		} else {
			PauseMode = 2;
			sound_stop();
			qtextflag = false;
			LastMouseButtonAction = MouseActionType::None;
		}

		RedrawEverything();
	}
}

bool GameWasAlreadyPaused = false;
bool MinimizePaused = false;

bool diablo_is_focused()
{
#ifndef USE_SDL1
	return SDL_GetKeyboardFocus() == ghMainWnd;
#else
	Uint8 appState = SDL_GetAppState();
	return (appState & SDL_APPINPUTFOCUS) != 0;
#endif
}

void diablo_focus_pause()
{
	if (!movie_playing && (gbIsMultiplayer || MinimizePaused)) {
		return;
	}

	GameWasAlreadyPaused = PauseMode != 0;

	if (!GameWasAlreadyPaused) {
		PauseMode = 2;
		sound_stop();
		LastMouseButtonAction = MouseActionType::None;
	}

	SVidMute();
	music_mute();

	MinimizePaused = true;
}

void diablo_focus_unpause()
{
	if (!GameWasAlreadyPaused) {
		PauseMode = 0;
	}

	SVidUnmute();
	music_unmute();

	MinimizePaused = false;
}

bool PressEscKey()
{
	bool rv = false;

	if (DoomFlag) {
		doom_close();
		rv = true;
	}

	if (HelpFlag) {
		HelpFlag = false;
		rv = true;
	}

	if (ChatLogFlag) {
		ChatLogFlag = false;
		rv = true;
	}

	if (qtextflag) {
		qtextflag = false;
		stream_stop();
		rv = true;
	}

	if (IsPlayerInStore()) {
		StoreESC();
		rv = true;
	}

	if (IsDiabloMsgAvailable()) {
		CancelCurrentDiabloMsg();
		rv = true;
	}

	if (ChatFlag) {
		ResetChat();
		rv = true;
	}

	if (DropGoldFlag) {
		control_drop_gold(SDLK_ESCAPE);
		rv = true;
	}

	if (IsWithdrawGoldOpen) {
		WithdrawGoldKeyPress(SDLK_ESCAPE);
		rv = true;
	}

	if (SpellSelectFlag) {
		SpellSelectFlag = false;
		rv = true;
	}

	if (IsLeftPanelOpen() || IsRightPanelOpen()) {
		ClosePanels();
		rv = true;
	}

	return rv;
}

void DisableInputEventHandler(const SDL_Event &event, uint16_t modState)
{
	switch (event.type) {
	case SDL_MOUSEMOTION:
		MousePosition = { event.motion.x, event.motion.y };
		return;
	case SDL_MOUSEBUTTONDOWN:
		if (sgbMouseDown != CLICK_NONE)
			return;
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			sgbMouseDown = CLICK_LEFT;
			return;
		case SDL_BUTTON_RIGHT:
			sgbMouseDown = CLICK_RIGHT;
			return;
		default:
			return;
		}
	case SDL_MOUSEBUTTONUP:
		sgbMouseDown = CLICK_NONE;
		return;
	}

	MainWndProc(event);
}

void LoadGameLevelStopMusic(_music_id neededTrack)
{
	if (neededTrack != sgnMusicTrack)
		music_stop();
}

void LoadGameLevelStartMusic(_music_id neededTrack)
{
	if (sgnMusicTrack != neededTrack)
		music_start(neededTrack);

	if (MinimizePaused) {
		music_mute();
	}
}

void LoadGameLevelResetCursor()
{
	if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
		NewCursor(CURSOR_HAND);
	}
}

void SetRndSeedForDungeonLevel()
{
	if (setlevel) {
		// Maps are not randomly generated, but the monsters max hitpoints are.
		// So we need to ensure that we have a stable seed when generating quest/set-maps.
		// For this purpose we reuse the normal dungeon seeds.
		SetRndSeed(DungeonSeeds[static_cast<size_t>(setlvlnum)]);
	} else {
		SetRndSeed(DungeonSeeds[currlevel]);
	}
}

void LoadGameLevelFirstFlagEntry()
{
	CloseInventory();
	qtextflag = false;
	if (!HeadlessMode) {
		InitInv();
		ClearUniqueItemFlags();
		InitQuestText();
		InitInfoBoxGfx();
		InitHelp();
	}
	InitStores();
	InitAutomapOnce();
}

void LoadGameLevelStores()
{
	if (leveltype == DTYPE_TOWN) {
		SetupTownStores();
	} else {
		FreeStoreMem();
	}
}

void LoadGameLevelStash()
{
	bool isHellfireSaveGame = gbIsHellfireSaveGame;

	gbIsHellfireSaveGame = gbIsHellfire;
	LoadStash();
	gbIsHellfireSaveGame = isHellfireSaveGame;
}

tl::expected<void, std::string> LoadGameLevelDungeon(bool firstflag, lvl_entry lvldir, const Player &myPlayer)
{
	if (firstflag || lvldir == ENTRY_LOAD || !myPlayer._pLvlVisited[currlevel] || gbIsMultiplayer) {
		HoldThemeRooms();
		[[maybe_unused]] uint32_t mid1Seed = GetLCGEngineState();
		InitGolems();
		InitObjects();
		[[maybe_unused]] uint32_t mid2Seed = GetLCGEngineState();

		IncProgress();

		RETURN_IF_ERROR(InitMonsters());
		InitItems();
		CreateThemeRooms();

		IncProgress();

		[[maybe_unused]] uint32_t mid3Seed = GetLCGEngineState();
		InitMissiles();
		InitCorpses();
#ifdef _DEBUG
		SetDebugLevelSeedInfos(mid1Seed, mid2Seed, mid3Seed, GetLCGEngineState());
#endif
		SavePreLighting();

		IncProgress();

		if (gbIsMultiplayer)
			DeltaLoadLevel();
	} else {
		HoldThemeRooms();
		InitGolems();
		RETURN_IF_ERROR(InitMonsters());
		InitMissiles();
		InitCorpses();

		IncProgress();

		RETURN_IF_ERROR(LoadLevel());

		IncProgress();
	}
	return {};
}

void LoadGameLevelSyncPlayerEntry(lvl_entry lvldir)
{
	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel() && (!player._pLvlChanging || &player == MyPlayer)) {
			if (player._pHitPoints > 0) {
				if (lvldir != ENTRY_LOAD)
					SyncInitPlrPos(player);
			} else {
				dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
			}
		}
	}
}

void LoadGameLevelLightVision()
{
	if (leveltype != DTYPE_TOWN) {
		memcpy(dLight, dPreLight, sizeof(dLight));                                     // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(Players[MyPlayerId].lightId, Players[MyPlayerId].position.tile); // forces player light refresh
		ProcessLightList();
		ProcessVisionList();
	}
}

void LoadGameLevelReturn()
{
	ViewPosition = GetMapReturnPosition();
	if (Quests[Q_BETRAYER]._qactive == QUEST_DONE)
		Quests[Q_BETRAYER]._qvar2 = 2;
}

void LoadGameLevelInitPlayers(bool firstflag, lvl_entry lvldir)
{
	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel()) {
			InitPlayerGFX(player);
			if (lvldir != ENTRY_LOAD)
				InitPlayer(player, firstflag);
		}
	}
}

void LoadGameLevelSetVisited()
{
	bool visited = false;
	for (const Player &player : Players) {
		if (player.plractive)
			visited = visited || player._pLvlVisited[currlevel];
	}
}

tl::expected<void, std::string> LoadGameLevelTown(bool firstflag, lvl_entry lvldir, const Player &myPlayer)
{
	for (int i = 0; i < MAXDUNX; i++) { // NOLINT(modernize-loop-convert)
		for (int j = 0; j < MAXDUNY; j++) {
			dFlags[i][j] |= DungeonFlag::Lit;
		}
	}

	InitTowners();
	InitStash();
	InitItems();
	InitMissiles();

	IncProgress();

	if (!firstflag && lvldir != ENTRY_LOAD && myPlayer._pLvlVisited[currlevel] && !gbIsMultiplayer)
		RETURN_IF_ERROR(LoadLevel());
	if (gbIsMultiplayer)
		DeltaLoadLevel();

	IncProgress();

	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SELF);
	return {};
}

tl::expected<void, std::string> LoadGameLevelSetLevel(bool firstflag, lvl_entry lvldir, const Player &myPlayer)
{
	LoadSetMap();
	IncProgress();
	RETURN_IF_ERROR(GetLevelMTypes());
	IncProgress();
	InitGolems();
	RETURN_IF_ERROR(InitMonsters());
	IncProgress();
	if (!HeadlessMode) {
#if !defined(USE_SDL1) && !defined(__vita__)
		InitVirtualGamepadGFX();
#endif
		RETURN_IF_ERROR(InitMissileGFX());
		IncProgress();
	}
	InitCorpses();
	IncProgress();

	if (lvldir == ENTRY_WARPLVL)
		GetPortalLvlPos();
	IncProgress();

	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel()) {
			InitPlayerGFX(player);
			if (lvldir != ENTRY_LOAD)
				InitPlayer(player, firstflag);
		}
	}
	IncProgress();
	InitMultiView();
	IncProgress();

	if (firstflag || lvldir == ENTRY_LOAD || !myPlayer._pSLvlVisited[setlvlnum] || gbIsMultiplayer) {
		InitItems();
		SavePreLighting();
	} else {
		RETURN_IF_ERROR(LoadLevel());
	}
	if (gbIsMultiplayer) {
		DeltaLoadLevel();
		if (!UseMultiplayerQuests())
			ResyncQuests();
	}

	PlayDungMsgs();
	InitMissiles();
	IncProgress();
	return {};
}

tl::expected<void, std::string> LoadGameLevelStandardLevel(bool firstflag, lvl_entry lvldir, const Player &myPlayer)
{
	CreateLevel(lvldir);

	IncProgress();

	SetRndSeedForDungeonLevel();

	if (leveltype != DTYPE_TOWN) {
		RETURN_IF_ERROR(GetLevelMTypes());
		InitThemes();
		if (!HeadlessMode)
			RETURN_IF_ERROR(LoadAllGFX());
	} else if (!HeadlessMode) {
		IncProgress();

#if !defined(USE_SDL1) && !defined(__vita__)
		InitVirtualGamepadGFX();
#endif

		IncProgress();

		RETURN_IF_ERROR(InitMissileGFX());

		IncProgress();
		IncProgress();
	}

	IncProgress();

	if (lvldir == ENTRY_RTNLVL) {
		LoadGameLevelReturn();
	}

	if (lvldir == ENTRY_WARPLVL)
		GetPortalLvlPos();

	IncProgress();

	LoadGameLevelInitPlayers(firstflag, lvldir);
	InitMultiView();

	IncProgress();

	LoadGameLevelSetVisited();

	SetRndSeedForDungeonLevel();

	if (leveltype == DTYPE_TOWN) {
		LoadGameLevelTown(firstflag, lvldir, myPlayer);
	} else {
		LoadGameLevelDungeon(firstflag, lvldir, myPlayer);
	}

	PlayDungMsgs();

	if (UseMultiplayerQuests())
		ResyncMPQuests();
	else
		ResyncQuests();
	return {};
}

void LoadGameLevelCrypt()
{
	if (CornerStone.isAvailable()) {
		CornerstoneLoad(CornerStone.position);
	}
	if (Quests[Q_NAKRUL]._qactive == QUEST_DONE && currlevel == 24) {
		SyncNakrulRoom();
	}
}

void LoadGameLevelCalculateCursor()
{
	// Recalculate mouse selection of entities after level change/load
	LastMouseButtonAction = MouseActionType::None;
	sgbMouseDown = CLICK_NONE;
	ResetItemlabelHighlighted(); // level changed => item changed
	pcursmonst = -1;             // ensure pcurstemp is set to a valid value
	CheckCursMove();
}

tl::expected<void, std::string> LoadGameLevel(bool firstflag, lvl_entry lvldir)
{
	_music_id neededTrack = GetLevelMusic(leveltype);

	ClearFloatingNumbers();
	LoadGameLevelStopMusic(neededTrack);
	LoadGameLevelResetCursor();
	SetRndSeedForDungeonLevel();

	IncProgress();

	RETURN_IF_ERROR(LoadTrns());
	MakeLightTable();
	RETURN_IF_ERROR(LoadLevelSOLData());

	IncProgress();

	RETURN_IF_ERROR(LoadLvlGFX());
	SetDungeonMicros(pDungeonCels, MicroTileLen);
	ClearClxDrawCache();

	IncProgress();

	if (firstflag) {
		LoadGameLevelFirstFlagEntry();
	}

	SetRndSeedForDungeonLevel();

	LoadGameLevelStores();

	if (firstflag || lvldir == ENTRY_LOAD) {
		LoadGameLevelStash();
	}

	IncProgress();

	InitAutomap();

	if (leveltype != DTYPE_TOWN && lvldir != ENTRY_LOAD) {
		InitLighting();
	}

	InitLevelMonsters();

	IncProgress();

	Player &myPlayer = *MyPlayer;

	if (setlevel) {
		RETURN_IF_ERROR(LoadGameLevelSetLevel(firstflag, lvldir, myPlayer));
	} else {
		RETURN_IF_ERROR(LoadGameLevelStandardLevel(firstflag, lvldir, myPlayer));
	}

	SyncPortals();
	LoadGameLevelSyncPlayerEntry(lvldir);

	IncProgress();
	IncProgress();

	if (firstflag) {
		RETURN_IF_ERROR(InitMainPanel());
	}

	IncProgress();

	UpdateMonsterLights();
	UnstuckChargers();

	LoadGameLevelLightVision();

	if (leveltype == DTYPE_CRYPT) {
		LoadGameLevelCrypt();
	}

#ifndef USE_SDL1
	ActivateVirtualGamepad();
#endif
	LoadGameLevelStartMusic(neededTrack);

	CompleteProgress();

	LoadGameLevelCalculateCursor();
	return {};
}

bool game_loop(bool bStartup)
{
	uint16_t wait = bStartup ? sgGameInitInfo.nTickRate * 3 : 3;

	for (unsigned i = 0; i < wait; i++) {
		if (!multi_handle_delta()) {
			TimeoutCursor(true);
			return false;
		}
		TimeoutCursor(false);
		GameLogic();
		ClearLastSentPlayerCmd();

		if (!gbRunGame || !gbIsMultiplayer || demo::IsRunning() || demo::IsRecording() || !nthread_has_500ms_passed())
			break;
	}
	return true;
}

void diablo_color_cyc_logic()
{
	if (!*GetOptions().Graphics.colorCycling)
		return;

	if (PauseMode != 0)
		return;

	if (leveltype == DTYPE_CAVES) {
		if (setlevel && setlvlnum == Quests[Q_PWATER]._qslvl) {
			UpdatePWaterPalette();
		} else {
			palette_update_caves();
		}
	} else if (leveltype == DTYPE_HELL) {
		lighting_color_cycling();
	} else if (leveltype == DTYPE_NEST) {
		palette_update_hive();
	} else if (leveltype == DTYPE_CRYPT) {
		palette_update_crypt();
	}
}

bool IsDiabloAlive(bool playSFX)
{
	if (Quests[Q_DIABLO]._qactive == QUEST_DONE && !gbIsMultiplayer) {
		if (playSFX)
			PlaySFX(SfxID::DiabloDeath);
		return false;
	}

	return true;
}

void PrintScreen(SDL_Keycode vkey)
{
	ReleaseKey(vkey);
}

} // namespace devilution
