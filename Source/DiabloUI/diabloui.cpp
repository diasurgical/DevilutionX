#include "DiabloUI/diabloui.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <function_ref.hpp>

#include "DiabloUI/button.h"
#include "DiabloUI/scrollbar.h"
#include "DiabloUI/text_input.hpp"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "appfat.h"
#include "controls/control_mode.hpp"
#include "controls/controller.h"
#include "controls/input.h"
#include "controls/menu_controls.h"
#include "diablo.h"
#include "discord/discord.h"
#include "effects.h"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/sound.h"
#include "engine/surface.hpp"
#include "engine/ticks.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "init.hpp"
#include "options.h"
#include "player.h"
#include "playerdat.hpp"
#include "sound_effect_enums.h"
#include "utils/algorithm/container.hpp"
#include "utils/display.h"
#include "utils/enum_traits.h"
#include "utils/is_of.hpp"
#include "utils/screen_reader.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

#ifdef __SWITCH__
// for virtual keyboard on Switch
#include "platform/switch/keyboard.h"
#endif
#ifdef __vita__
// for virtual keyboard on Vita
#include "platform/vita/keyboard.h"
#endif
#ifdef __3DS__
// for virtual keyboard on 3DS
#include "platform/ctr/keyboard.h"
#endif

namespace devilution {

OptionalOwnedClxSpriteList ArtLogo;
OptionalOwnedClxSpriteList DifficultyIndicator;

std::array<OptionalOwnedClxSpriteList, 3> ArtFocus;

OptionalOwnedClxSpriteList ArtBackgroundWidescreen;
OptionalOwnedClxSpriteList ArtBackground;
OptionalOwnedClxSpriteList ArtCursor;

std::size_t SelectedItem = 0;

namespace {

OptionalOwnedClxSpriteList ArtHero;
std::vector<uint8_t> ArtHeroPortraitOrder;
std::vector<OptionalOwnedClxSpriteList> ArtHeroOverrides;

std::size_t SelectedItemMax;
std::size_t ListViewportSize = 1;
std::size_t listOffset = 0;

void (*gfnListFocus)(size_t value);
void (*gfnListSelect)(size_t value);
void (*gfnListEsc)();
void (*gfnFullscreen)();
bool (*gfnListYesNo)();
std::vector<UiItemBase *> gUiItems;
UiList *gUiList = nullptr;
bool UiItemsWraps;

std::optional<TextInputState> UiTextInputState;
bool allowEmptyTextInput = false;

constexpr Uint32 ListDoubleClickTimeMs = 500;
std::size_t lastListClickIndex = static_cast<std::size_t>(-1);
Uint32 lastListClickTicks = 0;

struct ScrollBarState {
	bool upArrowPressed;
	bool downArrowPressed;

