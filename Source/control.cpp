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
#include <string>

#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "automap.h"
#include "controls/modifier_hints.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "error.h"
#include "gamemenu.h"
#include "init.h"
#include "inv.h"
#include "inv_iterators.hpp"
#include "levels/setmaps.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "panels/mainpanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "stores.h"
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"
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

bool chrbtn[4];
bool lvlbtndown;
bool chrbtnactive;
UiFlags InfoColor;
int sbooktab;
bool talkflag;
bool sbookflag;
bool chrflag;
StringOrView InfoString;
bool panelflag;
bool panbtndown;
bool spselflag;
Rectangle MainPanel;
Rectangle LeftPanel;
Rectangle RightPanel;
std::optional<OwnedSurface> pBtmBuff;
OptionalOwnedClxSpriteList pGBoxBuff;

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
	return chrflag || QuestLogIsOpen || IsStashOpen;
}
bool IsRightPanelOpen()
{
	return invflag || sbookflag;
}

constexpr Size IncrementAttributeButtonSize { 41, 22 };
/** Maps from attribute_id to the rectangle on screen used for attribute increment buttons. */
Rectangle ChrBtnsRect[4] = {
	{ { 137, 138 }, IncrementAttributeButtonSize },
	{ { 137, 166 }, IncrementAttributeButtonSize },
	{ { 137, 195 }, IncrementAttributeButtonSize },
	{ { 137, 223 }, IncrementAttributeButtonSize }
};

/** Positions of panel buttons. */
SDL_Rect PanBtnPos[8] = {
	// clang-format off
	{   9,   9, 71, 19 }, // char button
	{   9,  35, 71, 19 }, // quests button
	{   9,  75, 71, 19 }, // map button
	{   9, 101, 71, 19 }, // menu button
	{ 560,   9, 71, 19 }, // inv button
	{ 560,  35, 71, 19 }, // spells button
	{  87,  91, 33, 32 }, // chat button
	{ 527,  91, 33, 32 }, // friendly fire button
	// clang-format on
};

