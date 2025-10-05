#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "controls/axis_direction.h"
#include "controls/controller.h"

namespace devilution {

enum GameActionType : uint8_t {
	GameActionType_NONE,
	GameActionType_USE_HEALTH_POTION,
	GameActionType_USE_MANA_POTION,
	GameActionType_PRIMARY_ACTION,   // Talk to towners, click on inv items, attack, etc.
	GameActionType_SECONDARY_ACTION, // Open chests, doors, pickup items.
	GameActionType_CAST_SPELL,
	GameActionType_TOGGLE_INVENTORY,
	GameActionType_TOGGLE_CHARACTER_INFO,
	GameActionType_TOGGLE_QUICK_SPELL_MENU,
	GameActionType_TOGGLE_SPELL_BOOK,
	GameActionType_TOGGLE_QUEST_LOG,
	GameActionType_SEND_KEY,
};

struct GameActionSendKey {
	uint32_t vk_code;
	bool up;
};

struct GameAction {
	GameActionType type;

	GameAction()
	    : type(GameActionType_NONE)
	{
	}

	explicit GameAction(GameActionType type)
	    : type(type)
	{
	}

	GameAction(GameActionSendKey send_key)
	    : type(GameActionType_SEND_KEY)
	    , send_key(send_key)
	{
	}

	union {
		GameActionSendKey send_key;
	};
};

ControllerButton TranslateTo(GamepadLayout layout, ControllerButton button);

bool SkipsMovie(ControllerButtonEvent ctrlEvent);

bool IsSimulatedMouseClickBinding(ControllerButtonEvent ctrlEvent);

AxisDirection GetMoveDirection();

bool HandleControllerButtonEvent(const SDL_Event &event, const ControllerButtonEvent ctrlEvent, GameAction &action);

extern bool PadMenuNavigatorActive;
extern bool PadHotspellMenuActive;

// Tracks the button most recently used as a modifier for another button.
//
// If two buttons are pressed simultaneously, SDL sends two events for which both buttons are in the pressed state.
// The event processor may interpret the second event's button as a modifier for the action taken when processing the first event.
// The code for the modifier will be stored here, and the event processor can check this value when processing the second event to suppress it.
extern ControllerButton SuppressedButton;

} // namespace devilution
