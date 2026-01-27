/**
 * @file diablo.cpp
 *
 * Implementation of the main game initialization functions.
 */
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <queue>
#include <string_view>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include <fmt/format.h>

#include <config.h>

#include "DiabloUI/selstart.h"
#include "appfat.h"
#include "automap.h"
#include "capture.h"
#include "control/control.hpp"
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
#include "engine/path.h"
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
#include "levels/tile_properties.hpp"
#include "lighting.h"
#include "loadsave.h"
#include "lua/lua_global.hpp"
#include "menu.h"
#include "minitext.h"
#include "missiles.h"
#include "movie.h"
#include "multi.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "panels/console.hpp"
#include "panels/info_box.hpp"
#include "panels/charpanel.hpp"
#include "panels/partypanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "portal.h"
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
#include "tables/monstdat.h"
#include "tables/playerdat.hpp"
#include "towners.h"
#include "track.h"
#include "utils/console.h"
#include "utils/display.h"
#include "utils/format_int.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/parse_int.hpp"
#include "utils/paths.h"
#include "utils/proximity_audio.hpp"
#include "utils/screen_reader.hpp"
#include "utils/sdl_compat.h"
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
PlayerActionType LastPlayerAction = PlayerActionType::None;

// Controller support: Actions to run after updating the cursor state.
// Defined in SourceX/controls/plctrls.cpp.
extern void plrctrls_after_check_curs_move();
extern void plrctrls_every_frame();
extern void plrctrls_after_game_logic();

namespace {

char gszVersionNumber[64] = "internal version unknown";

 void SelectNextTownNpcKeyPressed();
 void SelectPreviousTownNpcKeyPressed();
 void UpdateAutoWalkTownNpc();
 void UpdateAutoWalkTracker();
 void SpeakSelectedSpeedbookSpell();
 void SpellBookKeyPressed();
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeech(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoors(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechLenient(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeech(const Player &player, Point startPosition, Point destinationPosition, Point &closestPosition);
void AppendKeyboardWalkPathForSpeech(std::string &message, const std::vector<int8_t> &path);
 void AppendDirectionalFallback(std::string &message, const Displacement &delta);

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
	LastPlayerAction = PlayerActionType::None;
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
		RepeatPlayerAction();
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
			LastPlayerAction = PlayerActionType::Walk;
			NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
		}
		return;
	}

	const Player &myPlayer = *MyPlayer;
	bNear = myPlayer.position.tile.WalkingDistance(cursPosition) < 2;
	if (pcursitem != -1 && pcurs == CURSOR_HAND && !bShift) {
		NetSendCmdLocParam1(true, invflag ? CMD_GOTOGETITEM : CMD_GOTOAGETITEM, cursPosition, pcursitem);
	} else if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled() && (!bShift || (bNear && ObjectUnderCursor->_oBreak == 1))) {
		LastPlayerAction = PlayerActionType::OperateObject;
		NetSendCmdLoc(MyPlayerId, true, pcurs == CURSOR_DISARM ? CMD_DISARMXY : CMD_OPOBJXY, cursPosition);
	} else if (myPlayer.UsesRangedWeapon()) {
		if (bShift) {
			LastPlayerAction = PlayerActionType::Attack;
			NetSendCmdLoc(MyPlayerId, true, CMD_RATTACKXY, cursPosition);
		} else if (pcursmonst != -1) {
			if (CanTalkToMonst(Monsters[pcursmonst])) {
				NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
			} else {
				LastPlayerAction = PlayerActionType::AttackMonsterTarget;
				NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
			}
		} else if (PlayerUnderCursor != nullptr && !PlayerUnderCursor->hasNoLife() && !myPlayer.friendlyMode) {
			LastPlayerAction = PlayerActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_RATTACKPID, PlayerUnderCursor->getId());
		}
	} else {
		if (bShift) {
			if (pcursmonst != -1) {
				if (CanTalkToMonst(Monsters[pcursmonst])) {
					NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
				} else {
					LastPlayerAction = PlayerActionType::Attack;
					NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
				}
			} else {
				LastPlayerAction = PlayerActionType::Attack;
				NetSendCmdLoc(MyPlayerId, true, CMD_SATTACKXY, cursPosition);
			}
		} else if (pcursmonst != -1) {
			LastPlayerAction = PlayerActionType::AttackMonsterTarget;
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else if (PlayerUnderCursor != nullptr && !PlayerUnderCursor->hasNoLife() && !myPlayer.friendlyMode) {
			LastPlayerAction = PlayerActionType::AttackPlayerTarget;
			NetSendCmdParam1(true, CMD_ATTACKPID, PlayerUnderCursor->getId());
		}
	}
	if (!bShift && pcursitem == -1 && ObjectUnderCursor == nullptr && pcursmonst == -1 && PlayerUnderCursor == nullptr) {
		LastPlayerAction = PlayerActionType::Walk;
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, cursPosition);
	}
}

bool TryOpenDungeonWithMouse()
{
	if (leveltype != DTYPE_TOWN)
		return false;

	const Item &holdItem = MyPlayer->HoldItem;
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
	LastPlayerAction = PlayerActionType::None;

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

	const bool isShiftHeld = (modState & SDL_KMOD_SHIFT) != 0;
	const bool isCtrlHeld = (modState & SDL_KMOD_CTRL) != 0;

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
					const Point currentPosition = MyPlayer->position.tile;
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
		const bool isShiftHeld = (modState & SDL_KMOD_SHIFT) != 0;
		ReleaseChrBtns(isShiftHeld);
	}
	if (LevelButtonDown)
		CheckLevelButtonUp();
	if (IsPlayerInStore())
		ReleaseStoreBtn();
}

void RightMouseDown(bool isShiftHeld)
{
	LastPlayerAction = PlayerActionType::None;

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
	if (DidRightClickPartyPortrait())
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
			if ((modState & SDL_KMOD_ALT) != 0) {
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
			LastPlayerAction = PlayerActionType::None;
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
		if ((vkey == SDLK_RETURN || vkey == SDLK_KP_ENTER) && (modState & SDL_KMOD_ALT) != 0) {
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
	case SDLK_V:
		if ((modState & SDL_KMOD_SHIFT) != 0)
			NextDebugMonster();
		else
			GetDebugMonster();
		return;
#endif
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if ((modState & SDL_KMOD_ALT) != 0) {
			options.Graphics.fullscreen.SetValue(!IsFullScreen());
			if (!demo::IsRunning()) SaveOptions();
		} else if (CharFlag) {
			CharacterScreenActivateSelection((modState & SDL_KMOD_SHIFT) != 0);
		} else if (IsPlayerInStore()) {
			StoreEnter();
		} else if (QuestLogIsOpen) {
			QuestlogEnter();
		} else if (SpellSelectFlag) {
			SetSpell();
		} else if (SpellbookFlag && MyPlayer != nullptr && !IsInspectingPlayer()) {
			const Player &player = *MyPlayer;
			if (IsValidSpell(player._pRSpell)) {
				std::string msg;
				StrAppend(msg, _("Selected: "), pgettext("spell", GetSpellData(player._pRSpell).sNameText));
				SpeakText(msg, /*force=*/true);
			} else {
				SpeakText(_("No spell selected."), /*force=*/true);
			}
			SpellBookKeyPressed();
		} else {
			TypeChatMessage();
		}
		return;
	case SDLK_UP:
		if (IsPlayerInStore()) {
			StoreUp();
		} else if (QuestLogIsOpen) {
			QuestlogUp();
		} else if (CharFlag) {
			CharacterScreenMoveSelection(-1);
		} else if (HelpFlag) {
			HelpScrollUp();
		} else if (ChatLogFlag) {
			ChatLogScrollUp();
		} else if (SpellSelectFlag) {
			HotSpellMove({ AxisDirectionX_NONE, AxisDirectionY_UP });
			SpeakSelectedSpeedbookSpell();
		} else if (SpellbookFlag && MyPlayer != nullptr && !IsInspectingPlayer()) {
			const std::optional<SpellID> next = GetSpellBookAdjacentAvailableSpell(SpellbookTab, *MyPlayer, MyPlayer->_pRSpell, -1);
			if (next) {
				MyPlayer->_pRSpell = *next;
				MyPlayer->_pRSplType = (MyPlayer->_pAblSpells & GetSpellBitmask(*next)) != 0 ? SpellType::Skill
				    : (MyPlayer->_pISpells & GetSpellBitmask(*next)) != 0       ? SpellType::Charges
				                                                              : SpellType::Spell;
				UpdateSpellTarget(*next);
				RedrawEverything();
				SpeakText(pgettext("spell", GetSpellData(*next).sNameText), /*force=*/true);
			}
		} else if (invflag) {
			InventoryMoveFromKeyboard({ AxisDirectionX_NONE, AxisDirectionY_UP });
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
		} else if (CharFlag) {
			CharacterScreenMoveSelection(+1);
		} else if (HelpFlag) {
			HelpScrollDown();
		} else if (ChatLogFlag) {
			ChatLogScrollDown();
		} else if (SpellSelectFlag) {
			HotSpellMove({ AxisDirectionX_NONE, AxisDirectionY_DOWN });
			SpeakSelectedSpeedbookSpell();
		} else if (SpellbookFlag && MyPlayer != nullptr && !IsInspectingPlayer()) {
			const std::optional<SpellID> next = GetSpellBookAdjacentAvailableSpell(SpellbookTab, *MyPlayer, MyPlayer->_pRSpell, +1);
			if (next) {
				MyPlayer->_pRSpell = *next;
				MyPlayer->_pRSplType = (MyPlayer->_pAblSpells & GetSpellBitmask(*next)) != 0 ? SpellType::Skill
				    : (MyPlayer->_pISpells & GetSpellBitmask(*next)) != 0       ? SpellType::Charges
				                                                              : SpellType::Spell;
				UpdateSpellTarget(*next);
				RedrawEverything();
				SpeakText(pgettext("spell", GetSpellData(*next).sNameText), /*force=*/true);
			}
		} else if (invflag) {
			InventoryMoveFromKeyboard({ AxisDirectionX_NONE, AxisDirectionY_DOWN });
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
		} else {
			const KeymapperOptions::Action *action = GetOptions().Keymapper.findAction(static_cast<uint32_t>(vkey));
			if (action == nullptr || !action->isEnabled())
				SelectPreviousTownNpcKeyPressed();
		}
		return;
	case SDLK_PAGEDOWN:
		if (IsPlayerInStore()) {
			StoreNext();
		} else if (ChatLogFlag) {
			ChatLogScrollBottom();
		} else {
			const KeymapperOptions::Action *action = GetOptions().Keymapper.findAction(static_cast<uint32_t>(vkey));
			if (action == nullptr || !action->isEnabled())
				SelectNextTownNpcKeyPressed();
		}
		return;
	case SDLK_LEFT:
		if (CharFlag) {
			CharacterScreenMoveSelection(-1);
		} else if (SpellSelectFlag) {
			HotSpellMove({ AxisDirectionX_LEFT, AxisDirectionY_NONE });
			SpeakSelectedSpeedbookSpell();
		} else if (SpellbookFlag && MyPlayer != nullptr && !IsInspectingPlayer()) {
			if (SpellbookTab > 0) {
				SpellbookTab--;
				const std::optional<SpellID> first = GetSpellBookFirstAvailableSpell(SpellbookTab, *MyPlayer);
				if (first) {
					MyPlayer->_pRSpell = *first;
					MyPlayer->_pRSplType = (MyPlayer->_pAblSpells & GetSpellBitmask(*first)) != 0 ? SpellType::Skill
					    : (MyPlayer->_pISpells & GetSpellBitmask(*first)) != 0       ? SpellType::Charges
					                                                              : SpellType::Spell;
					UpdateSpellTarget(*first);
					RedrawEverything();
					SpeakText(pgettext("spell", GetSpellData(*first).sNameText), /*force=*/true);
				}
			}
		} else if (invflag) {
			InventoryMoveFromKeyboard({ AxisDirectionX_LEFT, AxisDirectionY_NONE });
		} else if (AutomapActive && !ChatFlag) {
			AutomapLeft();
		}
		return;
	case SDLK_RIGHT:
		if (CharFlag) {
			CharacterScreenMoveSelection(+1);
		} else if (SpellSelectFlag) {
			HotSpellMove({ AxisDirectionX_RIGHT, AxisDirectionY_NONE });
			SpeakSelectedSpeedbookSpell();
		} else if (SpellbookFlag && MyPlayer != nullptr && !IsInspectingPlayer()) {
			const int maxTab = gbIsHellfire ? 4 : 3;
			if (SpellbookTab < maxTab) {
				SpellbookTab++;
				const std::optional<SpellID> first = GetSpellBookFirstAvailableSpell(SpellbookTab, *MyPlayer);
				if (first) {
					MyPlayer->_pRSpell = *first;
					MyPlayer->_pRSplType = (MyPlayer->_pAblSpells & GetSpellBitmask(*first)) != 0 ? SpellType::Skill
					    : (MyPlayer->_pISpells & GetSpellBitmask(*first)) != 0       ? SpellType::Charges
					                                                              : SpellType::Spell;
					UpdateSpellTarget(*first);
					RedrawEverything();
					SpeakText(pgettext("spell", GetSpellData(*first).sNameText), /*force=*/true);
				}
			}
		} else if (invflag) {
			InventoryMoveFromKeyboard({ AxisDirectionX_RIGHT, AxisDirectionY_NONE });
		} else if (AutomapActive && !ChatFlag) {
			AutomapRight();
		}
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
			RightMouseDown((modState & SDL_KMOD_SHIFT) != 0);
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
		LastPlayerAction = PlayerActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(modState);
	} else if (sgbMouseDown == CLICK_RIGHT && button == SDL_BUTTON_RIGHT) {
		LastPlayerAction = PlayerActionType::None;
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
	[[maybe_unused]] const Options &options = GetOptions();
	StaticVector<ControllerButtonEvent, 4> ctrlEvents = ToControllerButtonEvents(event);
	for (const ControllerButtonEvent ctrlEvent : ctrlEvents) {
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
	case SDL_EVENT_KEY_DOWN:
		PressKey(SDLC_EventKey(event), modState);
		return;
	case SDL_EVENT_KEY_UP:
		ReleaseKey(SDLC_EventKey(event));
		return;
	case SDL_EVENT_MOUSE_MOTION:
		if (ControlMode == ControlTypes::KeyboardAndMouse && invflag)
			InvalidateInventorySlot();
		MousePosition = { SDLC_EventMotionIntX(event), SDLC_EventMotionIntY(event) };
		gmenu_on_mouse_move();
		return;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		MousePosition = { SDLC_EventButtonIntX(event), SDLC_EventButtonIntY(event) };
		HandleMouseButtonDown(event.button.button, modState);
		return;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		MousePosition = { SDLC_EventButtonIntX(event), SDLC_EventButtonIntY(event) };
		HandleMouseButtonUp(event.button.button, modState);
		return;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	case SDL_EVENT_MOUSE_WHEEL:
		if (SDLC_EventWheelIntY(event) > 0) { // Up
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
			} else if (SDL_GetModState() & SDL_KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomIn();
				}
			} else {
				KeymapperPress(MouseScrollUpButton);
			}
		} else if (SDLC_EventWheelIntY(event) < 0) { // down
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
			} else if (SDL_GetModState() & SDL_KMOD_CTRL) {
				if (AutomapActive) {
					AutomapZoomOut();
				}
			} else {
				KeymapperPress(MouseScrollDownButton);
			}
		} else if (SDLC_EventWheelIntX(event) > 0) { // left
			KeymapperPress(MouseScrollLeftButton);
		} else if (SDLC_EventWheelIntX(event) < 0) { // right
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
			if (event.type == SDL_EVENT_QUIT) {
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
		const bool runGameLoop = demo::IsRunning() ? demo::GetRunGameLoop(drawGame, processInput) : nthread_has_500ms_passed(&drawGame);
		if (demo::IsRecording())
			demo::RecordGameLoopResult(runGameLoop);

		discord_manager::UpdateGame();

		if (!runGameLoop) {
			if (processInput)
				ProcessInput();
			DvlNet_ProcessNetworkPackets();
			if (!drawGame)
				continue;
			RedrawViewport();
			DrawAndBlit();
			continue;
		}

		ProcessGameMessagePackets();
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

#if SDL_VERSION_ATLEAST(2, 0, 0)
FILE *SdlLogFile = nullptr;

extern "C" void SdlLogToFile(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	FILE *file = reinterpret_cast<FILE *>(userdata);
	static const char *const LogPriorityPrefixes[SDL_LOG_PRIORITY_COUNT] = {
		"",
		"VERBOSE",
		"DEBUG",
		"INFO",
		"WARN",
		"ERROR",
		"CRITICAL"
	};
	std::fprintf(file, "%s: %s\n", LogPriorityPrefixes[priority], message);
	std::fflush(file);
}
#endif

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
#if SDL_VERSION_ATLEAST(2, 0, 0)
	PrintHelpOption("--log-to-file <path>", _(/* TRANSLATORS: Commandline Option */ "Log to a file instead of stderr"));
#endif
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
			SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#if SDL_VERSION_ATLEAST(2, 0, 0)
		} else if (arg == "--log-to-file") {
			if (i + 1 == argc) {
				PrintFlagRequiresArgument("--log-to-file");
				diablo_quit(64);
			}
			SdlLogFile = OpenFile(argv[++i], "wb");
			if (SdlLogFile == nullptr) {
				printInConsole("Failed to open log file for writing");
				diablo_quit(64);
			}
			SDL_SetLogOutputFunction(&SdlLogToFile, /*userdata=*/SdlLogFile);
#endif
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
	if (SDL_WasInit((~0U) & ~SDL_INIT_HAPTIC) != 0)
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
			const int lightRadius = leveltype == DTYPE_NEST ? 9 : 3;
			monster.lightId = AddLight(monster.position.tile, lightRadius);
		}

		if (monster.lightId != NO_LIGHT) {
			if (monster.lightId == MyPlayer->lightId) { // Fix old saves where some monsters had 0 instead of NO_LIGHT
				monster.lightId = NO_LIGHT;
				continue;
			}

			const Light &light = Lights[monster.lightId];
			if (monster.position.tile != light.position.tile) {
				ChangeLightXY(monster.lightId, monster.position.tile);
			}
		}
	}
}

