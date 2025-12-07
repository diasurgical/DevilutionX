#include "DiabloUI/hero/selhero.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#else
#include <SDL.h>
#endif

#include <fmt/core.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/dialogs.h"
#include "DiabloUI/multi/selgame.h"
#include "DiabloUI/scrollbar.h"
#include "DiabloUI/selok.h"
#include "DiabloUI/selyesno.h"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "controls/control_mode.hpp"
#include "engine/assets.hpp"
#include "engine/point.hpp"
#include "game_mode.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "pfile.h"
#include "playerdat.hpp"
#include "utils/enum_traits.h"
#include "utils/language.h"
#include "utils/sdl_geometry.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

namespace devilution {

bool selhero_endMenu;
bool selhero_isMultiPlayer;

bool (*gfnHeroInfo)(bool (*fninfofunc)(_uiheroinfo *));
bool (*gfnHeroCreate)(_uiheroinfo *);
void (*gfnHeroStats)(HeroClass, _uidefaultstats *);

namespace {

std::size_t selhero_SaveCount = 0;
_uiheroinfo selhero_heros[MAX_CHARACTERS];
_uiheroinfo selhero_heroInfo;
char textStats[6][4];
const char *title = "";
_selhero_selections selhero_result;
bool selhero_navigateYesNo;
bool selhero_isSavegame;

std::vector<std::unique_ptr<UiItemBase>> vecSelHeroDialog;
std::vector<std::unique_ptr<UiListItem>> vecSelHeroDlgItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelDlgItems;

UiImageClx *SELHERO_DIALOG_HERO_IMG;

void SelheroListFocus(size_t value);
void SelheroListSelect(size_t value);
void SelheroListEsc();
void SelheroLoadFocus(size_t value);
void SelheroLoadSelect(size_t value);
void SelheroNameSelect(size_t value);
void SelheroNameEsc();
void SelheroClassSelectorFocus(size_t value);
void SelheroClassSelectorSelect(size_t value);
void SelheroClassSelectorEsc();
const char *SelheroGenerateName(HeroClass heroClass);

void SelheroUiFocusNavigationYesNo()
{
	if (selhero_isSavegame)
		UiFocusNavigationYesNo();
}

void SelheroFree()
{
	ArtBackground = std::nullopt;

	vecSelHeroDialog.clear();

	vecSelDlgItems.clear();
	vecSelHeroDlgItems.clear();
	UnloadScrollBar();
}

void SelheroSetStats()
{
	SELHERO_DIALOG_HERO_IMG->setSprite(UiGetHeroDialogSprite(static_cast<size_t>(selhero_heroInfo.heroclass)));
	CopyUtf8(textStats[0], StrCat(selhero_heroInfo.level), sizeof(textStats[0]));
	CopyUtf8(textStats[1], StrCat(selhero_heroInfo.strength), sizeof(textStats[1]));
	CopyUtf8(textStats[2], StrCat(selhero_heroInfo.magic), sizeof(textStats[2]));
	CopyUtf8(textStats[3], StrCat(selhero_heroInfo.dexterity), sizeof(textStats[3]));
	CopyUtf8(textStats[4], StrCat(selhero_heroInfo.vitality), sizeof(textStats[4]));
	CopyUtf8(textStats[5], StrCat(selhero_heroInfo.saveNumber), sizeof(textStats[5]));
}

void RenderDifficultyIndicators()
{
	if (!selhero_isSavegame)
		return;
	const uint16_t width = (*DifficultyIndicator)[0].width();
	const uint16_t height = (*DifficultyIndicator)[0].height();
	SDL_Rect rect = MakeSdlRect(
	    SELHERO_DIALOG_HERO_IMG->m_rect.x + 1,
	    SELHERO_DIALOG_HERO_IMG->m_rect.y + SELHERO_DIALOG_HERO_IMG->m_rect.h - height - 1,
	    width,
	    height);
	for (int i = 0; i <= DIFF_LAST; i++) {
		if (i >= selhero_heroInfo.herorank)
			break;
		UiRenderItem(UiImageClx((*DifficultyIndicator)[0], rect, UiFlags::None));
		rect.x += width;
	}
}

UiArtTextButton *SELLIST_DIALOG_DELETE_BUTTON;

bool SelHeroGetHeroInfo(_uiheroinfo *pInfo)
{
	selhero_heros[selhero_SaveCount] = *pInfo;

	selhero_SaveCount++;

	return true;
}

void SelheroListFocus(size_t value)
{
	const UiFlags baseFlags = UiFlags::AlignCenter | UiFlags::FontSize30;
	if (selhero_SaveCount != 0 && value < selhero_SaveCount) {
		memcpy(&selhero_heroInfo, &selhero_heros[value], sizeof(selhero_heroInfo));
		SelheroSetStats();
		SELLIST_DIALOG_DELETE_BUTTON->SetFlags(baseFlags | UiFlags::ColorUiGold);
		selhero_isSavegame = true;
		return;
	}

	SELHERO_DIALOG_HERO_IMG->setSprite(UiGetHeroDialogSprite(GetNumPlayerClasses()));
	for (char *textStat : textStats)
		strcpy(textStat, "--");
	SELLIST_DIALOG_DELETE_BUTTON->SetFlags(baseFlags | UiFlags::ColorUiSilver | UiFlags::ElementDisabled);
	selhero_isSavegame = false;
}

bool SelheroListDeleteYesNo()
{
	selhero_navigateYesNo = selhero_isSavegame;

	return selhero_navigateYesNo;
}

void SelheroListSelect(size_t value)
{
	const Point uiPosition = GetUIRectangle().position;

	if (static_cast<std::size_t>(value) == selhero_SaveCount) {
		vecSelDlgItems.clear();

		const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 242), (Sint16)(uiPosition.y + 211), 365, 33 };
		vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Choose Class").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		vecSelHeroDlgItems.clear();
		int itemH = 33;
		for (size_t i = 0; i < GetNumPlayerClasses(); ++i) {
			const HeroClass heroClass = static_cast<HeroClass>(i);

			if (heroClass == HeroClass::Monk && !gbIsHellfire) {
				continue;
			}

			if (heroClass == HeroClass::Bard && !HaveBardAssets() && !(*GetOptions().Gameplay.testBard)) {
				continue;
			}

			if (heroClass == HeroClass::Barbarian && !HaveBarbarianAssets() && !(*GetOptions().Gameplay.testBarbarian)) {
				continue;
			}

			const PlayerData &playerData = GetPlayerDataForClass(heroClass);
			vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_(playerData.className), static_cast<int>(heroClass)));
		}
		if (vecSelHeroDlgItems.size() > 4)
			itemH = 26;
		const int itemY = static_cast<int>(246 + (176 - std::min<size_t>(vecSelHeroDlgItems.size(), 6) * itemH) / 2);
		vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, std::min<size_t>(vecSelHeroDlgItems.size(), 6), uiPosition.x + 264, (uiPosition.y + itemY), 320, itemH, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

		const SDL_Rect rectScrollBar = { (Sint16)(uiPosition.x + 585), (Sint16)(uiPosition.y + 244), 25, 178 };
		vecSelDlgItems.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0], *ArtScrollBarArrow, rectScrollBar));

		const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 279), (Sint16)(uiPosition.y + 429), 140, 35 };
		vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect2, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 429), (Sint16)(uiPosition.y + 429), 144, 35 };
		vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, rect3, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		UiInitList(SelheroClassSelectorFocus, SelheroClassSelectorSelect, SelheroClassSelectorEsc, vecSelDlgItems, true);
		memset(&selhero_heroInfo.name, 0, sizeof(selhero_heroInfo.name));
		selhero_heroInfo.saveNumber = pfile_ui_get_first_unused_save_num();
		SelheroSetStats();
		title = selhero_isMultiPlayer ? _("New Multi Player Hero").data() : _("New Single Player Hero").data();
		selhero_isSavegame = false;
		return;
	}

	if (selhero_heroInfo.hassaved) {
		vecSelDlgItems.clear();

		const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 242), (Sint16)(uiPosition.y + 211), 365, 33 };
		vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Save File Exists").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		vecSelHeroDlgItems.clear();
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Load Game"), 0));
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("New Game"), 1));
		vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, vecSelHeroDlgItems.size(), uiPosition.x + 265, (uiPosition.y + 285), 320, 33, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

		const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 279), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect2, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 429), (Sint16)(uiPosition.y + 427), 144, 35 };
		vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, rect3, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		UiInitList(SelheroLoadFocus, SelheroLoadSelect, selhero_List_Init, vecSelDlgItems, true);
		title = _("Single Player Characters").data();
		return;
	}

	SelheroLoadSelect(1);
}

