#include "control/control.hpp"
#include "control/control_chat.hpp"
#include "control/control_flasks.hpp"

#include "automap.h"
#include "controls/control_mode.hpp"
#include "controls/modifier_hints.h"
#include "diablo_msg.hpp"
#include "engine/backbuffer_state.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/trn.hpp"
#include "gamemenu.h"
#include "headless_mode.hpp"
#include "minitext.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "panels/mainpanel.hpp"
#include "panels/partypanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "pfile.h"
#include "qol/stash.h"
#include "stores.h"
#include "utils/sdl_compat.h"

namespace devilution {

bool CharPanelButton[4];
bool LevelButtonDown;
bool CharPanelButtonActive;
UiFlags InfoColor;
int SpellbookTab;
bool ChatFlag;
bool SpellbookFlag;
bool CharFlag;
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

constexpr int BeltItems = 8;
constexpr Size BeltSize { (INV_SLOT_SIZE_PX + 1) * BeltItems, INV_SLOT_SIZE_PX };
Rectangle BeltRect { { 205, 5 }, BeltSize };

Rectangle SpellButtonRect { { 565, 64 }, { 56, 56 } };

int PanelPaddingHeight = 16;

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

/** Maps from panel_button_id to hotkey name. */
const char *const PanBtnHotKey[8] = { "'c'", "'q'", N_("Tab"), N_("Esc"), "'i'", "'b'", N_("Enter"), nullptr };

int TotalSpMainPanelButtons = 6;
int TotalMpMainPanelButtons = 8;

namespace {

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

int CapStatPointsToAdd(int remainingStatPoints, const Player &player, CharacterAttribute attribute)
{
	const int pointsToReachCap = player.GetMaximumAttributeValue(attribute) - player.GetBaseAttributeValue(attribute);

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
	const int height = (*pDurIcons)[c].height(); // Height of durability icon CEL
	int partition = 0;
	if (pItem._iDurability > durabilityThresholdRed) {
		const int current = pItem._iDurability - durabilityThresholdRed;
		partition = (height * current) / (durabilityThresholdGold - durabilityThresholdRed);
	}

	// Draw icon
	const int y = -17 + GetMainPanel().position.y;
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

void FocusOnCharInfo()
{
	const Player &myPlayer = *MyPlayer;

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

		if (InspectingFromPartyPanel)
			InspectingFromPartyPanel = false;
		else
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

Point GetPanelPosition(UiPanels panel, Point offset)
{
	const Displacement displacement { offset.x, offset.y };

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

tl::expected<void, std::string> InitMainPanel()
{
	if (!HeadlessMode) {
		BottomBuffer.emplace(GetMainPanel().size.width, (GetMainPanel().size.height + PanelPaddingHeight) * (IsChatAvailable() ? 2 : 1));
		pManaBuff.emplace(88, 88);
		pLifeBuff.emplace(88, 88);

		RETURN_IF_ERROR(LoadPartyPanel());
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
	FloatingInfoString = StringOrView {};
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
			const Point position = mainPanelPosition + Displacement { MainPanelButtonRect[i].position.x, MainPanelButtonRect[i].position.y };
			RenderClxSprite(out, (*pMainPanelButtons)[i], position);
			RenderClxSprite(out, (*PanelButtonDown)[i], position + Displacement { 4, 0 });
		}
	}

	if (IsChatAvailable()) {
		RenderClxSprite(out, (*multiButtons)[MainPanelButtons[PanelButtonSendmsg] ? 1 : 0], mainPanelPosition + Displacement { MainPanelButtonRect[PanelButtonSendmsg].position.x, MainPanelButtonRect[PanelButtonSendmsg].position.y });

		const Point friendlyButtonPosition = mainPanelPosition + Displacement { MainPanelButtonRect[PanelButtonFriendly].position.x, MainPanelButtonRect[PanelButtonFriendly].position.y };

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
	const int totalButtons = IsChatAvailable() ? TotalMpMainPanelButtons : TotalSpMainPanelButtons;

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
		if ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0) {
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
			} else if (MyPlayer->hasNoLife()) {
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
	FreePartyPanel();
	FreeCharPanel();
	FreeModifierHints();
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
		const int nCel = LevelButtonDown ? 2 : 1;
		DrawString(out, _("Level Up"), { GetMainPanel().position + Displacement { 0, LevelButtonRect.position.y - 23 }, { 120, 0 } },
		    { .flags = UiFlags::ColorWhite | UiFlags::AlignCenter | UiFlags::KerningFitSpacing });
		RenderClxSprite(out, (*pChrButtons)[nCel], GetMainPanel().position + Displacement { LevelButtonRect.position.x, LevelButtonRect.position.y });
	}
}

void CheckChrBtns()
{
	const Player &myPlayer = *MyPlayer;

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
	const bool hasRoomBetweenPanels = RightPanel.position.x - (LeftPanel.position.x + LeftPanel.size.width) >= 16 + (32 + 8 + 32 + 8 + 32 + 8 + 32) + 16;
	const bool hasRoomUnderPanels = MainPanel.position.y - (RightPanel.position.y + RightPanel.size.height) >= 16 + 32 + 16;

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
	const int verticalPadding = 42;
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

void SetPanelObjectPosition(UiPanels panel, Rectangle &button)
{
	button.position = GetPanelPosition(panel, button.position);
}

} // namespace devilution
