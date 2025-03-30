/**
 * @file mainmenu.cpp
 *
 * Implementation of functions for interacting with the main menu.
 */

#include <cstdint>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/settingsmenu.h"
#include "engine/demomode.h"
#include "game_mode.hpp"
#include "init.h"
#include "movie.h"
#include "options.h"
#include "pfile.h"
#include "storm/storm_net.hpp"
#include "utils/language.h"

namespace devilution {

uint32_t gSaveNumber;

namespace {

_music_id NextTrack()
{
	if (gbIsSpawn) {
		return TMUSIC_INTRO;
	}

	switch (sgnMusicTrack) {
	case TMUSIC_INTRO:
		return TMUSIC_CATACOMBS;
	case TMUSIC_CATACOMBS:
		return TMUSIC_CAVES;
	case TMUSIC_CAVES:
		return TMUSIC_HELL;
	case TMUSIC_HELL:
		return gbIsHellfire ? TMUSIC_NEST : TMUSIC_INTRO;
	case TMUSIC_NEST:
		return gbIsHellfire ? TMUSIC_CRYPT : TMUSIC_INTRO;
	default:
		return TMUSIC_INTRO;
	}
}

void RefreshMusic()
{
	music_start(NextTrack());
}

bool InitMenu(_selhero_selections type)
{
	bool success;

	if (type == SELHERO_PREVIOUS)
		return true;

	success = StartGame(type != SELHERO_CONTINUE, type != SELHERO_CONNECT);
	if (success)
		RefreshMusic();

	return success;
}

bool InitSinglePlayerMenu()
{
	gbIsMultiplayer = false;
	return InitMenu(SELHERO_NEW_DUNGEON);
}

bool InitMultiPlayerMenu()
{
	gbIsMultiplayer = true;
	return InitMenu(SELHERO_CONNECT);
}

void PlayIntro()
{
	music_stop();
	if (gbIsHellfire)
		play_movie("gendata\\Hellfire.smk", true);
	else
		play_movie("gendata\\diablo1.smk", true);
	RefreshMusic();
}

bool DummyGetHeroInfo(_uiheroinfo * /*pInfo*/)
{
	return true;
}

} // namespace

bool mainmenu_select_hero_dialog(GameData *gameData)
{
	OptionEntryInt<uint32_t> *pSaveNumberFromOptions = nullptr;
	_selhero_selections dlgresult = SELHERO_NEW_DUNGEON;
	if (demo::IsRunning()) {
		pfile_ui_set_hero_infos(DummyGetHeroInfo);
		gbLoadGame = true;
	} else if (!gbIsMultiplayer) {
		pSaveNumberFromOptions = gbIsHellfire ? &GetOptions().Hellfire.lastSinglePlayerHero : &GetOptions().Diablo.lastSinglePlayerHero;
		gSaveNumber = **pSaveNumberFromOptions;
		UiSelHeroSingDialog(
		    pfile_ui_set_hero_infos,
		    pfile_ui_save_create,
		    pfile_delete_save,
		    pfile_ui_set_class_stats,
		    &dlgresult,
		    &gSaveNumber,
		    &gameData->nDifficulty);

		gbLoadGame = (dlgresult == SELHERO_CONTINUE);
	} else {
		pSaveNumberFromOptions = gbIsHellfire ? &GetOptions().Hellfire.lastMultiplayerHero : &GetOptions().Diablo.lastMultiplayerHero;
		gSaveNumber = **pSaveNumberFromOptions;
		UiSelHeroMultDialog(
		    pfile_ui_set_hero_infos,
		    pfile_ui_save_create,
		    pfile_delete_save,
		    pfile_ui_set_class_stats,
		    &dlgresult,
		    &gSaveNumber);
	}
	if (dlgresult == SELHERO_PREVIOUS) {
		return false;
	}

	if (pSaveNumberFromOptions != nullptr)
		pSaveNumberFromOptions->SetValue(gSaveNumber);

	return true;
}

void mainmenu_wait_for_button_sound()
{
	SDL_FillRect(DiabloUiSurface(), nullptr, 0x000000);
	UiFadeIn();
	SDL_Delay(350); // delay to let button pressed sound finish playing
}

void mainmenu_loop()
{
	bool done;

	RefreshMusic();
	done = false;

	do {
		_mainmenu_selections menu = MAINMENU_NONE;
		if (demo::IsRunning())
			menu = MAINMENU_SINGLE_PLAYER;
		else if (!UiMainMenuDialog(gszProductName, &menu, 30))
			app_fatal(_("Unable to display mainmenu"));

		switch (menu) {
		case MAINMENU_NONE:
			break;
		case MAINMENU_SINGLE_PLAYER:
			if (!InitSinglePlayerMenu())
				done = true;
			break;
		case MAINMENU_MULTIPLAYER:
			if (!InitMultiPlayerMenu())
				done = true;
			break;
		case MAINMENU_ATTRACT_MODE:
			if (gbIsSpawn && !HaveIntro())
				done = false;
			else if (gbActive)
				PlayIntro();
			break;
		case MAINMENU_SHOW_CREDITS:
			UiCreditsDialog();
			break;
		case MAINMENU_SHOW_SUPPORT:
			UiSupportDialog();
			break;
		case MAINMENU_EXIT_DIABLO:
			mainmenu_wait_for_button_sound();
			done = true;
			break;
		case MAINMENU_SETTINGS:
			UiSettingsMenu();
			break;
		}
	} while (!done);

	music_stop();
}

} // namespace devilution