void SelheroListEsc()
{
	UiInitList_clear();

	selhero_endMenu = true;
	selhero_result = SELHERO_PREVIOUS;
}

void SelheroClassSelectorFocus(size_t value)
{
	const auto heroClass = static_cast<HeroClass>(vecSelHeroDlgItems[value]->m_value);

	_uidefaultstats defaults;
	gfnHeroStats(heroClass, &defaults);

	selhero_heroInfo.level = 1;
	selhero_heroInfo.heroclass = heroClass;
	selhero_heroInfo.strength = defaults.strength;
	selhero_heroInfo.magic = defaults.magic;
	selhero_heroInfo.dexterity = defaults.dexterity;
	selhero_heroInfo.vitality = defaults.vitality;

	SelheroSetStats();
}

bool ShouldPrefillHeroName()
{
#if defined(PREFILL_PLAYER_NAME)
	return true;
#else
	return ControlMode != ControlTypes::KeyboardAndMouse;
#endif
}

void RemoveSelHeroBackground()
{
	vecSelHeroDialog.erase(vecSelHeroDialog.begin());
	ArtBackground = std::nullopt;
}

void AddSelHeroBackground()
{
	LoadBackgroundArt("ui_art\\selhero");
	vecSelHeroDialog.insert(vecSelHeroDialog.begin(),
	    std::make_unique<UiImageClx>((*ArtBackground)[0], MakeSdlRect(0, GetUIRectangle().position.y, 0, 0), UiFlags::AlignCenter));
}

