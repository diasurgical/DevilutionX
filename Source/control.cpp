/**
 * @file control.cpp
 *
 * Implementation of the character and main control panels
 */
#include "control.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "automap.h"
#include "controls/control_mode.hpp"
#include "controls/modifier_hints.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo_msg.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "gamemenu.h"
#include "headless_mode.hpp"
#include "inv.h"
#include "inv_iterators.hpp"
#include "levels/setmaps.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "panels/console.hpp"
#include "panels/mainpanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "quick_messages.hpp"
#include "stores.h"
#include "towners.h"
#include "utils/algorithm/container.hpp"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/parse_int.hpp"
#include "utils/screen_reader.hpp"
#include "utils/sdl_geometry.h"
#include "utils/sdl_ptrs.h"
#include "utils/status_macros.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/string_or_view.hpp"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

bool DropGoldFlag;
TextInputCursorState GoldDropCursor;
char GoldDropText[21];
namespace {
int8_t GoldDropInvIndex;
std::optional<NumberInputState> GoldDropInputState;
} // namespace

bool CharPanelButton[4];
bool LevelButtonDown;
bool CharPanelButtonActive;
UiFlags InfoColor;
int SpellbookTab;
bool ChatFlag;
bool SpellbookFlag;
bool CharFlag;
StringOrView InfoString;
bool MainPanelFlag;
bool MainPanelButtonDown;
bool SpellSelectFlag;
Rectangle MainPanel;
Rectangle LeftPanel;
Rectangle RightPanel;
std::optional<OwnedSurface> BottomBuffer;
OptionalOwnedClxSpriteList GoldBoxBuffer;

const Rectangle &GetMainPanel()
{
	return MainPanel;
}
const Rectangle &GetLeftPanel()
{
	return LeftPanel;
}
const Rectangle &GetRightPanel()
{
	return RightPanel;
}
bool IsLeftPanelOpen()
{
	return CharFlag || QuestLogIsOpen || IsStashOpen;
}
bool IsRightPanelOpen()
{
	return invflag || SpellbookFlag;
}

constexpr Size IncrementAttributeButtonSize { 41, 22 };
/** Maps from attribute_id to the rectangle on screen used for attribute increment buttons. */
Rectangle CharPanelButtonRect[4] = {
	{ { 137, 138 }, IncrementAttributeButtonSize },
	{ { 137, 166 }, IncrementAttributeButtonSize },
	{ { 137, 195 }, IncrementAttributeButtonSize },
	{ { 137, 223 }, IncrementAttributeButtonSize }
};

constexpr Size WidePanelButtonSize { 71, 20 };
constexpr Size PanelButtonSize { 33, 32 };
/** Positions of panel buttons. */
Rectangle MainPanelButtonRect[8] = {
	// clang-format off
	{ {   9,   9 }, WidePanelButtonSize }, // char button
	{ {   9,  35 }, WidePanelButtonSize }, // quests button
	{ {   9,  75 }, WidePanelButtonSize }, // map button
	{ {   9, 101 }, WidePanelButtonSize }, // menu button
	{ { 560,   9 }, WidePanelButtonSize }, // inv button
	{ { 560,  35 }, WidePanelButtonSize }, // spells button
	{ {  87,  91 }, PanelButtonSize     }, // chat button
	{ { 527,  91 }, PanelButtonSize     }, // friendly fire button
	// clang-format on
};

Rectangle LevelButtonRect = { { 40, -39 }, { 41, 22 } };

int BeltItems = 8;
Size BeltSize { (INV_SLOT_SIZE_PX + 1) * BeltItems, INV_SLOT_SIZE_PX };
Rectangle BeltRect { { 205, 5 }, BeltSize };

Rectangle SpellButtonRect { { 565, 64 }, { 56, 56 } };

Rectangle FlaskTopRect { { 13, 3 }, { 60, 13 } };
Rectangle FlaskBottomRect { { 0, 16 }, { 84, 69 } };

int MuteButtons = 3;
int MuteButtonPadding = 2;
Rectangle MuteButtonRect { { 172, 69 }, { 61, 16 } };