#ifdef NOSOUND
void UpdatePlayerLowHpWarningSound()
{
}
#else
namespace {

std::unique_ptr<TSnd> PlayerLowHpWarningSound;
bool TriedLoadingPlayerLowHpWarningSound = false;

TSnd *GetPlayerLowHpWarningSound()
{
	if (TriedLoadingPlayerLowHpWarningSound)
		return PlayerLowHpWarningSound.get();
	TriedLoadingPlayerLowHpWarningSound = true;

	if (!gbSndInited)
		return nullptr;

	PlayerLowHpWarningSound = std::make_unique<TSnd>();
	PlayerLowHpWarningSound->start_tc = SDL_GetTicks() - 80 - 1;

	// Support both the new "playerhaslowhp" name and the older underscore version.
	if (PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0) {
		PlayerLowHpWarningSound = nullptr;
	}

	return PlayerLowHpWarningSound.get();
}

void StopPlayerLowHpWarningSound()
{
	if (PlayerLowHpWarningSound != nullptr)
		PlayerLowHpWarningSound->DSB.Stop();
}

[[nodiscard]] uint32_t LowHpIntervalMs(int hpPercent)
{
	// The sound starts at 50% HP (slow) and speeds up every 10% down to 0%.
	if (hpPercent > 40)
		return 1500;
	if (hpPercent > 30)
		return 1200;
	if (hpPercent > 20)
		return 900;
	if (hpPercent > 10)
		return 600;
	return 300;
}

} // namespace

void UpdatePlayerLowHpWarningSound()
{
	static uint32_t LastWarningStartMs = 0;

	if (!gbSndInited || !gbSoundOn || MyPlayer == nullptr || InGameMenu()) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	// Stop immediately when dead.
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife()) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	const int maxHp = MyPlayer->_pMaxHP;
	if (maxHp <= 0) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	const int hp = std::clamp(MyPlayer->_pHitPoints, 0, maxHp);
	const int hpPercent = std::clamp(hp * 100 / maxHp, 0, 100);

	// Only play below (or equal to) 50% and above 0%.
	if (hpPercent > 50 || hpPercent <= 0) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	TSnd *snd = GetPlayerLowHpWarningSound();
	if (snd == nullptr || !snd->DSB.IsLoaded())
		return;

	const uint32_t now = SDL_GetTicks();
	const uint32_t intervalMs = LowHpIntervalMs(hpPercent);
	if (LastWarningStartMs == 0)
		LastWarningStartMs = now - intervalMs;
	if (now - LastWarningStartMs < intervalMs)
		return;

	// Restart the cue even if it's already playing so the "tempo" is controlled by HP.
	snd->DSB.Stop();
	snd_play_snd(snd, /*lVolume=*/0, /*lPan=*/0);
	LastWarningStartMs = now;
}
#endif // NOSOUND

namespace {

[[nodiscard]] bool IsBossMonsterForHpAnnouncement(const Monster &monster)
{
	return monster.isUnique() || monster.ai == MonsterAIID::Diablo;
}

} // namespace

void UpdateLowDurabilityWarnings()
{
	static std::array<uint32_t, NUM_INVLOC> WarnedSeeds {};
	static std::array<bool, NUM_INVLOC> HasWarned {};

	if (MyPlayer == nullptr)
		return;
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife())
		return;

	std::vector<std::string> newlyLow;
	newlyLow.reserve(NUM_INVLOC);

	for (int slot = 0; slot < NUM_INVLOC; ++slot) {
		const Item &item = MyPlayer->InvBody[slot];
		if (item.isEmpty() || item._iMaxDur <= 0 || item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE) {
			HasWarned[slot] = false;
			continue;
		}

		const int maxDur = item._iMaxDur;
		const int durability = item._iDurability;
		if (durability <= 0) {
			HasWarned[slot] = false;
			continue;
		}

		int threshold = std::max(2, maxDur / 10);
		threshold = std::clamp(threshold, 1, maxDur);

		const bool isLow = durability <= threshold;
		if (!isLow) {
			HasWarned[slot] = false;
			continue;
		}

		if (HasWarned[slot] && WarnedSeeds[slot] == item._iSeed)
			continue;

		HasWarned[slot] = true;
		WarnedSeeds[slot] = item._iSeed;

		const StringOrView name = item.getName();
		if (!name.empty())
			newlyLow.emplace_back(name.str().data(), name.str().size());
	}

	if (newlyLow.empty())
		return;

	// Add ordinal numbers for duplicates (e.g. two rings with the same name).
	for (size_t i = 0; i < newlyLow.size(); ++i) {
		int total = 0;
		for (size_t j = 0; j < newlyLow.size(); ++j) {
			if (newlyLow[j] == newlyLow[i])
				++total;
		}
		if (total <= 1)
			continue;

		int ordinal = 1;
		for (size_t j = 0; j < i; ++j) {
			if (newlyLow[j] == newlyLow[i])
				++ordinal;
		}
		newlyLow[i] = fmt::format("{} {}", newlyLow[i], ordinal);
	}

	std::string joined;
	for (size_t i = 0; i < newlyLow.size(); ++i) {
		if (i != 0)
			joined += ", ";
		joined += newlyLow[i];
	}

	SpeakText(fmt::format(fmt::runtime(_("Low durability: {:s}")), joined), /*force=*/true);
}

void UpdateBossHealthAnnouncements()
{
	static dungeon_type LastLevelType = DTYPE_NONE;
	static int LastCurrLevel = -1;
	static bool LastSetLevel = false;
	static _setlevels LastSetLevelNum = SL_NONE;
	static std::array<int8_t, MaxMonsters> LastAnnouncedBucket {};

	if (MyPlayer == nullptr)
		return;
	if (leveltype == DTYPE_TOWN)
		return;

	const bool levelChanged = LastLevelType != leveltype || LastCurrLevel != currlevel || LastSetLevel != setlevel || LastSetLevelNum != setlvlnum;
	if (levelChanged) {
		LastAnnouncedBucket.fill(-1);
		LastLevelType = leveltype;
		LastCurrLevel = currlevel;
		LastSetLevel = setlevel;
		LastSetLevelNum = setlvlnum;
	}

	for (size_t monsterId = 0; monsterId < MaxMonsters; ++monsterId) {
		if (LastAnnouncedBucket[monsterId] < 0)
			continue;

		const Monster &monster = Monsters[monsterId];
		if (monster.isInvalid || monster.hitPoints <= 0 || !IsBossMonsterForHpAnnouncement(monster))
			LastAnnouncedBucket[monsterId] = -1;
	}

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (!IsBossMonsterForHpAnnouncement(monster))
			continue;
		if (monster.hitPoints <= 0 || monster.maxHitPoints <= 0)
			continue;

		const int64_t hp = std::clamp<int64_t>(monster.hitPoints, 0, monster.maxHitPoints);
		const int64_t maxHp = monster.maxHitPoints;
		const int hpPercent = static_cast<int>(std::clamp<int64_t>(hp * 100 / maxHp, 0, 100));
		const int bucket = ((hpPercent + 9) / 10) * 10;

		int8_t &lastBucket = LastAnnouncedBucket[monsterId];
		if (lastBucket < 0) {
			lastBucket = static_cast<int8_t>(((hpPercent + 9) / 10) * 10);
			continue;
		}

		if (bucket >= lastBucket)
			continue;

		lastBucket = static_cast<int8_t>(bucket);
		SpeakText(fmt::format(fmt::runtime(_("{:s} health: {:d}%")), monster.name(), bucket), /*force=*/false);
	}
}

void UpdateAttackableMonsterAnnouncements()
{
	static std::optional<int> LastAttackableMonsterId;

	if (MyPlayer == nullptr) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (leveltype == DTYPE_TOWN) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife()) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (InGameMenu() || invflag) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}

	const Player &player = *MyPlayer;
	const Point playerPosition = player.position.tile;

	int bestRotations = 5;
	std::optional<int> bestId;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;
		if (monster.isPlayerMinion())
			continue;
		if (!monster.isPossibleToHit())
			continue;

		const Point monsterPosition = monster.position.tile;
		if (playerPosition.WalkingDistance(monsterPosition) > 1)
			continue;

		const int d1 = static_cast<int>(player._pdir);
		const int d2 = static_cast<int>(GetDirection(playerPosition, monsterPosition));

		int rotations = std::abs(d1 - d2);
		if (rotations > 4)
			rotations = 4 - (rotations % 4);

		if (!bestId || rotations < bestRotations || (rotations == bestRotations && monsterId < *bestId)) {
			bestRotations = rotations;
			bestId = monsterId;
		}
	}

	if (!bestId) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}

	if (LastAttackableMonsterId && *LastAttackableMonsterId == *bestId)
		return;

	LastAttackableMonsterId = *bestId;

	const std::string_view name = Monsters[*bestId].name();
	if (!name.empty())
		SpeakText(name, /*force=*/true);
}