void SelheroClassSelectorSelect(size_t value)
{
	auto hClass = static_cast<HeroClass>(vecSelHeroDlgItems[value]->m_value);
	if (gbIsSpawn && (hClass == HeroClass::Rogue || hClass == HeroClass::Sorcerer || (hClass == HeroClass::Bard && !HaveBardAssets()))) {
		RemoveSelHeroBackground();
		UiSelOkDialog(nullptr, _("The Rogue and Sorcerer are only available in the full retail version of Diablo. Visit https://www.gog.com/game/diablo to purchase.").data(), false);
		AddSelHeroBackground();
		SelheroListSelect(selhero_SaveCount);
		return;
	}

	const Point uiPosition = GetUIRectangle().position;

	title = selhero_isMultiPlayer ? _("New Multi Player Hero").data() : _("New Single Player Hero").data();
	memset(selhero_heroInfo.name, '\0', sizeof(selhero_heroInfo.name));
	if (ShouldPrefillHeroName())
		strcpy(selhero_heroInfo.name, SelheroGenerateName(selhero_heroInfo.heroclass));
	vecSelDlgItems.clear();
	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 242), (Sint16)(uiPosition.y + 211), 365, 33 };
	vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Enter Name").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 265), (Sint16)(uiPosition.y + 317), 320, 33 };
	vecSelDlgItems.push_back(std::make_unique<UiEdit>(_("Enter Name"), selhero_heroInfo.name, 15, false, rect2, UiFlags::FontSize24 | UiFlags::ColorUiGold));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 279), (Sint16)(uiPosition.y + 429), 140, 35 };
	vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect3, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 429), (Sint16)(uiPosition.y + 429), 144, 35 };
	vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(nullptr, SelheroNameSelect, SelheroNameEsc, vecSelDlgItems);
}

