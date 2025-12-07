/**
 * @file gmenu.cpp
 *
 * Implementation of the in-game navigation and interaction.
 */
#include "gmenu.h"

#include <algorithm>
#include <cstdint>
#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/ui_flags.hpp"
#include "appfat.h"
#include "control.h"
#include "controls/axis_direction.h"
#include "controls/controller_motion.h"
#include "engine/clx_sprite.hpp"
#include "engine/demomode.h"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "headless_mode.hpp"
#include "options.h"
#include "stores.h"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

// Width of the slider menu item, including the label.
constexpr int SliderItemWidth = 490;

// Horizontal dimensions of the slider value
constexpr int SliderValueBoxLeft = 16 + SliderItemWidth / 2;
constexpr int SliderValueBoxWidth = 287;

constexpr int SliderValueBorderWidth = 2;
constexpr int SliderValueLeft = SliderValueBoxLeft + SliderValueBorderWidth;
constexpr int SliderValueWidth = SliderValueBoxWidth - 2 * SliderValueBorderWidth;
constexpr int SliderValueHeight = 29;
constexpr int SliderValuePaddingTop = 10;
constexpr int SliderMarkerWidth = 27;

constexpr int SliderFillMin = SliderMarkerWidth / 2;
constexpr int SliderFillMax = SliderValueWidth - SliderMarkerWidth / 2 - 1;

constexpr int GMenuTop = 117;
constexpr int GMenuItemHeight = 45;

OptionalOwnedClxSpriteList optbar_cel;
OptionalOwnedClxSpriteList PentSpin_cel;
OptionalOwnedClxSpriteList option_cel;
OptionalOwnedClxSpriteList sgpLogo;
bool isDraggingSlider;
TMenuItem *sgpCurrItem;
int LogoAnim_tick;
uint8_t LogoAnim_frame;
void (*gmenu_current_option)();
int sgCurrentMenuIdx;

void GmenuUpDown(bool isDown)
{
	if (sgpCurrItem == nullptr) {
		return;
	}
	isDraggingSlider = false;
	int i = sgCurrentMenuIdx;
	if (sgCurrentMenuIdx != 0) {
		while (i != 0) {
			i--;
			if (isDown) {
				sgpCurrItem++;
				if (sgpCurrItem->fnMenu == nullptr)
					sgpCurrItem = &sgpCurrentMenu[0];
			} else {
				if (sgpCurrItem == sgpCurrentMenu)
					sgpCurrItem = &sgpCurrentMenu[sgCurrentMenuIdx];
				sgpCurrItem--;
			}
			if (sgpCurrItem->enabled()) {
				if (i != 0)
					PlaySFX(SfxID::MenuMove);
				return;
			}
		}
	}
}

void GmenuLeftRight(bool isRight)
{
	if (!sgpCurrItem->isSlider())
		return;

	uint16_t step = sgpCurrItem->sliderStep();
	if (isRight) {
		if (step == sgpCurrItem->sliderSteps())
			return;
		step++;
	} else {
		if (step == 0)
			return;
		step--;
	}
	sgpCurrItem->setSliderStep(step);
	sgpCurrItem->fnMenu(false);
}

int GmenuGetLineWidth(TMenuItem *pItem)
{
	if (pItem->isSlider())
		return SliderItemWidth;

	return GetLineWidth(_(pItem->pszStr), GameFont46, 2);
}

void GmenuDrawMenuItem(const Surface &out, TMenuItem *pItem, int y)
{
	const int w = GmenuGetLineWidth(pItem);
	if (pItem->isSlider()) {
		const int uiPositionX = GetUIRectangle().position.x;
		ClxDraw(out, { SliderValueBoxLeft + uiPositionX, y + 40 }, (*optbar_cel)[0]);
		const uint16_t step = pItem->dwFlags & 0xFFF;
		const uint16_t steps = std::max<uint16_t>(pItem->sliderSteps(), 2);
		const uint16_t pos = SliderFillMin + step * (SliderFillMax - SliderFillMin) / steps;
		SDL_Rect rect = MakeSdlRect(SliderValueLeft + uiPositionX, y + SliderValuePaddingTop, pos, SliderValueHeight);
		SDL_FillSurfaceRect(out.surface, &rect, 205);
		ClxDraw(out, { SliderValueLeft + pos - SliderMarkerWidth / 2 + uiPositionX, y + SliderValuePaddingTop + SliderValueHeight - 1 }, (*option_cel)[0]);
	}

	const int x = (gnScreenWidth - w) / 2;
	const UiFlags style = pItem->enabled() ? UiFlags::ColorGold : UiFlags::ColorBlack;
	DrawString(out, _(pItem->pszStr), Point { x, y },
	    { .flags = style | UiFlags::FontSize46, .spacing = 2 });
	if (pItem == sgpCurrItem) {
		const ClxSprite sprite = (*PentSpin_cel)[PentSpn2Spin()];
		ClxDraw(out, { x - 54, y + 51 }, sprite);
		ClxDraw(out, { x + 4 + w, y + 51 }, sprite);
	}
}

