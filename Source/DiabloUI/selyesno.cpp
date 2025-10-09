#include "selyesno.h"

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
#include "utils/language.h"
#include "utils/ui_fwd.h"
#include "utils/utf8.hpp"

namespace devilution {
namespace {

bool selyesno_endMenu;
bool selyesno_value;
char selyesno_confirmationMessage[256];

std::vector<std::unique_ptr<UiListItem>> vecSelYesNoDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelYesNoDialog;

#define MESSAGE_WIDTH 400

void SelyesnoFree()
{
	ArtBackground = std::nullopt;

	vecSelYesNoDialogItems.clear();

	vecSelYesNoDialog.clear();
}

void SelyesnoSelect(size_t value)
{
	selyesno_value = vecSelYesNoDialogItems[value]->m_value == 0;
	selyesno_endMenu = true;
}

void SelyesnoEsc()
{
	selyesno_value = false;
	selyesno_endMenu = true;
}

} // namespace

bool UiSelHeroYesNoDialog(const char *title, const char *body)
{
	UiLoadBlackBackground();
	UiAddBackground(&vecSelYesNoDialog);
	UiAddLogo(&vecSelYesNoDialog);

	const Point uiPosition = GetUIRectangle().position;

	const SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(title, rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	const SDL_Rect rect2 = { (Sint16)(uiPosition.x + 120), (Sint16)(uiPosition.y + 236), MESSAGE_WIDTH, 168 };
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(selyesno_confirmationMessage, rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	vecSelYesNoDialogItems.push_back(std::make_unique<UiListItem>(_("Yes"), 0));
	vecSelYesNoDialogItems.push_back(std::make_unique<UiListItem>(_("No"), 1));
	vecSelYesNoDialog.push_back(std::make_unique<UiList>(vecSelYesNoDialogItems, vecSelYesNoDialogItems.size(), uiPosition.x + 230, (uiPosition.y + 390), 180, 35, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	CopyUtf8(selyesno_confirmationMessage, WordWrapString(body, MESSAGE_WIDTH, GameFont24), sizeof(selyesno_confirmationMessage));

	UiInitList(nullptr, SelyesnoSelect, SelyesnoEsc, vecSelYesNoDialog, true);

	selyesno_value = true;
	selyesno_endMenu = false;
	while (!selyesno_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelYesNoDialog);
		UiPollAndRender();
	}

	SelyesnoFree();

	return selyesno_value;
}
} // namespace devilution