void SelheroClassSelectorEsc()
{
	vecSelDlgItems.clear();
	vecSelHeroDlgItems.clear();

	if (selhero_SaveCount != 0) {
		selhero_List_Init();
		return;
	}

	SelheroListEsc();
}

void SelheroNameSelect(size_t /*value*/)
{
	// only check names in multiplayer, we don't care about them in single
	if (selhero_isMultiPlayer && !UiValidPlayerName(selhero_heroInfo.name)) {
		RemoveSelHeroBackground();
		UiSelOkDialog(title, _("Invalid name. A name cannot contain spaces, reserved characters, or reserved words.\n").data(), false);
		AddSelHeroBackground();
	} else {
		if (gfnHeroCreate(&selhero_heroInfo)) {
			SelheroLoadSelect(1);
			return;
		}
		UiErrorOkDialog(_(/* TRANSLATORS: Error Message */ "Unable to create character."), vecSelHeroDialog);
	}

	memset(selhero_heroInfo.name, '\0', sizeof(selhero_heroInfo.name));
	SelheroClassSelectorSelect(0);
}

void SelheroNameEsc()
{
	SelheroListSelect(selhero_SaveCount);
}

void SelheroLoadFocus(size_t value)
{
}

void SelheroLoadSelect(size_t value)
{
	UiInitList_clear();
	selhero_endMenu = true;
	if (vecSelHeroDlgItems[value]->m_value == 0) {
		selhero_result = SELHERO_CONTINUE;
		return;
	}

	if (!selhero_isMultiPlayer) {
		// This is part of a dangerous hack to enable difficulty selection in single-player.
		// FIXME: Dialogs should not refer to each other's variables.

		// We disable `selhero_endMenu` and replace the background and art
		// and the item list with the difficulty selection ones.
		//
		// This means selhero's render loop will render selgame's items,
		// which happens to work because the render loops are similar.
		selhero_endMenu = false;

		// Set this to false so that we do not attempt to render difficulty indicators.
		selhero_isSavegame = false;

		SelheroFree();
		LoadBackgroundArt("ui_art\\selgame");
		selgame_GameSelection_Select(0);
	}

	selhero_result = SELHERO_NEW_DUNGEON;
}

const char *SelheroGenerateName(HeroClass heroClass)
{
	static const char *const Names[6][10] = {
		{
		    // Warrior
		    "Aidan",
		    "Qarak",
		    "Born",
		    "Cathan",
		    "Halbu",
		    "Lenalas",
		    "Maximus",
		    "Vane",
		    "Myrdgar",
		    "Rothat",
		},
		{
		    // Rogue
		    "Moreina",
		    "Akara",
		    "Kashya",
		    "Flavie",
		    "Divo",
		    "Oriana",
		    "Iantha",
		    "Shikha",
		    "Basanti",
		    "Elexa",
		},
		{
		    // Sorcerer
		    "Jazreth",
		    "Drognan",
		    "Armin",
		    "Fauztin",
		    "Jere",
		    "Kazzulk",
		    "Ranslor",
		    "Sarnakyle",
		    "Valthek",
		    "Horazon",
		},
		{
		    // Monk
		    "Akyev",
		    "Dvorak",
		    "Kekegi",
		    "Kharazim",
		    "Mikulov",
		    "Shenlong",
		    "Vedenin",
		    "Vhalit",
		    "Vylnas",
		    "Zhota",
		},
		{
		    // Bard (uses Rogue names)
		    "Moreina",
		    "Akara",
		    "Kashya",
		    "Flavie",
		    "Divo",
		    "Oriana",
		    "Iantha",
		    "Shikha",
		    "Basanti",
		    "Elexa",
		},
		{
		    // Barbarian
		    "Alaric",
		    "Barloc",
		    "Egtheow",
		    "Guthlaf",
		    "Heorogar",
		    "Hrothgar",
		    "Oslaf",
		    "Qual-Kehk",
		    "Ragnar",
		    "Ulf",
		},
	};

	const int iRand = rand() % 10;

	return Names[static_cast<std::size_t>(heroClass) % 6][iRand];
}

} // namespace

