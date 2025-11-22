#include "DiabloUI/multi/selgame.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <fmt/core.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/hero/selhero.h"
#include "DiabloUI/scrollbar.h"
#include "DiabloUI/selok.h"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "config.h"
#include "diablo.h"
#include "engine/point.hpp"
#include "engine/render/text_render.hpp"
#include "levels/gendung.h"
#include "menu.h"
#include "multi.h"
#include "options.h"
#include "storm/storm_net.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

namespace devilution {

char selgame_Label[32];
char selgame_Ip[129] = "";
char selgame_Password[16] = "";
char selgame_Description[512];
std::string selgame_Title;
bool selgame_enteringGame;
size_t selgame_selectedGame;
bool selgame_endMenu;
int *gdwPlayerId;
_difficulty nDifficulty;
int nTickRate;
int heroLevel;

static GameData *m_game_data;
extern int provider;

#define DESCRIPTION_WIDTH 205

namespace {

const char *title = "";

std::vector<std::unique_ptr<UiListItem>> vecSelGameDlgItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelGameDialog;
std::vector<GameInfo> Gamelist;
uint32_t firstPublicGameInfoRequestSend = 0;
size_t HighlightedItem;

void selgame_FreeVectors()
{
	vecSelGameDlgItems.clear();

	vecSelGameDialog.clear();
}

void selgame_Init()
{
	LoadBackgroundArt("ui_art\\selgame");
	LoadScrollBar();
}

void selgame_Free()
{
	ArtBackground = std::nullopt;
	UnloadScrollBar();
	selgame_FreeVectors();
}

bool IsGameCompatible(const GameData &data)
{
	return (data.versionMajor == PROJECT_VERSION_MAJOR
	    && data.versionMinor == PROJECT_VERSION_MINOR
	    && data.versionPatch == PROJECT_VERSION_PATCH
	    && data.programid == GAME_ID);
	return false;
}

static std::string GetErrorMessageIncompatibility(const GameData &data)
{
	if (data.programid != GAME_ID) {
		std::string_view gameMode;
		switch (data.programid) {
		case GameIdDiabloFull:
			gameMode = _("Diablo");
			break;
		case GameIdDiabloSpawn:
			gameMode = _("Diablo Shareware");
			break;
		case GameIdHellfireFull:
			gameMode = _("Hellfire");
			break;
		case GameIdHellfireSpawn:
			gameMode = _("Hellfire Shareware");
			break;
		default:
			return std::string(_("The host is running a different game than you."));
		}
		return fmt::format(fmt::runtime(_("The host is running a different game mode ({:s}) than you.")), gameMode);
	} else {
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: Error message when somebody tries to join a game running another version. */ "Your version {:s} does not match the host {:d}.{:d}.{:d}.")), PROJECT_VERSION, data.versionMajor, data.versionMinor, data.versionPatch);
	}
}

void UiInitGameSelectionList(std::string_view search)
{
	selgame_enteringGame = false;
	selgame_selectedGame = 0;

	if (provider == SELCONN_LOOPBACK) {
		selgame_enteringGame = true;
		selgame_GameSelection_Select(0);
		return;
	}

	if (provider == SELCONN_ZT) {
		CopyUtf8(selgame_Ip, GetOptions().Network.szPreviousZTGame, sizeof(selgame_Ip));
	} else {
		CopyUtf8(selgame_Ip, GetOptions().Network.szPreviousHost, sizeof(selgame_Ip));
	}

	selgame_FreeVectors();

	selgame_Label[0] = '\0';

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	const SDL_Rect rectScrollbar = { (Sint16)(uiPosition.x + 590), (Sint16)(uiPosition.y + 244), 25, 178 };
	vecSelGameDialog.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0], *ArtScrollBarArrow, rectScrollbar));

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_(ConnectionNames[provider]).data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 211), 205, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Description:").data(), rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 300), (Sint16)(uiPosition.y + 211), 295, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Action").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

