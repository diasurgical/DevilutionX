#include "DiabloUI/selok.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/diabloui.h"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "engine/point.hpp"
#include "engine/render/text_render.hpp"
#include "game_mode.hpp"
#include "utils/language.h"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

char dialogText[256];

} // namespace

bool selok_endMenu;

std::vector<std::unique_ptr<UiListItem>> vecSelOkDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelOkDialog;

#define MESSAGE_WIDTH 400

void selok_Free()
{
	ArtBackground = std::nullopt;

	vecSelOkDialogItems.clear();

	vecSelOkDialog.clear();
}

void selok_Select(size_t /*value*/)
{
	selok_endMenu = true;
}

void selok_Esc()
{
	selok_endMenu = true;
}

void UiSelOkDialog(const char *title, const char *body, bool background)
{
	if (!background) {
		UiLoadBlackBackground();
	} else {
		if (!gbIsSpawn) {
			LoadBackgroundArt("ui_art\\mainmenu");
		} else {
			LoadBackgroundArt("ui_art\\swmmenu");
		}
	}

	UiAddBackground(&vecSelOkDialog);
	UiAddLogo(&vecSelOkDialog);

	const Point uiPosition = GetUIRectangle().position;

	if (title != nullptr) {
		const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
		vecSelOkDialog.push_back(std::make_unique<UiArtText>(title, rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 140), (Sint16)(uiPosition.y + 210), 560, 168 };
		vecSelOkDialog.push_back(std::make_unique<UiArtText>(dialogText, rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));
	} else {
		const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 140), (Sint16)(uiPosition.y + 197), 560, 168 };
		vecSelOkDialog.push_back(std::make_unique<UiArtText>(dialogText, rect1, UiFlags::FontSize24 | UiFlags::ColorUiSilver));
	}

	vecSelOkDialogItems.push_back(std::make_unique<UiListItem>(_("OK"), 0));
	vecSelOkDialog.push_back(std::make_unique<UiList>(vecSelOkDialogItems, 1, uiPosition.x + 230, (uiPosition.y + 390), 180, 35, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	CopyUtf8(dialogText, WordWrapString(body, MESSAGE_WIDTH, GameFont24), sizeof(dialogText));

	UiInitList(nullptr, selok_Select, selok_Esc, vecSelOkDialog, false);

	selok_endMenu = false;
	while (!selok_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelOkDialog);
		UiPollAndRender();
	}

	selok_Free();
}
} // namespace devilution
