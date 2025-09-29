#include "DiabloUI/diabloui.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "DiabloUI/button.h"
#include "DiabloUI/dialogs.h"
#include "DiabloUI/scrollbar.h"
#include "controls/controller.h"
#include "controls/devices/kbcontroller.h"
#include "controls/input.h"
#include "controls/menu_controls.h"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "discord/discord.h"
#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_pcx.hpp"
#include "engine/render/clx_render.hpp"
#include "hwcursor.hpp"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/pcx_to_clx.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/stubs.h"
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

bool textInputActive = true;
std::size_t SelectedItem = 0;

const string_view BannedNames[] = {
	"gvdl",
	"dvou",
	"tiju",
	"cjudi",
	"bttipmf",
	"ojhhfs",
	"cmj{{bse",
	"benjo",
	"cfbofs",
	"dijol",
	"gbhhpu",
	"ezlf",
	"hppl",
	"hzqtz",
	"kjhbcpp",
	"ljlf",
	"lzlf",
	"ojhhb",
	"qpsdinpolfz",
	"sbhifbe",
	"sfubse",
	"tmboufzf",
	"usbooz",
	"usppo",
	"xfucbdl",
	"xijufusbti",
	"xijufqpxfs",
	"cmbdlqpxfs",
	"{jqqfsifbe",
	"ejbtvshjdbm",
	"efwjmvujpo",
};

const size_t BannedNamesCount = sizeof(BannedNames) / sizeof(BannedNames[0]);

namespace {

OptionalOwnedClxSpriteList ArtHero;
std::array<uint8_t, enum_size<HeroClass>::value + 1> ArtHeroPortraitOrder;
std::array<OptionalOwnedClxSpriteList, enum_size<HeroClass>::value + 1> ArtHeroOverrides;

std::size_t SelectedItemMax;
std::size_t ListViewportSize = 1;
std::size_t listOffset = 0;

void (*gfnListFocus)(int value);
void (*gfnListSelect)(int value);
void (*gfnListEsc)();
void (*gfnFullscreen)();
bool (*gfnListYesNo)();
std::vector<UiItemBase *> gUiItems;
UiList *gUiList = nullptr;
bool UiItemsWraps;
std::optional<TextInputState> UiTextInputState;
bool allowEmptyTextInput = false;

uint32_t fadeTc;
int fadeValue = 0;

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

} // namespace

void UiInitList(void (*fnFocus)(int value), void (*fnSelect)(int value), void (*fnEsc)(), const std::vector<std::unique_ptr<UiItemBase>> &items, bool itemsWraps, void (*fnFullscreen)(), bool (*fnYesNo)(), size_t selectedItem /*= 0*/)
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
	SDL_StopTextInput(); // input is enabled by default
#endif
	textInputActive = false;
	UiScrollbar *uiScrollbar = nullptr;
	for (const auto &item : items) {
		if (item->IsType(UiType::Edit)) {
			auto *pItemUIEdit = static_cast<UiEdit *>(item.get());
			SDL_SetTextInputRect(&item->m_rect);
			textInputActive = true;
			allowEmptyTextInput = pItemUIEdit->m_allowEmpty;
#ifdef __SWITCH__
			switch_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__vita__)
			vita_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__3DS__)
			ctr_vkbdInput(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#else
			SDL_StartTextInput();
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
	effects_play_sound(IS_TITLEMOV);
}

void UiPlaySelectSound()
{
	effects_play_sound(IS_TITLSLCT);
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

void SelheroCatToName(const char *inBuf, char *outBuf, int cnt)
{
	size_t outLen = strlen(outBuf);
	char *dest = outBuf + outLen;
	size_t destCount = cnt - outLen;
	CopyUtf8(dest, inBuf, destCount);
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
	fadeTc = 0;
	fadeValue = 0;

	BlackPalette();

	if (IsHardwareCursorEnabled() && ArtCursor && ControlDevice == ControlTypes::KeyboardAndMouse && GetCurrentCursorInfo().type() != CursorType::UserInterface) {
		SetHardwareCursor(CursorInfo::UserInterfaceCursor());
	}

	SDL_FillRect(DiabloUiSurface(), nullptr, 0x000000);
	if (DiabloUiSurface() == PalSurface)
		BltFast(nullptr, nullptr);
	RenderPresent();
}

} // namespace

