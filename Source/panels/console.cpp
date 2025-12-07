#ifdef _DEBUG
#include "panels/console.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include <function_ref.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_scancode.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "DiabloUI/text_input.hpp"
#include "control.h"
#include "engine/assets.hpp"
#include "engine/displacement.hpp"
#include "engine/dx.h"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "lua/autocomplete.hpp"
#include "lua/repl.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/display.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

constexpr std::string_view Prompt = "> ";
constexpr std::string_view HelpText =
    // Displayed as the first console message
    "Lua console\n"
    "Shift+Enter to insert a newline, PageUp/Down to scroll,"
    " Up/Down to fill the input from history,"
    " Shift+Up/Down to fill the input from output history,"
    " Ctrl+L to clear history, Esc to close.";
std::optional<tl::expected<AssetData, std::string>> ConsolePrelude;

bool IsConsoleVisible;
char ConsoleInputBuffer[4096];
TextInputCursorState ConsoleInputCursor;
TextInputState ConsoleInputState {
	TextInputState::Options {
	    .value = ConsoleInputBuffer,
	    .cursor = &ConsoleInputCursor,
	    .maxLength = sizeof(ConsoleInputBuffer) - 1,
	}
};

enum class InputTextState {
	UpToDate,
	Edited,
	RestoredFromHistory
};

InputTextState CurrentInputTextState = InputTextState::UpToDate;
std::string WrappedInputText { Prompt };
std::vector<LuaAutocompleteSuggestion> AutocompleteSuggestions;
int AutocompleteSuggestionsMaxWidth = -1;
int AutocompleteSuggestionFocusIndex = -1;
constexpr size_t MaxSuggestions = 12;

struct ConsoleLine {
	enum Type : uint8_t {
		Help,
		Input,
		Output,
		Warning,
		Error
	};

	Type type;
	std::string text;
	std::string wrapped = {};
	int numLines = 0;

	[[nodiscard]] std::string_view textWithoutPrompt() const
	{
		std::string_view result = text;
		if (type == ConsoleLine::Input) {
			result.remove_prefix(Prompt.size());
		}
		return result;
	}
};

std::vector<ConsoleLine> ConsoleLines;
size_t NumPreparedConsoleLines;
int ConsoleLinesTotalHeight;

// Index of the currently filled input/output, counting from end.
int HistoryIndex = -1;

// Draft input, saved when navigating history.
std::string DraftInput;

Rectangle OuterRect;
Rectangle InputRect;
int InputRectHeight;
constexpr int LineHeight = 20;
constexpr int TextPaddingYTop = 0;
constexpr int TextPaddingYBottom = 4;
constexpr int TextPaddingX = 4;
constexpr uint8_t BorderColor = PAL8_YELLOW;
bool FirstRender;

constexpr UiFlags TextUiFlags = UiFlags::FontSizeDialog;
constexpr UiFlags InputTextUiFlags = TextUiFlags | UiFlags::ColorDialogWhite;
constexpr UiFlags OutputTextUiFlags = TextUiFlags | UiFlags::ColorDialogWhite;
constexpr UiFlags WarningTextUiFlags = TextUiFlags | UiFlags::ColorDialogYellow;
constexpr UiFlags ErrorTextUiFlags = TextUiFlags | UiFlags::ColorDialogRed;
constexpr UiFlags AutocompleteSuggestionsTextUiFlags = TextUiFlags | UiFlags::ColorDialogWhite;
constexpr UiFlags AutocompleteSuggestionsFocusedTextUiFlags = TextUiFlags | UiFlags::ColorDialogYellow;

constexpr int TextSpacing = 0;
constexpr GameFontTables TextFontSize = GetFontSizeFromUiFlags(InputTextUiFlags);
constexpr GameFontTables AutocompleteSuggestionsTextFontSize = GetFontSizeFromUiFlags(AutocompleteSuggestionsTextUiFlags);

// Scroll offset from the bottom (in pages), to be applied on next render.
int PendingScrollPages;
// Scroll offset from the bottom in pixels.
int ScrollOffset;
constexpr int ScrollStep = LineHeight * 3;

