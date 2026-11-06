#include "control/control_text_prompt.hpp"

#include <algorithm>
#include <utility>

#include "DiabloUI/text_input.hpp"
#include "control/control.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "utils/display.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

char TextPromptText[256];
TextInputCursorState TextPromptCursor;
std::optional<TextInputState> TextPromptInput;
std::string TextPromptTitle;
std::function<void(std::optional<std::string>)> TextPromptCallback;

/** Panel-relative origin that centres the gold dialog box (drawn at origin + {30, 178}, 261px wide) on screen. */
Point TextPromptOrigin()
{
	return { (GetScreenWidth() - 261) / 2 - 30, GetViewportHeight() / 2 - 120 };
}

void ClosePrompt()
{
	SDLC_StopTextInput(ghMainWnd);
	TextPromptInput = std::nullopt;
}

void FinishPrompt(std::optional<std::string> result)
{
	std::function<void(std::optional<std::string>)> callback = std::move(TextPromptCallback);
	TextPromptCallback = nullptr;
	ClosePrompt();
	if (callback)
		callback(std::move(result));
}

} // namespace

void OpenTextPrompt(std::string_view title, size_t maxLength, std::function<void(std::optional<std::string>)> onFinish)
{
	if (IsTextPromptOpen())
		FinishPrompt(std::nullopt);

	TextPromptTitle = title;
	TextPromptText[0] = '\0';
	TextPromptCallback = std::move(onFinish);
	TextPromptInput.emplace(TextInputState::Options {
	    .value = TextPromptText,
	    .cursor = &TextPromptCursor,
	    .maxLength = std::clamp<size_t>(maxLength, 1, sizeof(TextPromptText) - 1),
	});

	const Point inputPosition = TextPromptOrigin() + Displacement { 67, 128 };
	SDL_Rect rect = MakeSdlRect(inputPosition.x, inputPosition.y, 180, 20);
	SDL_SetTextInputArea(ghMainWnd, &rect, /*cursor=*/0);
	SDLC_StartTextInput(ghMainWnd);
}

bool IsTextPromptOpen()
{
	return TextPromptInput.has_value();
}

void TextPromptKeyPress(SDL_Keycode vkey)
{
	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		FinishPrompt(std::string(TextPromptInput->value()));
		break;
	case SDLK_ESCAPE:
		FinishPrompt(std::nullopt);
		break;
	default:
		break;
	}
}

bool HandleTextPromptTextInputEvent(const SDL_Event &event)
{
	return HandleTextInputEvent(event, *TextPromptInput);
}

void DrawTextPrompt(const Surface &out)
{
	if (!IsTextPromptOpen())
		return;

	const Point origin = TextPromptOrigin();
	ClxDraw(out, origin + Displacement { 30, 178 }, (*GoldBoxBuffer)[0]);

	const std::string wrapped = WordWrapString(TextPromptTitle, 200);
	DrawString(out, wrapped, { origin + Displacement { 61, 75 }, { 200, 50 } },
	    { .flags = UiFlags::ColorWhitegold | UiFlags::AlignCenter, .lineHeight = 17 });

	DrawString(out, TextPromptInput->value(), origin + Displacement { 67, 128 },
	    {
	        .flags = UiFlags::ColorWhite | UiFlags::PentaCursor,
	        .cursorPosition = static_cast<int>(TextPromptCursor.position),
	        .highlightRange = { static_cast<int>(TextPromptCursor.selection.begin), static_cast<int>(TextPromptCursor.selection.end) },
	    });
}

void DiscardTextPrompt()
{
	TextPromptCallback = nullptr;
	if (IsTextPromptOpen())
		ClosePrompt();
}

} // namespace devilution