void GameLogic()
{
	if (!ProcessInput()) {
		return;
	}
	if (gbProcessPlayers) {
		gGameLogicStep = GameLogicStep::ProcessPlayers;
		ProcessPlayers();
		UpdateAutoWalkTownNpc();
		UpdateAutoWalkTracker();
		UpdateLowDurabilityWarnings();
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
		UpdateBossHealthAnnouncements();
		UpdateProximityAudioCues();
		UpdateAttackableMonsterAnnouncements();
	} else {
		gGameLogicStep = GameLogicStep::ProcessTowners;
		ProcessTowners();
		gGameLogicStep = GameLogicStep::ProcessItemsTown;
		ProcessItems();
		gGameLogicStep = GameLogicStep::ProcessMissilesTown;
		ProcessMissiles();
	}

	UpdatePlayerLowHpWarningSound();

	gGameLogicStep = GameLogicStep::None;

#ifdef _DEBUG
	if (DebugScrollViewEnabled && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0) {
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
			for (uint8_t i = 0; i < Players.size(); i++) {
				bool isConnected = (player_state[i] & PS_CONNECTED) != 0;
				bool isActive = (player_state[i] & PS_ACTIVE) != 0;
				if (!(isConnected && !isActive)) continue;

				DvlNetLatencies latencies = DvlNet_GetLatencies(i);

				std::string ping = fmt::format(
				    fmt::runtime(_(/* TRANSLATORS: {:s} means: Character Name */ "Player {:s} is timing out!")),
				    Players[i].name());

				StrAppend(ping, "\n  ", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Echo latency: {:d} ms")), latencies.echoLatency));

				if (latencies.providerLatency) {
					if (latencies.isRelayed && *latencies.isRelayed) {
						StrAppend(ping, "\n  ", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Provider latency: {:d} ms (Relayed)")), *latencies.providerLatency));
					} else {
						StrAppend(ping, "\n  ", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Provider latency: {:d} ms")), *latencies.providerLatency));
					}
				}
				EventPlrMsg(ping);
			}
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
		LastPlayerAction = PlayerActionType::None;
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

bool CanPlayerTakeAction();

std::vector<int> TownNpcOrder;
int SelectedTownNpc = -1;
int AutoWalkTownNpcTarget = -1;

enum class TrackerTargetCategory : uint8_t {
	Items,
	Chests,
	Doors,
	Shrines,
	Objects,
	Breakables,
	Monsters,
};

TrackerTargetCategory SelectedTrackerTargetCategory = TrackerTargetCategory::Items;
TrackerTargetCategory AutoWalkTrackerTargetCategory = TrackerTargetCategory::Items;
int AutoWalkTrackerTargetId = -1;

Point NextPositionForWalkDirection(Point position, int8_t walkDir)
{
	switch (walkDir) {
	case WALK_NE:
		return { position.x, position.y - 1 };
	case WALK_NW:
		return { position.x - 1, position.y };
	case WALK_SE:
		return { position.x + 1, position.y };
	case WALK_SW:
		return { position.x, position.y + 1 };
	case WALK_N:
		return { position.x - 1, position.y - 1 };
	case WALK_E:
		return { position.x + 1, position.y - 1 };
	case WALK_S:
		return { position.x + 1, position.y + 1 };
	case WALK_W:
		return { position.x - 1, position.y + 1 };
	default:
		return position;
	}
}

Point PositionAfterWalkPathSteps(Point start, const int8_t *path, int steps)
{
	Point position = start;
	for (int i = 0; i < steps; ++i) {
		position = NextPositionForWalkDirection(position, path[i]);
	}
	return position;
}

int8_t OppositeWalkDirection(int8_t walkDir)
{
	switch (walkDir) {
	case WALK_NE:
		return WALK_SW;
	case WALK_SW:
		return WALK_NE;
	case WALK_NW:
		return WALK_SE;
	case WALK_SE:
		return WALK_NW;
	case WALK_N:
		return WALK_S;
	case WALK_S:
		return WALK_N;
	case WALK_E:
		return WALK_W;
	case WALK_W:
		return WALK_E;
	default:
		return WALK_NONE;
	}
}

bool IsTownNpcActionAllowed()
{
	return CanPlayerTakeAction()
	    && leveltype == DTYPE_TOWN
	    && !IsPlayerInStore()
	    && !ChatLogFlag
	    && !HelpFlag;
}

void ResetTownNpcSelection()
{
	TownNpcOrder.clear();
	SelectedTownNpc = -1;
}

void RefreshTownNpcOrder(bool selectFirst = false)
{
	TownNpcOrder.clear();
	if (leveltype != DTYPE_TOWN)
		return;

	const Point playerPosition = MyPlayer->position.future;

	for (size_t i = 0; i < GetNumTowners(); ++i) {
		const Towner &towner = Towners[i];
		if (!IsTownerPresent(towner._ttype))
			continue;
		if (towner._ttype == TOWN_COW)
			continue;
		TownNpcOrder.push_back(static_cast<int>(i));
	}

	if (TownNpcOrder.empty()) {
		SelectedTownNpc = -1;
		return;
	}

	std::sort(TownNpcOrder.begin(), TownNpcOrder.end(), [&playerPosition](int a, int b) {
		const Towner &townerA = Towners[a];
		const Towner &townerB = Towners[b];
		const int distanceA = playerPosition.WalkingDistance(townerA.position);
		const int distanceB = playerPosition.WalkingDistance(townerB.position);
		if (distanceA != distanceB)
			return distanceA < distanceB;
		return townerA.name < townerB.name;
	});

	if (selectFirst) {
		SelectedTownNpc = TownNpcOrder.front();
		return;
	}

	const auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	if (it == TownNpcOrder.end())
		SelectedTownNpc = TownNpcOrder.front();
}

void EnsureTownNpcOrder()
{
	if (leveltype != DTYPE_TOWN) {
		ResetTownNpcSelection();
		return;
	}
	if (TownNpcOrder.empty()) {
		RefreshTownNpcOrder(true);
		return;
	}
	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		RefreshTownNpcOrder(true);
		return;
	}
	const auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	if (it == TownNpcOrder.end())
		SelectedTownNpc = TownNpcOrder.front();
}

void SpeakSelectedTownNpc()
{
	EnsureTownNpcOrder();

	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[SelectedTownNpc];
	const Point playerPosition = MyPlayer->position.future;
	const int distance = playerPosition.WalkingDistance(towner.position);

	std::string msg;
	StrAppend(msg, towner.name);
	StrAppend(msg, "\n", _("Distance: "), distance);
	StrAppend(msg, "\n", _("Position: "), towner.position.x, ", ", towner.position.y);
	SpeakText(msg, true);
}

void SelectTownNpcRelative(int delta)
{
	if (!IsTownNpcActionAllowed())
		return;

	EnsureTownNpcOrder();
	if (TownNpcOrder.empty()) {
		SpeakText(_("No town NPCs found."), true);
		return;
	}

	auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	int currentIndex = (it != TownNpcOrder.end()) ? static_cast<int>(it - TownNpcOrder.begin()) : 0;

	const int size = static_cast<int>(TownNpcOrder.size());
	int newIndex = (currentIndex + delta) % size;
	if (newIndex < 0)
		newIndex += size;
	SelectedTownNpc = TownNpcOrder[static_cast<size_t>(newIndex)];
	SpeakSelectedTownNpc();
}

void SelectNextTownNpcKeyPressed()
{
	SelectTownNpcRelative(+1);
}

void SelectPreviousTownNpcKeyPressed()
{
	SelectTownNpcRelative(-1);
}

void GoToSelectedTownNpcKeyPressed()
{
	if (!IsTownNpcActionAllowed())
		return;

	EnsureTownNpcOrder();
	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[SelectedTownNpc];

	std::string msg;
	StrAppend(msg, _("Going to: "), towner.name);
	SpeakText(msg, true);

	AutoWalkTownNpcTarget = SelectedTownNpc;
	UpdateAutoWalkTownNpc();
}

void UpdateAutoWalkTownNpc()
{
	if (AutoWalkTownNpcTarget < 0)
		return;
	if (leveltype != DTYPE_TOWN || IsPlayerInStore() || ChatLogFlag || HelpFlag) {
		AutoWalkTownNpcTarget = -1;
		return;
	}
	if (!CanPlayerTakeAction())
		return;

	if (MyPlayer->_pmode != PM_STAND)
		return;
	if (MyPlayer->walkpath[0] != WALK_NONE)
		return;
	if (MyPlayer->destAction != ACTION_NONE)
		return;

	if (AutoWalkTownNpcTarget >= static_cast<int>(GetNumTowners())) {
		AutoWalkTownNpcTarget = -1;
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[AutoWalkTownNpcTarget];
	if (!IsTownerPresent(towner._ttype) || towner._ttype == TOWN_COW) {
		AutoWalkTownNpcTarget = -1;
		SpeakText(_("No NPC selected."), true);
		return;
	}

	Player &myPlayer = *MyPlayer;
	const Point playerPosition = myPlayer.position.future;
	if (playerPosition.WalkingDistance(towner.position) < 2) {
		const int townerIdx = AutoWalkTownNpcTarget;
		AutoWalkTownNpcTarget = -1;
		NetSendCmdLocParam1(true, CMD_TALKXY, towner.position, static_cast<uint16_t>(townerIdx));
		return;
	}

	constexpr size_t MaxAutoWalkPathLength = 512;
	std::array<int8_t, MaxAutoWalkPathLength> path;
	path.fill(WALK_NONE);

	const int steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, towner.position, path.data(), path.size());
	if (steps == 0) {
		AutoWalkTownNpcTarget = -1;
		std::string error;
		StrAppend(error, _("Can't find a path to: "), towner.name);
		SpeakText(error, true);
		return;
	}

	// FindPath returns 0 if the path length is equal to the maximum.
	// The player walkpath buffer is MaxPathLengthPlayer, so keep segments strictly shorter.
	if (steps < static_cast<int>(MaxPathLengthPlayer)) {
		const int townerIdx = AutoWalkTownNpcTarget;
		AutoWalkTownNpcTarget = -1;
		NetSendCmdLocParam1(true, CMD_TALKXY, towner.position, static_cast<uint16_t>(townerIdx));
		return;
	}

	const int segmentSteps = std::min(steps - 1, static_cast<int>(MaxPathLengthPlayer - 1));
	const Point waypoint = PositionAfterWalkPathSteps(playerPosition, path.data(), segmentSteps);
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, waypoint);
}

namespace {

constexpr int TrackerInteractDistanceTiles = 1;
constexpr int TrackerCycleDistanceTiles = 12;

int LockedTrackerItemId = -1;
int LockedTrackerChestId = -1;
int LockedTrackerDoorId = -1;
int LockedTrackerShrineId = -1;
int LockedTrackerObjectId = -1;
int LockedTrackerBreakableId = -1;
int LockedTrackerMonsterId = -1;

struct TrackerLevelKey {
	dungeon_type levelType;
	int currLevel;
	bool isSetLevel;
	int setLevelNum;
};

std::optional<TrackerLevelKey> LockedTrackerLevelKey;

void ClearTrackerLocks()
{
	LockedTrackerItemId = -1;
	LockedTrackerChestId = -1;
	LockedTrackerDoorId = -1;
	LockedTrackerShrineId = -1;
	LockedTrackerObjectId = -1;
	LockedTrackerBreakableId = -1;
	LockedTrackerMonsterId = -1;
}

void EnsureTrackerLocksMatchCurrentLevel()
{
	const TrackerLevelKey current {
		.levelType = leveltype,
		.currLevel = currlevel,
		.isSetLevel = setlevel,
		.setLevelNum = setlvlnum,
	};

	if (!LockedTrackerLevelKey || LockedTrackerLevelKey->levelType != current.levelType || LockedTrackerLevelKey->currLevel != current.currLevel
	    || LockedTrackerLevelKey->isSetLevel != current.isSetLevel || LockedTrackerLevelKey->setLevelNum != current.setLevelNum) {
		ClearTrackerLocks();
		LockedTrackerLevelKey = current;
	}
}

int &LockedTrackerTargetId(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return LockedTrackerItemId;
	case TrackerTargetCategory::Chests:
		return LockedTrackerChestId;
	case TrackerTargetCategory::Doors:
		return LockedTrackerDoorId;
	case TrackerTargetCategory::Shrines:
		return LockedTrackerShrineId;
	case TrackerTargetCategory::Objects:
		return LockedTrackerObjectId;
	case TrackerTargetCategory::Breakables:
		return LockedTrackerBreakableId;
	case TrackerTargetCategory::Monsters:
	default:
		return LockedTrackerMonsterId;
	}
}

std::string_view TrackerTargetCategoryLabel(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("items");
	case TrackerTargetCategory::Chests:
		return _("chests");
	case TrackerTargetCategory::Doors:
		return _("doors");
	case TrackerTargetCategory::Shrines:
		return _("shrines");
	case TrackerTargetCategory::Objects:
		return _("objects");
	case TrackerTargetCategory::Breakables:
		return _("breakables");
	case TrackerTargetCategory::Monsters:
		return _("monsters");
	default:
		return _("items");
	}
}

void SpeakTrackerTargetCategory()
{
	std::string message;
	StrAppend(message, _("Tracker target: "), TrackerTargetCategoryLabel(SelectedTrackerTargetCategory));
	SpeakText(message, true);
}

void CycleTrackerTargetKeyPressed()
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;

	AutoWalkTrackerTargetId = -1;

	switch (SelectedTrackerTargetCategory) {
	case TrackerTargetCategory::Items:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Chests;
		break;
	case TrackerTargetCategory::Chests:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Doors;
		break;
	case TrackerTargetCategory::Doors:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Shrines;
		break;
	case TrackerTargetCategory::Shrines:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Objects;
		break;
	case TrackerTargetCategory::Objects:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Breakables;
		break;
	case TrackerTargetCategory::Breakables:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Monsters;
		break;
	case TrackerTargetCategory::Monsters:
	default:
		SelectedTrackerTargetCategory = TrackerTargetCategory::Items;
		break;
	}

	SpeakTrackerTargetCategory();
}

std::optional<int> FindNearestGroundItemId(Point playerPosition)
{
	std::optional<int> bestId;
	int bestDistance = 0;

	for (int y = 0; y < MAXDUNY; ++y) {
		for (int x = 0; x < MAXDUNX; ++x) {
			const int itemId = std::abs(dItem[x][y]) - 1;
			if (itemId < 0 || itemId > MAXITEMS)
				continue;

			const Item &item = Items[itemId];
			if (item.isEmpty() || item._iClass == ICLASS_NONE)
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (!bestId || distance < bestDistance) {
				bestId = itemId;
				bestDistance = distance;
			}
		}
	}

	return bestId;
}

struct TrackerCandidate {
	int id;
	int distance;
	StringOrView name;
};

[[nodiscard]] bool IsBetterTrackerCandidate(const TrackerCandidate &a, const TrackerCandidate &b)
{
	if (a.distance != b.distance)
		return a.distance < b.distance;
	return a.id < b.id;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyItemTrackerCandidates(Point playerPosition, int maxDistance)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveItemCount);

	const int minX = std::max(0, playerPosition.x - maxDistance);
	const int minY = std::max(0, playerPosition.y - maxDistance);
	const int maxX = std::min(MAXDUNX - 1, playerPosition.x + maxDistance);
	const int maxY = std::min(MAXDUNY - 1, playerPosition.y + maxDistance);

	std::array<bool, MAXITEMS + 1> seen {};

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const int itemId = std::abs(dItem[x][y]) - 1;
			if (itemId < 0 || itemId > MAXITEMS)
				continue;
			if (seen[itemId])
				continue;
			seen[itemId] = true;

			const Item &item = Items[itemId];
			if (item.isEmpty() || item._iClass == ICLASS_NONE)
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (distance > maxDistance)
				continue;

			result.push_back(TrackerCandidate {
			    .id = itemId,
			    .distance = distance,
			    .name = item.getName(),
			});
		}
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

[[nodiscard]] constexpr bool IsTrackedChestObject(const Object &object)
{
	return object.canInteractWith() && (object.IsChest() || object._otype == _object_id::OBJ_SIGNCHEST);
}

[[nodiscard]] constexpr bool IsTrackedDoorObject(const Object &object)
{
	// Track both closed and open doors (to match proximity audio cues).
	return object.isDoor() && object.canInteractWith();
}

[[nodiscard]] constexpr bool IsShrineLikeObject(const Object &object)
{
	return object.canInteractWith()
	    && (object.IsShrine()
	        || IsAnyOf(object._otype, _object_id::OBJ_BLOODFTN, _object_id::OBJ_PURIFYINGFTN, _object_id::OBJ_GOATSHRINE, _object_id::OBJ_CAULDRON,
	            _object_id::OBJ_MURKYFTN, _object_id::OBJ_TEARFTN));
}

[[nodiscard]] constexpr bool IsTrackedBreakableObject(const Object &object)
{
	return object.IsBreakable();
}

[[nodiscard]] constexpr bool IsTrackedMiscInteractableObject(const Object &object)
{
	if (!object.canInteractWith())
		return false;
	if (object.IsChest() || object._otype == _object_id::OBJ_SIGNCHEST)
		return false;
	if (object.isDoor())
		return false;
	if (IsShrineLikeObject(object))
		return false;
	if (object.IsBreakable())
		return false;
	return true;
}

