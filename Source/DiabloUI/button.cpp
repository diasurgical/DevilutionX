#include "DiabloUI/button.h"

#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/diabloui.h"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/load_pcx.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/sdl_compat.h"

namespace devilution {

namespace {

OptionalOwnedClxSpriteList ButtonSprites;

} // namespace

void LoadDialogButtonGraphics()
{
	ButtonSprites = LoadOptionalClx("ui_art\\dvl_but_sml.clx");
	if (!ButtonSprites) {
		ButtonSprites = LoadPcxSpriteList("ui_art\\but_sml", 15);
	}
}

void FreeDialogButtonGraphics()
{
	ButtonSprites = std::nullopt;
}

ClxSprite ButtonSprite(bool pressed)
{
	return (*ButtonSprites)[pressed ? 1 : 0];
}

void RenderButton(const UiButton &button)
{
	const Surface &out = Surface(DiabloUiSurface()).subregion(button.m_rect.x, button.m_rect.y, button.m_rect.w, button.m_rect.h);
	RenderClxSprite(out, ButtonSprite(button.IsPressed()), { 0, 0 });

	Rectangle textRect { { 0, 0 }, { button.m_rect.w, button.m_rect.h } };
	if (!button.IsPressed()) {
		--textRect.position.y;
	}

	DrawString(out, button.GetText(), textRect,
	    { .flags = UiFlags::AlignCenter | UiFlags::FontSizeDialog | UiFlags::ColorDialogWhite });
}

bool HandleMouseEventButton(const SDL_Event &event, UiButton *button)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;
	switch (event.type) {
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (button->IsPressed()) {
			button->Activate();
			return true;
		}
		return false;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		button->Press();
		return true;
	default:
		return false;
	}
}

void HandleGlobalMouseUpButton(UiButton *button)
{
	button->Release();
}

} // namespace devilution
