/**
 * @file diablo.h
 *
 * Interface of the main game initialization functions.
 */
#pragma once

#include <cstdint>

#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#ifdef _DEBUG
#include "monstdat.h"
#endif
#include "levels/gendung.h"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"

namespace devilution {

constexpr uint32_t GameIdDiabloFull = LoadBE32("DRTL");
constexpr uint32_t GameIdDiabloSpawn = LoadBE32("DSHR");
constexpr uint32_t GameIdHellfireFull = LoadBE32("HRTL");
constexpr uint32_t GameIdHellfireSpawn = LoadBE32("HSHR");
#define GAME_ID (gbIsHellfire ? (gbIsSpawn ? GameIdHellfireSpawn : GameIdHellfireFull) : (gbIsSpawn ? GameIdDiabloSpawn : GameIdDiabloFull))

#define NUMLEVELS 25

enum clicktype : int8_t {
	CLICK_NONE,
	CLICK_LEFT,
	CLICK_RIGHT,
};

/**
 * @brief Specifies what game logic step is currently executed
 */
enum class GameLogicStep : uint8_t {
	None,
	ProcessPlayers,
	ProcessMonsters,
	ProcessObjects,
	ProcessMissiles,
	ProcessItems,
	ProcessTowners,
	ProcessItemsTown,
	ProcessMissilesTown,
};

enum class MouseActionType : uint8_t {
	None,
	Walk,
	Spell,
	SpellMonsterTarget,
	SpellPlayerTarget,
	Attack,
	AttackMonsterTarget,
	AttackPlayerTarget,
	OperateObject,
};

extern uint32_t DungeonSeeds[NUMLEVELS];
extern DVL_API_FOR_TEST std::optional<uint32_t> LevelSeeds[NUMLEVELS];
extern Point MousePosition;
extern DVL_API_FOR_TEST bool gbRunGame;
extern bool gbRunGameResult;
extern bool ReturnToMainMenu;
extern bool gbProcessPlayers;
extern DVL_API_FOR_TEST bool gbLoadGame;
extern bool cineflag;
/* These are defined in fonts.h */
extern void FontsCleanup();
extern DVL_API_FOR_TEST int PauseMode;
extern clicktype sgbMouseDown;
extern uint16_t gnTickDelay;
extern char gszProductName[64];

extern MouseActionType LastMouseButtonAction;

void InitKeymapActions();
void SetCursorPos(Point position);
void FreeGameMem();
bool StartGame(bool bNewGame, bool bSinglePlayer);
[[noreturn]] void diablo_quit(int exitStatus);
int DiabloMain(int argc, char **argv);
bool TryIconCurs();
void diablo_pause_game();
bool diablo_is_focused();
void diablo_focus_pause();
void diablo_focus_unpause();
bool PressEscKey();
void DisableInputEventHandler(const SDL_Event &event, uint16_t modState);
tl::expected<void, std::string> LoadGameLevel(bool firstflag, lvl_entry lvldir);
bool IsDiabloAlive(bool playSFX);
void PrintScreen(SDL_Keycode vkey);

/**
 * @param bStartup Process additional ticks before returning
 */
bool game_loop(bool bStartup);
void diablo_color_cyc_logic();

/* rdata */

#ifdef _DEBUG
extern bool DebugDisableNetworkTimeout;
#endif

/**
 * @brief Specifies what game logic step is currently executed
 */
extern GameLogicStep gGameLogicStep;

#ifdef __UWP__
void setOnInitialized(void (*)());
#endif

} // namespace devilution