template <typename Predicate>
[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyObjectTrackerCandidates(Point playerPosition, int maxDistance, Predicate predicate)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveObjectCount);

	const int minX = std::max(0, playerPosition.x - maxDistance);
	const int minY = std::max(0, playerPosition.y - maxDistance);
	const int maxX = std::min(MAXDUNX - 1, playerPosition.x + maxDistance);
	const int maxY = std::min(MAXDUNY - 1, playerPosition.y + maxDistance);

	std::array<int, MAXOBJECTS> bestDistanceById {};
	bestDistanceById.fill(std::numeric_limits<int>::max());

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const int objectId = std::abs(dObject[x][y]) - 1;
			if (objectId < 0 || objectId >= MAXOBJECTS)
				continue;

			const Object &object = Objects[objectId];
			if (object._otype == OBJ_NULL)
				continue;
			if (!predicate(object))
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (distance > maxDistance)
				continue;

			int &bestDistance = bestDistanceById[objectId];
			if (distance < bestDistance)
				bestDistance = distance;
		}
	}

	for (int objectId = 0; objectId < MAXOBJECTS; ++objectId) {
		const int distance = bestDistanceById[objectId];
		if (distance == std::numeric_limits<int>::max())
			continue;

		const Object &object = Objects[objectId];
		result.push_back(TrackerCandidate {
		    .id = objectId,
		    .distance = distance,
		    .name = object.name(),
		});
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

template <typename Predicate>
[[nodiscard]] std::optional<int> FindNearestObjectId(Point playerPosition, Predicate predicate)
{
	std::array<int, MAXOBJECTS> bestDistanceById {};
	bestDistanceById.fill(std::numeric_limits<int>::max());

	for (int y = 0; y < MAXDUNY; ++y) {
		for (int x = 0; x < MAXDUNX; ++x) {
			const int objectId = std::abs(dObject[x][y]) - 1;
			if (objectId < 0 || objectId >= MAXOBJECTS)
				continue;

			const Object &object = Objects[objectId];
			if (object._otype == OBJ_NULL)
				continue;
			if (!predicate(object))
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			int &bestDistance = bestDistanceById[objectId];
			if (distance < bestDistance)
				bestDistance = distance;
		}
	}

	std::optional<int> bestId;
	int bestDistance = 0;
	for (int objectId = 0; objectId < MAXOBJECTS; ++objectId) {
		const int distance = bestDistanceById[objectId];
		if (distance == std::numeric_limits<int>::max())
			continue;

		if (!bestId || distance < bestDistance) {
			bestId = objectId;
			bestDistance = distance;
		}
	}

	return bestId;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyChestTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedChestObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyDoorTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedDoorObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyShrineTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsShrineLikeObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyBreakableTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedBreakableObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyObjectInteractableTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedMiscInteractableObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyMonsterTrackerCandidates(Point playerPosition, int maxDistance)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveMonsterCount);

	for (size_t i = 0; i < ActiveMonsterCount; ++i) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (distance > maxDistance)
			continue;

		result.push_back(TrackerCandidate {
		    .id = monsterId,
		    .distance = distance,
		    .name = monster.name(),
		});
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

[[nodiscard]] std::optional<int> FindNextTrackerCandidateId(const std::vector<TrackerCandidate> &candidates, int currentId)
{
	if (candidates.empty())
		return std::nullopt;
	if (currentId < 0)
		return candidates.front().id;

	const auto it = std::find_if(candidates.begin(), candidates.end(), [currentId](const TrackerCandidate &c) { return c.id == currentId; });
	if (it == candidates.end())
		return candidates.front().id;

	if (candidates.size() <= 1)
		return std::nullopt;

	const size_t idx = static_cast<size_t>(it - candidates.begin());
	const size_t nextIdx = (idx + 1) % candidates.size();
	return candidates[nextIdx].id;
}

void DecorateTrackerTargetNameWithOrdinalIfNeeded(int targetId, StringOrView &targetName, const std::vector<TrackerCandidate> &candidates)
{
	if (targetName.empty())
		return;

	const std::string_view baseName = targetName.str();
	int total = 0;
	for (const TrackerCandidate &c : candidates) {
		if (c.name.str() == baseName)
			++total;
	}
	if (total <= 1)
		return;

	int ordinal = 0;
	int seen = 0;
	for (const TrackerCandidate &c : candidates) {
		if (c.name.str() != baseName)
			continue;
		++seen;
		if (c.id == targetId) {
			ordinal = seen;
			break;
		}
	}
	if (ordinal <= 0)
		return;

	std::string decorated;
	StrAppend(decorated, baseName, " ", ordinal);
	targetName = std::move(decorated);
}

[[nodiscard]] bool IsGroundItemPresent(int itemId)
{
	if (itemId < 0 || itemId > MAXITEMS)
		return false;

	for (uint8_t i = 0; i < ActiveItemCount; ++i) {
		if (ActiveItems[i] == itemId)
			return true;
	}

	return false;
}

std::optional<int> FindNearestUnopenedChestObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedChestObject);
}

std::optional<int> FindNearestDoorObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedDoorObject);
}

std::optional<int> FindNearestShrineObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsShrineLikeObject);
}

std::optional<int> FindNearestBreakableObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedBreakableObject);
}

std::optional<int> FindNearestMiscInteractableObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedMiscInteractableObject);
}

std::optional<int> FindNearestMonsterId(Point playerPosition)
{
	std::optional<int> bestId;
	int bestDistance = 0;

	for (size_t i = 0; i < ActiveMonsterCount; ++i) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (!bestId || distance < bestDistance) {
			bestId = monsterId;
			bestDistance = distance;
		}
	}

	return bestId;
}

std::optional<Point> FindBestAdjacentApproachTile(const Player &player, Point playerPosition, Point targetPosition)
{
	std::optional<Point> best;
	size_t bestPathLength = 0;
	int bestDistance = 0;

	std::optional<Point> bestFallback;
	int bestFallbackDistance = 0;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0)
				continue;

			const Point tile { targetPosition.x + dx, targetPosition.y + dy };
			if (!PosOkPlayer(player, tile))
				continue;

			const int distance = playerPosition.WalkingDistance(tile);

			if (!bestFallback || distance < bestFallbackDistance) {
				bestFallback = tile;
				bestFallbackDistance = distance;
			}

			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(player, playerPosition, tile);
			if (!path)
				continue;

			const size_t pathLength = path->size();
			if (!best || pathLength < bestPathLength || (pathLength == bestPathLength && distance < bestDistance)) {
				best = tile;
				bestPathLength = pathLength;
				bestDistance = distance;
			}
		}
	}

	if (best)
		return best;

	return bestFallback;
}

bool PosOkPlayerIgnoreDoors(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkable(position, /*ignoreDoors=*/true))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	if (dMonster[position.x][position.y] != 0) {
		if (leveltype == DTYPE_TOWN)
			return false;
		if (dMonster[position.x][position.y] <= 0)
			return false;
		if (!Monsters[dMonster[position.x][position.y] - 1].hasNoLife())
			return false;
	}

	return true;
}

[[nodiscard]] bool IsTileWalkableForTrackerPath(Point position, bool ignoreDoors, bool ignoreBreakables)
{
	Object *object = FindObjectAtPosition(position);
	if (object != nullptr) {
		if (ignoreDoors && object->isDoor()) {
			return true;
		}
		if (ignoreBreakables && object->_oSolidFlag && object->IsBreakable()) {
			return true;
		}
		if (object->_oSolidFlag) {
			return false;
		}
	}

	return IsTileNotSolid(position);
}

bool PosOkPlayerIgnoreMonsters(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/false, /*ignoreBreakables=*/false))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

bool PosOkPlayerIgnoreDoorsAndMonsters(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/true, /*ignoreBreakables=*/false))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

bool PosOkPlayerIgnoreDoorsMonstersAndBreakables(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/true, /*ignoreBreakables=*/true))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

std::optional<Point> FindBestApproachTileForObject(const Player &player, Point playerPosition, const Object &object)
{
	// Some interactable objects are placed on a walkable tile (e.g. floor switches). Prefer stepping on the tile in that case.
	if (!object._oSolidFlag && PosOkPlayer(player, object.position))
		return object.position;

	std::optional<Point> best;
	size_t bestPathLength = 0;
	int bestDistance = 0;

	std::optional<Point> bestFallback;
	int bestFallbackDistance = 0;

	const auto considerTile = [&](Point tile) {
		if (!PosOkPlayerIgnoreDoors(player, tile))
			return;

		const int distance = playerPosition.WalkingDistance(tile);
		if (!bestFallback || distance < bestFallbackDistance) {
			bestFallback = tile;
			bestFallbackDistance = distance;
		}

		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(player, playerPosition, tile);
		if (!path)
			return;

		const size_t pathLength = path->size();
		if (!best || pathLength < bestPathLength || (pathLength == bestPathLength && distance < bestDistance)) {
			best = tile;
			bestPathLength = pathLength;
			bestDistance = distance;
		}
	};

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0)
				continue;
			considerTile(object.position + Displacement { dx, dy });
		}
	}

	if (FindObjectAtPosition(object.position + Direction::NorthEast) == &object) {
		// Special case for large objects (e.g. sarcophagi): allow approaching from one tile further to the north.
		for (int dx = -1; dx <= 1; ++dx) {
			considerTile(object.position + Displacement { dx, -2 });
		}
	}

	if (best)
		return best;

	return bestFallback;
}

struct DoorBlockInfo {
	Point beforeDoor;
	Point doorPosition;
};

std::optional<DoorBlockInfo> FindFirstClosedDoorOnWalkPath(Point startPosition, const int8_t *path, int steps)
{
	Point position = startPosition;
	for (int i = 0; i < steps; ++i) {
		const Point next = NextPositionForWalkDirection(position, path[i]);
		Object *object = FindObjectAtPosition(next);
		if (object != nullptr && object->isDoor() && object->_oSolidFlag) {
			return DoorBlockInfo { .beforeDoor = position, .doorPosition = next };
		}
		position = next;
	}
	return std::nullopt;
}

enum class TrackerPathBlockType : uint8_t {
	Door,
	Monster,
	Breakable,
};

struct TrackerPathBlockInfo {
	TrackerPathBlockType type;
	size_t stepIndex;
	Point beforeBlock;
	Point blockPosition;
};

[[nodiscard]] std::optional<TrackerPathBlockInfo> FindFirstTrackerPathBlock(Point startPosition, const int8_t *path, size_t steps, bool considerDoors, bool considerMonsters, bool considerBreakables, Point targetPosition)
{
	Point position = startPosition;
	for (size_t i = 0; i < steps; ++i) {
		const Point next = NextPositionForWalkDirection(position, path[i]);
		if (next == targetPosition) {
			position = next;
			continue;
		}

		Object *object = FindObjectAtPosition(next);
		if (considerDoors && object != nullptr && object->isDoor() && object->_oSolidFlag) {
			return TrackerPathBlockInfo {
				.type = TrackerPathBlockType::Door,
				.stepIndex = i,
				.beforeBlock = position,
				.blockPosition = next,
			};
		}
		if (considerBreakables && object != nullptr && object->_oSolidFlag && object->IsBreakable()) {
			return TrackerPathBlockInfo {
				.type = TrackerPathBlockType::Breakable,
				.stepIndex = i,
				.beforeBlock = position,
				.blockPosition = next,
			};
		}

		if (considerMonsters && leveltype != DTYPE_TOWN && dMonster[next.x][next.y] != 0) {
			const int monsterRef = dMonster[next.x][next.y];
			const int monsterId = std::abs(monsterRef) - 1;
			const bool blocks = monsterRef <= 0 || (monsterId >= 0 && monsterId < static_cast<int>(MaxMonsters) && !Monsters[monsterId].hasNoLife());
			if (blocks) {
				return TrackerPathBlockInfo {
					.type = TrackerPathBlockType::Monster,
					.stepIndex = i,
					.beforeBlock = position,
					.blockPosition = next,
				};
			}
		}

		position = next;
	}

	return std::nullopt;
}

