#include "DiabloUI/text_input.hpp"

#include <memory>
#include <string>
#include <string_view>

#include <function_ref.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "utils/log.hpp"
#include "utils/parse_int.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_ptrs.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

bool HandleInputEvent(const SDL_Event &event, TextInputState &state,
    tl::function_ref<bool(std::string_view)> typeFn,
    [[maybe_unused]] tl::function_ref<bool(std::string_view)> assignFn)
{
	const auto modState = SDL_GetModState();
	const bool isCtrl = (modState & SDL_KMOD_CTRL) != 0;
	const bool isAlt = (modState & SDL_KMOD_ALT) != 0;
	const bool isShift = (modState & SDL_KMOD_SHIFT) != 0;
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN: {
		switch (SDLC_EventKey(event)) {
#ifndef USE_SDL1
		case SDLK_A:
			if (isCtrl) {
				state.setCursorToStart();
				state.setSelectCursorToEnd();
			}
			return true;
		case SDLK_C:
			if (isCtrl) {
				const std::string selectedText { state.selectedText() };
				if (SDLC_SetClipboardText(selectedText.c_str())) {
					LogError("Failed to set clipboard text: {}", SDL_GetError());
					SDL_ClearError();
				}
			}
			return true;
		case SDLK_X:
			if (isCtrl) {
				const std::string selectedText { state.selectedText() };
				if (SDLC_SetClipboardText(selectedText.c_str())) {
					LogError("Failed to set clipboard text: {}", SDL_GetError());
					SDL_ClearError();
				} else {
					state.eraseSelection();
				}
			}
			return true;
		case SDLK_V:
			if (isCtrl) {
				if (SDLC_HasClipboardText()) {
					const std::unique_ptr<char, SDLFreeDeleter<char>> clipboard { SDL_GetClipboardText() };
					if (clipboard == nullptr) {
						LogError("Failed to get clipboard text: {}", SDL_GetError());
						SDL_ClearError();
					} else if (*clipboard != '\0') {
						typeFn(clipboard.get());
					}
				}
			}
			return true;
#endif
		case SDLK_BACKSPACE:
			state.backspace(/*word=*/isCtrl || isAlt);
			return true;
		case SDLK_DELETE:
			state.del(/*word=*/isCtrl || isAlt);
			return true;
		case SDLK_LEFT:
			isShift ? state.moveSelectCursorLeft(/*word=*/isCtrl || isAlt) : state.moveCursorLeft(/*word=*/isCtrl || isAlt);
			return true;
		case SDLK_RIGHT:
			isShift ? state.moveSelectCursorRight(/*word=*/isCtrl || isAlt) : state.moveCursorRight(/*word=*/isCtrl || isAlt);
			return true;
		case SDLK_HOME:
			isShift ? state.setSelectCursorToStart() : state.setCursorToStart();
			return true;
		case SDLK_END:
			isShift ? state.setSelectCursorToEnd() : state.setCursorToEnd();
			return true;
		default:
			break;
		}
#ifdef USE_SDL1
		if ((event.key.keysym.mod & KMOD_CTRL) == 0 && event.key.keysym.unicode >= ' ') {
			std::string utf8;
			AppendUtf8(event.key.keysym.unicode, utf8);
			typeFn(utf8);
			return true;
		}
#else
		// Mark events that will also trigger SDL_TEXTINPUT as handled.
		return !isCtrl && !isAlt
		    && SDLC_EventKey(event) >= SDLK_SPACE && SDLC_EventKey(event) <= SDLK_Z;
#endif
	} break;
#ifndef USE_SDL1
	case SDL_EVENT_TEXT_INPUT:
#ifdef __vita__
		assignFn(event.text.text);
#else
		typeFn(event.text.text);
#endif
		return true;
#ifdef USE_SDL3
	case SDL_EVENT_TEXT_EDITING:
	case SDL_EVENT_TEXT_EDITING_CANDIDATES:
#else
	case SDL_TEXTEDITING:
#endif
		return true;
#endif
	default:
		return false;
	}
	return false;
}

} // namespace

bool HandleTextInputEvent(const SDL_Event &event, TextInputState &state)
{
	return HandleInputEvent(
	    event, state, [&](std::string_view str) {
				state.type(str);
				return true; },
	    [&](std::string_view str) {
		    state.assign(str);
		    return true;
	    });
}

[[nodiscard]] int NumberInputState::value(int defaultValue) const
{
	return ParseInt<int>(textInput_.value()).value_or(defaultValue);
}

std::string NumberInputState::filterStr(std::string_view str, bool allowMinus)
{
	std::string result;
	if (allowMinus && !str.empty() && str[0] == '-') {
		str.remove_prefix(1);
		result += '-';
	}
	for (const char c : str) {
		if (c >= '0' && c <= '9') {
			result += c;
		}
	}
	return result;
}

void NumberInputState::type(std::string_view str)
{
	const std::string filtered = filterStr(
	    str, /*allowMinus=*/min_ < 0 && textInput_.cursorPosition() == 0);
	if (filtered.empty())
		return;
	textInput_.type(filtered);
	enforceRange();
}

void NumberInputState::assign(std::string_view str)
{
	const std::string filtered = filterStr(str, /*allowMinus=*/min_ < 0);
	if (filtered.empty()) {
		textInput_.clear();
		return;
	}
	textInput_.assign(filtered);
	enforceRange();
}

void NumberInputState::enforceRange()
{
	if (textInput_.empty())
		return;
	ParseIntResult<int> parsed = ParseInt<int>(textInput_.value());
	if (parsed.has_value()) {
		if (*parsed > max_) {
			textInput_.assign(StrCat(max_));
		} else if (*parsed < min_) {
			textInput_.assign(StrCat(min_));
		}
	}
}

bool HandleNumberInputEvent(const SDL_Event &event, NumberInputState &state)
{
	return HandleInputEvent(
	    event, state.textInput(), [&](std::string_view str) {
				state.type(str);
				return true; },
	    [&](std::string_view str) {
		    state.assign(str);
		    return true;
	    });
}

} // namespace devilution