namespace {

std::optional<OwnedSurface> pLifeBuff;
std::optional<OwnedSurface> pManaBuff;
OptionalOwnedClxSpriteList talkButtons;
OptionalOwnedClxSpriteList pDurIcons;
OptionalOwnedClxSpriteList multiButtons;
OptionalOwnedClxSpriteList pMainPanelButtons;

enum panel_button_id : uint8_t {
	PanelButtonCharinfo,
	PanelButtonFirst = PanelButtonCharinfo,
	PanelButtonQlog,
	PanelButtonAutomap,
	PanelButtonMainmenu,
	PanelButtonInventory,
	PanelButtonSpellbook,
	PanelButtonSendmsg,
	PanelButtonFriendly,
	PanelButtonLast = PanelButtonFriendly,
};

bool MainPanelButtons[PanelButtonLast + 1];
int TotalSpMainPanelButtons = 6;
int TotalMpMainPanelButtons = 8;
char TalkSave[8][MAX_SEND_STR_LEN];
uint8_t TalkSaveIndex;
uint8_t NextTalkSave;
char TalkMessage[MAX_SEND_STR_LEN];
bool TalkButtonsDown[3];
int sgbPlrTalkTbl;
bool WhisperList[MAX_PLRS];
int PanelPaddingHeight = 16;

TextInputCursorState ChatCursor;
std::optional<TextInputState> ChatInputState;

/** Maps from panel_button_id to hotkey name. */
const char *const PanBtnHotKey[8] = { "'c'", "'q'", N_("Tab"), N_("Esc"), "'i'", "'b'", N_("Enter"), nullptr };
/** Maps from panel_button_id to panel button description. */
const char *const PanBtnStr[8] = {
	N_("Character Information"),
	N_("Quests log"),
	N_("Automap"),
	N_("Main Menu"),
	N_("Inventory"),
	N_("Spell book"),
	N_("Send Message"),
	"" // Player attack
};

/**
 * Draws the dome of the flask that protrudes above the panel top line.
 * It draws a rectangle of fixed width 59 and height 'h' from the source buffer
 * into the target buffer.
 * @param out The target buffer.
 * @param celBuf Buffer of the empty flask cel.
 * @param sourcePosition Source buffer start coordinate.
 * @param targetPosition Target buffer coordinate.
 * @param h How many lines of the source buffer that will be copied.
 */
void DrawFlaskAbovePanel(const Surface &out, const Surface &celBuf, Point sourcePosition, Point targetPosition, int h)
{
	out.BlitFromSkipColorIndexZero(celBuf, MakeSdlRect(sourcePosition.x, sourcePosition.y, FlaskTopRect.size.width, h), targetPosition);
}

/**
 * @brief Draws the part of the life/mana flasks protruding above the bottom panel
 * @see DrawFlaskLower()
 * @param out The display region to draw to
 * @param sourceBuffer A sprite representing the appropriate background/empty flask style
 * @param offset X coordinate offset for where the flask should be drawn
 * @param fillPer How full the flask is (a value from 0 to 81)
 */
void DrawFlaskUpper(const Surface &out, const Surface &sourceBuffer, int offset, int fillPer)
{
	int emptyRows = std::clamp(81 - fillPer, 0, FlaskTopRect.size.height);
	int filledRows = FlaskTopRect.size.height - emptyRows;

	// Draw the empty part of the flask
	DrawFlaskAbovePanel(out, sourceBuffer, FlaskTopRect.position, GetMainPanel().position + Displacement { offset, -FlaskTopRect.size.height }, FlaskTopRect.size.height);

	// Draw the filled part of the flask over the empty part
	if (filledRows > 0) {
		DrawFlaskAbovePanel(out, *BottomBuffer, { offset, FlaskTopRect.position.y + emptyRows }, GetMainPanel().position + Displacement { offset, -FlaskTopRect.size.height + emptyRows }, filledRows);
	}
}

/**
 * Draws a section of the empty flask cel on top of the panel to create the illusion
 * of the flask getting empty. This function takes a cel and draws a
 * horizontal stripe of height (max-min) onto the given buffer.
 * @param out Target buffer.
 * @param position Buffer coordinate.
 * @param celBuf Buffer of the empty flask cel.
 * @param y0 Top of the flask cel section to draw.
 * @param y1 Bottom of the flask cel section to draw.
 */
void DrawFlaskOnPanel(const Surface &out, Point position, const Surface &celBuf, int y0, int y1)
{
	out.BlitFrom(celBuf, MakeSdlRect(0, static_cast<decltype(SDL_Rect {}.y)>(y0), celBuf.w(), y1 - y0), position);
}

/**
 * @brief Draws the part of the life/mana flasks inside the bottom panel
 * @see DrawFlaskUpper()
 * @param out The display region to draw to
 * @param sourceBuffer A sprite representing the appropriate background/empty flask style
 * @param offset X coordinate offset for where the flask should be drawn
 * @param fillPer How full the flask is (a value from 0 to 80)
 */
void DrawFlaskLower(const Surface &out, const Surface &sourceBuffer, int offset, int fillPer)
{
	int filled = std::clamp(fillPer, 0, FlaskBottomRect.size.height);

	if (filled < FlaskBottomRect.size.height)
		DrawFlaskOnPanel(out, GetMainPanel().position + Displacement { offset, 0 }, sourceBuffer, FlaskBottomRect.position.y, FlaskBottomRect.position.y + FlaskBottomRect.size.height - filled);
}

void SetMainPanelButtonDown(int btnId)
{
	MainPanelButtons[btnId] = true;
	RedrawComponent(PanelDrawComponent::ControlButtons);
	MainPanelButtonDown = true;
}

void SetMainPanelButtonUp()
{
	RedrawComponent(PanelDrawComponent::ControlButtons);
	MainPanelButtonDown = false;
}

void SetPanelObjectPosition(UiPanels panel, Rectangle &button)
{
	button.position = GetPanelPosition(panel, button.position);
}

void PrintInfo(const Surface &out)
{
	if (ChatFlag)
		return;

	const int space[] = { 18, 12, 6, 3, 0 };
	Rectangle infoBox = InfoBoxRect;

	SetPanelObjectPosition(UiPanels::Main, infoBox);

	const auto newLineCount = static_cast<int>(c_count(InfoString.str(), '\n'));
	const int spaceIndex = std::min(4, newLineCount);
	const int spacing = space[spaceIndex];
	const int lineHeight = 12 + spacing;

	// Adjusting the line height to add spacing between lines
	// will also add additional space beneath the last line
	// which throws off the vertical centering
	infoBox.position.y += spacing / 2;

	SpeakText(InfoString);

	DrawString(out, InfoString, infoBox,
	    {
	        .flags = InfoColor | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::KerningFitSpacing,
	        .spacing = 2,
	        .lineHeight = lineHeight,
	    });
}

int CapStatPointsToAdd(int remainingStatPoints, const Player &player, CharacterAttribute attribute)
{
	int pointsToReachCap = player.GetMaximumAttributeValue(attribute) - player.GetBaseAttributeValue(attribute);

	return std::min(remainingStatPoints, pointsToReachCap);
}

int DrawDurIcon4Item(const Surface &out, Item &pItem, int x, int c)
{
	const int durabilityThresholdGold = 5;
	const int durabilityThresholdRed = 2;

	if (pItem.isEmpty())
		return x;
	if (pItem._iDurability > durabilityThresholdGold)
		return x;
	if (c == 0) {
		switch (pItem._itype) {
		case ItemType::Sword:
			c = 1;
			break;
		case ItemType::Axe:
			c = 5;
			break;
		case ItemType::Bow:
			c = 6;
			break;
		case ItemType::Mace:
			c = 4;
			break;
		case ItemType::Staff:
			c = 7;
			break;
		case ItemType::Shield:
		default:
			c = 0;
			break;
		}
	}

	// Calculate how much of the icon should be gold and red
	int height = (*pDurIcons)[c].height(); // Height of durability icon CEL
	int partition = 0;
	if (pItem._iDurability > durabilityThresholdRed) {
		int current = pItem._iDurability - durabilityThresholdRed;
		partition = (height * current) / (durabilityThresholdGold - durabilityThresholdRed);
	}

	// Draw icon
	int y = -17 + GetMainPanel().position.y;
	if (partition > 0) {
		const Surface stenciledBuffer = out.subregionY(y - partition, partition);
		ClxDraw(stenciledBuffer, { x, partition }, (*pDurIcons)[c + 8]); // Gold icon
	}
	if (partition != height) {
		const Surface stenciledBuffer = out.subregionY(y - height, height - partition);
		ClxDraw(stenciledBuffer, { x, height }, (*pDurIcons)[c]); // Red icon
	}

	return x - (*pDurIcons)[c].height() - 8; // Add in spacing for the next durability icon
}

struct TextCmdItem {
	const std::string text;
	const std::string description;
	const std::string requiredParameter;
	std::string (*actionProc)(const std::string_view);
};

extern std::vector<TextCmdItem> TextCmdList;

std::string TextCmdHelp(const std::string_view parameter)
{
	if (parameter.empty()) {
		std::string ret;
		StrAppend(ret, _("Available Commands:"));
		for (const TextCmdItem &textCmd : TextCmdList) {
			StrAppend(ret, " ", _(textCmd.text));
		}
		return ret;
	}
	auto textCmdIterator = c_find_if(TextCmdList, [&](const TextCmdItem &elem) { return elem.text == parameter; });
	if (textCmdIterator == TextCmdList.end())
		return StrCat(_("Command "), parameter, _(" is unknown."));
	auto &textCmdItem = *textCmdIterator;
	if (textCmdItem.requiredParameter.empty())
		return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: No additional parameter needed."));
	return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: "), _(textCmdItem.requiredParameter));
}

void AppendArenaOverview(std::string &ret)
{
	for (int arena = SL_FIRST_ARENA; arena <= SL_LAST; arena++) {
		StrAppend(ret, "\n", arena - SL_FIRST_ARENA + 1, " (", QuestLevelNames[arena], ")");
	}
}

const dungeon_type DungeonTypeForArena[] = {
	dungeon_type::DTYPE_CATHEDRAL, // SL_ARENA_CHURCH
	dungeon_type::DTYPE_HELL,      // SL_ARENA_HELL
	dungeon_type::DTYPE_HELL,      // SL_ARENA_CIRCLE_OF_LIFE
};

std::string TextCmdArena(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("What arena do you want to visit?"));
		AppendArenaOverview(ret);
		return ret;
	}

	const ParseIntResult<int> parsedParam = ParseInt<int>(parameter, /*min=*/0);
	const _setlevels arenaLevel = parsedParam.has_value() ? static_cast<_setlevels>(parsedParam.value() - 1 + SL_FIRST_ARENA) : _setlevels::SL_NONE;
	if (!IsArenaLevel(arenaLevel)) {
		StrAppend(ret, _("Invalid arena-number. Valid numbers are:"));
		AppendArenaOverview(ret);
		return ret;
	}

	if (!MyPlayer->isOnLevel(0) && !MyPlayer->isOnArenaLevel()) {
		StrAppend(ret, _("To enter a arena, you need to be in town or another arena."));
		return ret;
	}

	setlvltype = DungeonTypeForArena[arenaLevel - SL_FIRST_ARENA];
	StartNewLvl(*MyPlayer, WM_DIABSETLVL, arenaLevel);
	return ret;
}