void UiFocusNavigation(SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYUP:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEMOTION:
#ifndef USE_SDL1
	case SDL_MOUSEWHEEL:
#endif
	case SDL_JOYBUTTONUP:
	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
	case SDL_JOYHATMOTION:
#ifndef USE_SDL1
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
	case SDL_CONTROLLERBUTTONUP:
	case SDL_CONTROLLERAXISMOTION:
	case SDL_WINDOWEVENT:
#endif
	case SDL_SYSWMEVENT:
		mainmenu_restart_repintro();
		break;
	}

	bool menuActionHandled = false;
	for (MenuAction menuAction : GetMenuActions(*event))
		menuActionHandled |= HandleMenuAction(menuAction);
	if (menuActionHandled)
		return;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (event->type == SDL_MOUSEWHEEL) {
		if (event->wheel.y > 0) {
			UiFocusUp();
		} else if (event->wheel.y < 0) {
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

	if (UiTextInputState && HandleTextInputEvent(*event, *UiTextInputState)) {
		return;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
		if (UiItemMouseEvents(event, gUiItems))
			return;
	}
}

void UiHandleEvents(SDL_Event *event)
{
	if (event->type == SDL_MOUSEMOTION) {
#ifdef USE_SDL1
		OutputToLogical(&event->motion.x, &event->motion.y);
#endif
		MousePosition = { event->motion.x, event->motion.y };
		return;
	}

#if HAS_KBCTRL == 0
	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
		const Uint8 *state = SDLC_GetKeyState();
		if (state[SDLC_KEYSTATE_LALT] != 0 || state[SDLC_KEYSTATE_RALT] != 0) {
			sgOptions.Graphics.fullscreen.SetValue(!IsFullScreen());
			SaveOptions();
			if (gfnFullscreen != nullptr)
				gfnFullscreen();
			return;
		}
	}
#endif

	if (event->type == SDL_QUIT)
		diablo_quit(0);

#ifndef USE_SDL1
	HandleControllerAddedOrRemovedEvent(*event);

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
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			music_mute();
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			diablo_focus_unpause();
		}
	}
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
	if (IsTextInputActive()) {
		if (!allowEmptyTextInput && UiTextInputState->empty()) {
			return;
		}
#ifndef __SWITCH__
		SDL_StopTextInput();
#endif
		UiTextInputState = std::nullopt;
	}
	if (gfnListSelect != nullptr)
		gfnListSelect(SelectedItem);
}

void UiFocusNavigationEsc()
{
	UiPlaySelectSound();
	if (IsTextInputActive()) {
#ifndef __SWITCH__
		SDL_StopTextInput();
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
	const SDL_Point point = { event.button.x, event.button.y };
	return SDL_PointInRect(&point, &rect) == SDL_TRUE;
}

void LoadHeros()
{
	constexpr unsigned PortraitHeight = 76;
	ArtHero = LoadPcxSpriteList("ui_art\\heros", -static_cast<int>(PortraitHeight));
	if (!ArtHero)
		return;
	const uint16_t numPortraits = ClxSpriteList { *ArtHero }.numSprites();

	ArtHeroPortraitOrder = { 0, 1, 2, 2, 1, 0, 3 };
	if (numPortraits >= 6) {
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Monk)] = 3;
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Bard)] = 4;
		ArtHeroPortraitOrder[enum_size<HeroClass>::value] = 5;
	}
	if (numPortraits >= 7) {
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Barbarian)] = 6;
	}

	for (size_t i = 0; i <= enum_size<HeroClass>::value; ++i) {
		char portraitPath[18];
		*BufCopy(portraitPath, "ui_art\\hero", i) = '\0';
		ArtHeroOverrides[i] = LoadPcx(portraitPath, /*transparentColor=*/std::nullopt, /*outPalette=*/nullptr, /*logError=*/false);
	}
}