	ScrollBarState()
	{
		upArrowPressed = false;
		downArrowPressed = false;
	}
} scrollBarState;

void AdjustListOffset(std::size_t itemIndex)
{
	if (itemIndex >= listOffset + ListViewportSize)
		listOffset = itemIndex - (ListViewportSize - 1);
	if (itemIndex < listOffset)
		listOffset = itemIndex;
}

uint32_t fadeTc;
int fadeValue;

void StartUiFadeIn()
{
	fadeValue = 0;
	fadeTc = 0;
}

void UiUpdateFadePalette()
{
	if (fadeValue == 256) return;
	if (fadeValue == 0 && fadeTc == 0) {
		// Start the fade-in.
		fadeTc = SDL_GetTicks();
		fadeValue = 0;
		BlackPalette();
		// We can skip hardware cursor update for fade level 0 (everything is black).
		return;
	}

	const int prevFadeValue = fadeValue;
	fadeValue = static_cast<int>((SDL_GetTicks() - fadeTc) / 2.083); // 32 frames @ 60hz
	if (fadeValue == prevFadeValue) return;

	if (fadeValue >= 256) {
		// Finish the fade-in:
		fadeValue = 256;
		fadeTc = 0;
		ApplyGlobalBrightness(system_palette.data(), logical_palette.data());
		SystemPaletteUpdated();
		if (IsHardwareCursor()) ReinitializeHardwareCursor();
		return;
	}

	SDL_Color palette[256];
	ApplyGlobalBrightness(palette, logical_palette.data());
	ApplyFadeLevel(fadeValue, system_palette.data(), palette);

	SystemPaletteUpdated();
	if (IsHardwareCursor()) ReinitializeHardwareCursor();
}

} // namespace

bool IsTextInputActive()
{
	return UiTextInputState.has_value();
}

void UiInitList(void (*fnFocus)(size_t value), void (*fnSelect)(size_t value), void (*fnEsc)(), const std::vector<std::unique_ptr<UiItemBase>> &items, bool itemsWraps, void (*fnFullscreen)(), bool (*fnYesNo)(), size_t selectedItem /*= 0*/)
{
	SelectedItem = selectedItem;
	SelectedItemMax = 0;
	ListViewportSize = 0;
	gfnListFocus = fnFocus;
	gfnListSelect = fnSelect;
	gfnListEsc = fnEsc;
	gfnFullscreen = fnFullscreen;
	gfnListYesNo = fnYesNo;
	gUiItems.clear();
	for (const auto &item : items)
		gUiItems.push_back(item.get());
	UiItemsWraps = itemsWraps;
	listOffset = 0;
	if (fnFocus != nullptr)
		fnFocus(selectedItem);

#ifndef __SWITCH__
	SDLC_StopTextInput(ghMainWnd); // input is enabled by default
#endif
	UiScrollbar *uiScrollbar = nullptr;
	for (const auto &item : items) {
		if (item->IsType(UiType::Edit)) {
			auto *pItemUIEdit = static_cast<UiEdit *>(item.get());
			SDL_SetTextInputArea(ghMainWnd, &item->m_rect, 0);
			allowEmptyTextInput = pItemUIEdit->m_allowEmpty;
#ifdef __SWITCH__
			switch_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__vita__)
			vita_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__3DS__)
			ctr_vkbdInput(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#else
			SDLC_StartTextInput(ghMainWnd);
#endif
			UiTextInputState.emplace(TextInputState::Options {
			    .value = pItemUIEdit->m_value,
			    .cursor = &pItemUIEdit->m_cursor,
			    .maxLength = pItemUIEdit->m_max_length,
			});
		} else if (item->IsType(UiType::List)) {
			auto *uiList = static_cast<UiList *>(item.get());
			SelectedItemMax = std::max(uiList->m_vecItems.size() - 1, static_cast<size_t>(0));
			ListViewportSize = uiList->viewportSize;
			gUiList = uiList;
			if (selectedItem <= SelectedItemMax && HasAnyOf(uiList->GetItem(selectedItem)->uiFlags, UiFlags::NeedsNextElement))
				AdjustListOffset(selectedItem + 1);
			SpeakText(uiList->GetItem(selectedItem)->m_text);
		} else if (item->IsType(UiType::Scrollbar)) {
			uiScrollbar = static_cast<UiScrollbar *>(item.get());
		}
	}

	AdjustListOffset(selectedItem);

	if (uiScrollbar != nullptr) {
		if (ListViewportSize >= static_cast<std::size_t>(SelectedItemMax + 1)) {
			uiScrollbar->Hide();
		} else {
			uiScrollbar->Show();
		}
	}
}

void UiRenderListItems()
{
	UiRenderItems(gUiItems);
}

void UiInitList_clear()
{
	SelectedItem = 0;
	SelectedItemMax = 0;
	ListViewportSize = 1;
	gfnListFocus = nullptr;
	gfnListSelect = nullptr;
	gfnListEsc = nullptr;
	gfnFullscreen = nullptr;
	gfnListYesNo = nullptr;
	gUiList = nullptr;
	gUiItems.clear();
	UiItemsWraps = false;
}

void UiPlayMoveSound()
{
	effects_play_sound(SfxID::MenuMove);
}

void UiPlaySelectSound()
{
	effects_play_sound(SfxID::MenuSelect);
}

