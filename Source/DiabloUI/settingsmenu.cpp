#include "selstart.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <function_ref.hpp>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/scrollbar.h"
#include "control.h"
#include "controls/controller_motion.h"
#include "controls/plrctrls.h"
#include "controls/remap_keyboard.h"
#include "engine/assets.hpp"
#include "engine/render/text_render.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "utils/display.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {
namespace {

constexpr size_t IndexKeyOrPadInput = 1;
constexpr size_t IndexPadTimerText = 2;

bool endMenu = false;
bool backToMain = false;

std::vector<std::unique_ptr<UiListItem>> vecDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecDialog;
std::vector<OptionEntryBase *> vecOptions;
OptionCategoryBase *selectedCategory = nullptr;
OptionEntryBase *selectedOption = nullptr;

enum class ShownMenuType : uint8_t {
	Categories,
	Settings,
	ListOption,
	KeyInput,
	PadInput,
};

ShownMenuType shownMenu;

char optionDescription[512];

Rectangle rectList;
Rectangle rectDescription;

enum class SpecialMenuEntry : int8_t {
	None = -1,
	PreviousMenu = -2,
	UnbindKey = -3,
	BindPadButton = -4,
	UnbindPadButton = -5,
};

ControllerButtonCombo padEntryCombo {};
Uint32 padEntryStartTime = 0;
std::string padEntryTimerText;

bool IsValidEntry(OptionEntryBase *pOptionEntry)
{
	auto flags = pOptionEntry->GetFlags();
	if (HasAnyOf(flags, OptionEntryFlags::NeedDiabloMpq) && !HaveIntro())
		return false;
	if (HasAnyOf(flags, OptionEntryFlags::NeedHellfireMpq) && !HaveHellfire())
		return false;
	return HasNoneOf(flags, OptionEntryFlags::Invisible | (gbIsHellfire ? OptionEntryFlags::OnlyDiablo : OptionEntryFlags::OnlyHellfire));
}

std::vector<DrawStringFormatArg> CreateDrawStringFormatArgForEntry(OptionEntryBase *pEntry)
{
	return std::vector<DrawStringFormatArg> {
		{ pEntry->GetName(), UiFlags::ColorUiGold },
		{ pEntry->GetValueDescription(), UiFlags::ColorUiSilver }
	};
}

/** @brief Check if the option text can't fit in one list line (list width minus drawn selector) */
bool NeedsTwoLinesToDisplayOption(std::vector<DrawStringFormatArg> &formatArgs)
{
	return GetLineWidth("{}: {}", formatArgs.data(), formatArgs.size(), 0, GameFontTables::GameFont24, 1) >= (rectList.size.width - 90);
}

void CleanUpSettingsUI()
{
	UiInitList_clear();

	vecDialogItems.clear();
	vecDialog.clear();
	vecOptions.clear();

	ArtBackground = std::nullopt;
	ArtBackgroundWidescreen = std::nullopt;
	UnloadScrollBar();
}

void GoBackOneMenuLevel()
{
	endMenu = true;
	switch (shownMenu) {
	case ShownMenuType::Categories:
		backToMain = true;
		break;
	case ShownMenuType::Settings:
		shownMenu = ShownMenuType::Categories;
		break;
	default:
		shownMenu = ShownMenuType::Settings;
		break;
	}
}

void StartPadEntryTimer()
{
	padEntryCombo = ControllerButton_NONE;
	padEntryStartTime = SDL_GetTicks();
	if (padEntryStartTime == 0)
		padEntryStartTime++;
	// Removes access to these dialog items while entering bindings
	for (size_t i = IndexPadTimerText + 1; i < vecDialogItems.size(); i++)
		vecDialogItems[i]->uiFlags |= UiFlags::ElementHidden;
}

void StopPadEntryTimer()
{
	padEntryCombo = ControllerButton_NONE;
	padEntryStartTime = 0;
	padEntryTimerText = "";
	vecDialogItems[IndexPadTimerText]->m_text = padEntryTimerText;
	// Restores access to these dialog items after binding is complete
	for (size_t i = IndexPadTimerText + 1; i < vecDialogItems.size(); i++)
		vecDialogItems[i]->uiFlags &= ~UiFlags::ElementHidden;
}

void UpdatePadEntryTimerText()
{
	if (shownMenu != ShownMenuType::PadInput)
		return;
	Uint32 elapsed = SDL_GetTicks() - padEntryStartTime;
	if (padEntryStartTime == 0 || elapsed > 10000) {
		StopPadEntryTimer();
		return;
	}
	padEntryTimerText = StrCat(_("Press gamepad buttons to change."), " ", 10 - elapsed / 1000);
	vecDialogItems[IndexPadTimerText]->m_text = padEntryTimerText;
}

void UpdateDescription(const OptionEntryBase &option)
{
	auto paragraphs = WordWrapString(option.GetDescription(), rectDescription.size.width, GameFont12, 1);
	CopyUtf8(optionDescription, paragraphs, sizeof(optionDescription));
}

void UpdateDescription(const OptionCategoryBase &category)
{
	auto paragraphs = WordWrapString(category.GetDescription(), rectDescription.size.width, GameFont12, 1);
	CopyUtf8(optionDescription, paragraphs, sizeof(optionDescription));
}

void ItemFocused(size_t value)
{
	switch (shownMenu) {
	case ShownMenuType::Categories: {
		auto &vecItem = vecDialogItems[value];
		optionDescription[0] = '\0';
		if (vecItem->m_value < 0)
			return;
		auto *pCategory = GetOptions().GetCategories()[vecItem->m_value];
		UpdateDescription(*pCategory);
	} break;
	case ShownMenuType::Settings: {
		auto &vecItem = vecDialogItems[value];
		optionDescription[0] = '\0';
		if (vecItem->m_value < 0)
			return;
		auto *pOption = vecOptions[vecItem->m_value];
		UpdateDescription(*pOption);
	} break;
	default:
		break;
	}
}

bool ChangeOptionValue(OptionEntryBase *pOption, size_t listIndex)
{
	if (HasAnyOf(pOption->GetFlags(), OptionEntryFlags::RecreateUI)) {
		endMenu = true;
		// Clean up all UI related Data
		CleanUpSettingsUI();
		UnloadUiGFX();
		FreeItemGFX();
		selectedOption = pOption;
	}

	switch (pOption->GetType()) {
	case OptionEntryType::Boolean: {
		auto *pOptionBoolean = static_cast<OptionEntryBoolean *>(pOption);
		pOptionBoolean->SetValue(!**pOptionBoolean);
	} break;
	case OptionEntryType::List: {
		auto *pOptionList = static_cast<OptionEntryListBase *>(pOption);
		pOptionList->SetActiveListIndex(listIndex);
	} break;
	case OptionEntryType::Key:
	case OptionEntryType::PadButton:
		break;
	}

	if (HasAnyOf(pOption->GetFlags(), OptionEntryFlags::RecreateUI)) {
		// Reinitialize UI with changed settings (for example game mode, language or resolution)
		UiInitialize();
		InitItemGFX();
		SetHardwareCursor(CursorInfo::UnknownCursor());
		return false;
	}

	return true;
}

void ItemSelected(size_t value)
{
	auto &vecItem = vecDialogItems[value];
	int vecItemValue = vecItem->m_value;
	if (vecItemValue < 0) {
		auto specialMenuEntry = static_cast<SpecialMenuEntry>(vecItemValue);
		switch (specialMenuEntry) {
		case SpecialMenuEntry::None:
			break;
		case SpecialMenuEntry::PreviousMenu:
			GoBackOneMenuLevel();
			break;
		case SpecialMenuEntry::UnbindKey: {
			auto *pOptionKey = static_cast<KeymapperOptions::Action *>(selectedOption);
			pOptionKey->SetValue(SDLK_UNKNOWN);
			vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription();
			break;
		}
		case SpecialMenuEntry::BindPadButton:
			StartPadEntryTimer();
			break;
		case SpecialMenuEntry::UnbindPadButton:
			auto *pOptionPad = static_cast<PadmapperOptions::Action *>(selectedOption);
			pOptionPad->SetValue(ControllerButton_NONE);
			vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription();
			break;
		}
		return;
	}

	switch (shownMenu) {
	case ShownMenuType::Categories: {
		selectedCategory = GetOptions().GetCategories()[vecItemValue];
		endMenu = true;
		shownMenu = ShownMenuType::Settings;
	} break;
	case ShownMenuType::Settings: {
		auto *pOption = vecOptions[vecItemValue];
		bool updateValueDescription = false;
		if (pOption->GetType() == OptionEntryType::List) {
			auto *pOptionList = static_cast<OptionEntryListBase *>(pOption);
			if (pOptionList->GetListSize() > 2) {
				selectedOption = pOption;
				endMenu = true;
				shownMenu = ShownMenuType::ListOption;
			} else {
				// If the list contains only two items, we don't show a submenu and instead change the option value instantly
				size_t nextIndex = pOptionList->GetActiveListIndex() + 1;
				if (nextIndex >= pOptionList->GetListSize())
					nextIndex = 0;
				updateValueDescription = ChangeOptionValue(pOption, nextIndex);
			}
		} else if (pOption->GetType() == OptionEntryType::Key) {
			selectedOption = pOption;
			endMenu = true;
			shownMenu = ShownMenuType::KeyInput;
		} else if (pOption->GetType() == OptionEntryType::PadButton) {
			selectedOption = pOption;
			endMenu = true;
			shownMenu = ShownMenuType::PadInput;
		} else {
			updateValueDescription = ChangeOptionValue(pOption, 0);
		}
		if (updateValueDescription) {
			auto args = CreateDrawStringFormatArgForEntry(pOption);
			bool optionUsesTwoLines = ((value + 1) < vecDialogItems.size() && vecDialogItems[value]->m_value == vecDialogItems[value + 1]->m_value);
			if (NeedsTwoLinesToDisplayOption(args) != optionUsesTwoLines) {
				selectedOption = pOption;
				endMenu = true;
			} else {
				vecItem->args.clear();
				for (auto &arg : args)
					vecItem->args.push_back(arg);
				if (optionUsesTwoLines) {
					vecDialogItems[value + 1]->m_text = std::string(pOption->GetValueDescription());
				}
			}
		}
	} break;
	case ShownMenuType::ListOption: {
		ChangeOptionValue(selectedOption, vecItemValue);
		GoBackOneMenuLevel();
	} break;
	case ShownMenuType::KeyInput:
	case ShownMenuType::PadInput:
		break;
	}
}

void EscPressed()
{
	GoBackOneMenuLevel();
}

void FullscreenChanged()
{
	auto *fullscreenOption = &GetOptions().Graphics.fullscreen;

	for (auto &vecItem : vecDialogItems) {
		int vecItemValue = vecItem->m_value;
		if (vecItemValue < 0 || static_cast<size_t>(vecItemValue) >= vecOptions.size())
			continue;

		auto *pOption = vecOptions[vecItemValue];
		if (pOption != fullscreenOption)
			continue;

		vecItem->args.clear();
		for (auto &arg : CreateDrawStringFormatArgForEntry(pOption))
			vecItem->args.push_back(arg);
		break;
	}
}

} // namespace