void NavigateToTrackerTargetKeyPressed()
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;
	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	EnsureTrackerLocksMatchCurrentLevel();

	const SDL_Keymod modState = SDL_GetModState();
	const bool cycleTarget = (modState & SDL_KMOD_SHIFT) != 0;
	const bool clearTarget = (modState & SDL_KMOD_CTRL) != 0;

	const Point playerPosition = MyPlayer->position.future;
	AutoWalkTrackerTargetId = -1;

	int &lockedTargetId = LockedTrackerTargetId(SelectedTrackerTargetCategory);
	if (clearTarget) {
		lockedTargetId = -1;
		SpeakText(_("Tracker target cleared."), true);
		return;
	}

	std::optional<int> targetId;
	std::optional<Point> targetPosition;
	std::optional<Point> alternateTargetPosition;
	StringOrView targetName;

	switch (SelectedTrackerTargetCategory) {
	case TrackerTargetCategory::Items: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyItemTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No items found."), true);
				else
					SpeakText(_("No next item."), true);
				return;
			}
		} else if (IsGroundItemPresent(lockedTargetId)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestGroundItemId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No items found."), true);
			return;
		}

		if (!IsGroundItemPresent(*targetId)) {
			lockedTargetId = -1;
			SpeakText(_("No items found."), true);
			return;
		}

		lockedTargetId = *targetId;
		const Item &tracked = Items[*targetId];

		targetName = tracked.getName();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		targetPosition = tracked.position;
		break;
	}
	case TrackerTargetCategory::Chests: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyChestTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No chests found."), true);
				else
					SpeakText(_("No next chest."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestUnopenedChestObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No chests found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedChestObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestUnopenedChestObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No chests found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Doors: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyDoorTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No doors found."), true);
				else
					SpeakText(_("No next door."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestDoorObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No doors found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedDoorObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestDoorObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No doors found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Shrines: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyShrineTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No shrines found."), true);
				else
					SpeakText(_("No next shrine."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestShrineObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No shrines found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsShrineLikeObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestShrineObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No shrines found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Objects: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyObjectInteractableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No objects found."), true);
				else
					SpeakText(_("No next object."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestMiscInteractableObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No objects found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedMiscInteractableObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestMiscInteractableObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No objects found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Breakables: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyBreakableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No breakables found."), true);
				else
					SpeakText(_("No next breakable."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestBreakableObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No breakables found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedBreakableObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestBreakableObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No breakables found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Monsters:
	default:
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyMonsterTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No monsters found."), true);
				else
					SpeakText(_("No next monster."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < static_cast<int>(MaxMonsters)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestMonsterId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No monsters found."), true);
			return;
		}

		const Monster &monster = Monsters[*targetId];
		if (monster.isInvalid || (monster.flags & MFLAG_HIDDEN) != 0 || monster.hitPoints <= 0) {
			lockedTargetId = -1;
			targetId = FindNearestMonsterId(playerPosition);
			if (!targetId) {
				SpeakText(_("No monsters found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Monster &tracked = Monsters[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position.tile;
		}
		break;
	}

	if (cycleTarget) {
		SpeakText(targetName.str(), /*force=*/true);
		return;
	}

	if (!targetPosition) {
		SpeakText(_("Can't find a nearby tile to walk to."), true);
		return;
	}

	Point chosenTargetPosition = *targetPosition;
	enum class TrackerPathMode : uint8_t {
		RespectDoorsAndMonsters,
		IgnoreDoors,
		IgnoreMonsters,
		IgnoreDoorsAndMonsters,
		Lenient,
	};

	auto findPathToTarget = [&](Point destination, TrackerPathMode mode) -> std::optional<std::vector<int8_t>> {
		const bool allowDestinationNonWalkable = !PosOkPlayer(*MyPlayer, destination);
		switch (mode) {
		case TrackerPathMode::RespectDoorsAndMonsters:
			return FindKeyboardWalkPathForSpeechRespectingDoors(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreDoors:
			return FindKeyboardWalkPathForSpeech(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreMonsters:
			return FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreDoorsAndMonsters:
			return FindKeyboardWalkPathForSpeechIgnoringMonsters(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::Lenient:
			return FindKeyboardWalkPathForSpeechLenient(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		default:
			return std::nullopt;
		}
	};

	std::optional<std::vector<int8_t>> spokenPath;
	bool pathIgnoresDoors = false;
	bool pathIgnoresMonsters = false;
	bool pathIgnoresBreakables = false;

	const auto considerDestination = [&](Point destination, TrackerPathMode mode) {
		const std::optional<std::vector<int8_t>> candidate = findPathToTarget(destination, mode);
		if (!candidate)
			return;
		if (!spokenPath || candidate->size() < spokenPath->size()) {
			spokenPath = *candidate;
			chosenTargetPosition = destination;

			pathIgnoresDoors = mode == TrackerPathMode::IgnoreDoors || mode == TrackerPathMode::IgnoreDoorsAndMonsters || mode == TrackerPathMode::Lenient;
			pathIgnoresMonsters = mode == TrackerPathMode::IgnoreMonsters || mode == TrackerPathMode::IgnoreDoorsAndMonsters || mode == TrackerPathMode::Lenient;
			pathIgnoresBreakables = mode == TrackerPathMode::Lenient;
		}
	};

	considerDestination(*targetPosition, TrackerPathMode::RespectDoorsAndMonsters);
	if (alternateTargetPosition)
		considerDestination(*alternateTargetPosition, TrackerPathMode::RespectDoorsAndMonsters);

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreDoors);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreDoors);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreMonsters);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreMonsters);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreDoorsAndMonsters);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreDoorsAndMonsters);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::Lenient);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::Lenient);
	}

	bool showUnreachableWarning = false;
	if (!spokenPath) {
		showUnreachableWarning = true;
		Point closestPosition;
		spokenPath = FindKeyboardWalkPathToClosestReachableForSpeech(*MyPlayer, playerPosition, chosenTargetPosition, closestPosition);
		pathIgnoresDoors = true;
		pathIgnoresMonsters = false;
		pathIgnoresBreakables = false;
	}

	if (spokenPath && !showUnreachableWarning && !PosOkPlayer(*MyPlayer, chosenTargetPosition)) {
		if (!spokenPath->empty())
			spokenPath->pop_back();
	}

	if (spokenPath && (pathIgnoresDoors || pathIgnoresMonsters || pathIgnoresBreakables)) {
		const std::optional<TrackerPathBlockInfo> block = FindFirstTrackerPathBlock(playerPosition, spokenPath->data(), spokenPath->size(), pathIgnoresDoors, pathIgnoresMonsters, pathIgnoresBreakables, chosenTargetPosition);
		if (block) {
			if (playerPosition.WalkingDistance(block->blockPosition) <= TrackerInteractDistanceTiles) {
				switch (block->type) {
				case TrackerPathBlockType::Door:
					SpeakText(_("A door is blocking the path. Open it and try again."), true);
					return;
				case TrackerPathBlockType::Monster:
					SpeakText(_("A monster is blocking the path. Clear it and try again."), true);
					return;
				case TrackerPathBlockType::Breakable:
					SpeakText(_("A breakable object is blocking the path. Destroy it and try again."), true);
					return;
				}
			}

			spokenPath = std::vector<int8_t>(spokenPath->begin(), spokenPath->begin() + block->stepIndex);
		}
	}

	std::string message;
	if (!targetName.empty())
		StrAppend(message, targetName, "\n");
	if (showUnreachableWarning) {
		message.append(_("Can't find a path to the target."));
		if (spokenPath && !spokenPath->empty())
			message.append("\n");
	}
	if (spokenPath) {
		if (!showUnreachableWarning || !spokenPath->empty())
			AppendKeyboardWalkPathForSpeech(message, *spokenPath);
	}

	SpeakText(message, true);
}

} // namespace

void UpdateAutoWalkTracker()
{
	if (AutoWalkTrackerTargetId < 0)
		return;
	if (leveltype == DTYPE_TOWN || IsPlayerInStore() || ChatLogFlag || HelpFlag) {
		AutoWalkTrackerTargetId = -1;
		return;
	}
	if (!CanPlayerTakeAction())
		return;

	if (MyPlayer == nullptr)
		return;
	if (MyPlayer->_pmode != PM_STAND)
		return;
	if (MyPlayer->walkpath[0] != WALK_NONE)
		return;
	if (MyPlayer->destAction != ACTION_NONE)
		return;

	Player &myPlayer = *MyPlayer;
	const Point playerPosition = myPlayer.position.future;

	std::optional<Point> destination;

	switch (AutoWalkTrackerTargetCategory) {
	case TrackerTargetCategory::Items: {
		const int itemId = AutoWalkTrackerTargetId;
		if (itemId < 0 || itemId > MAXITEMS) {
			AutoWalkTrackerTargetId = -1;
			return;
		}
		if (!IsGroundItemPresent(itemId)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target item is gone."), true);
			return;
		}
		const Item &item = Items[itemId];
		if (playerPosition.WalkingDistance(item.position) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Item in range."), true);
			return;
		}
		destination = item.position;
		break;
	}
	case TrackerTargetCategory::Chests: {
		const int objectId = AutoWalkTrackerTargetId;
		if (objectId < 0 || objectId >= MAXOBJECTS) {
			AutoWalkTrackerTargetId = -1;
			return;
		}
		const Object &object = Objects[objectId];
		if (!object.IsChest() || !object.canInteractWith()) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target chest is gone."), true);
			return;
		}
		if (playerPosition.WalkingDistance(object.position) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Chest in range."), true);
			return;
		}
		destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, object.position);
		break;
	}
	case TrackerTargetCategory::Monsters:
	default: {
		const int monsterId = AutoWalkTrackerTargetId;
		if (monsterId < 0 || monsterId >= static_cast<int>(MaxMonsters)) {
			AutoWalkTrackerTargetId = -1;
			return;
		}
		const Monster &monster = Monsters[monsterId];
		if (monster.isInvalid || (monster.flags & MFLAG_HIDDEN) != 0 || monster.hitPoints <= 0) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target monster is gone."), true);
			return;
		}
		const Point monsterPosition { monster.position.tile };
		if (playerPosition.WalkingDistance(monsterPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Monster in range."), true);
			return;
		}
		destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, monsterPosition);
		break;
	}
	}

	if (!destination) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_("Can't find a nearby tile to walk to."), true);
		return;
	}

	constexpr size_t MaxAutoWalkPathLength = 512;
	std::array<int8_t, MaxAutoWalkPathLength> path;
	path.fill(WALK_NONE);

	int steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, *destination, path.data(), path.size());
	if (steps == 0) {
		std::array<int8_t, MaxAutoWalkPathLength> ignoreDoorPath;
		ignoreDoorPath.fill(WALK_NONE);

		const int ignoreDoorSteps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayerIgnoreDoors(myPlayer, position); }, playerPosition, *destination, ignoreDoorPath.data(), ignoreDoorPath.size());
		if (ignoreDoorSteps != 0) {
			const std::optional<DoorBlockInfo> block = FindFirstClosedDoorOnWalkPath(playerPosition, ignoreDoorPath.data(), ignoreDoorSteps);
			if (block) {
				if (playerPosition.WalkingDistance(block->doorPosition) <= TrackerInteractDistanceTiles) {
					AutoWalkTrackerTargetId = -1;
					SpeakText(_("A door is blocking the path. Open it and try again."), true);
					return;
				}

				*destination = block->beforeDoor;
				path.fill(WALK_NONE);
				steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, *destination, path.data(), path.size());
			}
		}

		if (steps == 0) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Can't find a path to the target."), true);
			return;
		}
	}

	if (steps < static_cast<int>(MaxPathLengthPlayer)) {
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, *destination);
		return;
	}

	const int segmentSteps = std::min(steps - 1, static_cast<int>(MaxPathLengthPlayer - 1));
	const Point waypoint = PositionAfterWalkPathSteps(playerPosition, path.data(), segmentSteps);
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, waypoint);
}

void ListTownNpcsKeyPressed()
{
	if (leveltype != DTYPE_TOWN) {
		ResetTownNpcSelection();
		SpeakText(_("Not in town."), true);
		return;
	}
	if (IsPlayerInStore())
		return;

	std::vector<const Towner *> townNpcs;
	std::vector<const Towner *> cows;

	townNpcs.reserve(Towners.size());
	cows.reserve(Towners.size());

	const Point playerPosition = MyPlayer->position.future;

	for (const Towner &towner : Towners) {
		if (!IsTownerPresent(towner._ttype))
			continue;

		if (towner._ttype == TOWN_COW) {
			cows.push_back(&towner);
			continue;
		}

		townNpcs.push_back(&towner);
	}

	if (townNpcs.empty() && cows.empty()) {
		ResetTownNpcSelection();
		SpeakText(_("No town NPCs found."), true);
		return;
	}

	std::sort(townNpcs.begin(), townNpcs.end(), [&playerPosition](const Towner *a, const Towner *b) {
		const int distanceA = playerPosition.WalkingDistance(a->position);
		const int distanceB = playerPosition.WalkingDistance(b->position);
		if (distanceA != distanceB)
			return distanceA < distanceB;
		return a->name < b->name;
	});

	std::string output;
	StrAppend(output, _("Town NPCs:"));
	for (size_t i = 0; i < townNpcs.size(); ++i) {
		StrAppend(output, "\n", i + 1, ". ", townNpcs[i]->name);
	}
	if (!cows.empty()) {
		StrAppend(output, "\n", _("Cows: "), static_cast<int>(cows.size()));
	}

	RefreshTownNpcOrder(true);
	if (SelectedTownNpc >= 0 && SelectedTownNpc < static_cast<int>(GetNumTowners())) {
		const Towner &towner = Towners[SelectedTownNpc];
		StrAppend(output, "\n", _("Selected: "), towner.name);
		StrAppend(output, "\n", _("PageUp/PageDown: select. Home: go. End: repeat."));
	}
	const std::string_view exitKey = GetOptions().Keymapper.KeyNameForAction("SpeakNearestExit");
	if (!exitKey.empty()) {
		StrAppend(output, "\n", fmt::format(fmt::runtime(_("Cathedral entrance: press {:s}.")), exitKey));
	}

	SpeakText(output, true);
}

namespace {

using PosOkForSpeechFn = bool (*)(const Player &, Point);

template <size_t NumDirections>
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechBfs(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, const std::array<int8_t, NumDirections> &walkDirections, bool allowDiagonalSteps, bool allowDestinationNonWalkable)
{
	if (!InDungeonBounds(startPosition) || !InDungeonBounds(destinationPosition))
		return std::nullopt;

	if (startPosition == destinationPosition)
		return std::vector<int8_t> {};

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::array<int8_t, MAXDUNX * MAXDUNY> parentDir {};
	parentDir.fill(WALK_NONE);

	std::queue<Point> queue;

	const auto indexOf = [](Point position) -> size_t {
		return static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
	};

	const auto enqueue = [&](Point current, int8_t dir) {
		const Point next = NextPositionForWalkDirection(current, dir);
		if (!InDungeonBounds(next))
			return;

		const size_t idx = indexOf(next);
		if (visited[idx])
			return;

		const bool ok = posOk(player, next);
		if (ok) {
			if (!CanStep(current, next))
				return;
		} else {
			if (!allowDestinationNonWalkable || next != destinationPosition)
				return;
		}

		visited[idx] = true;
		parentDir[idx] = dir;
		queue.push(next);
	};

	visited[indexOf(startPosition)] = true;
	queue.push(startPosition);

	const auto hasReachedDestination = [&]() -> bool {
		return visited[indexOf(destinationPosition)];
	};

	while (!queue.empty() && !hasReachedDestination()) {
		const Point current = queue.front();
		queue.pop();

		const Displacement delta = destinationPosition - current;
		const int deltaAbsX = delta.deltaX >= 0 ? delta.deltaX : -delta.deltaX;
		const int deltaAbsY = delta.deltaY >= 0 ? delta.deltaY : -delta.deltaY;

		std::array<int8_t, 8> prioritizedDirs;
		size_t prioritizedCount = 0;

		const auto addUniqueDir = [&](int8_t dir) {
			if (dir == WALK_NONE)
				return;
			for (size_t i = 0; i < prioritizedCount; ++i) {
				if (prioritizedDirs[i] == dir)
					return;
			}
			prioritizedDirs[prioritizedCount++] = dir;
		};

		const int8_t xDir = delta.deltaX > 0 ? WALK_SE : (delta.deltaX < 0 ? WALK_NW : WALK_NONE);
		const int8_t yDir = delta.deltaY > 0 ? WALK_SW : (delta.deltaY < 0 ? WALK_NE : WALK_NONE);

		if (allowDiagonalSteps && delta.deltaX != 0 && delta.deltaY != 0) {
			const int8_t diagDir =
			    delta.deltaX > 0 ? (delta.deltaY > 0 ? WALK_S : WALK_E) : (delta.deltaY > 0 ? WALK_W : WALK_N);
			addUniqueDir(diagDir);
		}

		if (deltaAbsX >= deltaAbsY) {
			addUniqueDir(xDir);
			addUniqueDir(yDir);
		} else {
			addUniqueDir(yDir);
			addUniqueDir(xDir);
		}
		for (const int8_t dir : walkDirections) {
			addUniqueDir(dir);
		}

		for (size_t i = 0; i < prioritizedCount; ++i) {
			enqueue(current, prioritizedDirs[i]);
		}
	}

	if (!hasReachedDestination())
		return std::nullopt;

	std::vector<int8_t> path;
	Point position = destinationPosition;
	while (position != startPosition) {
		const int8_t dir = parentDir[indexOf(position)];
		if (dir == WALK_NONE)
			return std::nullopt;

		path.push_back(dir);
		position = NextPositionForWalkDirection(position, OppositeWalkDirection(dir));
	}

	std::reverse(path.begin(), path.end());
	return path;
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechWithPosOk(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, bool allowDestinationNonWalkable)
{
	constexpr std::array<int8_t, 4> AxisDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
	};

	constexpr std::array<int8_t, 8> AllDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
		WALK_N,
		WALK_E,
		WALK_S,
		WALK_W,
	};