std::string TextCmdArenaPot(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}
	int numPots = ParseInt<int>(parameter, /*min=*/1).value_or(1);

	Player &myPlayer = *MyPlayer;

	for (int potNumber = numPots; potNumber > 0; potNumber--) {
		Item item {};
		InitializeItem(item, IDI_ARENAPOT);
		GenerateNewSeed(item);
		item.updateRequiredStatsCacheForPlayer(myPlayer);

		if (!AutoPlaceItemInBelt(myPlayer, item, true, true) && !AutoPlaceItemInInventory(myPlayer, item, true)) {
			break; // inventory is full
		}
	}

	return ret;
}

std::string TextCmdInspect(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Inspecting only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("Stopped inspecting players."));
		InspectPlayer = MyPlayer;
		return ret;
	}

	const std::string param = AsciiStrToLower(parameter);
	auto it = c_find_if(Players, [&param](const Player &player) {
		return AsciiStrToLower(player._pName) == param;
	});
	if (it == Players.end()) {
		it = c_find_if(Players, [&param](const Player &player) {
			return AsciiStrToLower(player._pName).find(param) != std::string::npos;
		});
	}
	if (it == Players.end()) {
		StrAppend(ret, _("No players found with such a name"));
		return ret;
	}

	Player &player = *it;
	InspectPlayer = &player;
	StrAppend(ret, _("Inspecting player: "));
	StrAppend(ret, player._pName);
	OpenCharPanel();
	if (!SpellbookFlag)
		invflag = true;
	RedrawEverything();
	return ret;
}

bool IsQuestEnabled(const Quest &quest)
{
	switch (quest._qidx) {
	case Q_FARMER:
		return gbIsHellfire && !sgGameInitInfo.bCowQuest;
	case Q_JERSEY:
		return gbIsHellfire && sgGameInitInfo.bCowQuest;
	case Q_GIRL:
		return gbIsHellfire && sgGameInitInfo.bTheoQuest;
	case Q_CORNSTN:
		return gbIsHellfire && !gbIsMultiplayer;
	case Q_GRAVE:
	case Q_DEFILER:
	case Q_NAKRUL:
		return gbIsHellfire;
	case Q_TRADER:
		return false;
	default:
		return quest._qactive != QUEST_NOTAVAIL;
	}
}

std::string TextCmdLevelSeed(const std::string_view parameter)
{
	std::string_view levelType = setlevel ? "set level" : "dungeon level";

	char gameId[] = {
		static_cast<char>((sgGameInitInfo.programid >> 24) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 16) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 8) & 0xFF),
		static_cast<char>(sgGameInitInfo.programid & 0xFF),
		'\0'
	};

	std::string_view mode = gbIsMultiplayer ? "MP" : "SP";
	std::string_view questPool = UseMultiplayerQuests() ? "MP" : "Full";

	uint32_t questFlags = 0;
	for (const Quest &quest : Quests) {
		questFlags <<= 1;
		if (IsQuestEnabled(quest))
			questFlags |= 1;
	}

	return StrCat(
	    "Seedinfo for ", levelType, " ", currlevel, "\n",
	    "seed: ", DungeonSeeds[currlevel], "\n",
#ifdef _DEBUG
	    "Mid1: ", glMid1Seed[currlevel], "\n",
	    "Mid2: ", glMid2Seed[currlevel], "\n",
	    "Mid3: ", glMid3Seed[currlevel], "\n",
	    "End: ", glEndSeed[currlevel], "\n",
#endif
	    "\n",
	    gameId, " ", mode, "\n",
	    questPool, " quests: ", questFlags, "\n",
	    "Storybook: ", DungeonSeeds[16]);
}

std::vector<TextCmdItem> TextCmdList = {
	{ "/help", N_("Prints help overview or help for a specific command."), N_("[command]"), &TextCmdHelp },
	{ "/arena", N_("Enter a PvP Arena."), N_("<arena-number>"), &TextCmdArena },
	{ "/arenapot", N_("Gives Arena Potions."), N_("<number>"), &TextCmdArenaPot },
	{ "/inspect", N_("Inspects stats and equipment of another player."), N_("<player name>"), &TextCmdInspect },
	{ "/seedinfo", N_("Show seed infos for current level."), "", &TextCmdLevelSeed },
};

bool CheckChatCommand(const std::string_view text)
{
	if (text.size() < 1 || text[0] != '/')
		return false;

	auto textCmdIterator = c_find_if(TextCmdList, [&](const TextCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (textCmdIterator == TextCmdList.end()) {
		InitDiabloMsg(StrCat(_("Command "), "\"", text, "\"", _(" is unknown.")));
		return true;
	}

	TextCmdItem &textCmd = *textCmdIterator;
	std::string_view parameter = "";
	if (text.length() > (textCmd.text.length() + 1))
		parameter = text.substr(textCmd.text.length() + 1);
	const std::string result = textCmd.actionProc(parameter);
	if (result != "")
		InitDiabloMsg(result);
	return true;
}

void ResetChatMessage()
{
	if (CheckChatCommand(TalkMessage))
		return;

	uint32_t pmask = 0;

	for (size_t i = 0; i < Players.size(); i++) {
		if (WhisperList[i])
			pmask |= 1 << i;
	}

	NetSendCmdString(pmask, TalkMessage);
}

void ControlPressEnter()
{
	if (TalkMessage[0] != 0) {
		ResetChatMessage();
		uint8_t i = 0;
		for (; i < 8; i++) {
			if (strcmp(TalkSave[i], TalkMessage) == 0)
				break;
		}
		if (i >= 8) {
			strcpy(TalkSave[NextTalkSave], TalkMessage);
			NextTalkSave++;
			NextTalkSave &= 7;
		} else {
			uint8_t talkSave = NextTalkSave - 1;
			talkSave &= 7;
			if (i != talkSave) {
				strcpy(TalkSave[i], TalkSave[talkSave]);
				*BufCopy(TalkSave[talkSave], ChatInputState->value()) = '\0';
			}
		}
		TalkMessage[0] = '\0';
		TalkSaveIndex = NextTalkSave;
	}
	ResetChat();
}

void ControlUpDown(int v)
{
	for (int i = 0; i < 8; i++) {
		TalkSaveIndex = (v + TalkSaveIndex) & 7;
		if (TalkSave[TalkSaveIndex][0] != 0) {
			ChatInputState->assign(TalkSave[TalkSaveIndex]);
			return;
		}
	}
}

void RemoveGold(Player &player, int goldIndex, int amount)
{
	const int gi = goldIndex - INVITEM_INV_FIRST;
	player.InvList[gi]._ivalue -= amount;
	if (player.InvList[gi]._ivalue > 0) {
		SetPlrHandGoldCurs(player.InvList[gi]);
		NetSyncInvItem(player, gi);
	} else {
		player.RemoveInvItem(gi);
	}

	MakeGoldStack(player.HoldItem, amount);
	NewCursor(player.HoldItem);

	player._pGold = CalculateGold(player);
}

bool IsLevelUpButtonVisible()
{
	if (SpellSelectFlag || CharFlag || MyPlayer->_pStatPts == 0) {
		return false;
	}
	if (ControlMode == ControlTypes::VirtualGamepad) {
		return false;
	}
	if (IsPlayerInStore() || IsStashOpen) {
		return false;
	}
	if (QuestLogIsOpen && GetLeftPanel().contains(GetMainPanel().position + Displacement { 0, -74 })) {
		return false;
	}

	return true;
}

} // namespace

void CalculatePanelAreas()
{
	constexpr Size MainPanelSize { 640, 128 };

	MainPanel = {
		{ (gnScreenWidth - MainPanelSize.width) / 2, gnScreenHeight - MainPanelSize.height },
		MainPanelSize
	};
	LeftPanel = {
		{ 0, 0 },
		SidePanelSize
	};
	RightPanel = {
		{ 0, 0 },
		SidePanelSize
	};

	if (ControlMode == ControlTypes::VirtualGamepad) {
		LeftPanel.position.x = gnScreenWidth / 2 - LeftPanel.size.width;
	} else {
		if (gnScreenWidth - LeftPanel.size.width - RightPanel.size.width > MainPanel.size.width) {
			LeftPanel.position.x = (gnScreenWidth - LeftPanel.size.width - RightPanel.size.width - MainPanel.size.width) / 2;
		}
	}
	LeftPanel.position.y = (gnScreenHeight - LeftPanel.size.height - MainPanel.size.height) / 2;

	if (ControlMode == ControlTypes::VirtualGamepad) {
		RightPanel.position.x = gnScreenWidth / 2;
	} else {
		RightPanel.position.x = gnScreenWidth - RightPanel.size.width - LeftPanel.position.x;
	}
	RightPanel.position.y = LeftPanel.position.y;

	gnViewportHeight = gnScreenHeight;
	if (gnScreenWidth <= MainPanel.size.width) {
		// Part of the screen is fully obscured by the UI
		gnViewportHeight -= MainPanel.size.height;
	}
}

bool IsChatAvailable()
{
	return gbIsMultiplayer;
}

void FocusOnCharInfo()
{
	Player &myPlayer = *MyPlayer;

	if (invflag || myPlayer._pStatPts <= 0)
		return;

	// Find the first incrementable stat.
	int stat = -1;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		if (myPlayer.GetBaseAttributeValue(attribute) >= myPlayer.GetMaximumAttributeValue(attribute))
			continue;
		stat = static_cast<int>(attribute);
	}
	if (stat == -1)
		return;

	SetCursorPos(CharPanelButtonRect[stat].Center());
}