void UiSettingsMenu()
{
	backToMain = false;
	shownMenu = ShownMenuType::Categories;
	selectedCategory = nullptr;
	selectedOption = nullptr;

	do {
		endMenu = false;

		// For the settings menu, we use the full height and allow some more width.
		const int uiWidth = std::clamp<int>(gnScreenWidth, 640, 720);
		const Rectangle uiRectangle = {
			{ (gnScreenWidth - uiWidth) / 2, 0 },
			{ uiWidth, gnScreenHeight }
		};

		UiLoadBlackBackground();
		LoadScrollBar();
		UiAddBackground(&vecDialog);
		UiAddLogo(&vecDialog, uiRectangle.position.y);

		const int descriptionLineHeight = IsSmallFontTall() ? 20 : 18;
		const int descriptionMarginTop = IsSmallFontTall() ? 10 : 16;

		optionDescription[0] = '\0';

		std::string_view titleText;
		switch (shownMenu) {
		case ShownMenuType::Categories:
			titleText = _("Settings");
			break;
		case ShownMenuType::Settings:
			titleText = selectedCategory->GetName();
			break;
		default:
			titleText = selectedOption->GetName();
			break;
		}
		vecDialog.push_back(std::make_unique<UiArtText>(titleText.data(), MakeSdlRect(uiRectangle.position.x, uiRectangle.position.y + 161, uiRectangle.size.width, 35), UiFlags::FontSize30 | UiFlags::ColorUiSilver | UiFlags::AlignCenter, 8));

		size_t itemToSelect = 0;
		std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler;

		switch (shownMenu) {
		case ShownMenuType::Categories: {
			size_t catIndex = 0;
			for (OptionCategoryBase *pCategory : GetOptions().GetCategories()) {
				for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
					if (!IsValidEntry(pEntry))
						continue;
					if (selectedCategory == pCategory)
						itemToSelect = vecDialogItems.size();
					vecDialogItems.push_back(std::make_unique<UiListItem>(pCategory->GetName(), static_cast<int>(catIndex), UiFlags::ColorUiGold));
					break;
				}
				catIndex++;
			}
		} break;
		case ShownMenuType::Settings: {
			for (OptionEntryBase *pEntry : selectedCategory->GetEntries()) {
				if (!IsValidEntry(pEntry))
					continue;
				if (selectedOption == pEntry)
					itemToSelect = vecDialogItems.size();
				auto formatArgs = CreateDrawStringFormatArgForEntry(pEntry);
				int optionId = static_cast<int>(vecOptions.size());
				if (NeedsTwoLinesToDisplayOption(formatArgs)) {
					vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view("{}:"), formatArgs, optionId, UiFlags::ColorUiGold | UiFlags::NeedsNextElement));
					vecDialogItems.push_back(std::make_unique<UiListItem>(std::string(pEntry->GetValueDescription()), optionId, UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
				} else {
					vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view("{}: {}"), formatArgs, optionId, UiFlags::ColorUiGold));
				}
				vecOptions.push_back(pEntry);
			}
		} break;
		case ShownMenuType::ListOption: {
			auto *pOptionList = static_cast<OptionEntryListBase *>(selectedOption);
			for (size_t i = 0; i < pOptionList->GetListSize(); i++) {
				vecDialogItems.push_back(std::make_unique<UiListItem>(pOptionList->GetListDescription(i), static_cast<int>(i), UiFlags::ColorUiGold));
			}
			itemToSelect = pOptionList->GetActiveListIndex();
			UpdateDescription(*pOptionList);
		} break;
		case ShownMenuType::KeyInput: {
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Bound key:"), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorWhitegold | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(std::string(selectedOption->GetValueDescription()), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiGold));
			assert(IndexKeyOrPadInput == vecDialogItems.size() - 1);
			itemToSelect = IndexKeyOrPadInput;
			eventHandler = [](SDL_Event &event) {
				if (SelectedItem != IndexKeyOrPadInput)
					return false;
				uint32_t key = SDLK_UNKNOWN;
				switch (event.type) {
				case SDL_KEYDOWN: {
					SDL_Keycode keycode = event.key.keysym.sym;
					remap_keyboard_key(&keycode);
					key = static_cast<uint32_t>(keycode);
					if (key >= SDLK_a && key <= SDLK_z) {
						key -= 'a' - 'A';
					}
				} break;
				case SDL_MOUSEBUTTONDOWN:
					switch (event.button.button) {
					case SDL_BUTTON_MIDDLE:
					case SDL_BUTTON_X1:
					case SDL_BUTTON_X2:
						key = event.button.button | KeymapperMouseButtonMask;
						break;
					}
					break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
				case SDL_MOUSEWHEEL:
					if (event.wheel.y > 0) {
						key = MouseScrollUpButton;
					} else if (event.wheel.y < 0) {
						key = MouseScrollDownButton;
					} else if (event.wheel.x > 0) {
						key = MouseScrollLeftButton;
					} else if (event.wheel.x < 0) {
						key = MouseScrollRightButton;
					}
					break;
#endif
				}
				// Ignore unknown keys
				if (key == SDLK_UNKNOWN)
					return false;
				auto *pOptionKey = static_cast<KeymapperOptions::Action *>(selectedOption);
				if (!pOptionKey->SetValue(key))
					return false;
				vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription();
				return true;
			};
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Press any key to change."), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view {}, static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Unbind key"), static_cast<int>(SpecialMenuEntry::UnbindKey), UiFlags::ColorUiGold));
			UpdateDescription(*selectedOption);
		} break;
		case ShownMenuType::PadInput: {
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Bound button combo:"), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorWhitegold | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(selectedOption->GetValueDescription(), static_cast<int>(SpecialMenuEntry::BindPadButton), UiFlags::ColorUiGold));
			assert(IndexKeyOrPadInput == vecDialogItems.size() - 1);
			itemToSelect = IndexKeyOrPadInput;

			vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view(padEntryTimerText), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
			assert(IndexPadTimerText == vecDialogItems.size() - 1);

			vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view {}, static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Unbind button combo"), static_cast<int>(SpecialMenuEntry::UnbindPadButton), UiFlags::ColorUiGold));

			padEntryStartTime = 0;
			eventHandler = [](SDL_Event &event) {
				if (padEntryStartTime == 0)
					return false;

				StaticVector<ControllerButtonEvent, 4> ctrlEvents = ToControllerButtonEvents(event);
				for (ControllerButtonEvent ctrlEvent : ctrlEvents) {
					bool isGamepadMotion = IsControllerMotion(event);
					DetectInputMethod(event, ctrlEvent);
					if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) {
						StopPadEntryTimer();
						return true;
					}
					if (isGamepadMotion || IsAnyOf(ctrlEvent.button, ControllerButton_NONE, ControllerButton_IGNORE)) {
						continue;
					}

					bool modifierPressed = padEntryCombo.modifier != ControllerButton_NONE && IsControllerButtonPressed(padEntryCombo.modifier);
					bool buttonPressed = padEntryCombo.button != ControllerButton_NONE && IsControllerButtonPressed(padEntryCombo.button);
					if (ctrlEvent.up) {
						// When the player has released all relevant inputs, assume the binding is finished and stop the timer
						if (padEntryCombo.button != ControllerButton_NONE && !modifierPressed && !buttonPressed) {
							StopPadEntryTimer();
							return true;
						}
						continue;
					}

					auto *pOptionPad = static_cast<PadmapperOptions::Action *>(selectedOption);
					if (!modifierPressed && buttonPressed)
						padEntryCombo.modifier = padEntryCombo.button;
					padEntryCombo.button = ctrlEvent.button;
					if (pOptionPad->SetValue(padEntryCombo))
						vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription();
				}
				return true;
			};
			UpdateDescription(*selectedOption);
		} break;
		}

		vecDialogItems.push_back(std::make_unique<UiListItem>(std::string_view {}, static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
		vecDialogItems.push_back(std::make_unique<UiListItem>(_("Previous Menu"), static_cast<int>(SpecialMenuEntry::PreviousMenu), UiFlags::ColorUiGold));

		constexpr int ListItemHeight = 26;
		rectList = { uiRectangle.position + Displacement { 50, 204 },
			Size { uiRectangle.size.width - 100, std::min<int>(static_cast<int>(vecDialogItems.size()) * ListItemHeight, uiRectangle.size.height - 272) } };
		rectDescription = { rectList.position + Displacement { -26, rectList.size.height + descriptionMarginTop },
			Size { uiRectangle.size.width - 50, 80 - descriptionMarginTop } };
		vecDialog.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0],
		    *ArtScrollBarArrow, MakeSdlRect(rectList.position.x + rectList.size.width + 5, rectList.position.y, 25, rectList.size.height)));
		vecDialog.push_back(std::make_unique<UiArtText>(optionDescription, MakeSdlRect(rectDescription),
		    UiFlags::FontSize12 | UiFlags::ColorUiSilverDark | UiFlags::AlignCenter, 1, descriptionLineHeight));
		vecDialog.push_back(std::make_unique<UiList>(vecDialogItems, rectList.size.height / ListItemHeight,
		    rectList.position.x, rectList.position.y, rectList.size.width, ListItemHeight, UiFlags::FontSize24 | UiFlags::AlignCenter));

		UiInitList(ItemFocused, ItemSelected, EscPressed, vecDialog, true, FullscreenChanged, nullptr, itemToSelect);

		while (!endMenu) {
			UiClearScreen();
			UpdatePadEntryTimerText();
			UiPollAndRender(eventHandler);
		}

		CleanUpSettingsUI();
	} while (!backToMain);

	SaveOptions();
}

} // namespace devilution