namespace {

void UiFocus(std::size_t itemIndex, bool checkUp, bool ignoreItemsWraps = false)
{
	if (SelectedItem == itemIndex)
		return;

	AdjustListOffset(itemIndex);

	const auto *pItem = gUiList->GetItem(itemIndex);
	while (HasAnyOf(pItem->uiFlags, UiFlags::ElementHidden | UiFlags::ElementDisabled)) {
		if (checkUp) {
			if (itemIndex > 0)
				itemIndex -= 1;
			else if (UiItemsWraps && !ignoreItemsWraps)
				itemIndex = SelectedItemMax;
			else
				checkUp = false;
		} else {
			if (itemIndex < SelectedItemMax)
				itemIndex += 1;
			else if (UiItemsWraps && !ignoreItemsWraps)
				itemIndex = 0;
			else
				checkUp = true;
		}
		pItem = gUiList->GetItem(itemIndex);
	}
	SpeakText(pItem->m_text);

	if (HasAnyOf(pItem->uiFlags, UiFlags::NeedsNextElement))
		AdjustListOffset(itemIndex + 1);
	AdjustListOffset(itemIndex);

	SelectedItem = itemIndex;

	UiPlayMoveSound();

	if (gfnListFocus != nullptr)
		gfnListFocus(itemIndex);
}

void UiFocusUp()
{
	if (SelectedItem > 0)
		UiFocus(SelectedItem - 1, true);
	else if (UiItemsWraps)
		UiFocus(SelectedItemMax, true);
}

void UiFocusDown()
{
	if (SelectedItem < SelectedItemMax)
		UiFocus(SelectedItem + 1, false);
	else if (UiItemsWraps)
		UiFocus(0, false);
}

// UiFocusPageUp/Down mimics the slightly weird behaviour of actual Diablo.

void UiFocusPageUp()
{
	if (listOffset == 0) {
		UiFocus(0, true, true);
	} else {
		const std::size_t relpos = SelectedItem - listOffset;
		std::size_t prevPageStart = SelectedItem - relpos;
		if (prevPageStart >= ListViewportSize)
			prevPageStart -= ListViewportSize;
		else
			prevPageStart = 0;
		AdjustListOffset(prevPageStart);
		UiFocus(listOffset + relpos, true, true);
	}
}

void UiFocusPageDown()
{
	if (listOffset + ListViewportSize > static_cast<std::size_t>(SelectedItemMax)) {
		UiFocus(SelectedItemMax, false, true);
	} else {
		const std::size_t relpos = SelectedItem - listOffset;
		std::size_t nextPageEnd = SelectedItem + (ListViewportSize - relpos - 1);
		if (nextPageEnd + ListViewportSize <= static_cast<std::size_t>(SelectedItemMax))
			nextPageEnd += ListViewportSize;
		else
			nextPageEnd = SelectedItemMax;
		AdjustListOffset(nextPageEnd);
		UiFocus(listOffset + relpos, false, true);
	}
}

bool HandleMenuAction(MenuAction menuAction)
{
	switch (menuAction) {
	case MenuAction_SELECT:
		UiFocusNavigationSelect();
		return true;
	case MenuAction_UP:
		UiFocusUp();
		return true;
	case MenuAction_DOWN:
		UiFocusDown();
		return true;
	case MenuAction_PAGE_UP:
		UiFocusPageUp();
		return true;
	case MenuAction_PAGE_DOWN:
		UiFocusPageDown();
		return true;
	case MenuAction_DELETE:
		UiFocusNavigationYesNo();
		return true;
	case MenuAction_BACK:
		if (gfnListEsc == nullptr)
			return false;
		UiFocusNavigationEsc();
		return true;
	default:
		return false;
	}
}

void UiOnBackgroundChange()
{
	StartUiFadeIn();

	if (IsHardwareCursorEnabled() && ArtCursor && ControlDevice == ControlTypes::KeyboardAndMouse && GetCurrentCursorInfo().type() != CursorType::UserInterface) {
		SetHardwareCursor(CursorInfo::UserInterfaceCursor());
	}

	// It may take some time to get to the first `UiFadeIn()` call from here
	// if there is non-trivial initialization work, such as loading the list
	// of single-player characters.
	//
	// Black out the screen immediately to make it appear more smooth.
	SDL_FillSurfaceRect(DiabloUiSurface(), nullptr, 0);
	if (DiabloUiSurface() == PalSurface)
		BltFast(nullptr, nullptr);
	RenderPresent();
}

void UiFocusNavigation(SDL_Event *event)
{
	switch (event->type) {
	case SDL_EVENT_KEY_UP:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
	case SDL_EVENT_JOYSTICK_AXIS_MOTION:
	case SDL_EVENT_JOYSTICK_BALL_MOTION:
	case SDL_EVENT_JOYSTICK_HAT_MOTION:
#ifndef USE_SDL1
	case SDL_EVENT_MOUSE_WHEEL:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
#endif
#ifdef USE_SDL3
	case SDL_EVENT_WINDOW_EXPOSED:
#else
#ifndef USE_SDL1
	case SDL_WINDOWEVENT:
#endif
	case SDL_SYSWMEVENT:
#endif
		mainmenu_restart_repintro();
		break;
	default:
		break;
	}

	bool menuActionHandled = false;
	for (const MenuAction menuAction : GetMenuActions(*event))
		menuActionHandled |= HandleMenuAction(menuAction);
	if (menuActionHandled)
		return;

#ifndef USE_SDL1
	if (event->type == SDL_EVENT_MOUSE_WHEEL) {
		if (SDLC_EventWheelIntY(*event) > 0) {
			UiFocusUp();
		} else if (SDLC_EventWheelIntY(*event) < 0) {
			UiFocusDown();
		}
		return;
	}
#else
	if (event->type == SDL_MOUSEBUTTONDOWN) {
		switch (event->button.button) {
		case SDL_BUTTON_WHEELUP:
			UiFocusUp();
			return;
		case SDL_BUTTON_WHEELDOWN:
			UiFocusDown();
			return;
		}
	}
#endif

	if (UiTextInputState.has_value() && HandleTextInputEvent(*event, *UiTextInputState)) {
		return;
	}

	if (IsAnyOf(event->type, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP)
	    && UiItemMouseEvents(event, gUiItems)) {
		return;
	}
}

} // namespace