void OpenCharPanel()
{
	QuestLogIsOpen = false;
	CloseGoldWithdraw();
	CloseStash();
	CharFlag = true;
}

void CloseCharPanel()
{
	CharFlag = false;
	if (IsInspectingPlayer()) {
		InspectPlayer = MyPlayer;
		RedrawEverything();
		InitDiabloMsg(_("Stopped inspecting players."));
	}
}

void ToggleCharPanel()
{
	if (CharFlag)
		CloseCharPanel();
	else
		OpenCharPanel();
}

void AddInfoBoxString(std::string_view str)
{
	if (InfoString.empty())
		InfoString = str;
	else
		InfoString = StrCat(InfoString, "\n", str);
}

void AddInfoBoxString(std::string &&str)
{
	if (InfoString.empty())
		InfoString = std::move(str);
	else
		InfoString = StrCat(InfoString, "\n", str);
}

Point GetPanelPosition(UiPanels panel, Point offset)
{
	Displacement displacement { offset.x, offset.y };

	switch (panel) {
	case UiPanels::Main:
		return GetMainPanel().position + displacement;
	case UiPanels::Quest:
	case UiPanels::Character:
	case UiPanels::Stash:
		return GetLeftPanel().position + displacement;
	case UiPanels::Spell:
	case UiPanels::Inventory:
		return GetRightPanel().position + displacement;
	default:
		return GetMainPanel().position + displacement;
	}
}

void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition)
{
	out.BlitFrom(*BottomBuffer, srcRect, targetPosition);
}

void DrawLifeFlaskUpper(const Surface &out)
{
	constexpr int LifeFlaskUpperOffset = 109;
	DrawFlaskUpper(out, *pLifeBuff, LifeFlaskUpperOffset, MyPlayer->_pHPPer);
}

void DrawManaFlaskUpper(const Surface &out)
{
	constexpr int ManaFlaskUpperOffset = 475;
	DrawFlaskUpper(out, *pManaBuff, ManaFlaskUpperOffset, MyPlayer->_pManaPer);
}

void DrawLifeFlaskLower(const Surface &out)
{
	constexpr int LifeFlaskLowerOffset = 96;
	DrawFlaskLower(out, *pLifeBuff, LifeFlaskLowerOffset, MyPlayer->_pHPPer);
}

void DrawManaFlaskLower(const Surface &out)
{
	constexpr int ManaFlaskLowerOffeset = 464;
	DrawFlaskLower(out, *pManaBuff, ManaFlaskLowerOffeset, MyPlayer->_pManaPer);
}

void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue)
{
	UiFlags color = (currValue > 0 ? (currValue == maxValue ? UiFlags::ColorGold : UiFlags::ColorWhite) : UiFlags::ColorRed);

	auto drawStringWithShadow = [out, color](std::string_view text, Point pos) {
		DrawString(out, text, pos + Displacement { -1, -1 },
		    { .flags = UiFlags::ColorBlack | UiFlags::KerningFitSpacing, .spacing = 0 });
		DrawString(out, text, pos,
		    { .flags = color | UiFlags::KerningFitSpacing, .spacing = 0 });
	};

	std::string currText = StrCat(currValue);
	drawStringWithShadow(currText, pos - Displacement { GetLineWidth(currText, GameFont12) + 1, 0 });
	drawStringWithShadow("/", pos);
	drawStringWithShadow(StrCat(maxValue), pos + Displacement { GetLineWidth("/", GameFont12) + 1, 0 });
}

void UpdateLifeManaPercent()
{
	MyPlayer->UpdateManaPercentage();
	MyPlayer->UpdateHitPointPercentage();
}

tl::expected<void, std::string> InitMainPanel()
{
	if (!HeadlessMode) {
		BottomBuffer.emplace(GetMainPanel().size.width, (GetMainPanel().size.height + PanelPaddingHeight) * (IsChatAvailable() ? 2 : 1));
		pManaBuff.emplace(88, 88);
		pLifeBuff.emplace(88, 88);

		RETURN_IF_ERROR(LoadCharPanel());
		RETURN_IF_ERROR(LoadLargeSpellIcons());
		{
			ASSIGN_OR_RETURN(const OwnedClxSpriteList sprite, LoadCelWithStatus("ctrlpan\\panel8", GetMainPanel().size.width));
			ClxDraw(*BottomBuffer, { 0, (GetMainPanel().size.height + PanelPaddingHeight) - 1 }, sprite[0]);
		}
		{
			const Point bulbsPosition { 0, 87 };
			ASSIGN_OR_RETURN(const OwnedClxSpriteList statusPanel, LoadCelWithStatus("ctrlpan\\p8bulbs", 88));
			ClxDraw(*pLifeBuff, bulbsPosition, statusPanel[0]);
			ClxDraw(*pManaBuff, bulbsPosition, statusPanel[1]);
		}
	}
	ChatFlag = false;
	ChatInputState = std::nullopt;
	if (IsChatAvailable()) {
		if (!HeadlessMode) {
			{
				ASSIGN_OR_RETURN(const OwnedClxSpriteList sprite, LoadCelWithStatus("ctrlpan\\talkpanl", GetMainPanel().size.width));
				ClxDraw(*BottomBuffer, { 0, (GetMainPanel().size.height + PanelPaddingHeight) * 2 - 1 }, sprite[0]);
			}
			multiButtons = LoadCel("ctrlpan\\p8but2", 33);
			talkButtons = LoadCel("ctrlpan\\talkbutt", 61);
		}
		sgbPlrTalkTbl = 0;
		TalkMessage[0] = '\0';
		for (bool &whisper : WhisperList)
			whisper = true;
		for (bool &talkButtonDown : TalkButtonsDown)
			talkButtonDown = false;
	}
	MainPanelFlag = false;
	LevelButtonDown = false;
	if (!HeadlessMode) {
		RETURN_IF_ERROR(LoadMainPanel());
		ASSIGN_OR_RETURN(pMainPanelButtons, LoadCelWithStatus("ctrlpan\\panel8bu", 71));

		static const uint16_t CharButtonsFrameWidths[9] { 95, 41, 41, 41, 41, 41, 41, 41, 41 };
		ASSIGN_OR_RETURN(pChrButtons, LoadCelWithStatus("data\\charbut", CharButtonsFrameWidths));
	}
	ResetMainPanelButtons();
	if (!HeadlessMode)
		pDurIcons = LoadCel("items\\duricons", 32);
	for (bool &buttonEnabled : CharPanelButton)
		buttonEnabled = false;
	CharPanelButtonActive = false;
	InfoString = StringOrView {};
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);
	CloseCharPanel();
	SpellSelectFlag = false;
	SpellbookTab = 0;
	SpellbookFlag = false;

	if (!HeadlessMode) {
		InitSpellBook();
		ASSIGN_OR_RETURN(pQLogCel, LoadCelWithStatus("data\\quest", static_cast<uint16_t>(SidePanelSize.width)));
		ASSIGN_OR_RETURN(GoldBoxBuffer, LoadCelWithStatus("ctrlpan\\golddrop", 261));
	}
	CloseGoldDrop();
	CalculatePanelAreas();

	if (!HeadlessMode)
		InitModifierHints();

	return {};
}