void CloseConsole()
{
	IsConsoleVisible = false;
	SDLC_StopTextInput(ghMainWnd);
}

int GetConsoleLinesInnerWidth()
{
	return OuterRect.size.width - 2 * TextPaddingX;
}

void PrepareForRender(ConsoleLine &consoleLine)
{
	consoleLine.wrapped = WordWrapString(consoleLine.text, GetConsoleLinesInnerWidth(), TextFontSize, TextSpacing);
	consoleLine.numLines += static_cast<int>(c_count(consoleLine.wrapped, '\n')) + 1;
	ConsoleLinesTotalHeight += consoleLine.numLines * LineHeight;
}

void AddConsoleLine(ConsoleLine &&consoleLine)
{
	ConsoleLines.emplace_back(std::move(consoleLine));
}

void SendInput()
{
	RunInConsole(ConsoleInputState.value());
	ConsoleInputState.clear();
	DraftInput.clear();
	HistoryIndex = -1;
}

void DrawAutocompleteSuggestions(const Surface &out, const std::vector<LuaAutocompleteSuggestion> &suggestions, Point position)
{
	const int maxInnerWidth = out.w() - TextPaddingX * 2;
	if (AutocompleteSuggestionsMaxWidth == -1) {
		int maxWidth = 0;
		for (const LuaAutocompleteSuggestion &suggestion : suggestions) {
			maxWidth = std::max(maxWidth, GetLineWidth(suggestion.displayText, AutocompleteSuggestionsTextFontSize, TextSpacing));
		}
		AutocompleteSuggestionsMaxWidth = std::min(maxWidth, maxInnerWidth);
	}

	const int outerWidth = AutocompleteSuggestionsMaxWidth + TextPaddingX * 2;

	if (position.x + outerWidth > out.w()) {
		position.x = out.w() - outerWidth;
	}
	const int height = static_cast<int>(suggestions.size()) * LineHeight + TextPaddingYBottom + TextPaddingYTop;

	position.y -= height;
	position.y = std::max(LineHeight, position.y);

	FillRect(out, position.x, position.y, outerWidth, height, PAL16_BLUE + 14);
	size_t i = 0;

	Point textPosition { position.x + TextPaddingX, position.y + TextPaddingYTop };
	for (const LuaAutocompleteSuggestion &suggestion : suggestions) {
		if (static_cast<int>(i) == AutocompleteSuggestionFocusIndex) {
			const int extraTop = i == 0 ? TextPaddingYTop : 0;
			const int extraHeight = extraTop + TextPaddingYBottom;
			FillRect(out, position.x, textPosition.y - extraTop, outerWidth, LineHeight + extraHeight, PAL16_BLUE + 8);
		}
		const int textHeight = LineHeight + TextPaddingYBottom;
		DrawString(
		    out.subregion(textPosition.x, textPosition.y, maxInnerWidth, textHeight), suggestion.displayText,
		    Rectangle { Point { 0, 0 }, Size { maxInnerWidth, textHeight } },
		    TextRenderOptions {
		        .flags = AutocompleteSuggestionsTextUiFlags,
		        .spacing = TextSpacing,
		    });
		textPosition.y += LineHeight;
		++i;
	}
}

bool IsBreakStart(std::string_view str, size_t &breakLen)
{
	const char32_t cp = DecodeFirstUtf8CodePoint(str, &breakLen);
	return cp == U'\n' || IsBreakableWhitespace(cp);
}