namespace {

std::optional<OwnedSurface> pLifeBuff;
std::optional<OwnedSurface> pManaBuff;
OptionalOwnedClxSpriteList talkButtons;
OptionalOwnedClxSpriteList pDurIcons;
OptionalOwnedClxSpriteList multiButtons;
OptionalOwnedClxSpriteList pPanelButtons;

bool PanelButtons[8];
int PanelButtonIndex;
char TalkSave[8][MAX_SEND_STR_LEN];
uint8_t TalkSaveIndex;
uint8_t NextTalkSave;
char TalkMessage[MAX_SEND_STR_LEN];
bool TalkButtonsDown[3];
int sgbPlrTalkTbl;
bool WhisperList[MAX_PLRS];

TextInputCursorState ChatCursor;
std::optional<TextInputState> ChatInputState;

enum panel_button_id : uint8_t {
	PanelButtonCharinfo,
	PanelButtonQlog,
	PanelButtonAutomap,
	PanelButtonMainmenu,
	PanelButtonInventory,
	PanelButtonSpellbook,
	PanelButtonSendmsg,
	PanelButtonFriendly,
};

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
 * Draws a section of the empty flask cel on top of the panel to create the illusion
 * of the flask getting empty. This function takes a cel and draws a
 * horizontal stripe of height (max-min) onto the given buffer.
 * @param out Target buffer.
 * @param position Buffer coordinate.
 * @param celBuf Buffer of the empty flask cel.
 * @param y0 Top of the flask cel section to draw.
 * @param y1 Bottom of the flask cel section to draw.
 */
void DrawFlaskTop(const Surface &out, Point position, const Surface &celBuf, int y0, int y1)
{
	out.BlitFrom(celBuf, MakeSdlRect(0, static_cast<decltype(SDL_Rect {}.y)>(y0), celBuf.w(), y1 - y0), position);
}

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
void DrawFlask(const Surface &out, const Surface &celBuf, Point sourcePosition, Point targetPosition, int h)
{
	constexpr int FlaskWidth = 59;
	out.BlitFromSkipColorIndexZero(celBuf, MakeSdlRect(sourcePosition.x, sourcePosition.y, FlaskWidth, h), targetPosition);
}

/**
 * @brief Draws the part of the life/mana flasks protruding above the bottom panel
 * @see DrawFlaskLower()
 * @param out The display region to draw to
 * @param sourceBuffer A sprite representing the appropriate background/empty flask style
 * @param offset X coordinate offset for where the flask should be drawn
 * @param fillPer How full the flask is (a value from 0 to 80)
 */
void DrawFlaskUpper(const Surface &out, const Surface &sourceBuffer, int offset, int fillPer)
{
	// clamping because this function only draws the top 12% of the flask display
	int emptyPortion = clamp(80 - fillPer, 0, 11) + 2; // +2 to account for the frame being included in the sprite

	// Draw the empty part of the flask
	DrawFlask(out, sourceBuffer, { 13, 3 }, GetMainPanel().position + Displacement { offset, -13 }, emptyPortion);
	if (emptyPortion < 13)
		// Draw the filled part of the flask
		DrawFlask(out, *pBtmBuff, { offset, emptyPortion + 3 }, GetMainPanel().position + Displacement { offset, -13 + emptyPortion }, 13 - emptyPortion);
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
	int filled = clamp(fillPer, 0, 69);

	if (filled < 69)
		DrawFlaskTop(out, GetMainPanel().position + Displacement { offset, 0 }, sourceBuffer, 16, 85 - filled);

	// It appears that the panel defaults to having a filled flask and DrawFlaskTop only overlays the appropriate amount of empty space.
	// This draw might not be necessary?
	if (filled > 0)
		DrawPanelBox(out, MakeSdlRect(offset, 85 - filled, 88, filled), GetMainPanel().position + Displacement { offset, 69 - filled });
}

void SetButtonStateDown(int btnId)
{
	PanelButtons[btnId] = true;
	RedrawComponent(PanelDrawComponent::ControlButtons);
	panbtndown = true;
}

void PrintInfo(const Surface &out)
{
	if (talkflag)
		return;

	const int space[] = { 18, 12, 6, 3, 0 };
	Rectangle infoArea { GetMainPanel().position + InfoBoxTopLeft, InfoBoxSize };

	const int newLineCount = std::count(InfoString.str().begin(), InfoString.str().end(), '\n');
	const int spaceIndex = std::min(4, newLineCount);
	const int spacing = space[spaceIndex];
	const int lineHeight = 12 + spacing;

	// Adjusting the line height to add spacing between lines
	// will also add additional space beneath the last line
	// which throws off the vertical centering
	infoArea.position.y += spacing / 2;

	DrawString(out, InfoString, infoArea, { InfoColor | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::KerningFitSpacing, 2, lineHeight });
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
	std::string (*actionProc)(const string_view);
};

extern std::vector<TextCmdItem> TextCmdList;

std::string TextCmdHelp(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret;
		StrAppend(ret, _("Available Commands:"));
		for (const TextCmdItem &textCmd : TextCmdList) {
			StrAppend(ret, " ", _(textCmd.text));
		}
		return ret;
	}
	auto textCmdIterator = std::find_if(TextCmdList.begin(), TextCmdList.end(), [&](const TextCmdItem &elem) { return elem.text == parameter; });
	if (textCmdIterator == TextCmdList.end())
		return StrCat(_("Command "), parameter, _(" is unkown."));
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

std::string TextCmdArena(const string_view parameter)
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

