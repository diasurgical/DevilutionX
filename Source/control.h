/**
 * @file control.h
 *
 * Interface of the character and main control panels
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <SDL.h>
#include <expected.hpp>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#include "DiabloUI/text_input.hpp"
#include "DiabloUI/ui_flags.hpp"
#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "panels/ui_panels.hpp"
#include "spelldat.h"
#include "spells.h"
#include "utils/attributes.h"
#include "utils/string_or_view.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

constexpr Size SidePanelSize { 320, 352 };

constexpr Rectangle InfoBoxRect = { { 177, 46 }, { 288, 64 } };

extern bool DropGoldFlag;
extern TextInputCursorState GoldDropCursor;
extern char GoldDropText[21];

extern bool CharPanelButton[4];
extern bool LevelButtonDown;
extern bool CharPanelButtonActive;
extern UiFlags InfoColor;
extern int SpellbookTab;
extern bool ChatFlag;
extern bool SpellbookFlag;
extern bool CharFlag;
extern StringOrView InfoString;
extern bool MainPanelFlag;
extern bool MainPanelButtonDown;
extern bool SpellSelectFlag;
const Rectangle &GetMainPanel();
const Rectangle &GetLeftPanel();
const Rectangle &GetRightPanel();
bool IsLeftPanelOpen();
bool IsRightPanelOpen();
extern std::optional<OwnedSurface> BottomBuffer;
extern OptionalOwnedClxSpriteList GoldBoxBuffer;
extern Rectangle MainPanelButtonRect[8];

void CalculatePanelAreas();
bool IsChatAvailable();

/**
 * @brief Moves the mouse to the first attribute "+" button.
 */
void FocusOnCharInfo();
void OpenCharPanel();
void CloseCharPanel();
void ToggleCharPanel();

/**
 * @brief Check if the UI can cover the game area entirely
 */
inline bool CanPanelsCoverView()
{
	const Rectangle &mainPanel = GetMainPanel();
	return GetScreenWidth() <= mainPanel.size.width && GetScreenHeight() <= SidePanelSize.height + mainPanel.size.height;
}

void AddInfoBoxString(std::string_view str);
void AddInfoBoxString(std::string &&str);
void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition);
Point GetPanelPosition(UiPanels panel, Point offset = { 0, 0 });

/**
 * Draws the top dome of the life flask (that part that protrudes out of the control panel).
 * The empty flask cel is drawn from the top of the flask to the fill level (there is always a 2 pixel "air gap") and
 * the filled flask cel is drawn from that level to the top of the control panel if required.
 */
void DrawLifeFlaskUpper(const Surface &out);

/**
 * Controls the drawing of the area of the life flask within the control panel.
 * First sets the fill amount then draws the empty flask cel portion then the filled
 * flask portion.
 */
void DrawLifeFlaskLower(const Surface &out);

/**
 * Draws the top dome of the mana flask (that part that protrudes out of the control panel).
 * The empty flask cel is drawn from the top of the flask to the fill level (there is always a 2 pixel "air gap") and
 * the filled flask cel is drawn from that level to the top of the control panel if required.
 */
void DrawManaFlaskUpper(const Surface &out);

/**
 * Controls the drawing of the area of the mana flask within the control panel.
 */
void DrawManaFlaskLower(const Surface &out);

/**
 * Controls drawing of current / max values (health, mana) within the control panel.
 */
void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue);

/**
 * @brief calls on the active player object to update HP/Mana percentage variables
 *
 * This is used to ensure that DrawFlaskAbovePanel routines display an accurate representation of the players health/mana
 *
 * @see Player::UpdateHitPointPercentage() and Player::UpdateManaPercentage()
 */
void UpdateLifeManaPercent();

tl::expected<void, std::string> InitMainPanel();
void DrawMainPanel(const Surface &out);

/**
 * Draws the control panel buttons in their current state. If the button is in the default
 * state draw it from the panel cel(extract its sub-rect). Else draw it from the buttons cel.
 */
void DrawMainPanelButtons(const Surface &out);

/**
 * Clears panel button flags.
 */
void ResetMainPanelButtons();

/**
 * Checks if the mouse cursor is within any of the panel buttons and flag it if so.
 */
void CheckMainPanelButton();

void CheckMainPanelButtonDead();
void DoAutoMap();
void CycleAutomapType();

/**
 * Checks the mouse cursor position within the control panel and sets information
 * strings if needed.
 */
void CheckPanelInfo();

/**
 * Check if the mouse is within a control panel button that's flagged.
 * Takes appropriate action if so.
 */
void CheckMainPanelButtonUp();
void FreeControlPan();

/**
 * Sets a string to be drawn in the info box and then draws it.
 */
void DrawInfoBox(const Surface &out);
void CheckLevelButton();
void CheckLevelButtonUp();
void DrawLevelButton(const Surface &out);
void CheckChrBtns();
void ReleaseChrBtns(bool addAllStatPoints);
void DrawDurIcon(const Surface &out);
void RedBack(const Surface &out);
void DrawDeathText(const Surface &out);
void DrawSpellBook(const Surface &out);
void DrawGoldSplit(const Surface &out);
void control_drop_gold(SDL_Keycode vkey);
void DrawChatBox(const Surface &out);
bool CheckMuteButton();
void CheckMuteButtonUp();
void TypeChatMessage();
void ResetChat();
bool IsChatActive();
bool HandleTalkTextInputEvent(const SDL_Event &event);
bool CheckKeypress(SDL_Keycode vkey);
void DiabloHotkeyMsg(uint32_t dwMsg);
void OpenGoldDrop(int8_t invIndex, int max);
void CloseGoldDrop();
int GetGoldDropMax();
bool HandleGoldDropTextInputEvent(const SDL_Event &event);
extern Rectangle CharPanelButtonRect[4];

} // namespace devilution