#ifdef PACKET_ENCRYPTION
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Create Game"), 0, UiFlags::ColorUiGold));
#endif
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Create Public Game"), 1, UiFlags::ColorUiGold));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Join Game"), 2, UiFlags::ColorUiGold));

	if (provider == SELCONN_ZT) {
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(std::string_view {}, -1, UiFlags::ElementDisabled));
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Public Games"), -1, UiFlags::ElementDisabled | UiFlags::ColorWhitegold));

		if (Gamelist.empty()) {
			// We expect the game list to be received after 3 seconds
			if (firstPublicGameInfoRequestSend == 0 || (SDL_GetTicks() - firstPublicGameInfoRequestSend) < 2000)
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Loading..."), -1, UiFlags::ElementDisabled | UiFlags::ColorUiSilver));
			else
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("None"), -1, UiFlags::ElementDisabled | UiFlags::ColorUiSilver));
		} else {
			for (unsigned i = 0; i < Gamelist.size(); i++) {
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(std::string_view(Gamelist[i].name), i + 3, UiFlags::ColorUiGold));
			}
		}
	}

	vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, 6, uiPosition.x + 305, (uiPosition.y + 255), 285, 26, UiFlags::AlignCenter | UiFlags::FontSize24));

	const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect5, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	const SDL_Rect rect6 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	auto selectFn = [](size_t index) {
		// UiListItem::m_value could be different from
		// the index if packet encryption is disabled
		const int itemValue = vecSelGameDlgItems[index]->m_value;
		selgame_GameSelection_Select(itemValue);
	};

	if (!search.empty()) {
		for (size_t i = 0; i < vecSelGameDlgItems.size(); i++) {
			const int gameIndex = vecSelGameDlgItems[i]->m_value - 3;
			if (gameIndex < 0)
				continue;
			if (search == Gamelist[gameIndex].name)
				HighlightedItem = i;
		}
	}

	if (HighlightedItem >= vecSelGameDlgItems.size()) {
		HighlightedItem = vecSelGameDlgItems.size() - 1;
	}

	UiInitList(selgame_GameSelection_Focus, selectFn, selgame_GameSelection_Esc, vecSelGameDialog, true, nullptr, nullptr, HighlightedItem);
}

} // namespace

void selgame_GameSelection_Init()
{
	UiInitGameSelectionList("");
}

void selgame_GameSelection_Focus(size_t value)
{
	HighlightedItem = value;
	const UiListItem &item = *vecSelGameDlgItems[value];
	switch (item.m_value) {
	case 0:
		CopyUtf8(selgame_Description, _("Create a new game with a difficulty setting of your choice."), sizeof(selgame_Description));
		break;
	case 1:
		CopyUtf8(selgame_Description, _("Create a new public game that anyone can join with a difficulty setting of your choice."), sizeof(selgame_Description));
		break;
	case 2:
		if (provider == SELCONN_ZT) {
			CopyUtf8(selgame_Description, _("Enter Game ID to join a game already in progress."), sizeof(selgame_Description));
		} else {
			CopyUtf8(selgame_Description, _("Enter an IP or a hostname to join a game already in progress."), sizeof(selgame_Description));
		}
		break;
	default:
		const GameInfo &gameInfo = Gamelist[item.m_value - 3];
		std::string infoString = std::string(_("Join the public game already in progress."));
		infoString.append("\n\n");
		if (IsGameCompatible(gameInfo.gameData)) {
			std::string_view difficulty;
			switch (gameInfo.gameData.nDifficulty) {
			case DIFF_NORMAL:
				difficulty = _("Normal");
				break;
			case DIFF_NIGHTMARE:
				difficulty = _("Nightmare");
				break;
			case DIFF_HELL:
				difficulty = _("Hell");
				break;
			}
			infoString.append(fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} means: Game Difficulty. */ "Difficulty: {:s}")), difficulty));
			infoString += '\n';
			switch (gameInfo.gameData.nTickRate) {
			case 20:
				infoString.append(_("Speed: Normal"));
				break;
			case 25:
				infoString.append(_("Speed: Fast"));
				break;
			case 30:
				infoString.append(_("Speed: Faster"));
				break;
			case 35:
				infoString.append(_("Speed: Fastest"));
				break;
			default:
				// This should not occur, so no translation is needed
				infoString.append(StrCat("Speed: ", gameInfo.gameData.nTickRate));
				break;
			}
			infoString += '\n';
			infoString.append(_("Players: "));
			for (const auto &playerName : gameInfo.players) {
				infoString.append(playerName);
				infoString += ' ';
			}
			infoString += '\n';
			if (gameInfo.peerIsRelayed.value_or(false))
				infoString.append(fmt::format(fmt::runtime(_("Ping: {:d} ms (RELAYED)")), gameInfo.latency.value_or(0)));
			else
				infoString.append(fmt::format(fmt::runtime(_("Ping: {:d} ms")), gameInfo.latency.value_or(0)));
		} else {
			infoString.append(GetErrorMessageIncompatibility(gameInfo.gameData));
		}
		CopyUtf8(selgame_Description, infoString, sizeof(selgame_Description));
		break;
	}
	CopyUtf8(selgame_Description, WordWrapString(selgame_Description, DESCRIPTION_WIDTH), sizeof(selgame_Description));
}