void LoadUiGFX()
{
	if (gbIsHellfire) {
		ArtLogo = LoadPcxSpriteList("ui_art\\hf_logo2", /*numFrames=*/16, /*transparentColor=*/0);
	} else {
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

bool IsTextInputActive()
{
	return bool(UiTextInputState);
}

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
		if (SDL_ShowCursor(SDL_DISABLE) <= -1) {
			ErrSdl();
		}
	}
}

void UiDestroy()
{
	UnloadFonts();
	UnloadUiGFX();
}

bool UiValidPlayerName(string_view name)
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
	if (!std::all_of(name.begin(), name.end(), IsBasicLatin))
		return false;

	std::string buffer { name };

	AsciiStrToLower(buffer);

	for (char &character : buffer)
		character++;

	for (string_view bannedName : BannedNames) {
		if (buffer.find(bannedName) != std::string::npos)
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
	LoadPalette(gbIsHellfire ? "ui_art\\hellfire.pal" : "ui_art\\diablo.pal", /*blend=*/false);
	ApplyGamma(logical_palette, orig_palette, 256);
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
	SDL_Color pPal[256];
	ArtBackground = LoadPcxSpriteList(pszFile, static_cast<uint16_t>(frames), /*transparentColor=*/std::nullopt, pPal);
	if (!ArtBackground)
		return;

	LoadPalInMem(pPal);
	ApplyGamma(logical_palette, orig_palette, 256);
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
	if (fadeValue < 256) {
		if (fadeValue == 0 && fadeTc == 0)
			fadeTc = SDL_GetTicks();
		const int prevFadeValue = fadeValue;
		fadeValue = static_cast<int>((SDL_GetTicks() - fadeTc) / 2.083); // 32 frames @ 60hz
		if (fadeValue > 256) {
			fadeValue = 256;
			fadeTc = 0;
		}
		if (fadeValue != prevFadeValue) {
			// We can skip hardware cursor update for fade level 0 (everything is black).
			SetFadeLevel(fadeValue, /*updateHardwareCursor=*/fadeValue != 0);
		}
	}

	if (DiabloUiSurface() == PalSurface)
		BltFast(nullptr, nullptr);
	RenderPresent();
}

void DrawSelector(const SDL_Rect &rect)
{
	int size = FOCUS_SMALL;
	if (rect.h >= 42)
		size = FOCUS_BIG;
	else if (rect.h >= 30)
		size = FOCUS_MED;
	const ClxSpriteList sprites = *ArtFocus[size];
	const ClxSprite sprite = sprites[GetAnimationFrame(sprites.numSprites())];

	// TODO FOCUS_MED appares higher than the box
	const int y = rect.y + (rect.h - static_cast<int>(sprite.height())) / 2;

	const Surface &out = Surface(DiabloUiSurface());
	RenderClxSprite(out, sprite, { rect.x, y });
	RenderClxSprite(out, sprite, { rect.x + rect.w - sprite.width(), y });
}

void UiClearScreen()
{
	if (!ArtBackground || gnScreenWidth > (*ArtBackground)[0].width() || gnScreenHeight > (*ArtBackground)[0].height())
		SDL_FillRect(DiabloUiSurface(), nullptr, 0x000000);
}

void UiPollAndRender(std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler)
{
	SDL_Event event;
	while (PollEvent(&event) != 0) {
		if (eventHandler && (*eventHandler)(event))
			continue;
		UiFocusNavigation(&event);
		UiHandleEvents(&event);
	}
	HandleMenuAction(GetMenuHeldUpDownAction());
	UiRenderListItems();
	DrawMouse();
	UiFadeIn();

	// Must happen after at least one call to `UiFadeIn` with non-zero fadeValue.
	// `UiFadeIn` calls `SetFadeLevel` which reinitializes the hardware cursor.
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
	DrawString(out, uiText.GetText(), MakeRectangle(uiText.m_rect), { uiText.GetFlags() | UiFlags::FontSizeDialog });
}

void Render(const UiArtText &uiArtText)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiArtText.GetText(), MakeRectangle(uiArtText.m_rect), { uiArtText.GetFlags(), uiArtText.GetSpacing(), uiArtText.GetLineHeight() });
}