	if (const std::optional<std::vector<int8_t>> axisPath = FindKeyboardWalkPathForSpeechBfs(player, startPosition, destinationPosition, posOk, AxisDirections, /*allowDiagonalSteps=*/false, allowDestinationNonWalkable); axisPath) {
		return axisPath;
	}

	return FindKeyboardWalkPathForSpeechBfs(player, startPosition, destinationPosition, posOk, AllDirections, /*allowDiagonalSteps=*/true, allowDestinationNonWalkable);
}

} // namespace

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeech(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoors(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayer, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoorsAndMonsters, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreMonsters, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechLenient(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoorsMonstersAndBreakables, allowDestinationNonWalkable);
}

namespace {

template <size_t NumDirections>
std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeechBfs(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, const std::array<int8_t, NumDirections> &walkDirections, bool allowDiagonalSteps, Point &closestPosition)
{
	if (!InDungeonBounds(startPosition) || !InDungeonBounds(destinationPosition))
		return std::nullopt;

	if (startPosition == destinationPosition) {
		closestPosition = destinationPosition;
		return std::vector<int8_t> {};
	}

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::array<int8_t, MAXDUNX * MAXDUNY> parentDir {};
	std::array<uint16_t, MAXDUNX * MAXDUNY> depth {};
	parentDir.fill(WALK_NONE);
	depth.fill(0);

	std::queue<Point> queue;

	const auto indexOf = [](Point position) -> size_t {
		return static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
	};

	const auto enqueue = [&](Point current, int8_t dir) {
		const Point next = NextPositionForWalkDirection(current, dir);
		if (!InDungeonBounds(next))
			return;

		const size_t nextIdx = indexOf(next);
		if (visited[nextIdx])
			return;

		if (!posOk(player, next))
			return;
		if (!CanStep(current, next))
			return;

		const size_t currentIdx = indexOf(current);
		visited[nextIdx] = true;
		parentDir[nextIdx] = dir;
		depth[nextIdx] = static_cast<uint16_t>(depth[currentIdx] + 1);
		queue.push(next);
	};

	const size_t startIdx = indexOf(startPosition);
	visited[startIdx] = true;
	queue.push(startPosition);

	Point best = startPosition;
	int bestDistance = startPosition.WalkingDistance(destinationPosition);
	uint16_t bestDepth = 0;

	const auto considerBest = [&](Point position) {
		const int distance = position.WalkingDistance(destinationPosition);
		const uint16_t posDepth = depth[indexOf(position)];
		if (distance < bestDistance || (distance == bestDistance && posDepth < bestDepth)) {
			best = position;
			bestDistance = distance;
			bestDepth = posDepth;
		}
	};

	while (!queue.empty()) {
		const Point current = queue.front();
		queue.pop();

		considerBest(current);

		const Displacement delta = destinationPosition - current;
		const int deltaAbsX = delta.deltaX >= 0 ? delta.deltaX : -delta.deltaX;
		const int deltaAbsY = delta.deltaY >= 0 ? delta.deltaY : -delta.deltaY;

		std::array<int8_t, 8> prioritizedDirs;
		size_t prioritizedCount = 0;

		const auto addUniqueDir = [&](int8_t dir) {
			if (dir == WALK_NONE)
				return;
			for (size_t i = 0; i < prioritizedCount; ++i) {
				if (prioritizedDirs[i] == dir)
					return;
			}
			prioritizedDirs[prioritizedCount++] = dir;
		};

		const int8_t xDir = delta.deltaX > 0 ? WALK_SE : (delta.deltaX < 0 ? WALK_NW : WALK_NONE);
		const int8_t yDir = delta.deltaY > 0 ? WALK_SW : (delta.deltaY < 0 ? WALK_NE : WALK_NONE);

		if (allowDiagonalSteps && delta.deltaX != 0 && delta.deltaY != 0) {
			const int8_t diagDir =
			    delta.deltaX > 0 ? (delta.deltaY > 0 ? WALK_S : WALK_E) : (delta.deltaY > 0 ? WALK_W : WALK_N);
			addUniqueDir(diagDir);
		}

		if (deltaAbsX >= deltaAbsY) {
			addUniqueDir(xDir);
			addUniqueDir(yDir);
		} else {
			addUniqueDir(yDir);
			addUniqueDir(xDir);
		}
		for (const int8_t dir : walkDirections) {
			addUniqueDir(dir);
		}

		for (size_t i = 0; i < prioritizedCount; ++i) {
			enqueue(current, prioritizedDirs[i]);
		}
	}

	closestPosition = best;
	if (best == startPosition)
		return std::vector<int8_t> {};

	std::vector<int8_t> path;
	Point position = best;
	while (position != startPosition) {
		const int8_t dir = parentDir[indexOf(position)];
		if (dir == WALK_NONE)
			return std::nullopt;

		path.push_back(dir);
		position = NextPositionForWalkDirection(position, OppositeWalkDirection(dir));
	}

	std::reverse(path.begin(), path.end());
	return path;
}

} // namespace

std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeech(const Player &player, Point startPosition, Point destinationPosition, Point &closestPosition)
{
	constexpr std::array<int8_t, 4> AxisDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
	};

	constexpr std::array<int8_t, 8> AllDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
		WALK_N,
		WALK_E,
		WALK_S,
		WALK_W,
	};

	Point axisClosest;
	const std::optional<std::vector<int8_t>> axisPath = FindKeyboardWalkPathToClosestReachableForSpeechBfs(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, AxisDirections, /*allowDiagonalSteps=*/false, axisClosest);

	Point diagClosest;
	const std::optional<std::vector<int8_t>> diagPath = FindKeyboardWalkPathToClosestReachableForSpeechBfs(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, AllDirections, /*allowDiagonalSteps=*/true, diagClosest);

	if (!axisPath && !diagPath)
		return std::nullopt;
	if (!axisPath) {
		closestPosition = diagClosest;
		return diagPath;
	}
	if (!diagPath) {
		closestPosition = axisClosest;
		return axisPath;
	}

	const int axisDistance = axisClosest.WalkingDistance(destinationPosition);
	const int diagDistance = diagClosest.WalkingDistance(destinationPosition);
	if (diagDistance < axisDistance) {
		closestPosition = diagClosest;
		return diagPath;
	}

	closestPosition = axisClosest;
	return axisPath;
}

void AppendKeyboardWalkPathForSpeech(std::string &message, const std::vector<int8_t> &path)
{
	if (path.empty()) {
		message.append(_("here"));
		return;
	}

	bool any = false;
	const auto appendPart = [&](std::string_view label, int distance) {
		if (distance == 0)
			return;
		if (any)
			message.append(", ");
		StrAppend(message, label, " ", distance);
		any = true;
	};

	const auto labelForWalkDirection = [](int8_t dir) -> std::string_view {
		switch (dir) {
		case WALK_NE:
			return _("north");
		case WALK_SW:
			return _("south");
		case WALK_SE:
			return _("east");
		case WALK_NW:
			return _("west");
		case WALK_N:
			return _("northwest");
		case WALK_E:
			return _("northeast");
		case WALK_S:
			return _("southeast");
		case WALK_W:
			return _("southwest");
		default:
			return {};
		}
	};

	int8_t currentDir = path.front();
	int runLength = 1;
	for (size_t i = 1; i < path.size(); ++i) {
		if (path[i] == currentDir) {
			++runLength;
			continue;
		}

		const std::string_view label = labelForWalkDirection(currentDir);
		if (!label.empty())
			appendPart(label, runLength);

		currentDir = path[i];
		runLength = 1;
	}

	const std::string_view label = labelForWalkDirection(currentDir);
	if (!label.empty())
		appendPart(label, runLength);

	if (!any)
		message.append(_("here"));
}

void AppendDirectionalFallback(std::string &message, const Displacement &delta)
{
	bool any = false;
	const auto appendPart = [&](std::string_view label, int distance) {
		if (distance == 0)
			return;
		if (any)
			message.append(", ");
		StrAppend(message, label, " ", distance);
		any = true;
	};

	if (delta.deltaY < 0)
		appendPart(_("north"), -delta.deltaY);
	else if (delta.deltaY > 0)
		appendPart(_("south"), delta.deltaY);

	if (delta.deltaX > 0)
		appendPart(_("east"), delta.deltaX);
	else if (delta.deltaX < 0)
		appendPart(_("west"), -delta.deltaX);

	if (!any)
		message.append(_("here"));
}

std::optional<Point> FindNearestUnexploredTile(Point startPosition)
{
	if (!InDungeonBounds(startPosition))
		return std::nullopt;

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::queue<Point> queue;

	const auto enqueue = [&](Point position) {
		if (!InDungeonBounds(position))
			return;

		const size_t index = static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
		if (visited[index])
			return;

		if (!IsTileWalkable(position, /*ignoreDoors=*/true))
			return;

		visited[index] = true;
		queue.push(position);
	};

	enqueue(startPosition);

	constexpr std::array<Direction, 4> Neighbors = {
		Direction::NorthEast,
		Direction::SouthWest,
		Direction::SouthEast,
		Direction::NorthWest,
	};

	while (!queue.empty()) {
		const Point position = queue.front();
		queue.pop();

		if (!HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Explored))
			return position;

		for (const Direction dir : Neighbors) {
			enqueue(position + dir);
		}
	}

	return std::nullopt;
}

std::string TriggerLabelForSpeech(const TriggerStruct &trigger)
{
	switch (trigger._tmsg) {
	case WM_DIABNEXTLVL:
		if (leveltype == DTYPE_TOWN)
			return std::string { _("Cathedral entrance") };
		return std::string { _("Stairs down") };
	case WM_DIABPREVLVL:
		return std::string { _("Stairs up") };
	case WM_DIABTOWNWARP:
		switch (trigger._tlvl) {
		case 5:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Catacombs"));
		case 9:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Caves"));
		case 13:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Hell"));
		case 17:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Nest"));
		case 21:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Crypt"));
		default:
			return fmt::format(fmt::runtime(_("Town warp to level {:d}")), trigger._tlvl);
		}
	case WM_DIABTWARPUP:
		return std::string { _("Warp up") };
	case WM_DIABRETOWN:
		return std::string { _("Return to town") };
	case WM_DIABWARPLVL:
		return std::string { _("Warp") };
	case WM_DIABSETLVL:
		return std::string { _("Set level") };
	case WM_DIABRTNLVL:
		return std::string { _("Return level") };
	default:
		return std::string { _("Exit") };
	}
}

std::optional<int> LockedTownDungeonTriggerIndex;

std::vector<int> CollectTownDungeonTriggerIndices()
{
	std::vector<int> result;
	result.reserve(static_cast<size_t>(std::max(0, numtrigs)));

	for (int i = 0; i < numtrigs; ++i) {
		if (IsAnyOf(trigs[i]._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP))
			result.push_back(i);
	}

	std::sort(result.begin(), result.end(), [](int a, int b) {
		const TriggerStruct &ta = trigs[a];
		const TriggerStruct &tb = trigs[b];

		const int kindA = ta._tmsg == WM_DIABNEXTLVL ? 0 : (ta._tmsg == WM_DIABTOWNWARP ? 1 : 2);
		const int kindB = tb._tmsg == WM_DIABNEXTLVL ? 0 : (tb._tmsg == WM_DIABTOWNWARP ? 1 : 2);
		if (kindA != kindB)
			return kindA < kindB;

		if (ta._tmsg == WM_DIABTOWNWARP && tb._tmsg == WM_DIABTOWNWARP && ta._tlvl != tb._tlvl)
			return ta._tlvl < tb._tlvl;

		return a < b;
	});

	return result;
}

std::optional<int> FindDefaultTownDungeonTriggerIndex(const std::vector<int> &candidates)
{
	for (const int index : candidates) {
		if (trigs[index]._tmsg == WM_DIABNEXTLVL)
			return index;
	}
	if (!candidates.empty())
		return candidates.front();
	return std::nullopt;
}

std::optional<int> FindLockedTownDungeonTriggerIndex(const std::vector<int> &candidates)
{
	if (!LockedTownDungeonTriggerIndex)
		return std::nullopt;
	if (std::find(candidates.begin(), candidates.end(), *LockedTownDungeonTriggerIndex) != candidates.end())
		return *LockedTownDungeonTriggerIndex;
	return std::nullopt;
}

std::optional<int> FindNextTownDungeonTriggerIndex(const std::vector<int> &candidates, int current)
{
	if (candidates.empty())
		return std::nullopt;

	const auto it = std::find(candidates.begin(), candidates.end(), current);
	if (it == candidates.end())
		return candidates.front();
	if (std::next(it) == candidates.end())
		return candidates.front();
	return *std::next(it);
}

std::optional<int> FindPreferredExitTriggerIndex()
{
	if (numtrigs <= 0)
		return std::nullopt;

	if (leveltype == DTYPE_TOWN && MyPlayer != nullptr) {
		const Point playerPosition = MyPlayer->position.future;
		std::optional<int> bestIndex;
		int bestDistance = 0;

		for (int i = 0; i < numtrigs; ++i) {
			if (!IsAnyOf(trigs[i]._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP))
				continue;

			const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
			const int distance = playerPosition.WalkingDistance(triggerPosition);
			if (!bestIndex || distance < bestDistance) {
				bestIndex = i;
				bestDistance = distance;
			}
		}

		if (bestIndex)
			return bestIndex;
	}

	const Point playerPosition = MyPlayer->position.future;
	std::optional<int> bestIndex;
	int bestDistance = 0;

	for (int i = 0; i < numtrigs; ++i) {
		const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		if (!bestIndex || distance < bestDistance) {
			bestIndex = i;
			bestDistance = distance;
		}
	}

	return bestIndex;
}

std::optional<int> FindNearestTriggerIndexWithMessage(int message)
{
	if (numtrigs <= 0 || MyPlayer == nullptr)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	std::optional<int> bestIndex;
	int bestDistance = 0;

	for (int i = 0; i < numtrigs; ++i) {
		if (trigs[i]._tmsg != message)
			continue;

		const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		if (!bestIndex || distance < bestDistance) {
			bestIndex = i;
			bestDistance = distance;
		}
	}

	return bestIndex;
}