void DrawInputText(const Surface &out,
    Rectangle rect, std::string_view originalInputText, std::string_view wrappedInputText)
{
	int lineY = 0;
	int numRendered = -static_cast<int>(Prompt.size());
	bool prevIsOriginalWhitespace = false;

	const Surface inputTextSurface = out.subregion(rect.position.x, rect.position.y, rect.size.width, rect.size.height);
	std::optional<Point> renderedCursorPositionOut;
	for (const std::string_view line : SplitByChar(wrappedInputText, '\n')) {
		const int lineCursorPosition = static_cast<int>(ConsoleInputCursor.position) - numRendered;
		const bool isCursorOnPrevLine = lineCursorPosition == 0 && !prevIsOriginalWhitespace && numRendered > 0;
		DrawString(
		    inputTextSurface, line, { 0, lineY },
		    TextRenderOptions {
		        .flags = InputTextUiFlags,
		        .spacing = TextSpacing,
		        .cursorPosition = isCursorOnPrevLine ? -1 : lineCursorPosition,
		        .highlightRange = { static_cast<int>(ConsoleInputCursor.selection.begin) - numRendered, static_cast<int>(ConsoleInputCursor.selection.end) - numRendered },
		        .renderedCursorPositionOut = &renderedCursorPositionOut });
		lineY += LineHeight;
		numRendered += static_cast<int>(line.size());

		size_t whitespaceLength;
		prevIsOriginalWhitespace = static_cast<size_t>(numRendered) < originalInputText.size()
		    && IsBreakStart(originalInputText.substr(static_cast<size_t>(numRendered)), whitespaceLength);
		if (prevIsOriginalWhitespace) {
			// If we replaced an original whitespace with a newline, count the original whitespace as rendered.
			numRendered += static_cast<int>(whitespaceLength);
		}
		if (numRendered < 0 && IsBreakStart(Prompt.substr(Prompt.size() - static_cast<size_t>(-numRendered)), whitespaceLength)) {
			// If we replaced the whitespace in a prompt with a newline, count it as rendered.
			numRendered += static_cast<int>(whitespaceLength);
		}
	}

	if (!AutocompleteSuggestions.empty() && renderedCursorPositionOut.has_value()) {
		Point position = *renderedCursorPositionOut;
		position.x += rect.position.x;
		position.y += rect.position.y;
		DrawAutocompleteSuggestions(out, AutocompleteSuggestions, position);
	}
}

void DrawConsoleLines(const Surface &out)
{
	const int innerHeight = out.h() - 4; // Extra space for letters like g.
	if (PendingScrollPages) {
		ScrollOffset += innerHeight * PendingScrollPages;
		PendingScrollPages = 0;
	}

	if (NumPreparedConsoleLines != ConsoleLines.size()) {
		for (size_t i = NumPreparedConsoleLines; i < ConsoleLines.size(); ++i) {
			PrepareForRender(ConsoleLines[i]);
		}
		NumPreparedConsoleLines = ConsoleLines.size();
		ScrollOffset = 0;
	}

	ScrollOffset = std::clamp(ScrollOffset, 0, std::max(0, ConsoleLinesTotalHeight - innerHeight));

	int lineYEnd = innerHeight + ScrollOffset;
	// NOLINTNEXTLINE(modernize-loop-convert)
	for (auto it = ConsoleLines.rbegin(), itEnd = ConsoleLines.rend(); it != itEnd; ++it) {
		ConsoleLine &consoleLine = *it;
		const int linesYBegin = lineYEnd - LineHeight * consoleLine.numLines;
		if (linesYBegin > innerHeight) {
			lineYEnd = linesYBegin;
			continue;
		}
		size_t end = consoleLine.wrapped.size();
		while (true) {
			const size_t begin = consoleLine.wrapped.rfind('\n', end - 1) + 1;
			const std::string_view line = std::string_view(consoleLine.wrapped.data() + begin, end - begin);
			lineYEnd -= LineHeight;
			switch (consoleLine.type) {
			case ConsoleLine::Input:
				DrawString(out, line, { 0, lineYEnd },
				    TextRenderOptions { .flags = InputTextUiFlags, .spacing = TextSpacing });
				break;
			case ConsoleLine::Output:
			case ConsoleLine::Help:
				DrawString(out, line, { 0, lineYEnd },
				    TextRenderOptions { .flags = OutputTextUiFlags, .spacing = TextSpacing });
				break;
			case ConsoleLine::Warning:
				DrawString(out, line, { 0, lineYEnd },
				    TextRenderOptions { .flags = WarningTextUiFlags, .spacing = TextSpacing });
				break;
			case ConsoleLine::Error:
				DrawString(out, line, { 0, lineYEnd },
				    TextRenderOptions { .flags = ErrorTextUiFlags, .spacing = TextSpacing });
				break;
			}
			if (lineYEnd < 0 || begin == 0)
				break;
			end = begin - 1;
		}
	}
}