void UiHandleEvents(SDL_Event *event)
{
	if (event->type == SDL_EVENT_MOUSE_MOTION) {
		MousePosition = { SDLC_EventMotionIntX(*event), SDLC_EventMotionIntY(*event) };
		return;
	}

	if (event->type == SDL_EVENT_KEY_DOWN && SDLC_EventKey(*event) == SDLK_RETURN) {
		const auto *state = SDLC_GetKeyState();
		if (state[SDLC_KEYSTATE_LALT] != 0 || state[SDLC_KEYSTATE_RALT] != 0) {
			GetOptions().Graphics.fullscreen.SetValue(!IsFullScreen());
			SaveOptions();
			if (gfnFullscreen != nullptr)
				gfnFullscreen();
			return;
		}
	}

	if (event->type == SDL_EVENT_QUIT) {
		diablo_quit(0);
	}

#ifndef USE_SDL1
	HandleControllerAddedOrRemovedEvent(*event);

#ifdef USE_SDL3
	switch (event->type) {
	case SDL_EVENT_WINDOW_SHOWN:
	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_RESTORED:
		gbActive = true;
		break;
	case SDL_EVENT_WINDOW_HIDDEN:
	case SDL_EVENT_WINDOW_MINIMIZED:
		gbActive = false;
		break;
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		DoReinitializeHardwareCursor();
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		if (*GetOptions().Gameplay.pauseOnFocusLoss) music_mute();
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		if (*GetOptions().Gameplay.pauseOnFocusLoss) diablo_focus_unpause();
		break;
	default:
		break;
	}
#else
	if (event->type == SDL_WINDOWEVENT) {
		if (IsAnyOf(event->window.event, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_EXPOSED, SDL_WINDOWEVENT_RESTORED)) {
			gbActive = true;
		} else if (IsAnyOf(event->window.event, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_MINIMIZED)) {
			gbActive = false;
		} else if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			// We reinitialize immediately (by calling `DoReinitializeHardwareCursor` instead of `ReinitializeHardwareCursor`)
			// because the cursor's Enabled state may have changed, resulting in changes to visibility.
			//
			// For example, if the previous size was too large for a hardware cursor then it was invisible
			// but may now become visible.
			DoReinitializeHardwareCursor();
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST && *GetOptions().Gameplay.pauseOnFocusLoss) {
			music_mute();
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED && *GetOptions().Gameplay.pauseOnFocusLoss) {
			diablo_focus_unpause();
		}
	}
#endif
#else
	if (event->type == SDL_ACTIVEEVENT && (event->active.state & SDL_APPINPUTFOCUS) != 0) {
		if (event->active.gain == 0)
			music_mute();
		else
			diablo_focus_unpause();
	}
#endif
}

void UiFocusNavigationSelect()
{
	UiPlaySelectSound();
	if (UiTextInputState.has_value()) {
		if (!allowEmptyTextInput && UiTextInputState->empty()) {
			return;
		}
#ifndef __SWITCH__
		SDLC_StopTextInput(ghMainWnd);
#endif
		UiTextInputState = std::nullopt;
	}
	if (gfnListSelect != nullptr)
		gfnListSelect(SelectedItem);
}

void UiFocusNavigationEsc()
{
	UiPlaySelectSound();
	if (UiTextInputState.has_value()) {
#ifndef __SWITCH__
		SDLC_StopTextInput(ghMainWnd);
#endif
		UiTextInputState = std::nullopt;
	}
	if (gfnListEsc != nullptr)
		gfnListEsc();
}

void UiFocusNavigationYesNo()
{
	if (gfnListYesNo == nullptr)
		return;

	if (gfnListYesNo())
		UiPlaySelectSound();
}