	int arenaNumber = atoi(parameter.data());
	_setlevels arenaLevel = static_cast<_setlevels>(arenaNumber - 1 + SL_FIRST_ARENA);
	if (arenaNumber < 0 || !IsArenaLevel(arenaLevel)) {
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

std::string TextCmdArenaPot(const string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}

	Player &myPlayer = *MyPlayer;

	for (int potNumber = std::max(1, atoi(parameter.data())); potNumber > 0; potNumber--) {
		Item item {};
		InitializeItem(item, IDI_ARENAPOT);
		GenerateNewSeed(item);
		item.updateRequiredStatsCacheForPlayer(myPlayer);

		if (!AutoPlaceItemInBelt(myPlayer, item, true) && !AutoPlaceItemInInventory(myPlayer, item, true)) {
			break; // inventory is full
		}
	}

	return ret;
}

std::string TextCmdInspect(const string_view parameter)
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
	for (auto &player : Players) {
		const std::string playerName = AsciiStrToLower(player._pName);
		if (playerName.find(param) != std::string::npos) {
			InspectPlayer = &player;
			StrAppend(ret, _("Inspecting player: "));
			StrAppend(ret, player._pName);
			OpenCharPanel();
			if (!sbookflag)
				invflag = true;
			RedrawEverything();
			return ret;
		}
	}
	StrAppend(ret, _("No players found with such a name"));
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

std::string TextCmdLevelSeed(const string_view parameter)
{
	string_view levelType = setlevel ? "set level" : "dungeon level";

	char gameId[] = {
		static_cast<char>((sgGameInitInfo.programid >> 24) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 16) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 8) & 0xFF),
		static_cast<char>(sgGameInitInfo.programid & 0xFF),
		'\0'
	};

	string_view mode = gbIsMultiplayer ? "MP" : "SP";
	string_view questPool = UseMultiplayerQuests() ? "MP" : "Full";

	uint32_t questFlags = 0;
	for (const Quest &quest : Quests) {
		questFlags <<= 1;
		if (IsQuestEnabled(quest))
			questFlags |= 1;
	}

	return StrCat(
	    "Seedinfo for ", levelType, " ", currlevel, "\n",
	    "seed: ", glSeedTbl[currlevel], "\n",
#ifdef _DEBUG
	    "Mid1: ", glMid1Seed[currlevel], "\n",
	    "Mid2: ", glMid2Seed[currlevel], "\n",
	    "Mid3: ", glMid3Seed[currlevel], "\n",
	    "End: ", glEndSeed[currlevel], "\n",
#endif
	    "\n",
	    gameId, " ", mode, "\n",
	    questPool, " quests: ", questFlags, "\n",
	    "Storybook: ", glSeedTbl[16]);
}

std::vector<TextCmdItem> TextCmdList = {
	{ N_("/help"), N_("Prints help overview or help for a specific command."), N_("[command]"), &TextCmdHelp },
	{ N_("/arena"), N_("Enter a PvP Arena."), N_("<arena-number>"), &TextCmdArena },
	{ N_("/arenapot"), N_("Gives Arena Potions."), N_("<number>"), &TextCmdArenaPot },
	{ N_("/inspect"), N_("Inspects stats and equipment of another player."), N_("<player name>"), &TextCmdInspect },
	{ N_("/seedinfo"), N_("Show seed infos for current level."), "", &TextCmdLevelSeed },
};