void DrawMainPanel(const Surface &out)
{
	DrawPanelBox(out, MakeSdlRect(0, sgbPlrTalkTbl + PanelPaddingHeight, GetMainPanel().size.width, GetMainPanel().size.height), GetMainPanel().position);
	DrawInfoBox(out);
}

void DrawMainPanelButtons(const Surface &out)
{
	const Point mainPanelPosition = GetMainPanel().position;

	for (int i = 0; i < TotalSpMainPanelButtons; i++) {
		if (!MainPanelButtons[i]) {
			DrawPanelBox(out, MakeSdlRect(MainPanelButtonRect[i].position.x, MainPanelButtonRect[i].position.y + PanelPaddingHeight, MainPanelButtonRect[i].size.width, MainPanelButtonRect[i].size.height + 1), mainPanelPosition + Displacement { MainPanelButtonRect[i].position.x, MainPanelButtonRect[i].position.y });
		} else {
			Point position = mainPanelPosition + Displacement { MainPanelButtonRect[i].position.x, MainPanelButtonRect[i].position.y };
			RenderClxSprite(out, (*pMainPanelButtons)[i], position);
			RenderClxSprite(out, (*PanelButtonDown)[i], position + Displacement { 4, 0 });
		}
	}

	if (IsChatAvailable()) {
		RenderClxSprite(out, (*multiButtons)[MainPanelButtons[PanelButtonSendmsg] ? 1 : 0], mainPanelPosition + Displacement { MainPanelButtonRect[PanelButtonSendmsg].position.x, MainPanelButtonRect[PanelButtonSendmsg].position.y });

		Point friendlyButtonPosition = mainPanelPosition + Displacement { MainPanelButtonRect[PanelButtonFriendly].position.x, MainPanelButtonRect[PanelButtonFriendly].position.y };

		if (MyPlayer->friendlyMode)
			RenderClxSprite(out, (*multiButtons)[MainPanelButtons[PanelButtonFriendly] ? 3 : 2], friendlyButtonPosition);
		else
			RenderClxSprite(out, (*multiButtons)[MainPanelButtons[PanelButtonFriendly] ? 5 : 4], friendlyButtonPosition);
	}
}

void ResetMainPanelButtons()
{
	for (bool &panelButton : MainPanelButtons)
		panelButton = false;
	SetMainPanelButtonUp();
}

void CheckMainPanelButton()
{
	int totalButtons = IsChatAvailable() ? TotalMpMainPanelButtons : TotalSpMainPanelButtons;

	for (int i = 0; i < totalButtons; i++) {
		Rectangle button = MainPanelButtonRect[i];

		SetPanelObjectPosition(UiPanels::Main, button);

		if (button.contains(MousePosition)) {
			SetMainPanelButtonDown(i);
		}
	}

	Rectangle spellSelectButton = SpellButtonRect;

	SetPanelObjectPosition(UiPanels::Main, spellSelectButton);

	if (!SpellSelectFlag && spellSelectButton.contains(MousePosition)) {
		if ((SDL_GetModState() & KMOD_SHIFT) != 0) {
			Player &myPlayer = *MyPlayer;
			myPlayer._pRSpell = SpellID::Invalid;
			myPlayer._pRSplType = SpellType::Invalid;
			RedrawEverything();
			return;
		}
		DoSpeedBook();
		gamemenu_off();
	}
}

void CheckMainPanelButtonDead()
{
	Rectangle menuButton = MainPanelButtonRect[PanelButtonMainmenu];

	SetPanelObjectPosition(UiPanels::Main, menuButton);

	if (menuButton.contains(MousePosition)) {
		SetMainPanelButtonDown(PanelButtonMainmenu);
		return;
	}

	Rectangle chatButton = MainPanelButtonRect[PanelButtonSendmsg];

	SetPanelObjectPosition(UiPanels::Main, chatButton);

	if (chatButton.contains(MousePosition)) {
		SetMainPanelButtonDown(PanelButtonSendmsg);
	}
}

void DoAutoMap()
{
	if (!AutomapActive)
		StartAutomap();
	else
		AutomapActive = false;
}

void CycleAutomapType()
{
	if (!AutomapActive) {
		StartAutomap();
		return;
	}
	const AutomapType newType { static_cast<std::underlying_type_t<AutomapType>>(
		(static_cast<unsigned>(GetAutomapType()) + 1) % enum_size<AutomapType>::value) };
	SetAutomapType(newType);
	if (newType == AutomapType::FIRST) {
		AutomapActive = false;
	}
}