namespace {

bool IsInsideRect(const SDL_Event &event, const SDL_Rect &rect)
{
	const SDL_Point point = { SDLC_EventButtonIntX(event), SDLC_EventButtonIntY(event) };
	return SDLC_PointInRect(&point, &rect);
}

void LoadHeros()
{
	constexpr unsigned PortraitHeight = 76;
	ArtHero = LoadPcxSpriteList("ui_art\\heros", -static_cast<int>(PortraitHeight));
	if (!ArtHero)
		return;
	const uint16_t numPortraits = ClxSpriteList { *ArtHero }.numSprites();

	ArtHeroPortraitOrder.resize(GetNumPlayerClasses() + 1);
	for (size_t i = 0; i < GetNumPlayerClasses(); ++i) {
		const PlayerData &playerClassData = GetPlayerDataForClass(static_cast<HeroClass>(i));
		ArtHeroPortraitOrder[i] = playerClassData.portrait;
	}
	ArtHeroPortraitOrder.back() = 3;
	if (numPortraits >= 6) {
		ArtHeroPortraitOrder.back() = 5;
	}

	ArtHeroOverrides.resize(GetNumPlayerClasses() + 1);
	for (size_t i = 0; i <= GetNumPlayerClasses(); ++i) {
		char portraitPath[18];
		*BufCopy(portraitPath, "ui_art\\hero", i) = '\0';
		ArtHeroOverrides[i] = LoadPcx(portraitPath, /*transparentColor=*/std::nullopt, /*outPalette=*/nullptr, /*logError=*/false);
	}
}

void LoadUiGFX()
{
	ArtLogo = LoadPcxSpriteList("ui_art\\hf_logo2", /*numFrames=*/16, /*transparentColor=*/0, nullptr, false);
	if (!ArtLogo.has_value()) {
		ArtLogo = LoadPcxSpriteList("ui_art\\smlogo", /*numFrames=*/15, /*transparentColor=*/250);
	}
	DifficultyIndicator = LoadPcx("ui_art\\r1_gry", /*transparentColor=*/0);
	ArtFocus[FOCUS_SMALL] = LoadPcxSpriteList("ui_art\\focus16", /*numFrames=*/8, /*transparentColor=*/250);
	ArtFocus[FOCUS_MED] = LoadPcxSpriteList("ui_art\\focus", /*numFrames=*/8, /*transparentColor=*/250);
	ArtFocus[FOCUS_BIG] = LoadPcxSpriteList("ui_art\\focus42", /*numFrames=*/8, /*transparentColor=*/250);

	ArtCursor = LoadPcx("ui_art\\cursor", /*transparentColor=*/0);

	LoadHeros();
}

} // namespace

ClxSprite UiGetHeroDialogSprite(size_t heroClassIndex)
{
	return ArtHeroOverrides[heroClassIndex]
	    ? (*ArtHeroOverrides[heroClassIndex])[0]
	    : (*ArtHero)[ArtHeroPortraitOrder[heroClassIndex]];
}

void UnloadUiGFX()
{
	ArtHero = std::nullopt;
	for (OptionalOwnedClxSpriteList &override : ArtHeroOverrides)
		override = std::nullopt;
	ArtCursor = std::nullopt;
	for (auto &art : ArtFocus)
		art = std::nullopt;
	ArtLogo = std::nullopt;
	DifficultyIndicator = std::nullopt;
}

void UiInitialize()
{
	LoadUiGFX();

	if (ArtCursor) {
		if (!SDLC_HideCursor()) ErrSdl();
	}
}

void UiDestroy()
{
	UnloadFonts();
	UnloadUiGFX();
}

bool UiValidPlayerName(std::string_view name)
{
	if (name.empty())
		return false;

	// Currently only allow saving PlayerNameLength bytes as a player name, so if the name is too long we'd have to truncate it.
	// That said the input buffer is only 16 bytes long...
	if (name.size() > PlayerNameLength)
		return false;

	if (name.find_first_of(",<>%&\\\"?*#/: ") != name.npos)
		return false;

	// Only basic latin alphabet is supported for multiplayer characters to avoid rendering issues for players who do
	// not have fonts.mpq installed
	if (!c_all_of(name, IsBasicLatin))
		return false;

	const std::string_view bannedNames[] = {
		"gvdl",
		"dvou",
		"tiju",
		"cjudi",
		"bttipmf",
		"ojhhfs",
		"cmj{{bse",
		"benjo",
	};

	std::string buffer { name };
	for (char &character : buffer)
		character++;

	const std::string_view tempName { buffer };
	for (const std::string_view bannedName : bannedNames) {
		if (tempName.find(bannedName) != tempName.npos)
			return false;
	}

	return true;
}

Sint16 GetCenterOffset(Sint16 w, Sint16 bw)
{
	if (bw == 0) {
		bw = gnScreenWidth;
	}

	return (bw - w) / 2;
}

void UiLoadDefaultPalette()
{
	LoadPalette("ui_art\\diablo.pal");
	UpdateSystemPalette(logical_palette);
}

bool UiLoadBlackBackground()
{
	ArtBackground = std::nullopt;
	UiLoadDefaultPalette();
	UiOnBackgroundChange();
	return true;
}

void LoadBackgroundArt(const char *pszFile, int frames)
{
	ArtBackground = std::nullopt;
	ArtBackground = LoadPcxSpriteList(pszFile, static_cast<uint16_t>(frames), /*transparentColor=*/std::nullopt, logical_palette.data());
	if (!ArtBackground)
		return;

	UpdateSystemPalette(logical_palette);
	UiOnBackgroundChange();
}

