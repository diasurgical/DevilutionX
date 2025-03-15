#include "controls/game_controls.h"

#include <cstdint>

#include "controls/control_mode.hpp"
#include "controls/controller_motion.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "controls/padmapper.hpp"
#include "controls/plrctrls.h"
#include "controls/touch/gamepad.h"
#include "doom.h"
#include "gamemenu.h"
#include "gmenu.h"
#include "options.h"
#include "panels/spell_list.hpp"
#include "qol/stash.h"
#include "stores.h"
#include "utils/is_of.hpp"

namespace devilution {

bool PadMenuNavigatorActive = false;
bool PadHotspellMenuActive = false;
ControllerButton SuppressedButton = ControllerButton_NONE;

namespace {

SDL_Keycode TranslateControllerButtonToGameMenuKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_B:
	case ControllerButton_BUTTON_BACK:
	case ControllerButton_BUTTON_START:
		return SDLK_ESCAPE;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	default:
		return SDLK_UNKNOWN;
	}
}

SDL_Keycode TranslateControllerButtonToMenuKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
		return SDLK_SPACE;
	case ControllerButton_BUTTON_B:
	case ControllerButton_BUTTON_BACK:
	case ControllerButton_BUTTON_START:
		return SDLK_ESCAPE;
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	case ControllerButton_BUTTON_DPAD_LEFT:
		return SDLK_LEFT;
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return SDLK_RIGHT;
	case ControllerButton_BUTTON_DPAD_UP:
		return SDLK_UP;
	case ControllerButton_BUTTON_DPAD_DOWN:
		return SDLK_DOWN;
	default:
		return SDLK_UNKNOWN;
	}
}

SDL_Keycode TranslateControllerButtonToQuestLogKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_B:
		return SDLK_SPACE;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	default:
		return SDLK_UNKNOWN;
	}
}

bool GetGameAction(const SDL_Event &event, ControllerButtonEvent ctrlEvent, GameAction *action)
{
	const bool inGameMenu = InGameMenu();

#ifndef USE_SDL1
	if (ControlMode == ControlTypes::VirtualGamepad) {
		if (event.type == SDL_FINGERDOWN) {
			if (VirtualGamepadState.menuPanel.charButton.isHeld && VirtualGamepadState.menuPanel.charButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
				return true;
			}
			if (VirtualGamepadState.menuPanel.questsButton.isHeld && VirtualGamepadState.menuPanel.questsButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
				return true;
			}
			if (VirtualGamepadState.menuPanel.inventoryButton.isHeld && VirtualGamepadState.menuPanel.inventoryButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_INVENTORY);
				return true;
			}
			if (VirtualGamepadState.menuPanel.mapButton.isHeld && VirtualGamepadState.menuPanel.mapButton.didStateChange) {
				*action = GameActionSendKey { SDLK_TAB, false };
				return true;
			}
			if (VirtualGamepadState.primaryActionButton.isHeld && VirtualGamepadState.primaryActionButton.didStateChange) {
				if (!inGameMenu && !QuestLogIsOpen && !SpellbookFlag) {
					*action = GameAction(GameActionType_PRIMARY_ACTION);
					if (ControllerActionHeld == GameActionType_NONE) {
						ControllerActionHeld = GameActionType_PRIMARY_ACTION;
					}
				} else if (sgpCurrentMenu != nullptr || IsPlayerInStore() || QuestLogIsOpen) {
					*action = GameActionSendKey { SDLK_RETURN, false };
				} else {
					*action = GameActionSendKey { SDLK_SPACE, false };
				}
				return true;
			}
			if (VirtualGamepadState.secondaryActionButton.isHeld && VirtualGamepadState.secondaryActionButton.didStateChange) {
				if (!inGameMenu && !QuestLogIsOpen && !SpellbookFlag) {
					*action = GameAction(GameActionType_SECONDARY_ACTION);
					if (ControllerActionHeld == GameActionType_NONE)
						ControllerActionHeld = GameActionType_SECONDARY_ACTION;
				}
				return true;
			}
			if (VirtualGamepadState.spellActionButton.isHeld && VirtualGamepadState.spellActionButton.didStateChange) {
				if (!inGameMenu && !QuestLogIsOpen && !SpellbookFlag) {
					*action = GameAction(GameActionType_CAST_SPELL);
					if (ControllerActionHeld == GameActionType_NONE)
						ControllerActionHeld = GameActionType_CAST_SPELL;
				}
				return true;
			}
			if (VirtualGamepadState.cancelButton.isHeld && VirtualGamepadState.cancelButton.didStateChange) {
				if (inGameMenu || DoomFlag || SpellSelectFlag)
					*action = GameActionSendKey { SDLK_ESCAPE, false };
				else if (invflag)
					*action = GameAction(GameActionType_TOGGLE_INVENTORY);
				else if (SpellbookFlag)
					*action = GameAction(GameActionType_TOGGLE_SPELL_BOOK);
				else if (QuestLogIsOpen)
					*action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
				else if (CharFlag)
					*action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
				return true;
			}
			if (VirtualGamepadState.healthButton.isHeld && VirtualGamepadState.healthButton.didStateChange) {
				if (!QuestLogIsOpen && !SpellbookFlag && !IsPlayerInStore())
					*action = GameAction(GameActionType_USE_HEALTH_POTION);
				return true;
			}
			if (VirtualGamepadState.manaButton.isHeld && VirtualGamepadState.manaButton.didStateChange) {
				if (!QuestLogIsOpen && !SpellbookFlag && !IsPlayerInStore())
					*action = GameAction(GameActionType_USE_MANA_POTION);
				return true;
			}
		} else if (event.type == SDL_FINGERUP) {
			if ((!VirtualGamepadState.primaryActionButton.isHeld && ControllerActionHeld == GameActionType_PRIMARY_ACTION)
			    || (!VirtualGamepadState.secondaryActionButton.isHeld && ControllerActionHeld == GameActionType_SECONDARY_ACTION)
			    || (!VirtualGamepadState.spellActionButton.isHeld && ControllerActionHeld == GameActionType_CAST_SPELL)) {
				ControllerActionHeld = GameActionType_NONE;
				LastMouseButtonAction = MouseActionType::None;
			}
		}
	}
