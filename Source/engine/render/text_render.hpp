/**
 * @file text_render.hpp
 *
 * Text rendering.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "utils/enum_traits.h"

namespace devilution {

enum GameFontTables : uint8_t {
	GameFont12,
	GameFont24,
	GameFont30,
	GameFont42,
	GameFont46,
	FontSizeDialog,
};

enum text_color : uint8_t {
	ColorUiGold,
	ColorUiSilver,
	ColorUiGoldDark,
	ColorUiSilverDark,

	ColorDialogWhite, // Dialog white in main menu
	ColorDialogRed,
	ColorYellow,

	ColorGold,
	ColorBlack,

	ColorWhite,
	ColorWhitegold,
	ColorRed,
	ColorBlue,
	ColorOrange,

	ColorButtonface,
	ColorButtonpushed,

	ColorInGameDialogWhite,  // Dialog white in-game
	ColorInGameDialogYellow, // Dialog yellow in-game
	ColorInGameDialogRed,    // Dialog red in-game
};

constexpr GameFontTables GetFontSizeFromUiFlags(UiFlags flags)
{
	if (HasAnyOf(flags, UiFlags::FontSize24))
		return GameFont24;
	if (HasAnyOf(flags, UiFlags::FontSize30))
		return GameFont30;
	if (HasAnyOf(flags, UiFlags::FontSize42))
		return GameFont42;
	if (HasAnyOf(flags, UiFlags::FontSize46))
		return GameFont46;
	if (HasAnyOf(flags, UiFlags::FontSizeDialog))
		return FontSizeDialog;
	return GameFont12;
}

/**
 * @brief A format argument for `DrawStringWithColors`.
 */
class DrawStringFormatArg {
public:
	using Value = std::variant<std::string_view, int>;

	DrawStringFormatArg(std::string_view value, UiFlags flags)
	    : value_(value)
	    , flags_(flags)
	{
	}

	DrawStringFormatArg(int value, UiFlags flags)
	    : value_(value)
	    , flags_(flags)
	{
	}

	std::string_view GetFormatted() const
	{
		if (std::holds_alternative<std::string_view>(value_))
			return std::get<std::string_view>(value_);
		return formatted_;
	}

	void SetFormatted(std::string &&value)
	{
		formatted_ = std::move(value);
	}

	bool HasFormatted() const
	{
		return std::holds_alternative<std::string_view>(value_) || !formatted_.empty();
	}

	const Value &value() const
	{
		return value_;
	}

	UiFlags GetFlags() const
	{
		return flags_;
	}

private:
	Value value_;
	UiFlags flags_;
	std::string formatted_;
};

/** @brief Text rendering options. */
struct TextRenderOptions {
	/** @brief A combination of UiFlags to describe font size, color, alignment, etc. See ui_items.h for available options */
	UiFlags flags = UiFlags::None;

	/**
	 * @brief Additional space to add between characters.
	 *
	 * This value may be adjusted if the flag UiFlags::KerningFitSpacing is set.
	 */
	int spacing = 1;

	/** @brief Allows overriding the default line height, useful for multi-line strings. */
	int lineHeight = -1;

	/** @brief If non-negative, draws a blinking cursor after the given byte index.*/
	int cursorPosition = -1;

	/** @brief Highlight text background in this range. */
	struct {
		int begin;
		int end;
	} highlightRange = { 0, 0 };

	uint8_t highlightColor = PAL8_RED + 6;

	/** @brief If a cursor is rendered, the surface coordinates are saved here. */
	std::optional<Point> *renderedCursorPositionOut = nullptr;
};

/**
 * @brief Small text selection cursor.
 *
 * Also used in the stores and the quest log.
 */
extern OptionalOwnedClxSpriteList pSPentSpn2Cels;

void LoadSmallSelectionSpinner();

/**
 * @brief Calculate pixel width of first line of text, respecting kerning
 * @param text Text to check, will read until first eol or terminator
 * @param size Font size to use
 * @param spacing Extra spacing to add per character
 * @param charactersInLine Receives characters read until newline or terminator
 * @return Line width in pixels
 */
int GetLineWidth(std::string_view text, GameFontTables size = GameFont12, int spacing = 1, int *charactersInLine = nullptr);