std::optional<Point> FindNearestTownPortalOnCurrentLevel()
{
	if (MyPlayer == nullptr || leveltype == DTYPE_TOWN)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	const int currentLevel = setlevel ? static_cast<int>(setlvlnum) : currlevel;

	std::optional<Point> bestPosition;
	int bestDistance = 0;

	for (int i = 0; i < MAXPORTAL; ++i) {
		const Portal &portal = Portals[i];
		if (!portal.open)
			continue;
		if (portal.setlvl != setlevel)
			continue;
		if (portal.level != currentLevel)
			continue;

		const int distance = playerPosition.WalkingDistance(portal.position);
		if (!bestPosition || distance < bestDistance) {
			bestPosition = portal.position;
			bestDistance = distance;
		}
	}

	return bestPosition;
}

struct TownPortalInTown {
	int portalIndex;
	Point position;
	int distance;
};

std::optional<TownPortalInTown> FindNearestTownPortalInTown()
{
	if (MyPlayer == nullptr || leveltype != DTYPE_TOWN)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;

	std::optional<TownPortalInTown> best;
	int bestDistance = 0;

	for (const Missile &missile : Missiles) {
		if (missile._mitype != MissileID::TownPortal)
			continue;
		if (missile._misource < 0 || missile._misource >= MAXPORTAL)
			continue;
		if (!Portals[missile._misource].open)
			continue;

		const Point portalPosition = missile.position.tile;
		const int distance = playerPosition.WalkingDistance(portalPosition);
		if (!best || distance < bestDistance) {
			best = TownPortalInTown {
				.portalIndex = missile._misource,
				.position = portalPosition,
				.distance = distance,
			};
			bestDistance = distance;
		}
	}

	return best;
}

[[nodiscard]] std::string TownPortalLabelForSpeech(const Portal &portal)
{
	if (portal.level <= 0)
		return std::string { _("Town portal") };

	if (portal.setlvl) {
		const auto questLevel = static_cast<_setlevels>(portal.level);
		const char *questLevelName = QuestLevelNames[questLevel];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			return std::string { _("Town portal to set level") };

		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a set/quest level name. */ "Town portal to {:s}")), _(questLevelName));
	}

	constexpr std::array<const char *, DTYPE_LAST + 1> DungeonStrs = {
		N_("Town"),
		N_("Cathedral"),
		N_("Catacombs"),
		N_("Caves"),
		N_("Hell"),
		N_("Nest"),
		N_("Crypt"),
	};
	std::string dungeonStr;
	if (portal.ltype >= DTYPE_TOWN && portal.ltype <= DTYPE_LAST) {
		dungeonStr = _(DungeonStrs[static_cast<size_t>(portal.ltype)]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	int floor = portal.level;
	if (portal.ltype == DTYPE_CATACOMBS)
		floor -= 4;
	else if (portal.ltype == DTYPE_CAVES)
		floor -= 8;
	else if (portal.ltype == DTYPE_HELL)
		floor -= 12;
	else if (portal.ltype == DTYPE_NEST)
		floor -= 16;
	else if (portal.ltype == DTYPE_CRYPT)
		floor -= 20;

	if (floor > 0)
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a dungeon name and {:d} is a floor number. */ "Town portal to {:s} {:d}")), dungeonStr, floor);

	return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a dungeon name. */ "Town portal to {:s}")), dungeonStr);
}

struct QuestSetLevelEntrance {
	_setlevels questLevel;
	Point entrancePosition;
	int distance;
};

std::optional<QuestSetLevelEntrance> FindNearestQuestSetLevelEntranceOnCurrentLevel()
{
	if (MyPlayer == nullptr || setlevel)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	std::optional<QuestSetLevelEntrance> best;
	int bestDistance = 0;

	for (const Quest &quest : Quests) {
		if (quest._qslvl == SL_NONE)
			continue;
		if (quest._qactive == QUEST_NOTAVAIL)
			continue;
		if (quest._qlevel != currlevel)
			continue;
		if (!InDungeonBounds(quest.position))
			continue;

		const int distance = playerPosition.WalkingDistance(quest.position);
		if (!best || distance < bestDistance) {
			best = QuestSetLevelEntrance {
				.questLevel = quest._qslvl,
				.entrancePosition = quest.position,
				.distance = distance,
			};
			bestDistance = distance;
		}
	}

	return best;
}

void SpeakNearestExitKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const Point startPosition = MyPlayer->position.future;

	const SDL_Keymod modState = SDL_GetModState();
	const bool seekQuestEntrance = (modState & SDL_KMOD_SHIFT) != 0;
	const bool cycleTownDungeon = (modState & SDL_KMOD_CTRL) != 0;

	if (seekQuestEntrance) {
		if (const std::optional<QuestSetLevelEntrance> entrance = FindNearestQuestSetLevelEntranceOnCurrentLevel(); entrance) {
			const Point targetPosition = entrance->entrancePosition;
			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);

			std::string message { _(QuestLevelNames[entrance->questLevel]) };
			message.append(": ");
			if (!path)
				AppendDirectionalFallback(message, targetPosition - startPosition);
			else
				AppendKeyboardWalkPathForSpeech(message, *path);
			SpeakText(message, true);
			return;
		}

		SpeakText(_("No quest entrances found."), true);
		return;
	}

	if (leveltype == DTYPE_TOWN) {
		const std::vector<int> dungeonCandidates = CollectTownDungeonTriggerIndices();
		if (dungeonCandidates.empty()) {
			SpeakText(_("No exits found."), true);
			return;
		}

		if (cycleTownDungeon) {
			if (dungeonCandidates.size() <= 1) {
				SpeakText(_("No other dungeon entrances found."), true);
				return;
			}

			const int current = LockedTownDungeonTriggerIndex.value_or(-1);
			const std::optional<int> next = FindNextTownDungeonTriggerIndex(dungeonCandidates, current);
			if (!next) {
				SpeakText(_("No other dungeon entrances found."), true);
				return;
			}

			LockedTownDungeonTriggerIndex = *next;
			const std::string label = TriggerLabelForSpeech(trigs[*next]);
			if (!label.empty())
				SpeakText(label, true);
			return;
		}

		const int triggerIndex = FindLockedTownDungeonTriggerIndex(dungeonCandidates)
		    .value_or(FindDefaultTownDungeonTriggerIndex(dungeonCandidates).value_or(dungeonCandidates.front()));
		LockedTownDungeonTriggerIndex = triggerIndex;

		const TriggerStruct &trigger = trigs[triggerIndex];
		const Point targetPosition { trigger.position.x, trigger.position.y };

		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
		std::string message = TriggerLabelForSpeech(trigger);
		if (!message.empty())
			message.append(": ");
		if (!path)
			AppendDirectionalFallback(message, targetPosition - startPosition);
		else
			AppendKeyboardWalkPathForSpeech(message, *path);

		SpeakText(message, true);
		return;
	}

	if (leveltype != DTYPE_TOWN) {
		if (const std::optional<Point> portalPosition = FindNearestTownPortalOnCurrentLevel(); portalPosition) {
			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, *portalPosition);
			std::string message { _("Return to town") };
			message.append(": ");
			if (!path)
				AppendDirectionalFallback(message, *portalPosition - startPosition);
			else
				AppendKeyboardWalkPathForSpeech(message, *path);
			SpeakText(message, true);
			return;
		}

		const std::optional<int> triggerIndex = FindNearestTriggerIndexWithMessage(WM_DIABPREVLVL);
		if (!triggerIndex) {
			SpeakText(_("No exits found."), true);
			return;
		}

		const TriggerStruct &trigger = trigs[*triggerIndex];
		const Point targetPosition { trigger.position.x, trigger.position.y };
		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
		std::string message = TriggerLabelForSpeech(trigger);
		if (!message.empty())
			message.append(": ");
		if (!path)
			AppendDirectionalFallback(message, targetPosition - startPosition);
		else
			AppendKeyboardWalkPathForSpeech(message, *path);
		SpeakText(message, true);
		return;
	}

	const std::optional<int> triggerIndex = FindPreferredExitTriggerIndex();
	if (!triggerIndex) {
		SpeakText(_("No exits found."), true);
		return;
	}

	const TriggerStruct &trigger = trigs[*triggerIndex];
	const Point targetPosition { trigger.position.x, trigger.position.y };

	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
	std::string message = TriggerLabelForSpeech(trigger);
	if (!message.empty())
		message.append(": ");
	if (!path)
		AppendDirectionalFallback(message, targetPosition - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);

	SpeakText(message, true);
}

void SpeakNearestTownPortalInTownKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (leveltype != DTYPE_TOWN) {
		SpeakText(_("Not in town."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const std::optional<TownPortalInTown> portal = FindNearestTownPortalInTown();
	if (!portal) {
		SpeakText(_("No town portals found."), true);
		return;
	}

	const Point startPosition = MyPlayer->position.future;
	const Point targetPosition = portal->position;

	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);

	std::string message = TownPortalLabelForSpeech(Portals[portal->portalIndex]);
	message.append(": ");
	if (!path)
		AppendDirectionalFallback(message, targetPosition - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);

	SpeakText(message, true);
}

void SpeakNearestStairsKeyPressed(int triggerMessage)
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const std::optional<int> triggerIndex = FindNearestTriggerIndexWithMessage(triggerMessage);
	if (!triggerIndex) {
		SpeakText(_("No exits found."), true);
		return;
	}

	const TriggerStruct &trigger = trigs[*triggerIndex];
	const Point startPosition = MyPlayer->position.future;
	const Point targetPosition { trigger.position.x, trigger.position.y };

	std::string message;
	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
	if (!path) {
		AppendDirectionalFallback(message, targetPosition - startPosition);
	} else {
		AppendKeyboardWalkPathForSpeech(message, *path);
	}

	SpeakText(message, true);
}

void SpeakNearestStairsDownKeyPressed()
{
	SpeakNearestStairsKeyPressed(WM_DIABNEXTLVL);
}

void SpeakNearestStairsUpKeyPressed()
{
	SpeakNearestStairsKeyPressed(WM_DIABPREVLVL);
}

bool IsKeyboardWalkAllowed()
{
	return CanPlayerTakeAction()
	    && !InGameMenu()
	    && !IsPlayerInStore()
	    && !QuestLogIsOpen
	    && !HelpFlag
	    && !ChatLogFlag
	    && !ChatFlag
	    && !DropGoldFlag
	    && !IsStashOpen
	    && !IsWithdrawGoldOpen
	    && !AutomapActive
	    && !invflag
	    && !CharFlag
	    && !SpellbookFlag
	    && !SpellSelectFlag
	    && !qtextflag;
}

void KeyboardWalkKeyPressed(Direction direction)
{
	if (!IsKeyboardWalkAllowed())
		return;

	if (MyPlayer == nullptr)
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, MyPlayer->position.future + direction);
}

void KeyboardWalkNorthKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::NorthEast);
}

void KeyboardWalkSouthKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::SouthWest);
}

void KeyboardWalkEastKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::SouthEast);
}

void KeyboardWalkWestKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::NorthWest);
}

void SpeakNearestUnexploredTileKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const Point startPosition = MyPlayer->position.future;
	const std::optional<Point> target = FindNearestUnexploredTile(startPosition);
	if (!target) {
		SpeakText(_("No unexplored areas found."), true);
		return;
	}
	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, *target);
	std::string message;
	if (!path)
		AppendDirectionalFallback(message, *target - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);

	SpeakText(message, true);
}

void SpeakPlayerHealthPercentageKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const int maxHp = MyPlayer->_pMaxHP;
	if (maxHp <= 0)
		return;

	const int currentHp = std::max(MyPlayer->_pHitPoints, 0);
	int hpPercent = static_cast<int>((static_cast<int64_t>(currentHp) * 100 + maxHp / 2) / maxHp);
	hpPercent = std::clamp(hpPercent, 0, 100);
	SpeakText(fmt::format("{:d}%", hpPercent), /*force=*/true);
}

void SpeakExperienceToNextLevelKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const Player &myPlayer = *MyPlayer;
	if (myPlayer.isMaxCharacterLevel()) {
		SpeakText(_("Max level."), /*force=*/true);
		return;
	}

	const uint32_t nextExperienceThreshold = myPlayer.getNextExperienceThreshold();
	const uint32_t currentExperience = myPlayer._pExperience;
	const uint32_t remainingExperience = currentExperience >= nextExperienceThreshold ? 0 : nextExperienceThreshold - currentExperience;
	const int nextLevel = myPlayer.getCharacterLevel() + 1;
	SpeakText(
	    fmt::format(fmt::runtime(_("{:s} to Level {:d}")), FormatInteger(remainingExperience), nextLevel),
	    /*force=*/true);
}