/**
 * @brief Load the current hero level from save file
 * @param pInfo Hero info
 * @return always true
 */
bool UpdateHeroLevel(_uiheroinfo *pInfo)
{
	if (pInfo->saveNumber == gSaveNumber)
		heroLevel = pInfo->level;

	return true;
}

void selgame_GameSelection_Select(size_t value)
{
	selgame_enteringGame = true;
	selgame_selectedGame = value;

	gfnHeroInfo(UpdateHeroLevel);

	selgame_FreeVectors();

	if (value > 2) {
		CopyUtf8(selgame_Ip, Gamelist[value - 3].name, sizeof(selgame_Ip));
		selgame_Password_Select(value);
		return;
	}

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(&title, rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 34), (Sint16)(uiPosition.y + 211), 205, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Label, rect2, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	switch (value) {
	case 0:
	case 1: {
		title = _("Create Game").data();

		const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 211), 295, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Difficulty").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Normal"), DIFF_NORMAL));
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Nightmare"), DIFF_NIGHTMARE));
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Hell"), DIFF_HELL));

		vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, vecSelGameDlgItems.size(), uiPosition.x + 300, (uiPosition.y + 282), 295, 26, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

		const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect5, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		const SDL_Rect rect6 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		UiInitList(selgame_Diff_Focus, selgame_Diff_Select, selgame_Diff_Esc, vecSelGameDialog, true);
		break;
	}
	case 2: {
		selgame_Title = fmt::format(fmt::runtime(_("Join {:s} Games")), _(ConnectionNames[provider]));
		title = selgame_Title.c_str();

		const char *inputHint;
		if (provider == SELCONN_ZT) {
			inputHint = _("Enter Game ID").data();
		} else {
			inputHint = _("Enter address").data();
		}

		const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 211), 285, 33 };
		vecSelGameDialog.push_back(std::make_unique<UiArtText>(inputHint, rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 314), 285, 33 };
		vecSelGameDialog.push_back(std::make_unique<UiEdit>(inputHint, selgame_Ip, 128, false, rect5, UiFlags::FontSize24 | UiFlags::ColorUiGold));

		const SDL_Rect rect6 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		const SDL_Rect rect7 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect7, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		HighlightedItem = 0;

#ifdef PACKET_ENCRYPTION
		UiInitList(nullptr, selgame_Password_Init, selgame_GameSelection_Init, vecSelGameDialog);
#else
		UiInitList(nullptr, selgame_Password_Select, selgame_GameSelection_Init, vecSelGameDialog);
#endif
		break;
	}
	}
}