void UiAddBackground(std::vector<std::unique_ptr<UiItemBase>> *vecDialog)
{
	const SDL_Rect rect = MakeSdlRect(0, GetUIRectangle().position.y, 0, 0);
	if (ArtBackgroundWidescreen) {
		vecDialog->push_back(std::make_unique<UiImageClx>((*ArtBackgroundWidescreen)[0], rect, UiFlags::AlignCenter));
	}
	if (ArtBackground) {
		vecDialog->push_back(std::make_unique<UiImageClx>((*ArtBackground)[0], rect, UiFlags::AlignCenter));
	}
}

void UiAddLogo(std::vector<std::unique_ptr<UiItemBase>> *vecDialog, int y)
{
	vecDialog->push_back(std::make_unique<UiImageAnimatedClx>(
	    *ArtLogo, MakeSdlRect(0, y, 0, 0), UiFlags::AlignCenter));
}

void UiFadeIn()
{
	if (HeadlessMode) return;
	UiUpdateFadePalette();
	if (DiabloUiSurface() == PalSurface) {
		BltFast(nullptr, nullptr);
	}
	RenderPresent();
}

namespace {
ClxSpriteList GetListSelectorSprites(int itemHeight)
{
	int size;
	if (itemHeight >= 42) {
		size = FOCUS_BIG;
	} else if (itemHeight >= 30) {
		size = FOCUS_MED;
	} else {
		size = FOCUS_SMALL;
	}
	return *ArtFocus[size];
}
} // namespace

void DrawSelector(const SDL_Rect &rect)
{
	const ClxSpriteList sprites = GetListSelectorSprites(rect.h);
	const ClxSprite sprite = sprites[GetAnimationFrame(sprites.numSprites())];

	// TODO FOCUS_MED appears higher than the box
	const int y = rect.y + ((rect.h - static_cast<int>(sprite.height())) / 2);

	const Surface &out = Surface(DiabloUiSurface());
	RenderClxSprite(out, sprite, { rect.x, y });
	RenderClxSprite(out, sprite, { rect.x + rect.w - sprite.width(), y });
}

void UiClearScreen()
{
	if (!ArtBackground || gnScreenWidth > (*ArtBackground)[0].width() || gnScreenHeight > (*ArtBackground)[0].height()) {
		SDL_FillSurfaceRect(DiabloUiSurface(), nullptr, 0);
	}
}

void UiPollAndRender(std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler)
{
	SDL_Event event;
	while (PollEvent(&event)) {
		if (eventHandler && (*eventHandler)(event))
			continue;
		if (!SDLC_ConvertEventToRenderCoordinates(renderer, &event)) {
			LogWarn(LogCategory::Application, "SDL_ConvertEventToRenderCoordinates: {}", SDL_GetError());
			SDL_ClearError();
		}
		UiFocusNavigation(&event);
		UiHandleEvents(&event);
	}
	HandleMenuAction(GetMenuHeldUpDownAction());
	UiRenderListItems();
	DrawMouse();
	UiFadeIn();

	// Must happen after at least one call to `UiFadeIn` with non-zero fadeValue.
	// `UiFadeIn` reinitializes the hardware cursor only for fadeValue > 0.
	if (IsHardwareCursor() && fadeValue != 0)
		SetHardwareCursorVisible(ControlDevice == ControlTypes::KeyboardAndMouse);

#ifdef __3DS__
	// Keyboard blocks until input is finished
	// so defer until after render and fade-in
	ctr_vkbdFlush();
#endif

	discord_manager::UpdateMenu();
}

namespace {

void Render(const UiText &uiText)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiText.GetText(), MakeRectangle(uiText.m_rect),
	    { .flags = uiText.GetFlags() | UiFlags::FontSizeDialog });
}

void Render(const UiArtText &uiArtText)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiArtText.GetText(), MakeRectangle(uiArtText.m_rect),
	    { .flags = uiArtText.GetFlags(), .spacing = uiArtText.GetSpacing(), .lineHeight = uiArtText.GetLineHeight() });
}

void Render(const UiImageClx &uiImage)
{
	const ClxSprite sprite = uiImage.sprite();
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiImageAnimatedClx &uiImage)
{
	const ClxSprite sprite = uiImage.sprite(GetAnimationFrame(uiImage.numFrames()));
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiArtTextButton &uiButton)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiButton.GetText(), MakeRectangle(uiButton.m_rect), { .flags = uiButton.GetFlags() });
}