namespace {
std::string BuildCurrentLocationForSpeech()
{
	// Quest Level Name
	if (setlevel) {
		const char *const questLevelName = QuestLevelNames[setlvlnum];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			return std::string { _("Set level") };

		return fmt::format("{:s}: {:s}", _("Set level"), _(questLevelName));
	}

	// Dungeon Name
	constexpr std::array<const char *, DTYPE_LAST + 1> DungeonStrs = {
		N_("Town"),
		N_("Cathedral"),
		N_("Catacombs"),
		N_("Caves"),
		N_("Hell"),
		N_("Nest"),
		N_("Crypt"),
	};
	std::string dungeonStr;
	if (leveltype >= DTYPE_TOWN && leveltype <= DTYPE_LAST) {
		dungeonStr = _(DungeonStrs[static_cast<size_t>(leveltype)]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	if (leveltype == DTYPE_TOWN || currlevel <= 0)
		return dungeonStr;

	// Dungeon Level
	int level = currlevel;
	if (leveltype == DTYPE_CATACOMBS)
		level -= 4;
	else if (leveltype == DTYPE_CAVES)
		level -= 8;
	else if (leveltype == DTYPE_HELL)
		level -= 12;
	else if (leveltype == DTYPE_NEST)
		level -= 16;
	else if (leveltype == DTYPE_CRYPT)
		level -= 20;

	if (level <= 0)
		return dungeonStr;

	return fmt::format(fmt::runtime(_(/* TRANSLATORS: dungeon type and floor number i.e. "Cathedral 3"*/ "{} {}")), dungeonStr, level);
}
} // namespace

void SpeakCurrentLocationKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;

	SpeakText(BuildCurrentLocationForSpeech(), /*force=*/true);
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
	if (invflag)
		FocusOnInventory();
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

void PartyPanelSideToggleKeyPressed()
{
	PartySidePanelOpen = !PartySidePanelOpen;
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

void SpeakSelectedSpeedbookSpell()
{
	for (const auto &spellListItem : GetSpellListItems()) {
		if (spellListItem.isSelected) {
			SpeakText(pgettext("spell", GetSpellData(spellListItem.id).sNameText), /*force=*/true);
			return;
		}
	}
	SpeakText(_("No spell selected."), /*force=*/true);
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
		SpeakSelectedSpeedbookSpell();
	} else {
		SpellSelectFlag = false;
	}
	LastPlayerAction = PlayerActionType::None;
}

void SpellBookKeyPressed()
{
	if (IsPlayerInStore())
		return;
	SpellbookFlag = !SpellbookFlag;
	if (SpellbookFlag && MyPlayer != nullptr) {
		const Player &player = *MyPlayer;
		if (IsValidSpell(player._pRSpell)) {
			SpeakText(pgettext("spell", GetSpellData(player._pRSpell).sNameText), /*force=*/true);
		} else {
			SpeakText(_("No spell selected."), /*force=*/true);
		}
	}
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
	effects_cleanup_sfx(false);
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
			    const Player &myPlayer = *MyPlayer;
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
	    "ItemHighlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    SDLK_LALT,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	options.Keymapper.AddAction(
	    "ToggleItemHighlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    SDLK_RCTRL,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	options.Keymapper.AddAction(
	    "ToggleAutomap",
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
	    SDLK_M,
	    CycleAutomapType,
	    nullptr,
	    IsGameRunning);

	options.Keymapper.AddAction(
	    "ListTownNpcs",
	    N_("List town NPCs"),
	    N_("Speaks a list of town NPCs."),
	    SDLK_F4,
	    ListTownNpcsKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "PreviousTownNpc",
	    N_("Previous town NPC"),
	    N_("Select previous town NPC (speaks)."),
	    SDLK_PAGEUP,
	    SelectPreviousTownNpcKeyPressed,
	    nullptr,
	    IsTownNpcActionAllowed);
	options.Keymapper.AddAction(
	    "NextTownNpc",
	    N_("Next town NPC"),
	    N_("Select next town NPC (speaks)."),
	    SDLK_PAGEDOWN,
	    SelectNextTownNpcKeyPressed,
	    nullptr,
	    IsTownNpcActionAllowed);
	options.Keymapper.AddAction(
	    "SpeakSelectedTownNpc",
	    N_("Speak selected town NPC"),
	    N_("Speaks the currently selected town NPC."),
	    SDLK_END,
	    SpeakSelectedTownNpc,
	    nullptr,
	    IsTownNpcActionAllowed);
	options.Keymapper.AddAction(
	    "GoToSelectedTownNpc",
	    N_("Go to selected town NPC"),
	    N_("Walks to the selected town NPC."),
	    SDLK_HOME,
	    GoToSelectedTownNpcKeyPressed,
	    nullptr,
	    IsTownNpcActionAllowed);
	options.Keymapper.AddAction(
	    "SpeakNearestUnexploredSpace",
	    N_("Nearest unexplored space"),
	    N_("Speaks the nearest unexplored space."),
	    'H',
	    SpeakNearestUnexploredTileKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "SpeakNearestExit",
	    N_("Nearest exit"),
	    N_("Speaks the nearest exit. Hold Shift for quest entrances. In town, press Ctrl+E to cycle dungeon entrances."),
	    'E',
	    SpeakNearestExitKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "SpeakNearestStairsDown",
	    N_("Nearest stairs down"),
	    N_("Speaks directions to the nearest stairs down."),
	    '.',
	    SpeakNearestStairsDownKeyPressed,
	    nullptr,
	    []() { return CanPlayerTakeAction() && leveltype != DTYPE_TOWN; });
	options.Keymapper.AddAction(
	    "SpeakNearestStairsUp",
	    N_("Nearest stairs up"),
	    N_("Speaks directions to the nearest stairs up."),
	    ',',
	    SpeakNearestStairsUpKeyPressed,
	    nullptr,
	    []() { return CanPlayerTakeAction() && leveltype != DTYPE_TOWN; });
	options.Keymapper.AddAction(
	    "CycleTrackerTarget",
	    N_("Cycle tracker target"),
	    N_("Cycles what the tracker looks for (items, chests, doors, shrines, objects, breakables, monsters)."),
	    'T',
	    CycleTrackerTargetKeyPressed,
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Keymapper.AddAction(
	    "NavigateToTrackerTarget",
	    N_("Tracker directions"),
	    N_("Speaks directions to a tracked target of the selected tracker category. Shift+N: cycle targets (speaks name only). Ctrl+N: clear target."),
	    'N',
	    NavigateToTrackerTargetKeyPressed,
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Keymapper.AddAction(
	    "KeyboardWalkNorth",
	    N_("Walk north"),
	    N_("Walk north (one tile)."),
	    SDLK_UP,
	    KeyboardWalkNorthKeyPressed);
	options.Keymapper.AddAction(
	    "KeyboardWalkSouth",
	    N_("Walk south"),
	    N_("Walk south (one tile)."),
	    SDLK_DOWN,
	    KeyboardWalkSouthKeyPressed);
	options.Keymapper.AddAction(
	    "KeyboardWalkEast",
	    N_("Walk east"),
	    N_("Walk east (one tile)."),
	    SDLK_RIGHT,
	    KeyboardWalkEastKeyPressed);
	options.Keymapper.AddAction(
	    "KeyboardWalkWest",
	    N_("Walk west"),
	    N_("Walk west (one tile)."),
	    SDLK_LEFT,
	    KeyboardWalkWestKeyPressed);
	options.Keymapper.AddAction(
	    "PrimaryAction",
	    N_("Primary action"),
	    N_("Attack monsters, talk to towners, lift and place inventory items."),
	    'A',
	    PerformPrimaryActionAutoTarget,
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Keymapper.AddAction(
	    "SecondaryAction",
	    N_("Secondary action"),
	    N_("Open chests, interact with doors, pick up items."),
	    'D',
	    PerformSecondaryActionAutoTarget,
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });
	options.Keymapper.AddAction(
	    "SpellAction",
	    N_("Spell action"),
	    N_("Cast the active spell."),
	    'W',
	    PerformSpellActionAutoTarget,
	    nullptr,
	    []() { return CanPlayerTakeAction() && !InGameMenu(); });

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
	    "Party",
	    N_("Party"),
	    N_("Open side Party panel."),
	    'Y',
	    PartyPanelSideToggleKeyPressed,
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
	    "HideInfoScreens",
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
	    SDLK_UNKNOWN,
	    [] {
		    GetOptions().Graphics.zoom.SetValue(!*GetOptions().Graphics.zoom);
		    CalcViewportGeometry();
	    },
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "SpeakPlayerHealthPercentage",
	    N_("Health percentage"),
	    N_("Speaks the player's health as a percentage."),
	    'Z',
	    SpeakPlayerHealthPercentageKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "SpeakExperienceToNextLevel",
	    N_("Experience to level"),
	    N_("Speaks how much experience remains to reach the next level."),
	    'X',
	    SpeakExperienceToNextLevelKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
	options.Keymapper.AddAction(
	    "PauseGame",
	    N_("Pause Game"),
	    N_("Pauses the game."),
	    SDLK_UNKNOWN,
	    diablo_pause_game);
	options.Keymapper.AddAction(
	    "PauseGameAlternate",
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
		                    fmt::runtime(_(/* TRANSLATORS: {:s} means: Project Name, Game Version. */ "{:s} {:s}")),
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
	    SDLK_INSERT,
	    [] {
		    ToggleChatLog();
	    });
	options.Keymapper.AddAction(
	    "SpeakCurrentLocation",
	    N_("Location"),
	    N_("Speaks the current dungeon and floor."),
	    'L',
	    SpeakCurrentLocationKeyPressed,
	    nullptr,
	    CanPlayerTakeAction);
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
	    SDLK_GRAVE,
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
	options.Keymapper.AddAction(
	    "SpeakNearestTownPortal",
	    N_("Nearest town portal"),
	    N_("Speaks directions to the nearest open town portal in town."),
	    'P',
	    SpeakNearestTownPortalInTownKeyPressed,
	    nullptr,
	    []() { return CanPlayerTakeAction() && leveltype == DTYPE_TOWN; });
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
			    const Player &myPlayer = *MyPlayer;
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
		    LastPlayerAction = PlayerActionType::None;
		    PerformPrimaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastPlayerAction = PlayerActionType::None;
	    },
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "SecondaryAction",
	    N_("Secondary action"),
	    N_("Open chests, interact with doors, pick up items."),
	    ControllerButton_BUTTON_Y,
	    [] {
		    ControllerActionHeld = GameActionType_SECONDARY_ACTION;
		    LastPlayerAction = PlayerActionType::None;
		    PerformSecondaryAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastPlayerAction = PlayerActionType::None;
	    },
	    CanPlayerTakeAction);
	options.Padmapper.AddAction(
	    "SpellAction",
	    N_("Spell action"),
	    N_("Cast the active spell."),
	    ControllerButton_BUTTON_X,
	    [] {
		    ControllerActionHeld = GameActionType_CAST_SPELL;
		    LastPlayerAction = PlayerActionType::None;
		    PerformSpellAction();
	    },
	    [] {
		    ControllerActionHeld = GameActionType_NONE;
		    LastPlayerAction = PlayerActionType::None;
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
	    "ToggleAutomap",
	    N_("Toggle automap"),
	    N_("Toggles if automap is displayed."),
	    ControllerButton_BUTTON_LEFTSTICK,
	    DoAutoMap);
	options.Padmapper.AddAction(
	    "AutomapMoveUp",
	    N_("Automap Move Up"),
	    N_("Moves the automap up when active."),
	    ControllerButton_NONE,
	    [] {});
	options.Padmapper.AddAction(
	    "AutomapMoveDown",
	    N_("Automap Move Down"),
	    N_("Moves the automap down when active."),
	    ControllerButton_NONE,
	    [] {});
	options.Padmapper.AddAction(
	    "AutomapMoveLeft",
	    N_("Automap Move Left"),
	    N_("Moves the automap left when active."),
	    ControllerButton_NONE,
	    [] {});
	options.Padmapper.AddAction(
	    "AutomapMoveRight",
	    N_("Automap Move Right"),
	    N_("Moves the automap right when active."),
	    ControllerButton_NONE,
	    [] {});
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
		const ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		const bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		sgbMouseDown = CLICK_LEFT;
		LeftMouseDown(standGround ? SDL_KMOD_SHIFT : SDL_KMOD_NONE);
	};
	auto leftMouseUp = [] {
		const ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		const bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastPlayerAction = PlayerActionType::None;
		sgbMouseDown = CLICK_NONE;
		LeftMouseUp(standGround ? SDL_KMOD_SHIFT : SDL_KMOD_NONE);
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
		const ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		const bool standGround = StandToggle || IsControllerButtonComboPressed(standGroundCombo);
		LastPlayerAction = PlayerActionType::None;
		sgbMouseDown = CLICK_RIGHT;
		RightMouseDown(standGround);
	};
	auto rightMouseUp = [] {
		LastPlayerAction = PlayerActionType::None;
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
		const bool inMenu = gmenu_is_active();
		PressEscKey();
		LastPlayerAction = PlayerActionType::None;
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
	    "ItemHighlighting",
	    N_("Item highlighting"),
	    N_("Show/hide items on ground."),
	    ControllerButton_NONE,
	    [] { HighlightKeyPressed(true); },
	    [] { HighlightKeyPressed(false); });
	options.Padmapper.AddAction(
	    "ToggleItemHighlighting",
	    N_("Toggle item highlighting"),
	    N_("Permanent show/hide items on ground."),
	    ControllerButton_NONE,
	    nullptr,
	    [] { ToggleItemLabelHighlight(); });
	options.Padmapper.AddAction(
	    "HideInfoScreens",
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
	    "PauseGame",
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
		                    fmt::runtime(_(/* TRANSLATORS: {:s} means: Project Name, Game Version. */ "{:s} {:s}")),
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
	MousePosition = position;
	if (ControlDevice != ControlTypes::KeyboardAndMouse) {
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

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (SdlLogFile != nullptr) std::fclose(SdlLogFile);
#endif

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
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
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

	LoadTextData();

	// Load dynamic data before we go into the menu as we need to initialise player characters in memory pretty early.
	LoadPlayerDataFiles();

	// TODO: We can probably load this much later (when the game is starting).
	LoadSpellData();
	LoadMissileData();
	LoadMonsterData();
	LoadItemData();
	LoadObjectData();
	LoadQuestData();

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
			const Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
			NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), static_cast<uint16_t>(sd), spellFrom);
		} else if (pcursmonst != -1 && leveltype != DTYPE_TOWN) {
			NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
		} else if (PlayerUnderCursor != nullptr && !PlayerUnderCursor->hasNoLife() && !myPlayer.friendlyMode) {
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
			LastPlayerAction = PlayerActionType::None;
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
		LastPlayerAction = PlayerActionType::None;
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
	case SDL_EVENT_MOUSE_MOTION:
		MousePosition = { SDLC_EventMotionIntX(event), SDLC_EventMotionIntY(event) };
		return;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
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
	case SDL_EVENT_MOUSE_BUTTON_UP:
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
	const bool isHellfireSaveGame = gbIsHellfireSaveGame;

	gbIsHellfireSaveGame = gbIsHellfire;
	LoadStash();
	gbIsHellfireSaveGame = isHellfireSaveGame;
}

tl::expected<void, std::string> LoadGameLevelDungeon(bool firstflag, lvl_entry lvldir, const Player &myPlayer)
{
	if (firstflag || lvldir == ENTRY_LOAD || !myPlayer._pLvlVisited[currlevel] || gbIsMultiplayer) {
		HoldThemeRooms();
		[[maybe_unused]] const uint32_t mid1Seed = GetLCGEngineState();
		InitGolems();
		InitObjects();
		[[maybe_unused]] const uint32_t mid2Seed = GetLCGEngineState();

		IncProgress();

		RETURN_IF_ERROR(InitMonsters());
		InitItems();
		CreateThemeRooms();

		IncProgress();

		[[maybe_unused]] const uint32_t mid3Seed = GetLCGEngineState();
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
	LastPlayerAction = PlayerActionType::None;
	sgbMouseDown = CLICK_NONE;
	ResetItemlabelHighlighted(); // level changed => item changed
	pcursmonst = -1;             // ensure pcurstemp is set to a valid value
	CheckCursMove();
}

tl::expected<void, std::string> LoadGameLevel(bool firstflag, lvl_entry lvldir)
{
	const _music_id neededTrack = GetLevelMusic(leveltype);

	ClearFloatingNumbers();
	LoadGameLevelStopMusic(neededTrack);
	LoadGameLevelResetCursor();
	SetRndSeedForDungeonLevel();
	NaKrulTomeSequence = 0;

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

	const Player &myPlayer = *MyPlayer;

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
	if (leveltype != DTYPE_TOWN)
		SpeakText(BuildCurrentLocationForSpeech(), /*force=*/true);
	return {};
}

bool game_loop(bool bStartup)
{
	const uint16_t wait = bStartup ? sgGameInitInfo.nTickRate * 3 : 3;

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
