#pragma once

#include <optional>
#include <string_view>

#include "DiabloUI/text_input.hpp"
#include "engine/clx_sprite.hpp"
#include "msg.h"
#include "multi.h"

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_rect.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

namespace devilution {

extern OptionalOwnedClxSpriteList talkButtons;
extern std::optional<TextInputState> ChatInputState;
extern char TalkMessage[MAX_SEND_STR_LEN];
extern bool TalkButtonsDown[3];
extern int sgbPlrTalkTbl;
extern bool WhisperList[MAX_PLRS];

bool CheckChatCommand(std::string_view text);

template <typename InputStateType>
bool HandleInputEvent(const SDL_Event &event, std::optional<InputStateType> &inputState)
{
	if (!inputState) {
		return false; // No input state to handle
	}

	if constexpr (std::is_same_v<InputStateType, TextInputState>) {
		return HandleTextInputEvent(event, *inputState);
	} else if constexpr (std::is_same_v<InputStateType, NumberInputState>) {
		return HandleNumberInputEvent(event, *inputState);
	}

	return false; // Unknown input state type
}

} // namespace devilution