void Render(const UiImageClx &uiImage)
{
	ClxSprite sprite = uiImage.sprite();
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiImageAnimatedClx &uiImage)
{
	ClxSprite sprite = uiImage.sprite(GetAnimationFrame(uiImage.numFrames()));
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiArtTextButton &uiButton)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiButton.GetText(), MakeRectangle(uiButton.m_rect), { uiButton.GetFlags() });
}

void Render(const UiList &uiList)
{
	const Surface &out = Surface(DiabloUiSurface());

	for (std::size_t i = listOffset; i < uiList.m_vecItems.size() && (i - listOffset) < ListViewportSize; ++i) {
		SDL_Rect rect = uiList.itemRect(i - listOffset);
		const UiListItem &item = *uiList.GetItem(i);
		if (i == SelectedItem)
			DrawSelector(rect);

		Rectangle rectangle = MakeRectangle(rect);
		if (item.args.empty())
			DrawString(out, item.m_text, rectangle, { uiList.GetFlags() | item.uiFlags, uiList.GetSpacing() });
		else
			DrawStringWithColors(out, item.m_text, item.args, rectangle, { uiList.GetFlags() | item.uiFlags, uiList.GetSpacing() });
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
	Rectangle rect = MakeRectangle(uiEdit.m_rect).inset({ 43, 1 });

	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiEdit.m_value, rect,
	    {
	        uiEdit.GetFlags(),
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(uiEdit.m_cursor.position),
	        /*highlightRange=*/ { static_cast<int>(uiEdit.m_cursor.selection.begin), static_cast<int>(uiEdit.m_cursor.selection.end) },
	        /*highlightColor=*/126,
	    });
}

bool HandleMouseEventArtTextButton(const SDL_Event &event, const UiArtTextButton *uiButton)
{
	if (event.type != SDL_MOUSEBUTTONUP || event.button.button != SDL_BUTTON_LEFT) {
		return false;
	}

	uiButton->Activate();
	return true;
}

#ifdef USE_SDL1
Uint32 dbClickTimer;
#endif

bool HandleMouseEventList(const SDL_Event &event, UiList *uiList)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;

	if (event.type != SDL_MOUSEBUTTONUP && event.type != SDL_MOUSEBUTTONDOWN)
		return false;

	std::size_t index = uiList->indexAt(event.button.y);
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		uiList->Press(index);
		return true;
	}

	if (event.type == SDL_MOUSEBUTTONUP && !uiList->IsPressed(index))
		return false;

	index += listOffset;

	if (gfnListFocus != nullptr && SelectedItem != index) {
		UiFocus(index, true, false);
#ifdef USE_SDL1
		dbClickTimer = SDL_GetTicks();
	} else if (gfnListFocus == NULL || dbClickTimer + 500 >= SDL_GetTicks()) {
#else
	} else if (gfnListFocus == nullptr || event.button.clicks >= 2) {
#endif
		if (HasAnyOf(uiList->GetItem(index)->uiFlags, UiFlags::ElementHidden | UiFlags::ElementDisabled))
			return false;
		SelectedItem = index;
		UiFocusNavigationSelect();
#ifdef USE_SDL1
	} else {
		dbClickTimer = SDL_GetTicks();
#endif
	}

	return true;
}

bool HandleMouseEventScrollBar(const SDL_Event &event, const UiScrollbar *uiSb)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;
	if (event.type == SDL_MOUSEBUTTONUP) {
		if (scrollBarState.upArrowPressed && IsInsideRect(event, UpArrowRect(*uiSb))) {
			UiFocusUp();
			return true;
		}
		if (scrollBarState.downArrowPressed && IsInsideRect(event, DownArrowRect(*uiSb))) {
			UiFocusDown();
			return true;
		}
	} else if (event.type == SDL_MOUSEBUTTONDOWN) {
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

void LoadPalInMem(const SDL_Color *pPal)
{
	for (int i = 0; i < 256; i++) {
		orig_palette[i] = pPal[i];
	}
}

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

	// In SDL2 mouse events already use logical coordinates.
#ifdef USE_SDL1
	OutputToLogical(&event->button.x, &event->button.y);
#endif

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item)) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
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

	// In SDL2 mouse events already use logical coordinates.
#ifdef USE_SDL1
	OutputToLogical(&event->button.x, &event->button.y);
#endif

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item.get())) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
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