void Render(const UiList &uiList)
{
	const Surface &out = Surface(DiabloUiSurface());

	for (std::size_t i = listOffset; i < uiList.m_vecItems.size() && (i - listOffset) < ListViewportSize; ++i) {
		const SDL_Rect rect = uiList.itemRect(static_cast<int>(i - listOffset));
		const UiListItem &item = *uiList.GetItem(i);
		if (i == SelectedItem)
			DrawSelector(rect);

		const Rectangle rectangle = MakeRectangle(rect).inset(
		    Displacement(GetListSelectorSprites(rect.h)[0].width(), 0));

		const UiFlags uiFlags = uiList.GetFlags() | item.uiFlags;
		const GameFontTables fontSize = GetFontSizeFromUiFlags(uiFlags);
		std::string_view text = item.m_text.str();
		while (GetLineWidth(text, fontSize, 1) > rectangle.size.width) {
			text = std::string_view(text.data(), FindLastUtf8Symbols(text));
		}

		if (item.args.empty()) {
			DrawString(out, text, rectangle, { .flags = uiFlags, .spacing = uiList.GetSpacing() });
		} else {
			DrawStringWithColors(out, text, item.args, rectangle, { .flags = uiFlags, .spacing = uiList.GetSpacing() });
		}
	}
}

void Render(const UiScrollbar &uiSb)
{
	const Surface out = Surface(DiabloUiSurface());

	// Bar background (tiled):
	{
		const int bgY = uiSb.m_rect.y + uiSb.m_arrow[0].height();
		const int bgH = DownArrowRect(uiSb).y - bgY;
		const Surface backgroundOut = out.subregion(uiSb.m_rect.x, bgY, ScrollBarBgWidth, bgH);
		int y = 0;
		while (y < bgH) {
			RenderClxSprite(backgroundOut, uiSb.m_bg, { 0, y });
			y += uiSb.m_bg.height();
		}
	}

	// Arrows:
	{
		const SDL_Rect rect = UpArrowRect(uiSb);
		const auto frame = static_cast<uint16_t>(scrollBarState.upArrowPressed ? ScrollBarArrowFrame_UP_ACTIVE : ScrollBarArrowFrame_UP);
		RenderClxSprite(out.subregion(rect.x, 0, ScrollBarArrowWidth, out.h()), uiSb.m_arrow[frame], { 0, rect.y });
	}
	{
		const SDL_Rect rect = DownArrowRect(uiSb);
		const auto frame = static_cast<uint16_t>(scrollBarState.downArrowPressed ? ScrollBarArrowFrame_DOWN_ACTIVE : ScrollBarArrowFrame_DOWN);
		RenderClxSprite(out.subregion(rect.x, 0, ScrollBarArrowWidth, out.h()), uiSb.m_arrow[frame], { 0, rect.y });
	}

	// Thumb:
	if (SelectedItemMax > 0) {
		const SDL_Rect rect = ThumbRect(uiSb, SelectedItem, SelectedItemMax + 1);
		RenderClxSprite(out, uiSb.m_thumb, { rect.x, rect.y });
	}
}

void Render(const UiEdit &uiEdit)
{
	DrawSelector(uiEdit.m_rect);

	// To simulate padding we inset the region used to draw text in an edit control
	const Rectangle rect = MakeRectangle(uiEdit.m_rect).inset({ 43, 1 });

	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiEdit.m_value, rect,
	    {
	        .flags = uiEdit.GetFlags(),
	        .cursorPosition = static_cast<int>(uiEdit.m_cursor.position),
	        .highlightRange = { static_cast<int>(uiEdit.m_cursor.selection.begin), static_cast<int>(uiEdit.m_cursor.selection.end) },
	        .highlightColor = 126,
	    });
}

bool HandleMouseEventArtTextButton(const SDL_Event &event, const UiArtTextButton *uiButton)
{
	if (event.type != SDL_EVENT_MOUSE_BUTTON_UP || event.button.button != SDL_BUTTON_LEFT) {
		return false;
	}

	uiButton->Activate();
	return true;
}

bool HandleMouseEventList(const SDL_Event &event, UiList *uiList)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;

	if (!IsAnyOf(event.type, SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_MOUSE_BUTTON_DOWN)) {
		return false;
	}

	std::size_t index = uiList->indexAt(event.button.y);
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		uiList->Press(index);
		return true;
	}

	if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && !uiList->IsPressed(index)) {
		return false;
	}

	index += listOffset;
	const bool hasFocusCallback = gfnListFocus != nullptr;
	const Uint32 ticksNow = SDL_GetTicks();
	const bool recentlyClickedSameItem = hasFocusCallback && lastListClickIndex == index && ticksNow - lastListClickTicks <= ListDoubleClickTimeMs;
#ifndef USE_SDL1
	const bool sdlReportedDoubleClick = event.button.clicks >= 2;
#else
	const bool sdlReportedDoubleClick = false;
#endif
	const bool doubleClicked = recentlyClickedSameItem || sdlReportedDoubleClick;
	lastListClickIndex = index;
	lastListClickTicks = ticksNow;

	if (hasFocusCallback && SelectedItem != index) {
		UiFocus(index, true, false);
		return true;
	}

	if (hasFocusCallback && !doubleClicked) {
		return true;
	}

	if (HasAnyOf(uiList->GetItem(index)->uiFlags, UiFlags::ElementHidden | UiFlags::ElementDisabled))
		return false;
	SelectedItem = index;
	UiFocusNavigationSelect();

	return true;
}

