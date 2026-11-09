/**
 * @file control/control_text_prompt.hpp
 *
 * A small modal text-entry box (the gold drop dialog's art and layout, centred on screen) that hands the entered
 * text to a callback. Used by the Lua API to ask the player for a line of text.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>
#endif

#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Opens the prompt (cancelling any open one). `onFinish` receives the text on Enter, or nullopt on Escape.
 * @param maxLength Maximum length of the entered text in bytes (capped internally).
 */
void OpenTextPrompt(std::string_view title, size_t maxLength, std::function<void(std::optional<std::string>)> onFinish);
bool IsTextPromptOpen();
void TextPromptKeyPress(SDL_Keycode vkey);
bool HandleTextPromptTextInputEvent(const SDL_Event &event);
void DrawTextPrompt(const Surface &out);
/** @brief Closes the prompt without invoking its callback (e.g. when the callback's owner goes away). */
void DiscardTextPrompt();

} // namespace devilution