void GameMenuMove()
{
	static AxisDirectionRepeater repeater;
	const AxisDirection moveDir = repeater.Get(GetLeftStickOrDpadDirection(false));
	if (moveDir.x != AxisDirectionX_NONE)
		GmenuLeftRight(moveDir.x == AxisDirectionX_RIGHT);
	if (moveDir.y != AxisDirectionY_NONE)
		GmenuUpDown(moveDir.y == AxisDirectionY_DOWN);
}

bool GmenuMouseIsOverSlider()
{
	const int uiPositionX = GetUIRectangle().position.x;
	if (MousePosition.x < SliderValueLeft + uiPositionX) {
		return false;
	}
	if (MousePosition.x >= SliderValueLeft + SliderValueWidth + uiPositionX) {
		return false;
	}
	return true;
}

int GmenuGetSliderFill()
{
	return std::clamp(MousePosition.x - SliderValueLeft - GetUIRectangle().position.x, SliderFillMin, SliderFillMax);
}

} // namespace

TMenuItem *sgpCurrentMenu;

void gmenu_draw_pause(const Surface &out)
{
	if (leveltype != DTYPE_TOWN)
		RedBack(out);
	if (sgpCurrentMenu == nullptr) {
		DrawString(out, _("Pause"), { { 0, 0 }, { gnScreenWidth, GetMainPanel().position.y } },
		    { .flags = UiFlags::FontSize46 | UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter, .spacing = 2 });
	}
}

void FreeGMenu()
{
	sgpLogo = std::nullopt;
	PentSpin_cel = std::nullopt;
	option_cel = std::nullopt;
	optbar_cel = std::nullopt;
}

void gmenu_init_menu()
{
	LogoAnim_frame = 0;
	sgpCurrentMenu = nullptr;
	sgpCurrItem = nullptr;
	gmenu_current_option = nullptr;
	sgCurrentMenuIdx = 0;
	isDraggingSlider = false;

	if (HeadlessMode)
		return;

	sgpLogo = LoadOptionalCel("data\\hf_logo3", 430);
	if (!sgpLogo.has_value())
		sgpLogo = LoadCel("data\\diabsmal", 296);
	PentSpin_cel = LoadCel("data\\pentspin", 48);
	option_cel = LoadCel("data\\option", SliderMarkerWidth);
	optbar_cel = LoadCel("data\\optbar", SliderValueBoxWidth);
}

bool gmenu_is_active()
{
	return sgpCurrentMenu != nullptr;
}

void gmenu_set_items(TMenuItem *pItem, void (*gmFunc)())
{
	PauseMode = 0;
	isDraggingSlider = false;
	sgpCurrentMenu = pItem;
	gmenu_current_option = gmFunc;
	if (gmenu_current_option != nullptr) {
		gmenu_current_option();
	}
	sgCurrentMenuIdx = 0;
	if (sgpCurrentMenu != nullptr) {
		for (int i = 0; sgpCurrentMenu[i].fnMenu != nullptr; i++) {
			sgCurrentMenuIdx++;
		}
	}
	// BUGFIX: OOB access when sgCurrentMenuIdx is 0; should be set to NULL instead. (fixed)
	sgpCurrItem = sgCurrentMenuIdx > 0 ? &sgpCurrentMenu[sgCurrentMenuIdx - 1] : nullptr;
	GmenuUpDown(true);
	if (sgpCurrentMenu == nullptr && !demo::IsRunning()) {
		SaveOptions();
	}
}