bool CheckTextCommand(const string_view text)
{
	if (text.size() < 1 || text[0] != '/')
		return false;

	auto textCmdIterator = std::find_if(TextCmdList.begin(), TextCmdList.end(), [&](const TextCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (textCmdIterator == TextCmdList.end()) {
		InitDiabloMsg(StrCat(_("Command \""), text, "\" is unknown."));
		return true;
	}

	TextCmdItem &textCmd = *textCmdIterator;
	string_view parameter = "";
	if (text.length() > (textCmd.text.length() + 1))
		parameter = text.substr(textCmd.text.length() + 1);
	const std::string result = textCmd.actionProc(parameter);
	if (result != "")
		InitDiabloMsg(result);
	return true;
}

void ResetTalkMsg()
{
#ifdef _DEBUG
	if (CheckDebugTextCommand(TalkMessage))
		return;
#endif
	if (CheckTextCommand(TalkMessage))
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
		ResetTalkMsg();
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
	control_reset_talk();
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
	if (spselflag || chrflag || MyPlayer->_pStatPts == 0) {
		return false;
	}
	if (ControlMode == ControlTypes::VirtualGamepad) {
		return false;
	}
	if (stextflag != TalkID::None || IsStashOpen) {
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
#ifdef _DEBUG
	return true;
#else
	return gbIsMultiplayer;
#endif
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

	SetCursorPos(ChrBtnsRect[stat].Center());
}

void OpenCharPanel()
{
	QuestLogIsOpen = false;
	CloseGoldWithdraw();
	CloseStash();
	chrflag = true;
}

void CloseCharPanel()
{
	chrflag = false;
	if (IsInspectingPlayer()) {
		InspectPlayer = MyPlayer;
		RedrawEverything();
		InitDiabloMsg(_("Stopped inspecting players."));
	}
}

void ToggleCharPanel()
{
	if (chrflag)
		CloseCharPanel();
	else
		OpenCharPanel();
}

void AddPanelString(string_view str)
{
	if (InfoString.empty())
		InfoString = str;
	else
		InfoString = StrCat(InfoString, "\n", str);
}

void AddPanelString(std::string &&str)
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
	out.BlitFrom(*pBtmBuff, srcRect, targetPosition);
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

	auto drawStringWithShadow = [out, color](string_view text, Point pos) {
		DrawString(out, text, pos + Displacement { -1, -1 }, { UiFlags::ColorBlack | UiFlags::KerningFitSpacing, 0 });
		DrawString(out, text, pos, { color | UiFlags::KerningFitSpacing, 0 });
	};

	std::string currText = StrCat(currValue);
	drawStringWithShadow(currText, pos - Displacement { GetLineWidth(currText, GameFont12) + 1, 0 });
	drawStringWithShadow("/", pos);
	drawStringWithShadow(StrCat(maxValue), pos + Displacement { GetLineWidth("/", GameFont12) + 1, 0 });
}

void control_update_life_mana()
{
	MyPlayer->UpdateManaPercentage();
	MyPlayer->UpdateHitPointPercentage();
}

void InitControlPan()
{
	if (!HeadlessMode) {
		pBtmBuff.emplace(GetMainPanel().size.width, (GetMainPanel().size.height + 16) * (IsChatAvailable() ? 2 : 1));
		pManaBuff.emplace(88, 88);
		pLifeBuff.emplace(88, 88);

		LoadCharPanel();
		LoadLargeSpellIcons();
		{
			const OwnedClxSpriteList sprite = LoadCel("ctrlpan\\panel8", GetMainPanel().size.width);
			ClxDraw(*pBtmBuff, { 0, (GetMainPanel().size.height + 16) - 1 }, sprite[0]);
		}
		{
			const Point bulbsPosition { 0, 87 };
			const OwnedClxSpriteList statusPanel = LoadCel("ctrlpan\\p8bulbs", 88);
			ClxDraw(*pLifeBuff, bulbsPosition, statusPanel[0]);
			ClxDraw(*pManaBuff, bulbsPosition, statusPanel[1]);
		}
	}
	talkflag = false;
	ChatInputState = std::nullopt;
	if (IsChatAvailable()) {
		if (!HeadlessMode) {
			{
				const OwnedClxSpriteList sprite = LoadCel("ctrlpan\\talkpanl", GetMainPanel().size.width);
				ClxDraw(*pBtmBuff, { 0, (GetMainPanel().size.height + 16) * 2 - 1 }, sprite[0]);
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
	panelflag = false;
	lvlbtndown = false;
	if (!HeadlessMode) {
		LoadMainPanel();
		pPanelButtons = LoadCel("ctrlpan\\panel8bu", 71);

		static const uint16_t CharButtonsFrameWidths[9] { 95, 41, 41, 41, 41, 41, 41, 41, 41 };
		pChrButtons = LoadCel("data\\charbut", CharButtonsFrameWidths);
	}
	ClearPanBtn();
	if (!IsChatAvailable())
		PanelButtonIndex = 6;
	else
		PanelButtonIndex = 8;
	if (!HeadlessMode)
		pDurIcons = LoadCel("items\\duricons", 32);
	for (bool &buttonEnabled : chrbtn)
		buttonEnabled = false;
	chrbtnactive = false;
	InfoString = {};
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);
	CloseCharPanel();
	spselflag = false;
	sbooktab = 0;
	sbookflag = false;

	if (!HeadlessMode) {
		InitSpellBook();
		pQLogCel = LoadCel("data\\quest", static_cast<uint16_t>(SidePanelSize.width));
		pGBoxBuff = LoadCel("ctrlpan\\golddrop", 261);
	}
	CloseGoldDrop();
	CalculatePanelAreas();

	if (!HeadlessMode)
		InitModifierHints();
}

void DrawCtrlPan(const Surface &out)
{
	DrawPanelBox(out, MakeSdlRect(0, sgbPlrTalkTbl + 16, GetMainPanel().size.width, GetMainPanel().size.height), GetMainPanel().position);
	DrawInfoBox(out);
}

void DrawCtrlBtns(const Surface &out)
{
	const Point mainPanelPosition = GetMainPanel().position;
	for (int i = 0; i < 6; i++) {
		if (!PanelButtons[i]) {
			DrawPanelBox(out, MakeSdlRect(PanBtnPos[i].x, PanBtnPos[i].y + 16, 71, 20), mainPanelPosition + Displacement { PanBtnPos[i].x, PanBtnPos[i].y });
		} else {
			Point position = mainPanelPosition + Displacement { PanBtnPos[i].x, PanBtnPos[i].y + 18 };
			ClxDraw(out, position, (*pPanelButtons)[i]);
			RenderClxSprite(out, (*PanelButtonDown)[i], position + Displacement { 4, -18 });
		}
	}

	if (PanelButtonIndex == 8) {
		ClxDraw(out, mainPanelPosition + Displacement { 87, 122 }, (*multiButtons)[PanelButtons[6] ? 1 : 0]);
		if (MyPlayer->friendlyMode)
			ClxDraw(out, mainPanelPosition + Displacement { 527, 122 }, (*multiButtons)[PanelButtons[7] ? 3 : 2]);
		else
			ClxDraw(out, mainPanelPosition + Displacement { 527, 122 }, (*multiButtons)[PanelButtons[7] ? 5 : 4]);
	}
}

void ClearPanBtn()
{
	for (bool &panelButton : PanelButtons)
		panelButton = false;
	RedrawComponent(PanelDrawComponent::ControlButtons);
	panbtndown = false;
}

void DoPanBtn()
{
	const Point mainPanelPosition = GetMainPanel().position;

	for (int i = 0; i < PanelButtonIndex; i++) {
		int x = PanBtnPos[i].x + mainPanelPosition.x + PanBtnPos[i].w;
		int y = PanBtnPos[i].y + mainPanelPosition.y + PanBtnPos[i].h;
		if (MousePosition.x >= PanBtnPos[i].x + mainPanelPosition.x && MousePosition.x <= x) {
			if (MousePosition.y >= PanBtnPos[i].y + mainPanelPosition.y && MousePosition.y <= y) {
				PanelButtons[i] = true;
				RedrawComponent(PanelDrawComponent::ControlButtons);
				panbtndown = true;
			}
		}
	}
	if (!spselflag && MousePosition.x >= 565 + mainPanelPosition.x && MousePosition.x < 621 + mainPanelPosition.x && MousePosition.y >= 64 + mainPanelPosition.y && MousePosition.y < 120 + mainPanelPosition.y) {
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

void control_check_btn_press()
{
	const Point mainPanelPosition = GetMainPanel().position;
	int x = PanBtnPos[3].x + mainPanelPosition.x + PanBtnPos[3].w;
	int y = PanBtnPos[3].y + mainPanelPosition.y + PanBtnPos[3].h;
	if (MousePosition.x >= PanBtnPos[3].x + mainPanelPosition.x
	    && MousePosition.x <= x
	    && MousePosition.y >= PanBtnPos[3].y + mainPanelPosition.y
	    && MousePosition.y <= y) {
		SetButtonStateDown(3);
	}
	x = PanBtnPos[6].x + mainPanelPosition.x + PanBtnPos[6].w;
	y = PanBtnPos[6].y + mainPanelPosition.y + PanBtnPos[6].h;
	if (MousePosition.x >= PanBtnPos[6].x + mainPanelPosition.x
	    && MousePosition.x <= x
	    && MousePosition.y >= PanBtnPos[6].y + mainPanelPosition.y
	    && MousePosition.y <= y) {
		SetButtonStateDown(6);
	}
}

void DoAutoMap()
{
	if (!AutomapActive)
		StartAutomap();
	else
		AutomapActive = false;
}

void CheckPanelInfo()
{
	panelflag = false;
	InfoString = StringOrView {};
	const Point mainPanelPosition = GetMainPanel().position;
	for (int i = 0; i < PanelButtonIndex; i++) {
		int xend = PanBtnPos[i].x + mainPanelPosition.x + PanBtnPos[i].w;
		int yend = PanBtnPos[i].y + mainPanelPosition.y + PanBtnPos[i].h;
		if (MousePosition.x >= PanBtnPos[i].x + mainPanelPosition.x && MousePosition.x <= xend && MousePosition.y >= PanBtnPos[i].y + mainPanelPosition.y && MousePosition.y <= yend) {
			if (i != 7) {
				InfoString = _(PanBtnStr[i]);
			} else {
				if (MyPlayer->friendlyMode)
					InfoString = _("Player friendly");
				else
					InfoString = _("Player attack");
			}
			if (PanBtnHotKey[i] != nullptr) {
				AddPanelString(fmt::format(fmt::runtime(_("Hotkey: {:s}")), _(PanBtnHotKey[i])));
			}
			InfoColor = UiFlags::ColorWhite;
			panelflag = true;
		}
	}
	if (!spselflag && MousePosition.x >= 565 + mainPanelPosition.x && MousePosition.x < 621 + mainPanelPosition.x && MousePosition.y >= 64 + mainPanelPosition.y && MousePosition.y < 120 + mainPanelPosition.y) {
		InfoString = _("Select current spell button");
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		AddPanelString(_("Hotkey: 's'"));
		Player &myPlayer = *MyPlayer;
		const SpellID spellId = myPlayer._pRSpell;
		if (IsValidSpell(spellId)) {
			switch (myPlayer._pRSplType) {
			case SpellType::Skill:
				AddPanelString(fmt::format(fmt::runtime(_("{:s} Skill")), pgettext("spell", GetSpellData(spellId).sNameText)));
				break;
			case SpellType::Spell: {
				AddPanelString(fmt::format(fmt::runtime(_("{:s} Spell")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const int spellLevel = myPlayer.GetSpellLevel(spellId);
				AddPanelString(spellLevel == 0 ? _("Spell Level 0 - Unusable") : fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			} break;
			case SpellType::Scroll: {
				AddPanelString(fmt::format(fmt::runtime(_("Scroll of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const InventoryAndBeltPlayerItemsRange items { myPlayer };
				const int scrollCount = std::count_if(items.begin(), items.end(), [spellId](const Item &item) {
					return item.isScrollOf(spellId);
				});
				AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Scroll", "{:d} Scrolls", scrollCount)), scrollCount));
			} break;
			case SpellType::Charges:
				AddPanelString(fmt::format(fmt::runtime(_("Staff of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Charge", "{:d} Charges", myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges)), myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges));
				break;
			case SpellType::Invalid:
				break;
			}
		}
	}
	if (MousePosition.x > 190 + mainPanelPosition.x && MousePosition.x < 437 + mainPanelPosition.x && MousePosition.y > 4 + mainPanelPosition.y && MousePosition.y < 33 + mainPanelPosition.y)
		pcursinvitem = CheckInvHLight();

	if (CheckXPBarInfo()) {
		panelflag = true;
	}
}

void CheckBtnUp()
{
	bool gamemenuOff = true;
	const Point mainPanelPosition = GetMainPanel().position;

	RedrawComponent(PanelDrawComponent::ControlButtons);
	panbtndown = false;

	for (int i = 0; i < 8; i++) {
		if (!PanelButtons[i]) {
			continue;
		}

		PanelButtons[i] = false;

		if (MousePosition.x < PanBtnPos[i].x + mainPanelPosition.x
		    || MousePosition.x > PanBtnPos[i].x + mainPanelPosition.x + PanBtnPos[i].w
		    || MousePosition.y < PanBtnPos[i].y + mainPanelPosition.y
		    || MousePosition.y > PanBtnPos[i].y + mainPanelPosition.y + PanBtnPos[i].h) {
			continue;
		}

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
			qtextflag = false;
			gamemenu_handle_previous();
			gamemenuOff = false;
			break;
		case PanelButtonInventory:
			sbookflag = false;
			CloseGoldWithdraw();
			CloseStash();
			invflag = !invflag;
			if (DropGoldFlag) {
				CloseGoldDrop();
			}
			break;
		case PanelButtonSpellbook:
			CloseInventory();
			if (DropGoldFlag) {
				CloseGoldDrop();
			}
			sbookflag = !sbookflag;
			break;
		case PanelButtonSendmsg:
			if (talkflag)
				control_reset_talk();
			else
				control_type_message();
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
	pBtmBuff = std::nullopt;
	pManaBuff = std::nullopt;
	pLifeBuff = std::nullopt;
	FreeLargeSpellIcons();
	FreeSpellBook();
	pPanelButtons = std::nullopt;
	multiButtons = std::nullopt;
	talkButtons = std::nullopt;
	pChrButtons = std::nullopt;
	pDurIcons = std::nullopt;
	pQLogCel = std::nullopt;
	pGBoxBuff = std::nullopt;
	FreeMainPanel();
	FreeCharPanel();
	FreeModifierHints();
}

void DrawInfoBox(const Surface &out)
{
	DrawPanelBox(out, { 177, 62, InfoBoxSize.width, InfoBoxSize.height }, GetMainPanel().position + InfoBoxTopLeft);
	if (!panelflag && !trigflag && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && !spselflag) {
		InfoString = {};
		InfoColor = UiFlags::ColorWhite;
	}
	Player &myPlayer = *MyPlayer;
	if (spselflag || trigflag) {
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
				const auto &monster = Monsters[pcursmonst];
				InfoColor = UiFlags::ColorWhite;
				InfoString = monster.name();
				if (monster.isUnique()) {
					InfoColor = UiFlags::ColorWhitegold;
					PrintUniqueHistory();
				} else {
					PrintMonstHistory(monster.type().type);
				}
			} else if (pcursitem == -1) {
				InfoString = string_view(Towners[pcursmonst].name);
			}
		}
		if (pcursplr != -1) {
			InfoColor = UiFlags::ColorWhitegold;
			auto &target = Players[pcursplr];
			InfoString = string_view(target._pName);
			AddPanelString(fmt::format(fmt::runtime(_("{:s}, Level: {:d}")), _(PlayersData[static_cast<std::size_t>(target._pClass)].className), target._pLevel));
			AddPanelString(fmt::format(fmt::runtime(_("Hit Points {:d} of {:d}")), target._pHitPoints >> 6, target._pMaxHP >> 6));
		}
	}
	if (!InfoString.empty())
		PrintInfo(out);
}

void CheckLvlBtn()
{
	if (!IsLevelUpButtonVisible()) {
		return;
	}

	const Point mainPanelPosition = GetMainPanel().position;
	if (!lvlbtndown && MousePosition.x >= 40 + mainPanelPosition.x && MousePosition.x <= 81 + mainPanelPosition.x && MousePosition.y >= -39 + mainPanelPosition.y && MousePosition.y <= -17 + mainPanelPosition.y)
		lvlbtndown = true;
}

void ReleaseLvlBtn()
{
	const Point mainPanelPosition = GetMainPanel().position;
	if (MousePosition.x >= 40 + mainPanelPosition.x && MousePosition.x <= 81 + mainPanelPosition.x && MousePosition.y >= -39 + mainPanelPosition.y && MousePosition.y <= -17 + mainPanelPosition.y) {
		OpenCharPanel();
	}
	lvlbtndown = false;
}

void DrawLevelUpIcon(const Surface &out)
{
	if (IsLevelUpButtonVisible()) {
		int nCel = lvlbtndown ? 2 : 1;
		DrawString(out, _("Level Up"), { GetMainPanel().position + Displacement { 0, -62 }, { 120, 0 } }, { UiFlags::ColorWhite | UiFlags::AlignCenter });
		ClxDraw(out, GetMainPanel().position + Displacement { 40, -17 }, (*pChrButtons)[nCel]);
	}
}

void CheckChrBtns()
{
	Player &myPlayer = *MyPlayer;

	if (chrbtnactive || myPlayer._pStatPts == 0)
		return;

	for (auto attribute : enum_values<CharacterAttribute>()) {
		if (myPlayer.GetBaseAttributeValue(attribute) >= myPlayer.GetMaximumAttributeValue(attribute))
			continue;
		auto buttonId = static_cast<size_t>(attribute);
		Rectangle button = ChrBtnsRect[buttonId];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
		if (button.contains(MousePosition)) {
			chrbtn[buttonId] = true;
			chrbtnactive = true;
		}
	}
}

void ReleaseChrBtns(bool addAllStatPoints)
{
	chrbtnactive = false;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		auto buttonId = static_cast<size_t>(attribute);
		if (!chrbtn[buttonId])
			continue;

		chrbtn[buttonId] = false;
		Rectangle button = ChrBtnsRect[buttonId];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
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

void DrawGoldSplit(const Surface &out)
{
	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Inventory, { dialogX, 178 }), (*pGBoxBuff)[0]);

	const string_view amountText = GoldDropText;
	const TextInputCursorState &cursor = GoldDropCursor;
	const int max = GoldDropInputState->max();

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
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Inventory, { dialogX + 37, 128 }),
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(cursor.position),
	        /*highlightRange=*/ { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
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
	case SDLK_KP_ENTER: {
		const int value = GoldDropInputState->value();
		if (value != 0) {
			RemoveGold(myPlayer, GoldDropInvIndex, value);
		}
		CloseGoldDrop();
	} break;
	case SDLK_ESCAPE:
		CloseGoldDrop();
		break;
	default:
		break;
	}
}

void DrawTalkPan(const Surface &out)
{
	if (!talkflag)
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
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/13,
	        /*cursorPosition=*/static_cast<int>(ChatCursor.position),
	        /*highlightRange=*/ { static_cast<int>(ChatCursor.selection.begin), static_cast<int>(ChatCursor.selection.end) },
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
			DrawString(out, player._pName, { { x, y + 60 + talkBtn * 18 }, { 204, 0 } }, { color });
		}

		talkBtn++;
	}
}

bool control_check_talk_btn()
{
	if (!talkflag)
		return false;

	const Point mainPanelPosition = GetMainPanel().position;

	if (MousePosition.x < 172 + mainPanelPosition.x)
		return false;
	if (MousePosition.y < 69 + mainPanelPosition.y)
		return false;
	if (MousePosition.x > 233 + mainPanelPosition.x)
		return false;
	if (MousePosition.y > 123 + mainPanelPosition.y)
		return false;

	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}

	TalkButtonsDown[(MousePosition.y - (69 + mainPanelPosition.y)) / 18] = true;

	return true;
}

void control_release_talk_btn()
{
	if (!talkflag)
		return;

	for (bool &talkButtonDown : TalkButtonsDown)
		talkButtonDown = false;

	const Point mainPanelPosition = GetMainPanel().position;

	if (MousePosition.x < 172 + mainPanelPosition.x || MousePosition.y < 69 + mainPanelPosition.y || MousePosition.x > 233 + mainPanelPosition.x || MousePosition.y > 123 + mainPanelPosition.y)
		return;

	int off = (MousePosition.y - (69 + mainPanelPosition.y)) / 18;

	size_t playerId = 0;
	for (; playerId < Players.size() && off != -1; ++playerId) {
		if (playerId != MyPlayerId)
			off--;
	}
	if (playerId > 0 && playerId <= Players.size())
		WhisperList[playerId - 1] = !WhisperList[playerId - 1];
}

void control_type_message()
{
	if (!IsChatAvailable())
		return;

	talkflag = true;
	ChatInputState.emplace(TextInputState::Options {
	    /*value=*/TalkMessage,
	    /*cursor=*/&ChatCursor,
	    /*maxLength=*/sizeof(TalkMessage) - 1 });
	SDL_Rect rect = MakeSdlRect(GetMainPanel().position.x + 200, GetMainPanel().position.y + 22, 0, 27);
	SDL_SetTextInputRect(&rect);
	TalkMessage[0] = '\0';
	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}
	sgbPlrTalkTbl = GetMainPanel().size.height + 16;
	RedrawEverything();
	TalkSaveIndex = NextTalkSave;
	SDL_StartTextInput();
}

void control_reset_talk()
{
	talkflag = false;
	SDL_StopTextInput();
	ChatInputState = std::nullopt;
	sgbPlrTalkTbl = 0;
	RedrawEverything();
}

bool IsTalkActive()
{
	if (!IsChatAvailable())
		return false;

	if (!talkflag)
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

bool control_presskeys(SDL_Keycode vkey)
{
	if (!IsChatAvailable())
		return false;
	if (!talkflag)
		return false;

	switch (vkey) {
	case SDLK_ESCAPE:
		control_reset_talk();
		return true;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		ControlPressEnter();
		return true;
	case SDLK_BACKSPACE:
		TalkMessage[FindLastUtf8Symbols(TalkMessage)] = '\0';
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
	if (!IsChatAvailable()) {
		return;
	}

	assert(dwMsg < QUICK_MESSAGE_OPTIONS);

	for (auto &msg : sgOptions.Chat.szHotKeyMsgs[dwMsg]) {

#ifdef _DEBUG
		if (CheckDebugTextCommand(msg))
			continue;
#endif
		if (CheckTextCommand(msg))
			continue;
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
	    /*textOptions*/ {
	        /*value=*/GoldDropText,
	        /*cursor=*/&GoldDropCursor,
	        /*maxLength=*/sizeof(GoldDropText) - 1,
	    },
	    /*min=*/0,
	    /*max=*/max,
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

bool HandleGoldDropTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, GoldDropInputState);
}

} // namespace devilution
