#include "DiabloUI/text_input.hpp"

#include <memory>
#include <string>

#include <SDL.h>
#include <function_ref.hpp>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#include "utils/log.hpp"
#include "utils/parse_int.hpp"
#include "utils/sdl_ptrs.h"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

bool HandleInputEvent(const SDL_Event &event, TextInputState &state,
    tl::function_ref<bool(string_view)> typeFn,
    [[maybe_unused]] tl::function_ref<bool(string_view)> assignFn)
{
	const auto modState = SDL_GetModState();
	const bool isCtrl = (modState & KMOD_CTRL) != 0;
	const bool isAlt = (modState & KMOD_ALT) != 0;
	const bool isShift = (modState & KMOD_SHIFT) != 0;
	switch (event.type) {
	case SDL_KEYDOWN: {
		switch (event.key.keysym.sym) {
#ifndef USE_SDL1
		case SDLK_a:
			if (isCtrl) {
				state.setCursorToStart();
				state.setSelectCursorToEnd();
			}
			return true;
		case SDLK_c:
			if (isCtrl) {
				const std::string selectedText { state.selectedText() };
				if (SDL_SetClipboardText(selectedText.c_str()) < 0) {
					Log("{}", SDL_GetError());
				}
			}
			return true;
		case SDLK_x:
			if (isCtrl) {
				const std::string selectedText { state.selectedText() };
				if (SDL_SetClipboardText(selectedText.c_str()) < 0) {
					Log("{}", SDL_GetError());
				} else {
					state.eraseSelection();
				}
			}
			return true;
		case SDLK_v:
			if (isCtrl) {
				if (SDL_HasClipboardText() == SDL_TRUE) {
					const std::unique_ptr<char, SDLFreeDeleter<char>> clipboard { SDL_GetClipboardText() };
					if (clipboard == nullptr || *clipboard == '\0') {
						Log("{}", SDL_GetError());
					} else {
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
		    && event.key.keysym.sym >= SDLK_SPACE && event.key.keysym.sym <= SDLK_z;
#endif
	} break;
#ifndef USE_SDL1
	case SDL_TEXTINPUT:
#ifdef __vita__
		assignFn(event.text.text);
#else
		typeFn(event.text.text);
#endif
		return true;
	case SDL_TEXTEDITING:
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
	    event, state, [&](string_view str) {
				state.type(str);
				return true; },
	    [&](string_view str) {
		    state.assign(str);
		    return true;
	    });
}

[[nodiscard]] int NumberInputState::value(int defaultValue) const
{
	return ParseInt<int>(textInput_.value()).value_or(defaultValue);
}

std::string NumberInputState::filterStr(string_view str, bool allowMinus)
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

void NumberInputState::type(string_view str)
{
	const std::string filtered = filterStr(
	    str, /*allowMinus=*/min_ < 0 && textInput_.cursorPosition() == 0);
	if (filtered.empty())
		return;
	textInput_.type(filtered);
	enforceRange();
}

void NumberInputState::assign(string_view str)
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
	    event, state.textInput(), [&](string_view str) {
				state.type(str);
				return true; },
	    [&](string_view str) {
		    state.assign(str);
		    return true;
	    });
}

} // namespace devilution
