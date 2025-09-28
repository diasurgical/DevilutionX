#include "controls/menu_controls.h"

#include "DiabloUI/diabloui.h"
#include "controls/axis_direction.h"
#include "controls/controller.h"
#include "controls/controller_motion.h"
#include "controls/plrctrls.h"
#include "controls/remap_keyboard.h"
#include "utils/sdl_compat.h"

namespace devilution {

MenuAction GetMenuHeldUpDownAction()
{
	static AxisDirectionRepeater repeater;
	const AxisDirection dir = repeater.Get(GetLeftStickOrDpadDirection(false));
	switch (dir.y) {
	case AxisDirectionY_UP:
		return MenuAction_UP;
	case AxisDirectionY_DOWN:
		return MenuAction_DOWN;
	default:
		return MenuAction_NONE;
	}
}

std::vector<MenuAction> GetMenuActions(const SDL_Event &event)
{
	std::vector<MenuAction> menuActions;
	for (const ControllerButtonEvent ctrlEvent : ToControllerButtonEvents(event)) {
		if (ctrlEvent.button == ControllerButton_IGNORE) {
			continue;
		}

		bool isGamepadMotion = IsControllerMotion(event);
		DetectInputMethod(event, ctrlEvent);
		if (isGamepadMotion) {
			menuActions.push_back(GetMenuHeldUpDownAction());
			continue;
		}

		if (!ctrlEvent.up) {
			switch (TranslateTo(GamepadType, ctrlEvent.button)) {
			case ControllerButton_BUTTON_A:
			case ControllerButton_BUTTON_START:
				menuActions.push_back(MenuAction_SELECT);
				break;
			case ControllerButton_BUTTON_BACK:
			case ControllerButton_BUTTON_B:
				menuActions.push_back(MenuAction_BACK);
				break;
			case ControllerButton_BUTTON_X:
				menuActions.push_back(MenuAction_DELETE);
				break;
			case ControllerButton_BUTTON_DPAD_UP:
			case ControllerButton_BUTTON_DPAD_DOWN:
				menuActions.push_back(GetMenuHeldUpDownAction());
				break;
			case ControllerButton_BUTTON_DPAD_LEFT:
				menuActions.push_back(MenuAction_LEFT);
				break;
			case ControllerButton_BUTTON_DPAD_RIGHT:
				menuActions.push_back(MenuAction_RIGHT);
				break;
			case ControllerButton_BUTTON_LEFTSHOULDER:
				menuActions.push_back(MenuAction_PAGE_UP);
				break;
			case ControllerButton_BUTTON_RIGHTSHOULDER:
				menuActions.push_back(MenuAction_PAGE_DOWN);
				break;
			default:
				break;
			}
		}
	}
	if (!menuActions.empty()) {
		return menuActions;
	}

	if (event.type == SDL_MOUSEBUTTONDOWN) {
		switch (event.button.button) {
		case SDL_BUTTON_X1:
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		case 8:
#endif
			return { MenuAction_BACK };
		}
	}

#if HAS_KBCTRL == 0
	if (event.type == SDL_KEYDOWN) {
		SDL_Keycode sym = event.key.keysym.sym;
		remap_keyboard_key(&sym);
		switch (sym) {
		case SDLK_UP:
			return { MenuAction_UP };
		case SDLK_DOWN:
			return { MenuAction_DOWN };
		case SDLK_TAB:
			if ((SDL_GetModState() & KMOD_SHIFT) != 0)
				return { MenuAction_UP };
			else
				return { MenuAction_DOWN };
		case SDLK_PAGEUP:
			return { MenuAction_PAGE_UP };
		case SDLK_PAGEDOWN:
			return { MenuAction_PAGE_DOWN };
		case SDLK_RETURN:
			if ((SDL_GetModState() & KMOD_ALT) == 0) {
				return { MenuAction_SELECT };
			}
			break;
		case SDLK_KP_ENTER:
			return { MenuAction_SELECT };
		case SDLK_SPACE:
			if (!IsTextInputActive()) {
				return { MenuAction_SELECT };
			}
			break;
		case SDLK_DELETE:
			if (!IsTextInputActive()) {
				return { MenuAction_DELETE };
			}
			break;
		case SDLK_LEFT:
			if (!IsTextInputActive()) {
				return { MenuAction_LEFT };
			}
			break;
		case SDLK_RIGHT:
			if (!IsTextInputActive()) {
				return { MenuAction_RIGHT };
			}
			break;
		case SDLK_ESCAPE:
			return { MenuAction_BACK };
		default:
			break;
		}
	}
#endif

	return {};
} // namespace devilution

} // namespace devilution
