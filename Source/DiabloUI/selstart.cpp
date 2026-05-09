#include "selstart.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/ui_flags.hpp"
#include "DiabloUI/ui_item.h"
#include "engine/load_clx.hpp"
#include "engine/point.hpp"
#include "options.h"
#include "utils/language.h"
#include "utils/ui_fwd.h"

namespace devilution {
namespace {

bool endMenu;

std::vector<std::unique_ptr<UiListItem>> vecDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecDialog;

void ItemSelected(size_t value)
{
	auto option = static_cast<StartUpGameMode>(vecDialogItems[value]->m_value);
	GetOptions().GameMode.gameMode.SetValue(option);
	GetOptions().Mods.SetHellfireEnabled(option == StartUpGameMode::Hellfire);
	SaveOptions();
	endMenu = true;
}

void EscPressed()
{
	endMenu = true;
}

} // namespace

void UiSelStartUpGameOption()
{
	ArtBackgroundWidescreen = LoadOptionalClx("ui_art\\mainmenuw.clx");
	LoadBackgroundArt("ui_art\\mainmenu");
	UiAddBackground(&vecDialog);
	UiAddLogo(&vecDialog);

	const Point uiPosition = GetUIRectangle().position;
	vecDialogItems.push_back(std::make_unique<UiListItem>(_("Enter Hellfire"), static_cast<int>(StartUpGameMode::Hellfire)));
	vecDialogItems.push_back(std::make_unique<UiListItem>(_("Switch to Diablo"), static_cast<int>(StartUpGameMode::Diablo)));
	vecDialog.push_back(std::make_unique<UiList>(vecDialogItems, vecDialogItems.size(), uiPosition.x + 64, uiPosition.y + 240, 510, 43, UiFlags::AlignCenter | UiFlags::FontSize42 | UiFlags::ColorUiGold, 5));

	UiInitList(nullptr, ItemSelected, EscPressed, vecDialog, true);

	endMenu = false;
	while (!endMenu) {
		UiClearScreen();
		UiRenderItems(vecDialog);
		UiPollAndRender();
	}

	ArtBackground = std::nullopt;
	ArtBackgroundWidescreen = std::nullopt;
	vecDialogItems.clear();
	vecDialog.clear();
}

} // namespace devilution