void gmenu_draw(const Surface &out)
{
	if (sgpCurrentMenu != nullptr) {
		GameMenuMove();
		if (gmenu_current_option != nullptr)
			gmenu_current_option();
		if (sgpLogo->numSprites() > 1) {
			const uint32_t ticks = SDL_GetTicks();
			if ((int)(ticks - LogoAnim_tick) > 25) {
				++LogoAnim_frame;
				LogoAnim_frame = LogoAnim_frame % sgpLogo->numSprites();
				LogoAnim_tick = ticks;
			}
		}
		const int uiPositionY = GetUIRectangle().position.y;
		const ClxSprite sprite = (*sgpLogo)[LogoAnim_frame];
		ClxDraw(out, { (gnScreenWidth - sprite.width()) / 2, 102 + uiPositionY }, sprite);
		int y = 110 + uiPositionY;
		TMenuItem *i = sgpCurrentMenu;
		if (sgpCurrentMenu->fnMenu != nullptr) {
			while (i->fnMenu != nullptr) {
				GmenuDrawMenuItem(out, i, y);
				i++;
				y += GMenuItemHeight;
			}
		}
	}
}

bool gmenu_presskeys(SDL_Keycode vkey)
{
	if (sgpCurrentMenu == nullptr)
		return false;
	switch (vkey) {
	case SDLK_KP_ENTER:
	case SDLK_RETURN:
		if (sgpCurrItem->enabled()) {
			PlaySFX(SfxID::MenuMove);
			sgpCurrItem->fnMenu(true);
		}
		break;
	case SDLK_ESCAPE:
		PlaySFX(SfxID::MenuMove);
		gmenu_set_items(nullptr, nullptr);
		break;
	case SDLK_SPACE:
		return false;
	case SDLK_LEFT:
		GmenuLeftRight(false);
		break;
	case SDLK_RIGHT:
		GmenuLeftRight(true);
		break;
	case SDLK_UP:
		GmenuUpDown(false);
		break;
	case SDLK_DOWN:
		GmenuUpDown(true);
		break;
	default:
		break;
	}
	return true;
}

bool gmenu_on_mouse_move()
{
	if (!isDraggingSlider)
		return false;

	const uint16_t step = sgpCurrItem->sliderSteps() * (GmenuGetSliderFill() - SliderFillMin) / (SliderFillMax - SliderFillMin);
	sgpCurrItem->setSliderStep(step);
	sgpCurrItem->fnMenu(false);

	return true;
}

bool gmenu_left_mouse(bool isDown)
{
	if (!isDown) {
		if (isDraggingSlider) {
			isDraggingSlider = false;
			return true;
		}
		return false;
	}

	if (sgpCurrentMenu == nullptr) {
		return false;
	}
	const Point uiPosition = GetUIRectangle().position;
	if (MousePosition.y >= GetMainPanel().position.y) {
		return false;
	}
	if (MousePosition.y - (GMenuTop + uiPosition.y) < 0) {
		return true;
	}
	const int i = (MousePosition.y - (GMenuTop + uiPosition.y)) / GMenuItemHeight;
	if (i >= sgCurrentMenuIdx) {
		return true;
	}
	TMenuItem *pItem = &sgpCurrentMenu[i];
	if (!pItem->enabled()) {
		return true;
	}
	const int w = GmenuGetLineWidth(pItem);
	const uint16_t screenWidth = GetScreenWidth();
	if (MousePosition.x < screenWidth / 2 - w / 2) {
		return true;
	}
	if (MousePosition.x > screenWidth / 2 + w / 2) {
		return true;
	}
	sgpCurrItem = pItem;
	PlaySFX(SfxID::MenuMove);
	if (pItem->isSlider()) {
		isDraggingSlider = GmenuMouseIsOverSlider();
		gmenu_on_mouse_move();
	} else {
		sgpCurrItem->fnMenu(true);
	}
	return true;
}

void gmenu_slider_set(TMenuItem *pItem, int min, int max, int value)
{
	assert(pItem);
	const uint16_t nSteps = std::max<uint16_t>(pItem->sliderSteps(), 2);
	pItem->setSliderStep(((max - min - 1) / 2 + (value - min) * nSteps) / (max - min));
}

int gmenu_slider_get(TMenuItem *pItem, int min, int max)
{
	const uint16_t step = pItem->sliderStep();
	const uint16_t steps = std::max<uint16_t>(pItem->sliderSteps(), 2);
	return min + (step * (max - min) + (steps - 1) / 2) / steps;
}

void gmenu_slider_steps(TMenuItem *pItem, int steps)
{
	pItem->dwFlags &= 0xFF000FFF;
	pItem->setSliderSteps(steps);
}

} // namespace devilution