const ConsoleLine &GetConsoleLineFromEnd(int index)
{
	return *(ConsoleLines.rbegin() + index);
}

void SetHistoryIndex(int index)
{
	CurrentInputTextState = InputTextState::RestoredFromHistory;
	HistoryIndex = static_cast<int>(std::ssize(ConsoleLines)) - (index + 1);
	if (HistoryIndex == -1) {
		ConsoleInputState.assign(DraftInput);
		return;
	}
	const ConsoleLine &line = ConsoleLines[index];
	ConsoleInputState.assign(line.textWithoutPrompt());
}

void PrevHistoryItem(tl::function_ref<bool(const ConsoleLine &line)> filter)
{
	if (HistoryIndex == -1) {
		DraftInput = ConsoleInputState.value();
	}
	const int n = static_cast<int>(std::ssize(ConsoleLines));
	for (int i = HistoryIndex + 1; i < n; ++i) {
		const int index = n - (i + 1);
		if (filter(ConsoleLines[index])) {
			SetHistoryIndex(index);
			return;
		}
	}
}

void NextHistoryItem(tl::function_ref<bool(const ConsoleLine &line)> filter)
{
	const int n = static_cast<int>(std::ssize(ConsoleLines));
	for (int i = n - HistoryIndex; i < n; ++i) {
		if (filter(ConsoleLines[i])) {
			SetHistoryIndex(i);
			return;
		}
	}
	if (HistoryIndex != -1) {
		SetHistoryIndex(n);
	}
}

bool IsHistoryInputLine(const ConsoleLine &line)
{
	if (line.type != ConsoleLine::Input)
		return false;
	std::string_view text = line.text;
	text.remove_prefix(Prompt.size());
	if (text.empty())
		return false;
	return HistoryIndex == -1 || GetConsoleLineFromEnd(HistoryIndex).textWithoutPrompt() != text;
}

void PrevInput()
{
	PrevHistoryItem(IsHistoryInputLine);
}

void NextInput()
{
	NextHistoryItem(IsHistoryInputLine);
}

bool IsHistoryOutputLine(const ConsoleLine &line)
{
	return !line.text.empty()
	    && (line.type == ConsoleLine::Output || line.type == ConsoleLine::Warning || line.type == ConsoleLine::Error)
	    && (HistoryIndex == -1
	        || GetConsoleLineFromEnd(HistoryIndex).textWithoutPrompt() != line.text);
}

void PrevOutput()
{
	PrevHistoryItem(IsHistoryOutputLine);
}

void NextOutput()
{
	NextHistoryItem(IsHistoryOutputLine);
}

void AddInitialConsoleLines()
{
	if (ConsolePrelude->has_value()) {
		std::string_view prelude { **ConsolePrelude };
		if (!prelude.empty() && prelude.back() == '\n')
			prelude.remove_suffix(1);
		AddConsoleLine(ConsoleLine { .type = ConsoleLine::Help, .text = StrCat(HelpText, "\n", prelude) });
	} else {
		AddConsoleLine(ConsoleLine { .type = ConsoleLine::Help, .text = std::string(HelpText) });
		AddConsoleLine(ConsoleLine { .type = ConsoleLine::Error, .text = ConsolePrelude->error() });
	}
}

void ClearConsole()
{
	ConsoleLines.clear();
	HistoryIndex = -1;
	ScrollOffset = 0;
	NumPreparedConsoleLines = 0;
	ConsoleLinesTotalHeight = 0;
	AddInitialConsoleLines();
}

} // namespace

bool IsConsoleOpen()
{
	return IsConsoleVisible;
}

void OpenConsole()
{
	IsConsoleVisible = true;
	FirstRender = true;
}