void selhero_Init()
{
	AddSelHeroBackground();
	UiAddLogo(&vecSelHeroDialog);
	LoadScrollBar();

	selhero_SaveCount = 0;
	gfnHeroInfo(SelHeroGetHeroInfo);
	std::reverse(selhero_heros, selhero_heros + selhero_SaveCount);

	const Point uiPosition = GetUIRectangle().position;

	vecSelDlgItems.clear();
	SDL_Rect rect = MakeSdlRect(uiPosition.x + 24, uiPosition.y + 161, 590, 35);
	vecSelHeroDialog.push_back(std::make_unique<UiArtText>(&title, rect, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	rect = MakeSdlRect(uiPosition.x + 30, uiPosition.y + 211, 180, 76);
	auto heroImg = std::make_unique<UiImageClx>(UiGetHeroDialogSprite(0), rect, UiFlags::None);
	SELHERO_DIALOG_HERO_IMG = heroImg.get();
	vecSelHeroDialog.push_back(std::move(heroImg));

	const UiFlags labelFlags = UiFlags::FontSize12 | UiFlags::ColorUiSilverDark | UiFlags::AlignRight;
	const UiFlags valueFlags = UiFlags::FontSize12 | UiFlags::ColorUiSilverDark | UiFlags::AlignCenter;
	const int labelX = uiPosition.x + 39;
	const int valueX = uiPosition.x + 159;
	const int labelWidth = 110;
	const int valueWidth = 40;
	const int statHeight = 21;

	vecSelHeroDialog.push_back(std::make_unique<UiArtText>(_("Level:").data(), MakeSdlRect(labelX, uiPosition.y + 323, labelWidth, statHeight), labelFlags));
	vecSelHeroDialog.push_back(std::make_unique<UiArtText>(textStats[0], MakeSdlRect(valueX, uiPosition.y + 323, valueWidth, statHeight), valueFlags));

	const char *statLabels[] {
		_("Strength:").data(), _("Magic:").data(), _("Dexterity:").data(), _("Vitality:").data(),
#ifdef _DEBUG
		_("Savegame:").data()
#endif
	};
	int statY = uiPosition.y + 358;
	for (size_t i = 0; i < sizeof(statLabels) / sizeof(statLabels[0]); ++i) {
		vecSelHeroDialog.push_back(std::make_unique<UiArtText>(statLabels[i], MakeSdlRect(labelX, statY, labelWidth, statHeight), labelFlags));
		vecSelHeroDialog.push_back(std::make_unique<UiArtText>(textStats[i + 1], MakeSdlRect(valueX, statY, valueWidth, statHeight), valueFlags));
		statY += statHeight;
	}
}

void selhero_List_Init()
{
	const Point uiPosition = GetUIRectangle().position;

	size_t selectedItem = 0;
	vecSelDlgItems.clear();

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 242), (Sint16)(uiPosition.y + 211), 365, 33 };
	vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Select Hero").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	vecSelHeroDlgItems.clear();
	for (std::size_t i = 0; i < selhero_SaveCount; i++) {
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(std::string_view(selhero_heros[i].name), static_cast<int>(i)));
		if (selhero_heros[i].saveNumber == selhero_heroInfo.saveNumber)
			selectedItem = i;
	}
	vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("New Hero"), static_cast<int>(selhero_SaveCount)));

	vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, 6, uiPosition.x + 265, (uiPosition.y + 256), 320, 26, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 585), (Sint16)(uiPosition.y + 244), 25, 178 };
	vecSelDlgItems.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0], *ArtScrollBarArrow, rect2));

	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 239), (Sint16)(uiPosition.y + 429), 120, 35 };
	vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect3, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	const SDL_Rect rect4 = { (Sint16)(uiPosition.x + 364), (Sint16)(uiPosition.y + 429), 120, 35 };
	auto setlistDialogDeleteButton = std::make_unique<UiArtTextButton>(_("Delete"), &SelheroUiFocusNavigationYesNo, rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver | UiFlags::ElementDisabled);
	SELLIST_DIALOG_DELETE_BUTTON = setlistDialogDeleteButton.get();
	vecSelDlgItems.push_back(std::move(setlistDialogDeleteButton));

	const SDL_Rect rect5 = { (Sint16)(uiPosition.x + 489), (Sint16)(uiPosition.y + 429), 144, 35 };
	vecSelDlgItems.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, rect5, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(SelheroListFocus, SelheroListSelect, SelheroListEsc, vecSelDlgItems, false, nullptr, SelheroListDeleteYesNo, selectedItem);
	if (selhero_isMultiPlayer) {
		title = _("Multi Player Characters").data();
	} else {
		title = _("Single Player Characters").data();
	}
}