#endif

	if (PadMenuNavigatorActive || PadHotspellMenuActive)
		return false;

	SDL_Keycode translation = SDLK_UNKNOWN;

	if (gmenu_is_active() || IsPlayerInStore())
		translation = TranslateControllerButtonToGameMenuKey(ctrlEvent.button);
	else if (inGameMenu)
		translation = TranslateControllerButtonToMenuKey(ctrlEvent.button);
	else if (QuestLogIsOpen)
		translation = TranslateControllerButtonToQuestLogKey(ctrlEvent.button);

	if (translation != SDLK_UNKNOWN) {
		*action = GameActionSendKey { static_cast<uint32_t>(translation), ctrlEvent.up };
		return true;
	}

	return false;
}

bool CanDeferToMovementHandler(const PadmapperOptions::Action &action)
{
	if (action.boundInput.modifier != ControllerButton_NONE)
		return false;

	if (SpellSelectFlag) {
		const std::string_view prefix { "QuickSpell" };
		const std::string_view key { action.key };
		if (key.size() >= prefix.size()) {
			const std::string_view truncated { key.data(), prefix.size() };
			if (truncated == prefix)
				return false;
		}
	}

	return IsAnyOf(action.boundInput.button,
	    ControllerButton_BUTTON_DPAD_UP,
	    ControllerButton_BUTTON_DPAD_DOWN,
	    ControllerButton_BUTTON_DPAD_LEFT,
	    ControllerButton_BUTTON_DPAD_RIGHT);
}

void PressControllerButton(ControllerButton button)
{
	if (IsStashOpen) {
		switch (button) {
		case ControllerButton_BUTTON_BACK:
			StartGoldWithdraw();
			return;
		case ControllerButton_BUTTON_LEFTSHOULDER:
			Stash.PreviousPage();
			return;
		case ControllerButton_BUTTON_RIGHTSHOULDER:
			Stash.NextPage();
			return;
		default:
			break;
		}
	}

	if (PadHotspellMenuActive) {
		auto quickSpellAction = [](size_t slot) {
			if (SpellSelectFlag) {
				SetSpeedSpell(slot);
				return;
			}
			if (!*GetOptions().Gameplay.quickCast)
				ToggleSpell(slot);
			else
				QuickCast(slot);
		};
		switch (button) {
		case devilution::ControllerButton_BUTTON_A:
			quickSpellAction(2);
			return;
		case devilution::ControllerButton_BUTTON_B:
			quickSpellAction(3);
			return;
		case devilution::ControllerButton_BUTTON_X:
			quickSpellAction(0);
			return;
		case devilution::ControllerButton_BUTTON_Y:
			quickSpellAction(1);
			return;
		default:
			break;
		}
	}

	if (PadMenuNavigatorActive) {
		switch (button) {
		case devilution::ControllerButton_BUTTON_DPAD_UP:
			PressEscKey();
			LastMouseButtonAction = MouseActionType::None;
			PadHotspellMenuActive = false;
			PadMenuNavigatorActive = false;
			gamemenu_on();
			return;
		case devilution::ControllerButton_BUTTON_DPAD_DOWN:
			CycleAutomapType();
			return;
		case devilution::ControllerButton_BUTTON_DPAD_LEFT:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_CHARACTER_INFO });
			return;
		case devilution::ControllerButton_BUTTON_DPAD_RIGHT:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_INVENTORY });
			return;
		case devilution::ControllerButton_BUTTON_A:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_SPELL_BOOK });
			return;
		case devilution::ControllerButton_BUTTON_B:
			return;
		case devilution::ControllerButton_BUTTON_X:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_QUEST_LOG });
			return;
		case devilution::ControllerButton_BUTTON_Y:
#ifdef __3DS__
			GetOptions().Graphics.zoom.SetValue(!*GetOptions().Graphics.zoom);
			CalcViewportGeometry();
#endif
			return;
		default:
			break;
		}
	}

	const PadmapperOptions::Action *action = GetOptions().Padmapper.findAction(button, IsControllerButtonPressed);
	if (action == nullptr) return;
	if (IsMovementHandlerActive() && CanDeferToMovementHandler(*action)) return;
	PadmapperPress(button, *action);
}

} // namespace

ControllerButton TranslateTo(GamepadLayout layout, ControllerButton button)
{
	if (layout != GamepadLayout::Nintendo)
		return button;

	switch (button) {
	case ControllerButton_BUTTON_A:
		return ControllerButton_BUTTON_B;
	case ControllerButton_BUTTON_B:
		return ControllerButton_BUTTON_A;
	case ControllerButton_BUTTON_X:
		return ControllerButton_BUTTON_Y;
	case ControllerButton_BUTTON_Y:
		return ControllerButton_BUTTON_X;
	default:
		return button;
	}
}

bool SkipsMovie(ControllerButtonEvent ctrlEvent)
{
	return IsAnyOf(ctrlEvent.button,
	    ControllerButton_BUTTON_A,
	    ControllerButton_BUTTON_B,
	    ControllerButton_BUTTON_START,
	    ControllerButton_BUTTON_BACK);
}

bool IsSimulatedMouseClickBinding(ControllerButtonEvent ctrlEvent)
{
	if (ctrlEvent.button == ControllerButton_NONE)
		return false;
	if (!ctrlEvent.up && ctrlEvent.button == SuppressedButton)
		return false;
	const std::string_view actionName = PadmapperActionNameTriggeredByButtonEvent(ctrlEvent);
	return IsAnyOf(actionName, "LeftMouseClick1", "LeftMouseClick2", "RightMouseClick1", "RightMouseClick2");
}

AxisDirection GetMoveDirection()
{
	return GetLeftStickOrDpadDirection(true);
}

bool HandleControllerButtonEvent(const SDL_Event &event, const ControllerButtonEvent ctrlEvent, GameAction &action)
{
	if (ctrlEvent.button == ControllerButton_IGNORE) {
		return false;
	}

	struct ButtonReleaser {
		~ButtonReleaser()
		{
			if (ctrlEvent.up) PadmapperRelease(ctrlEvent.button, /*invokeAction=*/false);
		}
		ControllerButtonEvent ctrlEvent;
	};

	const ButtonReleaser buttonReleaser { ctrlEvent };
	bool isGamepadMotion = IsControllerMotion(event);
	if (!isGamepadMotion) {
		SimulateRightStickWithPadmapper(ctrlEvent);
	}
	DetectInputMethod(event, ctrlEvent);
	if (isGamepadMotion) {
		return true;
	}

	if (ctrlEvent.button != ControllerButton_NONE && ctrlEvent.button == SuppressedButton) {
		if (!ctrlEvent.up)
			return true;
		SuppressedButton = ControllerButton_NONE;
	}

	if (ctrlEvent.up && !PadmapperActionNameTriggeredByButtonEvent(ctrlEvent).empty()) {
		// Button press may have brought up a menu;
		// don't confuse release of that button with intent to interact with the menu
		PadmapperRelease(ctrlEvent.button, /*invokeAction=*/true);
		return true;
	} else if (GetGameAction(event, ctrlEvent, &action)) {
		ProcessGameAction(action);
		return true;
	} else if (ctrlEvent.button != ControllerButton_NONE) {
		if (!ctrlEvent.up)
			PressControllerButton(ctrlEvent.button);
		return true;
	}

	return false;
}

} // namespace devilution