void CheckPanelInfo()
{
	MainPanelFlag = false;
	InfoString = StringOrView {};

	int totalButtons = IsChatAvailable() ? TotalMpMainPanelButtons : TotalSpMainPanelButtons;

	for (int i = 0; i < totalButtons; i++) {
		Rectangle button = MainPanelButtonRect[i];

		SetPanelObjectPosition(UiPanels::Main, button);

		if (button.contains(MousePosition)) {
			if (i != 7) {
				InfoString = _(PanBtnStr[i]);
			} else {
				if (MyPlayer->friendlyMode)
					InfoString = _("Player friendly");
				else
					InfoString = _("Player attack");
			}
			if (PanBtnHotKey[i] != nullptr) {
				AddInfoBoxString(fmt::format(fmt::runtime(_("Hotkey: {:s}")), _(PanBtnHotKey[i])));
			}
			InfoColor = UiFlags::ColorWhite;
			MainPanelFlag = true;
		}
	}

	Rectangle spellSelectButton = SpellButtonRect;

	SetPanelObjectPosition(UiPanels::Main, spellSelectButton);

	if (!SpellSelectFlag && spellSelectButton.contains(MousePosition)) {
		InfoString = _("Select current spell button");
		InfoColor = UiFlags::ColorWhite;
		MainPanelFlag = true;
		AddInfoBoxString(_("Hotkey: 's'"));
		const Player &myPlayer = *MyPlayer;
		const SpellID spellId = myPlayer._pRSpell;
		if (IsValidSpell(spellId)) {
			switch (myPlayer._pRSplType) {
			case SpellType::Skill:
				AddInfoBoxString(fmt::format(fmt::runtime(_("{:s} Skill")), pgettext("spell", GetSpellData(spellId).sNameText)));
				break;
			case SpellType::Spell: {
				AddInfoBoxString(fmt::format(fmt::runtime(_("{:s} Spell")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const int spellLevel = myPlayer.GetSpellLevel(spellId);
				AddInfoBoxString(spellLevel == 0 ? _("Spell Level 0 - Unusable") : fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			} break;
			case SpellType::Scroll: {
				AddInfoBoxString(fmt::format(fmt::runtime(_("Scroll of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const int scrollCount = c_count_if(InventoryAndBeltPlayerItemsRange { myPlayer }, [spellId](const Item &item) {
					return item.isScrollOf(spellId);
				});
				AddInfoBoxString(fmt::format(fmt::runtime(ngettext("{:d} Scroll", "{:d} Scrolls", scrollCount)), scrollCount));
			} break;
			case SpellType::Charges:
				AddInfoBoxString(fmt::format(fmt::runtime(_("Staff of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				AddInfoBoxString(fmt::format(fmt::runtime(ngettext("{:d} Charge", "{:d} Charges", myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges)), myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges));
				break;
			case SpellType::Invalid:
				break;
			}
		}
	}

	Rectangle belt = BeltRect;

	SetPanelObjectPosition(UiPanels::Main, belt);

	if (belt.contains(MousePosition))
		pcursinvitem = CheckInvHLight();

	if (CheckXPBarInfo())
		MainPanelFlag = true;
}

void CheckMainPanelButtonUp()
{
	bool gamemenuOff = true;

	SetMainPanelButtonUp();

	for (int i = PanelButtonFirst; i <= PanelButtonLast; i++) {
		if (!MainPanelButtons[i])
			continue;

		MainPanelButtons[i] = false;

		Rectangle button = MainPanelButtonRect[i];

		SetPanelObjectPosition(UiPanels::Main, button);

		if (!button.contains(MousePosition))
			continue;

		switch (i) {
		case PanelButtonCharinfo:
			ToggleCharPanel();
			break;
		case PanelButtonQlog:
			CloseCharPanel();
			CloseGoldWithdraw();
			CloseStash();
			if (!QuestLogIsOpen)
				StartQuestlog();
			else
				QuestLogIsOpen = false;
			break;
		case PanelButtonAutomap:
			DoAutoMap();
			break;
		case PanelButtonMainmenu:
			if (MyPlayerIsDead) {
				if (!gbIsMultiplayer) {
					if (gbValidSaveFile)
						gamemenu_load_game(false);
					else
						gamemenu_exit_game(false);
				} else {
					NetSendCmd(true, CMD_RETOWN);
				}
				break;
			} else if (MyPlayer->_pHitPoints == 0) {
				break;
			}
			qtextflag = false;
			gamemenu_handle_previous();
			gamemenuOff = false;
			break;
		case PanelButtonInventory:
			SpellbookFlag = false;
			CloseGoldWithdraw();
			CloseStash();
			invflag = !invflag;
			CloseGoldDrop();
			break;
		case PanelButtonSpellbook:
			CloseInventory();
			CloseGoldDrop();
			SpellbookFlag = !SpellbookFlag;
			break;
		case PanelButtonSendmsg:
			if (ChatFlag)
				ResetChat();
			else
				TypeChatMessage();
			break;
		case PanelButtonFriendly:
			// Toggle friendly Mode
			NetSendCmd(true, CMD_FRIENDLYMODE);
			break;
		}
	}

	if (gamemenuOff)
		gamemenu_off();
}

void FreeControlPan()
{
	BottomBuffer = std::nullopt;
	pManaBuff = std::nullopt;
	pLifeBuff = std::nullopt;
	FreeLargeSpellIcons();
	FreeSpellBook();
	pMainPanelButtons = std::nullopt;
	multiButtons = std::nullopt;
	talkButtons = std::nullopt;
	pChrButtons = std::nullopt;
	pDurIcons = std::nullopt;
	pQLogCel = std::nullopt;
	GoldBoxBuffer = std::nullopt;
	FreeMainPanel();
	FreeCharPanel();
	FreeModifierHints();
}

void DrawInfoBox(const Surface &out)
{
	DrawPanelBox(out, MakeSdlRect(InfoBoxRect.position.x, InfoBoxRect.position.y + PanelPaddingHeight, InfoBoxRect.size.width, InfoBoxRect.size.height), GetMainPanel().position + Displacement { InfoBoxRect.position.x, InfoBoxRect.position.y });
	if (!MainPanelFlag && !trigflag && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && !SpellSelectFlag && pcurs != CURSOR_HOURGLASS) {
		InfoString = StringOrView {};
		InfoColor = UiFlags::ColorWhite;
	}
	Player &myPlayer = *MyPlayer;
	if (SpellSelectFlag || trigflag || pcurs == CURSOR_HOURGLASS) {
		InfoColor = UiFlags::ColorWhite;
	} else if (!myPlayer.HoldItem.isEmpty()) {
		if (myPlayer.HoldItem._itype == ItemType::Gold) {
			int nGold = myPlayer.HoldItem._ivalue;
			InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
		} else if (!myPlayer.CanUseItem(myPlayer.HoldItem)) {
			InfoString = _("Requirements not met");
		} else {
			InfoString = myPlayer.HoldItem.getName();
			InfoColor = myPlayer.HoldItem.getTextColor();
		}
	} else {
		if (pcursitem != -1)
			GetItemStr(Items[pcursitem]);
		else if (ObjectUnderCursor != nullptr)
			GetObjectStr(*ObjectUnderCursor);
		if (pcursmonst != -1) {
			if (leveltype != DTYPE_TOWN) {
				const Monster &monster = Monsters[pcursmonst];
				InfoColor = UiFlags::ColorWhite;
				InfoString = monster.name();
				if (monster.isUnique()) {
					InfoColor = UiFlags::ColorWhitegold;
					PrintUniqueHistory();
				} else {
					PrintMonstHistory(monster.type().type);
				}
			} else if (pcursitem == -1) {
				InfoString = std::string_view(Towners[pcursmonst].name);
			}
		}
		if (PlayerUnderCursor != nullptr) {
			InfoColor = UiFlags::ColorWhitegold;
			auto &target = *PlayerUnderCursor;
			InfoString = std::string_view(target._pName);
			AddInfoBoxString(fmt::format(fmt::runtime(_("{:s}, Level: {:d}")), target.getClassName(), target.getCharacterLevel()));
			AddInfoBoxString(fmt::format(fmt::runtime(_("Hit Points {:d} of {:d}")), target._pHitPoints >> 6, target._pMaxHP >> 6));
		}
	}
	if (!InfoString.empty())
		PrintInfo(out);
}

void CheckLevelButton()
{
	if (!IsLevelUpButtonVisible()) {
		return;
	}

	Rectangle button = LevelButtonRect;

	SetPanelObjectPosition(UiPanels::Main, button);

	if (!LevelButtonDown && button.contains(MousePosition))
		LevelButtonDown = true;
}

void CheckLevelButtonUp()
{
	Rectangle button = LevelButtonRect;

	SetPanelObjectPosition(UiPanels::Main, button);

	if (button.contains(MousePosition)) {
		OpenCharPanel();
	}
	LevelButtonDown = false;
}

void DrawLevelButton(const Surface &out)
{
	if (IsLevelUpButtonVisible()) {
		int nCel = LevelButtonDown ? 2 : 1;
		DrawString(out, _("Level Up"), { GetMainPanel().position + Displacement { 0, LevelButtonRect.position.y - 23 }, { 120, 0 } },
		    { .flags = UiFlags::ColorWhite | UiFlags::AlignCenter | UiFlags::KerningFitSpacing });
		RenderClxSprite(out, (*pChrButtons)[nCel], GetMainPanel().position + Displacement { LevelButtonRect.position.x, LevelButtonRect.position.y });
	}
}

void CheckChrBtns()
{
	Player &myPlayer = *MyPlayer;

	if (CharPanelButtonActive || myPlayer._pStatPts == 0)
		return;

	for (auto attribute : enum_values<CharacterAttribute>()) {
		if (myPlayer.GetBaseAttributeValue(attribute) >= myPlayer.GetMaximumAttributeValue(attribute))
			continue;
		auto buttonId = static_cast<size_t>(attribute);
		Rectangle button = CharPanelButtonRect[buttonId];
		SetPanelObjectPosition(UiPanels::Character, button);
		if (button.contains(MousePosition)) {
			CharPanelButton[buttonId] = true;
			CharPanelButtonActive = true;
		}
	}
}

void ReleaseChrBtns(bool addAllStatPoints)
{
	CharPanelButtonActive = false;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		auto buttonId = static_cast<size_t>(attribute);
		if (!CharPanelButton[buttonId])
			continue;

		CharPanelButton[buttonId] = false;
		Rectangle button = CharPanelButtonRect[buttonId];
		SetPanelObjectPosition(UiPanels::Character, button);
		if (button.contains(MousePosition)) {
			Player &myPlayer = *MyPlayer;
			int statPointsToAdd = 1;
			if (addAllStatPoints)
				statPointsToAdd = CapStatPointsToAdd(myPlayer._pStatPts, myPlayer, attribute);
			switch (attribute) {
			case CharacterAttribute::Strength:
				NetSendCmdParam1(true, CMD_ADDSTR, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Magic:
				NetSendCmdParam1(true, CMD_ADDMAG, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Dexterity:
				NetSendCmdParam1(true, CMD_ADDDEX, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Vitality:
				NetSendCmdParam1(true, CMD_ADDVIT, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			}
		}
	}
}

void DrawDurIcon(const Surface &out)
{
	bool hasRoomBetweenPanels = RightPanel.position.x - (LeftPanel.position.x + LeftPanel.size.width) >= 16 + (32 + 8 + 32 + 8 + 32 + 8 + 32) + 16;
	bool hasRoomUnderPanels = MainPanel.position.y - (RightPanel.position.y + RightPanel.size.height) >= 16 + 32 + 16;

	if (!hasRoomBetweenPanels && !hasRoomUnderPanels) {
		if (IsLeftPanelOpen() && IsRightPanelOpen())
			return;
	}

	int x = MainPanel.position.x + MainPanel.size.width - 32 - 16;
	if (!hasRoomUnderPanels) {
		if (IsRightPanelOpen() && MainPanel.position.x + MainPanel.size.width > RightPanel.position.x)
			x -= MainPanel.position.x + MainPanel.size.width - RightPanel.position.x;
	}

	Player &myPlayer = *MyPlayer;
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HEAD], x, 3);
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_CHEST], x, 2);
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HAND_LEFT], x, 0);
	DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HAND_RIGHT], x, 0);
}

void RedBack(const Surface &out)
{
	uint8_t *dst = out.begin();
	uint8_t *tbl = GetPauseTRN();
	for (int h = gnViewportHeight; h != 0; h--, dst += out.pitch() - gnScreenWidth) {
		for (int w = gnScreenWidth; w != 0; w--) {
			if (leveltype != DTYPE_HELL || *dst >= 32)
				*dst = tbl[*dst];
			dst++;
		}
	}
}

void DrawDeathText(const Surface &out)
{
	const TextRenderOptions largeTextOptions {
		.flags = UiFlags::FontSize42 | UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter,
		.spacing = 2
	};
	const TextRenderOptions smallTextOptions {
		.flags = UiFlags::FontSize30 | UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter,
		.spacing = 2
	};
	std::string text;
	int verticalPadding = 42;
	Point linePosition { 0, gnScreenHeight / 2 - (verticalPadding * 2) };

	text = _("You have died");
	DrawString(out, text, linePosition, largeTextOptions);
	linePosition.y += verticalPadding;

	std::string buttonText;

	switch (ControlMode) {
	case ControlTypes::KeyboardAndMouse:
		buttonText = _("ESC");
		break;
	case ControlTypes::Gamepad:
		buttonText = ToString(GamepadType, ControllerButton_BUTTON_START);
		break;
	case ControlTypes::VirtualGamepad:
		buttonText = _("Menu Button");
		break;
	default:
		break;
	}

	if (!gbIsMultiplayer) {
		if (gbValidSaveFile)
			text = fmt::format(fmt::runtime(_("Press {} to load last save.")), buttonText);
		else
			text = fmt::format(fmt::runtime(_("Press {} to return to Main Menu.")), buttonText);

	} else {
		text = fmt::format(fmt::runtime(_("Press {} to restart in town.")), buttonText);
	}
	DrawString(out, text, linePosition, smallTextOptions);
}

void DrawGoldSplit(const Surface &out)
{
	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Inventory, { dialogX, 178 }), (*GoldBoxBuffer)[0]);

	const std::string_view amountText = GoldDropText;
	const TextInputCursorState &cursor = GoldDropCursor;
	const int max = GetGoldDropMax();

	const std::string description = fmt::format(
	    fmt::runtime(ngettext(
	        /* TRANSLATORS: {:s} is a number with separators. Dialog is shown when splitting a stash of Gold.*/
	        "You have {:s} gold piece. How many do you want to remove?",
	        "You have {:s} gold pieces. How many do you want to remove?",
	        max)),
	    FormatInteger(max));

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(description, 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Inventory, { dialogX + 31, 75 }), { 200, 50 } },
	    { .flags = UiFlags::ColorWhitegold | UiFlags::AlignCenter, .lineHeight = 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Inventory, { dialogX + 37, 128 }),
	    {
	        .flags = UiFlags::ColorWhite | UiFlags::PentaCursor,
	        .cursorPosition = static_cast<int>(cursor.position),
	        .highlightRange = { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
}

void control_drop_gold(SDL_Keycode vkey)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pHitPoints >> 6 <= 0) {
		CloseGoldDrop();
		return;
	}

	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (const int value = GoldDropInputState->value(); value != 0) {
			RemoveGold(myPlayer, GoldDropInvIndex, value);
		}
		CloseGoldDrop();
		break;
	case SDLK_ESCAPE:
		CloseGoldDrop();
		break;
	default:
		break;
	}
}

void DrawChatBox(const Surface &out)
{
	if (!ChatFlag)
		return;

	const Point mainPanelPosition = GetMainPanel().position;

	DrawPanelBox(out, MakeSdlRect(175, sgbPlrTalkTbl + 20, 294, 5), mainPanelPosition + Displacement { 175, 4 });
	int off = 0;
	for (int i = 293; i > 283; off++, i--) {
		DrawPanelBox(out, MakeSdlRect((off / 2) + 175, sgbPlrTalkTbl + off + 25, i, 1), mainPanelPosition + Displacement { (off / 2) + 175, off + 9 });
	}
	DrawPanelBox(out, MakeSdlRect(185, sgbPlrTalkTbl + 35, 274, 30), mainPanelPosition + Displacement { 185, 19 });
	DrawPanelBox(out, MakeSdlRect(180, sgbPlrTalkTbl + 65, 284, 5), mainPanelPosition + Displacement { 180, 49 });
	for (int i = 0; i < 10; i++) {
		DrawPanelBox(out, MakeSdlRect(180, sgbPlrTalkTbl + i + 70, i + 284, 1), mainPanelPosition + Displacement { 180, i + 54 });
	}
	DrawPanelBox(out, MakeSdlRect(170, sgbPlrTalkTbl + 80, 310, 55), mainPanelPosition + Displacement { 170, 64 });

	int x = mainPanelPosition.x + 200;
	int y = mainPanelPosition.y + 10;

	const uint32_t len = DrawString(out, TalkMessage, { { x, y }, { 250, 39 } },
	    {
	        .flags = UiFlags::ColorWhite | UiFlags::PentaCursor,
	        .lineHeight = 13,
	        .cursorPosition = static_cast<int>(ChatCursor.position),
	        .highlightRange = { static_cast<int>(ChatCursor.selection.begin), static_cast<int>(ChatCursor.selection.end) },
	    });
	ChatInputState->truncate(len);

	x += 46;
	int talkBtn = 0;
	for (size_t i = 0; i < Players.size(); i++) {
		Player &player = Players[i];
		if (&player == MyPlayer)
			continue;

		UiFlags color = player.friendlyMode ? UiFlags::ColorWhitegold : UiFlags::ColorRed;
		const Point talkPanPosition = mainPanelPosition + Displacement { 172, 84 + 18 * talkBtn };
		if (WhisperList[i]) {
			// the normal (unpressed) voice button is pre-rendered on the panel, only need to draw over it when the button is held
			if (TalkButtonsDown[talkBtn]) {
				unsigned spriteIndex = talkBtn == 0 ? 2 : 3; // the first button sprite includes a tip from the devils wing so is different to the rest.
				ClxDraw(out, talkPanPosition, (*talkButtons)[spriteIndex]);

				// Draw the translated string over the top of the default (english) button. This graphic is inset to avoid overlapping the wingtip, letting
				// the first button be treated the same as the other two further down the panel.
				RenderClxSprite(out, (*TalkButton)[2], talkPanPosition + Displacement { 4, -15 });
			}
		} else {
			unsigned spriteIndex = talkBtn == 0 ? 0 : 1; // the first button sprite includes a tip from the devils wing so is different to the rest.
			if (TalkButtonsDown[talkBtn])
				spriteIndex += 4; // held button sprites are at index 4 and 5 (with and without wingtip respectively)
			ClxDraw(out, talkPanPosition, (*talkButtons)[spriteIndex]);

			// Draw the translated string over the top of the default (english) button. This graphic is inset to avoid overlapping the wingtip, letting
			// the first button be treated the same as the other two further down the panel.
			RenderClxSprite(out, (*TalkButton)[TalkButtonsDown[talkBtn] ? 1 : 0], talkPanPosition + Displacement { 4, -15 });
		}
		if (player.plractive) {
			DrawString(out, player._pName, { { x, y + 60 + talkBtn * 18 }, { 204, 0 } }, { .flags = color });
		}

		talkBtn++;
	}
}

bool CheckMuteButton()
{
	if (!ChatFlag)
		return false;

	Rectangle buttons = MuteButtonRect;

	SetPanelObjectPosition(UiPanels::Main, buttons);

	buttons.size.height = (MuteButtons * buttons.size.height) + ((MuteButtons - 1) * MuteButtonPadding);

	if (!buttons.contains(MousePosition))
		return false;

	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}

	const Point mainPanelPosition = GetMainPanel().position;

	TalkButtonsDown[(MousePosition.y - (69 + mainPanelPosition.y)) / 18] = true;

	return true;
}

