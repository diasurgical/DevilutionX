#include <memory>
#include <optional>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/button.h"
#include "DiabloUI/diabloui.h"
#include "DiabloUI/ui_item.h"
#include "controls/input.h"
#include "controls/menu_controls.h"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_pcx.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/display.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/ui_fwd.h"

namespace devilution {
namespace {
OptionalOwnedClxSpriteList ArtPopupSm;
OptionalOwnedClxSpriteList ArtProgBG;
OptionalOwnedClxSpriteList ProgFil;
std::vector<std::unique_ptr<UiItemBase>> vecProgress;
bool endMenu;

void DialogActionCancel()
{
	endMenu = true;
}

void ProgressLoadBackground()
{
	UiLoadBlackBackground();
	ArtPopupSm = LoadPcx("ui_art\\spopup");
	ArtProgBG = LoadPcx("ui_art\\prog_bg");
}

void ProgressLoadForeground()
{
	LoadDialogButtonGraphics();
	ProgFil = LoadPcx("ui_art\\prog_fil");

	const Point uiPosition = GetUIRectangle().position;
	const SDL_Rect rect3 = { (Sint16)(uiPosition.x + 265), (Sint16)(uiPosition.y + 267), DialogButtonWidth, DialogButtonHeight };
	vecProgress.push_back(std::make_unique<UiButton>(_("Cancel"), &DialogActionCancel, rect3));
}

void ProgressFreeBackground()
{
	ArtBackground = std::nullopt;
	ArtPopupSm = std::nullopt;
	ArtProgBG = std::nullopt;
}

void ProgressFreeForeground()
{
	vecProgress.clear();
	ProgFil = std::nullopt;
	FreeDialogButtonGraphics();
}

Point GetPosition()
{
	return { GetCenterOffset(280), GetCenterOffset(144, gnScreenHeight) };
}

void ProgressRenderBackground()
{
	SDL_FillSurfaceRect(DiabloUiSurface(), nullptr, 0);

	const Surface &out = Surface(DiabloUiSurface());
	const Point position = GetPosition();
	RenderClxSprite(out.subregion(position.x, position.y, 280, 140), (*ArtPopupSm)[0], { 0, 0 });
	RenderClxSprite(out.subregion(GetCenterOffset(227), 0, 227, out.h()), (*ArtProgBG)[0], { 0, position.y + 52 });
}

void ProgressRenderForeground(int progress)
{
	const Surface &out = Surface(DiabloUiSurface());
	const Point position = GetPosition();
	if (progress != 0) {
		const int x = GetCenterOffset(227);
		const int w = 227 * progress / 100;
		RenderClxSprite(out.subregion(x, 0, w, out.h()), (*ProgFil)[0], { 0, position.y + 52 });
	}
	// Not rendering an actual button, only the top 2 rows of its graphics.
	RenderClxSprite(
	    out.subregion(GetCenterOffset(110), position.y + 99, DialogButtonWidth, 2),
	    ButtonSprite(/*pressed=*/false), { 0, 0 });
}

} // namespace

bool UiProgressDialog(int (*fnfunc)())
{
	// Blit the background once and then free it.
	ProgressLoadBackground();

	ProgressRenderBackground();

	if (RenderDirectlyToOutputSurface && PalSurface != nullptr) {
		// Render into all the backbuffers if there are multiple.
		const void *initialPixels = PalSurface->pixels;
		UiFadeIn();
		while (PalSurface->pixels != initialPixels) {
			ProgressRenderBackground();
			UiFadeIn();
		}
	}

	ProgressFreeBackground();

	ProgressLoadForeground();

	endMenu = false;
	int progress = 0;

	SDL_Event event;
	while (!endMenu && progress < 100) {
		progress = fnfunc();
		ProgressRenderForeground(progress);
		UiRenderItems(vecProgress);
		DrawMouse();
		UiFadeIn();

		while (PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				UiItemMouseEvents(&event, vecProgress);
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (SDLC_EventKey(event)) {
#ifndef USE_SDL1
				case SDLK_KP_ENTER:
#endif
				case SDLK_ESCAPE:
				case SDLK_RETURN:
				case SDLK_SPACE:
					endMenu = true;
					break;
				default:
					break;
				}
				break;
			default:
				for (const MenuAction menuAction : GetMenuActions(event)) {
					if (IsNoneOf(menuAction, MenuAction_BACK, MenuAction_SELECT))
						continue;
					endMenu = true;
					break;
				}
				break;
			}
			UiHandleEvents(&event);
		}
	}
	ProgressFreeForeground();

	return progress == 100;
}

} // namespace devilution