void AcceptSuggestion()
{
	const LuaAutocompleteSuggestion &suggestion = AutocompleteSuggestions[AutocompleteSuggestionFocusIndex];
	ConsoleInputState.type(suggestion.completionText);
	if (suggestion.cursorAdjust == -1) {
		ConsoleInputState.moveCursorLeft(/*word=*/false);
	}
}

bool ConsoleHandleEvent(const SDL_Event &event)
{
	if (!IsConsoleVisible) {
		// Make console open on the top-left keyboard key even if it is not a backtick.
		if (event.type == SDL_EVENT_KEY_DOWN && SDLC_EventScancode(event) == SDL_SCANCODE_GRAVE) {
			OpenConsole();
			return true;
		}
		return false;
	}
	if (HandleTextInputEvent(event, ConsoleInputState)) {
		CurrentInputTextState = InputTextState::Edited;
		return true;
	}
	const auto modState = SDL_GetModState();
	const bool isShift = (modState & SDL_KMOD_SHIFT) != 0;
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
		switch (SDLC_EventKey(event)) {
		case SDLK_ESCAPE:
			if (!AutocompleteSuggestions.empty()) {
				AutocompleteSuggestions.clear();
				AutocompleteSuggestionFocusIndex = -1;
			} else {
				CloseConsole();
			}
			return true;
		case SDLK_UP:
			if (AutocompleteSuggestionFocusIndex != -1) {
				AutocompleteSuggestionFocusIndex = std::max(
				    0, AutocompleteSuggestionFocusIndex - 1);
			} else {
				isShift ? PrevOutput() : PrevInput();
			}
			return true;
		case SDLK_DOWN:
			if (AutocompleteSuggestionFocusIndex != -1) {
				AutocompleteSuggestionFocusIndex = std::min(
				    static_cast<int>(AutocompleteSuggestions.size()) - 1,
				    AutocompleteSuggestionFocusIndex + 1);
			} else {
				isShift ? NextOutput() : NextInput();
			}
			return true;
		case SDLK_TAB:
			if (AutocompleteSuggestionFocusIndex != -1) {
				AcceptSuggestion();
				CurrentInputTextState = InputTextState::Edited;
			}
			return true;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (isShift) {
				ConsoleInputState.type("\n");
			} else {
				if (AutocompleteSuggestionFocusIndex != -1) {
					AcceptSuggestion();
				} else {
					SendInput();
				}
			}
			CurrentInputTextState = InputTextState::Edited;
			return true;
		case SDLK_PAGEUP:
			++PendingScrollPages;
			return true;
		case SDLK_PAGEDOWN:
			--PendingScrollPages;
			return true;
		case SDLK_L:
			ClearConsole();
			return true;
		default:
			return false;
		}
		break;
#ifndef USE_SDL1
	case SDL_EVENT_MOUSE_WHEEL:
		if (SDLC_EventWheelIntY(event) > 0) {
			ScrollOffset += ScrollStep;
		} else if (SDLC_EventWheelIntY(event) < 0) {
			ScrollOffset -= ScrollStep;
		}
		return true;
#else
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_WHEELUP) {
			ScrollOffset += ScrollStep;
			return true;
		}
		if (event.button.button == SDL_BUTTON_WHEELDOWN) {
			ScrollOffset -= ScrollStep;
			return true;
		}
		return false;
#endif
	default:
		return false;
	}
	return false;
}