void CheckMuteButtonUp()
{
	if (!ChatFlag)
		return;

	for (bool &talkButtonDown : TalkButtonsDown)
		talkButtonDown = false;

	Rectangle buttons = MuteButtonRect;

	SetPanelObjectPosition(UiPanels::Main, buttons);

	buttons.size.height = (MuteButtons * buttons.size.height) + ((MuteButtons - 1) * MuteButtonPadding);

	if (!buttons.contains(MousePosition))
		return;

	int off = (MousePosition.y - buttons.position.y) / (MuteButtonRect.size.height + MuteButtonPadding);

	size_t playerId = 0;
	for (; playerId < Players.size() && off != -1; ++playerId) {
		if (playerId != MyPlayerId)
			off--;
	}
	if (playerId > 0 && playerId <= Players.size())
		WhisperList[playerId - 1] = !WhisperList[playerId - 1];
}

void TypeChatMessage()
{
	if (!IsChatAvailable())
		return;

	ChatFlag = true;
	TalkMessage[0] = '\0';
	ChatInputState.emplace(TextInputState::Options {
	    .value = TalkMessage,
	    .cursor = &ChatCursor,
	    .maxLength = sizeof(TalkMessage) - 1 });
	SDL_Rect rect = MakeSdlRect(GetMainPanel().position.x + 200, GetMainPanel().position.y + 22, 0, 27);
	SDL_SetTextInputRect(&rect);
	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}
	sgbPlrTalkTbl = GetMainPanel().size.height + PanelPaddingHeight;
	RedrawEverything();
	TalkSaveIndex = NextTalkSave;
	SDL_StartTextInput();
}