static void UiSelHeroDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    void (*fnstats)(HeroClass, _uidefaultstats *),
    bool (*fnremove)(_uiheroinfo *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber)
{
	do {
		gfnHeroInfo = fninfo;
		gfnHeroCreate = fncreate;
		gfnHeroStats = fnstats;
		selhero_result = *dlgresult;

		selhero_navigateYesNo = false;

		selhero_Init();

		if (selhero_SaveCount != 0) {
			selhero_heroInfo = {};
			// Search last used save and remember it as selected item
			for (size_t i = 0; i < selhero_SaveCount; i++) {
				if (selhero_heros[i].saveNumber == *saveNumber) {
					memcpy(&selhero_heroInfo, &selhero_heros[i], sizeof(selhero_heroInfo));
					break;
				}
			}
			selhero_List_Init();
		} else {
			SelheroListSelect(selhero_SaveCount);
		}

		selhero_endMenu = false;
		while (!selhero_endMenu && !selhero_navigateYesNo) {
			UiClearScreen();
			UiRenderItems(vecSelHeroDialog);
			RenderDifficultyIndicators();
			UiPollAndRender();
		}
		SelheroFree();

		if (selhero_navigateYesNo) {
			char dialogTitle[128];
			char dialogText[256];
			if (selhero_isMultiPlayer) {
				CopyUtf8(dialogTitle, _("Delete Multi Player Hero"), sizeof(dialogTitle));
			} else {
				CopyUtf8(dialogTitle, _("Delete Single Player Hero"), sizeof(dialogTitle));
			}
			strcpy(dialogText, fmt::format(fmt::runtime(_("Are you sure you want to delete the character \"{:s}\"?")), selhero_heroInfo.name).c_str());

			if (UiSelHeroYesNoDialog(dialogTitle, dialogText))
				fnremove(&selhero_heroInfo);
		}
	} while (selhero_navigateYesNo);

	*dlgresult = selhero_result;
	*saveNumber = selhero_heroInfo.saveNumber;
}

void UiSelHeroSingDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    bool (*fnremove)(_uiheroinfo *),
    void (*fnstats)(HeroClass, _uidefaultstats *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber,
    _difficulty *difficulty)
{
	selhero_isMultiPlayer = false;
	UiSelHeroDialog(fninfo, fncreate, fnstats, fnremove, dlgresult, saveNumber);
	*difficulty = nDifficulty;
}

void UiSelHeroMultDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    bool (*fnremove)(_uiheroinfo *),
    void (*fnstats)(HeroClass, _uidefaultstats *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber)
{
	selhero_isMultiPlayer = true;
	UiSelHeroDialog(fninfo, fncreate, fnstats, fnremove, dlgresult, saveNumber);
}

} // namespace devilution