void DrawConsole(const Surface &out)
{
	if (!IsConsoleVisible)
		return;

	OuterRect.position = { 0, 0 };
	OuterRect.size = { out.w(), out.h() - GetMainPanel().size.height - 2 };

	const std::string_view originalInputText = ConsoleInputState.value();
	if (CurrentInputTextState != InputTextState::UpToDate) {
		WrappedInputText = WordWrapString(StrCat(Prompt, originalInputText), OuterRect.size.width - 2 * TextPaddingX, TextFontSize, TextSpacing);
		if (CurrentInputTextState == InputTextState::RestoredFromHistory) {
			AutocompleteSuggestions.clear();
		} else {
			GetLuaAutocompleteSuggestions(originalInputText, ConsoleInputCursor.position, GetLuaReplEnvironment(), /*maxSuggestions=*/MaxSuggestions, AutocompleteSuggestions);
		}
		AutocompleteSuggestionsMaxWidth = -1;
		AutocompleteSuggestionFocusIndex = AutocompleteSuggestions.empty() ? -1 : 0;
		CurrentInputTextState = InputTextState::UpToDate;
	}

	const int numLines = static_cast<int>(c_count(WrappedInputText, '\n')) + 1;
	InputRectHeight = std::min(OuterRect.size.height, numLines * LineHeight + TextPaddingYTop + TextPaddingYBottom);
	const int inputTextHeight = InputRectHeight - (TextPaddingYTop + TextPaddingYBottom);

	InputRect.position = { 0, OuterRect.size.height - InputRectHeight };
	InputRect.size = { OuterRect.size.width, InputRectHeight };
	const Rectangle inputTextRect {
		{ InputRect.position.x + TextPaddingX, InputRect.position.y + TextPaddingYTop },
		{ InputRect.size.width - 2 * TextPaddingX, inputTextHeight }
	};

	if (FirstRender) {
		SDL_Rect sdlInputRect = MakeSdlRect(InputRect);
		SDL_SetTextInputArea(ghMainWnd, &sdlInputRect, static_cast<int>(ConsoleInputState.cursorPosition()));
		SDLC_StartTextInput(ghMainWnd);
		FirstRender = false;
		if (ConsoleLines.empty()) {
			InitConsole();
		}
	}

	const Rectangle bgRect = OuterRect;
	DrawHalfTransparentRectTo(out, bgRect.position.x, bgRect.position.y, bgRect.size.width, bgRect.size.height);

	DrawConsoleLines(
	    out.subregion(
	        TextPaddingX,
	        TextPaddingYTop,
	        GetConsoleLinesInnerWidth(),
	        OuterRect.size.height - inputTextRect.size.height - 8));

	DrawHorizontalLine(out, InputRect.position - Displacement { 0, 1 }, InputRect.size.width, BorderColor);
	DrawInputText(
	    out,
	    Rectangle(
	        inputTextRect.position,
	        Size {
	            // Extra space for the cursor on the right:
	            inputTextRect.size.width + TextPaddingX,
	            // Extra space for letters like g.
	            inputTextRect.size.height + TextPaddingYBottom }),
	    originalInputText,
	    WrappedInputText);

	SDL_Rect sdlRect = MakeSdlRect(OuterRect);
	BltFast(&sdlRect, &sdlRect);
}

void InitConsole()
{
	if (!ConsoleLines.empty())
		return;
	ConsolePrelude = LoadAsset("lua\\repl_prelude.lua");
	AddInitialConsoleLines();
	if (ConsolePrelude->has_value())
		RunLuaReplLine(std::string_view(**ConsolePrelude));
}

void RunInConsole(std::string_view code)
{
	AddConsoleLine(ConsoleLine { .type = ConsoleLine::Input, .text = StrCat(Prompt, code) });
	tl::expected<std::string, std::string> result = RunLuaReplLine(code);

	if (result.has_value()) {
		if (!result->empty()) {
			AddConsoleLine(ConsoleLine { .type = ConsoleLine::Output, .text = *std::move(result) });
		}
	} else {
		if (!result.error().empty()) {
			AddConsoleLine(ConsoleLine { .type = ConsoleLine::Error, .text = std::move(result).error() });
		} else {
			AddConsoleLine(ConsoleLine { .type = ConsoleLine::Error, .text = "Unknown error" });
		}
	}
}

void PrintToConsole(std::string_view text)
{
	AddConsoleLine(ConsoleLine { .type = ConsoleLine::Output, .text = std::string(text) });
}

void PrintWarningToConsole(std::string_view text)
{
	AddConsoleLine(ConsoleLine { .type = ConsoleLine::Warning, .text = std::string(text) });
}

} // namespace devilution
#endif // _DEBUG