bool HandleMouseEventScrollBar(const SDL_Event &event, const UiScrollbar *uiSb)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;
	if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		if (scrollBarState.upArrowPressed && IsInsideRect(event, UpArrowRect(*uiSb))) {
			UiFocusUp();
			return true;
		}
		if (scrollBarState.downArrowPressed && IsInsideRect(event, DownArrowRect(*uiSb))) {
			UiFocusDown();
			return true;
		}
	} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (IsInsideRect(event, BarRect(*uiSb))) {
			// Scroll up or down based on thumb position.
			const SDL_Rect thumbRect = ThumbRect(*uiSb, SelectedItem, SelectedItemMax + 1);
			if (event.button.y < thumbRect.y) {
				UiFocusPageUp();
			} else if (event.button.y > thumbRect.y + thumbRect.h) {
				UiFocusPageDown();
			}
			return true;
		}
		if (IsInsideRect(event, UpArrowRect(*uiSb))) {
			scrollBarState.upArrowPressed = true;
			return true;
		}
		if (IsInsideRect(event, DownArrowRect(*uiSb))) {
			scrollBarState.downArrowPressed = true;
			return true;
		}
	}
	return false;
}

bool HandleMouseEvent(const SDL_Event &event, UiItemBase *item)
{
	if (item->IsNotInteractive() || !IsInsideRect(event, item->m_rect))
		return false;
	switch (item->GetType()) {
	case UiType::ArtTextButton:
		return HandleMouseEventArtTextButton(event, static_cast<UiArtTextButton *>(item));
	case UiType::Button:
		return HandleMouseEventButton(event, static_cast<UiButton *>(item));
	case UiType::List:
		return HandleMouseEventList(event, static_cast<UiList *>(item));
	case UiType::Scrollbar:
		return HandleMouseEventScrollBar(event, static_cast<UiScrollbar *>(item));
	default:
		return false;
	}
}

} // namespace

void UiRenderItem(const UiItemBase &item)
{
	if (item.IsHidden())
		return;
	switch (item.GetType()) {
	case UiType::Text:
		Render(static_cast<const UiText &>(item));
		break;
	case UiType::ArtText:
		Render(static_cast<const UiArtText &>(item));
		break;
	case UiType::ImageClx:
		Render(static_cast<const UiImageClx &>(item));
		break;
	case UiType::ImageAnimatedClx:
		Render(static_cast<const UiImageAnimatedClx &>(item));
		break;
	case UiType::ArtTextButton:
		Render(static_cast<const UiArtTextButton &>(item));
		break;
	case UiType::Button:
		RenderButton(static_cast<const UiButton &>(item));
		break;
	case UiType::List:
		Render(static_cast<const UiList &>(item));
		break;
	case UiType::Scrollbar:
		Render(static_cast<const UiScrollbar &>(item));
		break;
	case UiType::Edit:
		Render(static_cast<const UiEdit &>(item));
		break;
	}
}

void UiRenderItems(const std::vector<UiItemBase *> &items)
{
	for (const UiItemBase *item : items)
		UiRenderItem(*item);
}

void UiRenderItems(const std::vector<std::unique_ptr<UiItemBase>> &items)
{
	for (const std::unique_ptr<UiItemBase> &item : items)
		UiRenderItem(*item);
}

bool UiItemMouseEvents(SDL_Event *event, const std::vector<UiItemBase *> &items)
{
	if (items.empty()) {
		return false;
	}

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item)) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
		scrollBarState.downArrowPressed = scrollBarState.upArrowPressed = false;
		for (const auto &item : items) {
			if (item->IsType(UiType::Button)) {
				HandleGlobalMouseUpButton(static_cast<UiButton *>(item));
			} else if (item->IsType(UiType::List)) {
				static_cast<UiList *>(item)->Release();
			}
		}
	}

	return handled;
}

bool UiItemMouseEvents(SDL_Event *event, const std::vector<std::unique_ptr<UiItemBase>> &items)
{
	if (items.empty()) {
		return false;
	}

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item.get())) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
		scrollBarState.downArrowPressed = scrollBarState.upArrowPressed = false;
		for (const auto &item : items) {
			if (item->IsType(UiType::Button)) {
				HandleGlobalMouseUpButton(static_cast<UiButton *>(item.get()));
			} else if (item->IsType(UiType::List)) {
				static_cast<UiList *>(item.get())->Release();
			}
		}
	}

	return handled;
}

void DrawMouse()
{
	if (ControlDevice != ControlTypes::KeyboardAndMouse || IsHardwareCursor() || !ArtCursor)
		return;
	RenderClxSprite(Surface(DiabloUiSurface()), (*ArtCursor)[0], MousePosition);
}
} // namespace devilution