void selgame_GameSelection_Esc()
{
	UiInitList_clear();
	selgame_enteringGame = false;
	selgame_endMenu = true;
}

void selgame_Diff_Focus(size_t value)
{
	switch (vecSelGameDlgItems[value]->m_value) {
	case DIFF_NORMAL:
		CopyUtf8(selgame_Label, _("Normal"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Normal Difficulty\nThis is where a starting character should begin the quest to defeat Diablo."), sizeof(selgame_Description));
		break;
	case DIFF_NIGHTMARE:
		CopyUtf8(selgame_Label, _("Nightmare"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Nightmare Difficulty\nThe denizens of the Labyrinth have been bolstered and will prove to be a greater challenge. This is recommended for experienced characters only."), sizeof(selgame_Description));
		break;
	case DIFF_HELL:
		CopyUtf8(selgame_Label, _("Hell"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Hell Difficulty\nThe most powerful of the underworld's creatures lurk at the gateway into Hell. Only the most experienced characters should venture in this realm."), sizeof(selgame_Description));
		break;
	}
	CopyUtf8(selgame_Description, WordWrapString(selgame_Description, DESCRIPTION_WIDTH), sizeof(selgame_Description));
}

bool IsDifficultyAllowed(int value)
{
	if (value == 0 || (value == 1 && heroLevel >= 20) || (value == 2 && heroLevel >= 30)) {
		return true;
	}

	selgame_Free();

	if (value == 1)
		UiSelOkDialog(title, _("Your character must reach level 20 before you can enter a multiplayer game of Nightmare difficulty.").data(), false);
	if (value == 2)
		UiSelOkDialog(title, _("Your character must reach level 30 before you can enter a multiplayer game of Hell difficulty.").data(), false);

	selgame_Init();

	return false;
}

void selgame_Diff_Select(size_t value)
{
	if (selhero_isMultiPlayer && !IsDifficultyAllowed(vecSelGameDlgItems[value]->m_value)) {
		selgame_GameSelection_Select(selgame_selectedGame);
		return;
	}

	nDifficulty = (_difficulty)vecSelGameDlgItems[value]->m_value;

	if (!selhero_isMultiPlayer) {
		// This is part of a dangerous hack to enable difficulty selection in single-player.
		// FIXME: Dialogs should not refer to each other's variables.

		// We're in the selhero loop instead of the selgame one.
		// Free the selgame data and flag the end of the selhero loop.
		selhero_endMenu = true;

		// We only call FreeVectors because ArtBackground.Unload()
		// will be called by selheroFree().
		selgame_FreeVectors();

		// We must clear the InitList because selhero's loop will perform
		// one more iteration after this function exits.
		UiInitList_clear();

		return;
	}

	selgame_GameSpeedSelection();
}

void selgame_Diff_Esc()
{
	if (!selhero_isMultiPlayer) {
		selgame_Free();

		selhero_Init();
		selhero_List_Init();
		return;
	}

	if (provider == SELCONN_LOOPBACK) {
		selgame_GameSelection_Esc();
		return;
	}

	HighlightedItem = 0;
	selgame_GameSelection_Init();
}

void selgame_GameSpeedSelection()
{
	gfnHeroInfo(UpdateHeroLevel);

	selgame_FreeVectors();

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Create Game").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 34), (Sint16)(uiPosition.y + 211), 205, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Label, rect2, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 211), 295, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Game Speed").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Normal"), 20));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Fast"), 30));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Faster"), 40));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Fastest"), 50));

	vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, vecSelGameDlgItems.size(), uiPosition.x + 300, (uiPosition.y + 279), 295, 26, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

	const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect5, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	const SDL_Rect rect6 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(selgame_Speed_Focus, selgame_Speed_Select, selgame_Speed_Esc, vecSelGameDialog, true);
}