void ResetChat()
{
	ChatFlag = false;
	SDL_StopTextInput();
	ChatInputState = std::nullopt;
	sgbPlrTalkTbl = 0;
	RedrawEverything();
}

bool IsChatActive()
{
	if (!IsChatAvailable())
		return false;

	if (!ChatFlag)
		return false;

	return true;
}

template <typename InputStateType>
bool HandleInputEvent(const SDL_Event &event, std::optional<InputStateType> &inputState)
{
	if (!inputState) {
		return false; // No input state to handle
	}

	if constexpr (std::is_same_v<InputStateType, TextInputState>) {
		return HandleTextInputEvent(event, *inputState);
	} else if constexpr (std::is_same_v<InputStateType, NumberInputState>) {
		return HandleNumberInputEvent(event, *inputState);
	}

	return false; // Unknown input state type
}

bool HandleTalkTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, ChatInputState);
}

bool CheckKeypress(SDL_Keycode vkey)
{
	if (!IsChatAvailable())
		return false;
	if (!ChatFlag)
		return false;

	switch (vkey) {
	case SDLK_ESCAPE:
		ResetChat();
		return true;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		ControlPressEnter();
		return true;
	case SDLK_DOWN:
		ControlUpDown(1);
		return true;
	case SDLK_UP:
		ControlUpDown(-1);
		return true;
	default:
		return vkey >= SDLK_SPACE && vkey <= SDLK_z;
	}
}

void DiabloHotkeyMsg(uint32_t dwMsg)
{
	assert(dwMsg < QuickMessages.size());

#ifdef _DEBUG
	constexpr std::string_view LuaPrefix = "/lua ";
	for (const std::string &msg : GetOptions().Chat.szHotKeyMsgs[dwMsg]) {
		if (!msg.starts_with(LuaPrefix)) continue;
		InitConsole();
		RunInConsole(std::string_view(msg).substr(LuaPrefix.size()));
	}
#endif

	if (!IsChatAvailable()) {
		return;
	}

	for (const std::string &msg : GetOptions().Chat.szHotKeyMsgs[dwMsg]) {
#ifdef _DEBUG
		if (msg.starts_with(LuaPrefix)) continue;
#endif
		char charMsg[MAX_SEND_STR_LEN];
		CopyUtf8(charMsg, msg, sizeof(charMsg));
		NetSendCmdString(0xFFFFFF, charMsg);
	}
}

void OpenGoldDrop(int8_t invIndex, int max)
{
	DropGoldFlag = true;
	GoldDropInvIndex = invIndex;
	GoldDropText[0] = '\0';
	GoldDropInputState.emplace(NumberInputState::Options {
	    .textOptions {
	        .value = GoldDropText,
	        .cursor = &GoldDropCursor,
	        .maxLength = sizeof(GoldDropText) - 1,
	    },
	    .min = 0,
	    .max = max,
	});
	SDL_StartTextInput();
}

void CloseGoldDrop()
{
	if (!DropGoldFlag)
		return;
	SDL_StopTextInput();
	DropGoldFlag = false;
	GoldDropInputState = std::nullopt;
	GoldDropInvIndex = 0;
}

int GetGoldDropMax()
{
	return GoldDropInputState->max();
}

bool HandleGoldDropTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, GoldDropInputState);
}

} // namespace devilution