/**
 * @brief Calculate pixel width of first line of text, respecting kerning
 * @param fmt An fmt::format string.
 * @param args Format arguments.
 * @param argsLen Number of format arguments.
 * @param argsOffset Index of the first unprocessed format argument.
 * @param size Font size to use
 * @param spacing Extra spacing to add per character
 * @param charactersInLine Receives characters read until newline or terminator
 * @param firstArgOffset If given, starts counting at `args[argsOffset - 1].GetFormatted().substr(*firstArgOffset)`.
 * @return Line width in pixels
 */
int GetLineWidth(std::string_view fmt, DrawStringFormatArg *args, size_t argsLen, size_t argsOffset, GameFontTables size, int spacing, int *charactersInLine = nullptr,
    std::optional<size_t> firstArgOffset = std::nullopt);

int GetLineHeight(std::string_view text, GameFontTables fontIndex);

/**
 * @brief Builds a multi-line version of the given text so it'll fit within the given width.
 *
 * This function will not break words, if the given width is smaller than the width of the longest word in the given
 * font then it will likely overflow the output region.
 *
 * @param text Source text
 * @param width Width in pixels of the output region
 * @param size Font size to use for the width calculation
 * @param spacing Any adjustment to apply between each character
 * @return A copy of the source text with newlines inserted where appropriate
 */
[[nodiscard]] std::string WordWrapString(std::string_view text, unsigned width, GameFontTables size = GameFont12, int spacing = 1);

/**
 * @brief Draws a line of text within a clipping rectangle (positioned relative to the origin of the output buffer).
 *
 * Specifying a small width (0 to less than two characters wide) should be avoided as this causes issues when laying
 * out the text. To wrap based on available space use the overload taking a Point. If the rect passed through has 0
 * height then the clipping area is extended to the bottom edge of the output buffer. If the clipping rectangle
 * dimensions extend past the edge of the output buffer text wrapping will be calculated using those dimensions (as if
 * the text was being rendered off screen). The text will not actually be drawn beyond the bounds of the output
 * buffer, this is purely to allow for clipping without wrapping.
 *
 * @param out The screen buffer to draw on.
 * @param text String to be drawn.
 * @param rect Clipping region relative to the output buffer describing where to draw the text and when to wrap long lines.
 * @param opts Rendering options.
 * @return The number of bytes rendered, including characters "drawn" outside the buffer.
 */
uint32_t DrawString(const Surface &out, std::string_view text, const Rectangle &rect, TextRenderOptions opts = {});

/**
 * @brief Draws a line of text at the given position relative to the origin of the output buffer.
 *
 * This method is provided as a convenience to pass through to DrawString(..., Rectangle, ...) when no explicit
 * clipping/wrapping is requested. Note that this will still wrap the rendered string if it would end up being drawn
 * beyond the right edge of the output buffer and clip it if it would extend beyond the bottom edge of the buffer.
 *
 * @param out The screen buffer to draw on.
 * @param text String to be drawn.
 * @param position Location of the top left corner of the string relative to the top left corner of the output buffer.
 * @param opts Rendering options.
 */
inline void DrawString(const Surface &out, std::string_view text, const Point &position, TextRenderOptions opts = {})
{
	DrawString(out, text, { position, { out.w() - position.x, 0 } }, opts);
}

/**
 * @brief Draws a line of text with different colors for certain parts of the text.
 *
 *     DrawStringWithColors(out, "Press {} to start", {{"Ⓧ", UiFlags::ColorBlue}}, {.flags = UiFlags::ColorWhite})
 *
 * @param out Output buffer to draw the text on.
 * @param fmt An fmt::format string.
 * @param args Format arguments.
 * @param argsLen Number of format arguments.
 * @param rect Clipping region relative to the output buffer describing where to draw the text and when to wrap long lines.
 * @param opts Rendering options.
 */
void DrawStringWithColors(const Surface &out, std::string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, const Rectangle &rect, TextRenderOptions opts = {});

inline void DrawStringWithColors(const Surface &out, std::string_view fmt, std::vector<DrawStringFormatArg> args, const Rectangle &rect, TextRenderOptions opts = {})
{
	return DrawStringWithColors(out, fmt, args.data(), args.size(), rect, opts);
}

uint8_t PentSpn2Spin();
void UnloadFonts();

/** @brief Whether this character can be substituted by a newline when word-wrapping. */
bool IsBreakableWhitespace(char32_t c);

} // namespace devilution