void selgame_Speed_Focus(size_t value)
{
	switch (vecSelGameDlgItems[value]->m_value) {
	case 20:
		CopyUtf8(selgame_Label, _("Normal"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Normal Speed\nThis is where a starting character should begin the quest to defeat Diablo."), sizeof(selgame_Description));
		break;
	case 30:
		CopyUtf8(selgame_Label, _("Fast"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Fast Speed\nThe denizens of the Labyrinth have been hastened and will prove to be a greater challenge. This is recommended for experienced characters only."), sizeof(selgame_Description));
		break;
	case 40:
		CopyUtf8(selgame_Label, _("Faster"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Faster Speed\nMost monsters of the dungeon will seek you out quicker than ever before. Only an experienced champion should try their luck at this speed."), sizeof(selgame_Description));
		break;
	case 50:
		CopyUtf8(selgame_Label, _("Fastest"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Fastest Speed\nThe minions of the underworld will rush to attack without hesitation. Only a true speed demon should enter at this pace."), sizeof(selgame_Description));
		break;
	}
	CopyUtf8(selgame_Description, WordWrapString(selgame_Description, DESCRIPTION_WIDTH), sizeof(selgame_Description));
}

void selgame_Speed_Esc()
{
	selgame_GameSelection_Select(selgame_selectedGame);
}

void selgame_Speed_Select(size_t value)
{
	nTickRate = vecSelGameDlgItems[value]->m_value;

	if (provider == SELCONN_LOOPBACK || selgame_selectedGame == 1) {
		selgame_Password_Select(0);
		return;
	}

	selgame_Password_Init(0);
}

void selgame_Password_Init(size_t /*value*/)
{
	memset(&selgame_Password, 0, sizeof(selgame_Password));

	selgame_FreeVectors();

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_(ConnectionNames[provider]).data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 211), 205, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Description:").data(), rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 211), 285, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Enter Password").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	// Allow password to be empty only when joining games
	const bool allowEmpty = selgame_selectedGame == 2;
	const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 314), 285, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiEdit>(_("Enter Password"), selgame_Password, 15, allowEmpty, rect5, UiFlags::FontSize24 | UiFlags::ColorUiGold));

	const SDL_Rect rect6 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	const SDL_Rect rect7 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect7, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(nullptr, selgame_Password_Select, selgame_Password_Esc, vecSelGameDialog);
}

static bool IsGameCompatibleWithErrorMessage(const GameData &data)
{
	if (IsGameCompatible(data))
		return IsDifficultyAllowed(data.nDifficulty);

	selgame_Free();

	const std::string errorMessage = GetErrorMessageIncompatibility(data);
	UiSelOkDialog(title, errorMessage.c_str(), false);

	selgame_Init();

	return false;
}

void selgame_Password_Select(size_t /*value*/)
{
	char *gamePassword = nullptr;
	if (selgame_selectedGame == 0)
		gamePassword = selgame_Password;
	if (selgame_selectedGame == 2 && strlen(selgame_Password) > 0)
		gamePassword = selgame_Password;

	// If there is an error, the error message won't necessarily be set.
	// Clear the error so that we display "Unknown network error"
	// instead of an arbitrary message in that case.
	SDL_ClearError();

	if (selgame_selectedGame > 1) {
		bool allowJoin = true;
		if (selgame_selectedGame > 2)
			allowJoin = IsGameCompatible(Gamelist[selgame_selectedGame - 3].gameData);
		if (provider == SELCONN_ZT) {
			for (unsigned int i = 0; i < (sizeof(selgame_Ip) / sizeof(selgame_Ip[0])); i++) {
				selgame_Ip[i] = (selgame_Ip[i] >= 'A' && selgame_Ip[i] <= 'Z') ? selgame_Ip[i] + 'a' - 'A' : selgame_Ip[i];
			}
			strcpy(GetOptions().Network.szPreviousZTGame, selgame_Ip);
		} else {
			strcpy(GetOptions().Network.szPreviousHost, selgame_Ip);
		}
		if (allowJoin && SNetJoinGame(selgame_Ip, gamePassword, gdwPlayerId)) {
			if (!IsGameCompatibleWithErrorMessage(*m_game_data)) {
				InitGameInfo();
				selgame_GameSelection_Select(1);
				return;
			}

			UiInitList_clear();
			selgame_endMenu = true;
		} else {
			InitGameInfo();
			selgame_Free();
			std::string error;
			if (!allowJoin)
				error = GetErrorMessageIncompatibility(Gamelist[selgame_selectedGame - 3].gameData);
			else
				error = SDL_GetError();
			if (error.empty())
				error = "Unknown network error";
			UiSelOkDialog(_("Multi Player Game").data(), error.c_str(), false);
			selgame_Init();
			if (selgame_selectedGame == 2)
				selgame_Password_Init(selgame_selectedGame);
			else
				UiInitGameSelectionList("");
		}
		return;
	}

	m_game_data->nDifficulty = nDifficulty;
	m_game_data->nTickRate = nTickRate;
	m_game_data->bRunInTown = *GetOptions().Gameplay.runInTown ? 1 : 0;
	m_game_data->bTheoQuest = *GetOptions().Gameplay.theoQuest ? 1 : 0;
	m_game_data->bCowQuest = *GetOptions().Gameplay.cowQuest ? 1 : 0;

	GameData gameInitInfo = *m_game_data;
	gameInitInfo.swapLE();
	if (SNetCreateGame(nullptr, gamePassword, reinterpret_cast<char *>(&gameInitInfo), sizeof(gameInitInfo), gdwPlayerId)) {
		UiInitList_clear();
		selgame_endMenu = true;
	} else {
		selgame_Free();
		std::string error = SDL_GetError();
		if (error.empty())
			error = "Unknown network error";
		UiSelOkDialog(_("Multi Player Game").data(), error.c_str(), false);
		selgame_Init();
		selgame_Password_Init(0);
	}
}

void selgame_Password_Esc()
{
	if (selgame_selectedGame == 2)
		selgame_GameSelection_Select(2);
	else
		selgame_GameSpeedSelection();
}

void RefreshGameList()
{
	static uint32_t lastRequest = 0;
	static uint32_t lastUpdate = 0;

	if (selgame_enteringGame)
		return;

	const uint32_t currentTime = SDL_GetTicks();

	if ((lastRequest == 0 || currentTime - lastRequest > 30000) && DvlNet_SendInfoRequest()) {
		lastRequest = currentTime;
		lastUpdate = currentTime - 3000; // Give 2 sec for responses, but don't wait 5
		if (firstPublicGameInfoRequestSend == 0)
			firstPublicGameInfoRequestSend = currentTime;
	}

	if (lastUpdate == 0 || currentTime - lastUpdate > 5000) {
		const int gameIndex = vecSelGameDlgItems[HighlightedItem]->m_value - 3;
		const std::string gameSearch = gameIndex >= 0 ? Gamelist[gameIndex].name : "";
		std::vector<GameInfo> gamelist = DvlNet_GetGamelist();
		Gamelist.clear();
		for (unsigned i = 0; i < gamelist.size(); i++) {
			Gamelist.push_back(gamelist[i]);
		}
		UiInitGameSelectionList(gameSearch);
		lastUpdate = currentTime;
	}
}

bool UiSelectGame(GameData *gameData, int *playerId)
{
	firstPublicGameInfoRequestSend = 0;
	gdwPlayerId = playerId;
	m_game_data = gameData;
	selgame_Init();
	HighlightedItem = 0;
	selgame_GameSelection_Init();

	selgame_endMenu = false;

	DvlNet_ClearPassword();
	DvlNet_ClearGamelist();

	while (!selgame_endMenu) {
		UiClearScreen();
		UiPollAndRender();
		if (provider == SELCONN_ZT)
			RefreshGameList();
	}
	selgame_Free();

	return selgame_enteringGame;
}
} // namespace devilution
