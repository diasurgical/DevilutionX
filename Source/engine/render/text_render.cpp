/**
 * @file text_render.cpp
 *
 * Text rendering.
 */
#include "text_render.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include <ankerl/unordered_dense.h>
#include <fmt/core.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/ui_item.h"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/load_file.hpp"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/ticks.hpp"
#include "options.h"
#include "utils/algorithm/container.hpp"
#include "utils/display.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/utf8.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pSPentSpn2Cels;

namespace {

constexpr char32_t ZWSP = U'\u200B'; // Zero-width space

ankerl::unordered_dense::map<uint32_t, OptionalOwnedClxSpriteList> Fonts;

std::array<int, 6> FontSizes = { 12, 24, 30, 42, 46, 22 };
constexpr std::array<int, 6> LineHeights = { 12, 26, 38, 42, 50, 22 };
constexpr int SmallFontTallLineHeight = 16;
std::array<int, 6> BaseLineOffset = { -3, -2, -3, -6, -7, 3 };

std::array<const char *, 19> ColorTranslations = {
	"fonts\\goldui.trn",
	"fonts\\grayui.trn",
	"fonts\\golduis.trn",
	"fonts\\grayuis.trn",

	nullptr, // ColorDialogWhite
	nullptr, // ColorDialogRed
	"fonts\\yellow.trn",

	nullptr,
	"fonts\\black.trn",

	"fonts\\white.trn",
	"fonts\\whitegold.trn",
	"fonts\\red.trn",
	"fonts\\blue.trn",
	"fonts\\orange.trn",

	"fonts\\buttonface.trn",
	"fonts\\buttonpushed.trn",
	"fonts\\gamedialogwhite.trn",
	"fonts\\gamedialogyellow.trn",
	"fonts\\gamedialogred.trn",
};

std::array<std::optional<std::array<uint8_t, 256>>, 19> ColorTranslationsData;

text_color GetColorFromFlags(UiFlags flags)
{
	if (HasAnyOf(flags, UiFlags::ColorWhite))
		return ColorWhite;
	if (HasAnyOf(flags, UiFlags::ColorBlue))
		return ColorBlue;
	if (HasAnyOf(flags, UiFlags::ColorOrange))
		return ColorOrange;
	if (HasAnyOf(flags, UiFlags::ColorRed))
		return ColorRed;
	if (HasAnyOf(flags, UiFlags::ColorBlack))
		return ColorBlack;
	if (HasAnyOf(flags, UiFlags::ColorGold))
		return ColorGold;
	if (HasAnyOf(flags, UiFlags::ColorUiGold))
		return ColorUiGold;
	if (HasAnyOf(flags, UiFlags::ColorUiSilver))
		return ColorUiSilver;
	if (HasAnyOf(flags, UiFlags::ColorUiGoldDark))
		return ColorUiGoldDark;
	if (HasAnyOf(flags, UiFlags::ColorUiSilverDark))
		return ColorUiSilverDark;
	if (HasAnyOf(flags, UiFlags::ColorDialogWhite))
		return gbRunGame ? ColorInGameDialogWhite : ColorDialogWhite;
	if (HasAnyOf(flags, UiFlags::ColorDialogYellow))
		return ColorInGameDialogYellow;
	if (HasAnyOf(flags, UiFlags::ColorDialogRed))
		return ColorInGameDialogRed;
	if (HasAnyOf(flags, UiFlags::ColorYellow))
		return ColorYellow;
	if (HasAnyOf(flags, UiFlags::ColorButtonface))
		return ColorButtonface;
	if (HasAnyOf(flags, UiFlags::ColorButtonpushed))
		return ColorButtonpushed;

	return ColorWhitegold;
}

uint16_t GetUnicodeRow(char32_t codePoint)
{
	return static_cast<uint32_t>(codePoint) >> 8;
}

bool IsCJK(uint16_t row)
{
	return row >= 0x30 && row <= 0x9f;
}

bool IsHangul(uint16_t row)
{
	return row >= 0xac && row <= 0xd7;
}

bool IsSmallFontTallRow(uint16_t row)
{
	return IsCJK(row) || IsHangul(row);
}

void GetFontPath(GameFontTables size, uint16_t row, std::string_view ext, char *out)
{
	*fmt::format_to(out, R"(fonts\{}-{:02x}{})", FontSizes[size], row, ext) = '\0';
}

void GetFontPath(std::string_view language_code, GameFontTables size, uint16_t row, std::string_view ext, char *out)
{
	*fmt::format_to(out, R"(fonts\{}\{}-{:02x}{})", language_code, FontSizes[size], row, ext) = '\0';
}

uint32_t GetFontId(GameFontTables size, uint16_t row)
{
	return (size << 16) | row;
}

OptionalClxSpriteList LoadFont(GameFontTables size, text_color color, uint16_t row)
{
	if (ColorTranslations[color] != nullptr && !ColorTranslationsData[color]) {
		ColorTranslationsData[color].emplace();
		LoadFileInMem(ColorTranslations[color], *ColorTranslationsData[color]);
	}

	const uint32_t fontId = GetFontId(size, row);
	auto hotFont = Fonts.find(fontId);
	if (hotFont != Fonts.end()) {
		return OptionalClxSpriteList(*hotFont->second);
	}

	OptionalOwnedClxSpriteList &font = Fonts[fontId];
	char path[32];

	// Try loading the language-specific variant first:
	const std::string_view language_code = GetLanguageCode();
	const std::string_view language_tag = language_code.substr(0, 2);
	if (language_tag == "zh" || language_tag == "ja" || language_tag == "ko"
	    || (language_tag == "tr" && row == 0)) {
		GetFontPath(language_code, size, row, ".clx", &path[0]);
		font = LoadOptionalClx(path);
	}
	if (!font) {
		// Fall back to the base variant:
		GetFontPath(size, row, ".clx", &path[0]);
		font = LoadOptionalClx(path);
	}

#ifndef UNPACKED_MPQS
	if (!font) {
		// Could be an old devilutionx.mpq or fonts.mpq with PCX instead of CLX.
		//
		// We'll show an error elsewhere (in `CheckArchivesUpToDate`) and we need to load
		// the font files to display it.
		char pcxPath[32];
		GetFontPath(size, row, "", &pcxPath[0]);
		font = LoadPcxSpriteList(pcxPath, /*numFramesOrFrameHeight=*/256, /*transparentColor=*/1);
	}
#endif

	if (!font) {
		LogError("Error loading font: {}", path);
	}

	return OptionalClxSpriteList(*font);
}

class CurrentFont {
public:
	OptionalClxSpriteList sprite;

	bool load(GameFontTables size, text_color color, char32_t next)
	{
		const uint32_t unicodeRow = GetUnicodeRow(next);
		if (unicodeRow == currentUnicodeRow_ && hasAttemptedLoad_) {
			return true;
		}

		sprite = LoadFont(size, color, unicodeRow);
		hasAttemptedLoad_ = true;
		currentUnicodeRow_ = unicodeRow;

		return sprite;
	}

	void clear()
	{
		hasAttemptedLoad_ = false;
	}

private:
	bool hasAttemptedLoad_ = false;
	uint32_t currentUnicodeRow_ = 0;
};

void DrawFont(const Surface &out, Point position, ClxSprite glyph, text_color color, bool outline)
{
	if (outline) {
		ClxDrawOutlineSkipColorZero(out, 0, { position.x, position.y + glyph.height() - 1 }, glyph);
	}
	if (ColorTranslationsData[color]) {
		RenderClxSpriteWithTRN(out, glyph, position, ColorTranslationsData[color]->data());
	} else {
		RenderClxSprite(out, glyph, position);
	}
}

bool IsFullWidthPunct(char32_t c)
{
	return IsAnyOf(c, U'，', U'、', U'。', U'？', U'！');
}

bool IsBreakAllowed(char32_t codepoint, char32_t nextCodepoint)
{
	return IsFullWidthPunct(codepoint) && !IsFullWidthPunct(nextCodepoint);
}

std::size_t CountNewlines(std::string_view fmt, const DrawStringFormatArg *args, std::size_t argsLen)
{
	std::size_t result = c_count(fmt, '\n');
	for (std::size_t i = 0; i < argsLen; ++i) {
		if (std::holds_alternative<std::string_view>(args[i].value()))
			result += c_count(args[i].GetFormatted(), '\n');
	}
	return result;
}

class FmtArgParser {
public:
	FmtArgParser(std::string_view fmt,
	    DrawStringFormatArg *args,
	    size_t len,
	    size_t offset = 0)
	    : fmt_(fmt)
	    , args_(args)
	    , len_(len)
	    , next_(offset)
	{
	}

	std::optional<std::size_t> operator()(std::string_view &rest)
	{
		std::optional<std::size_t> result;
		if (rest[0] != '{')
			return result;

		std::size_t closingBracePos = rest.find('}', 1);
		if (closingBracePos == std::string_view::npos) {
			LogError("Unclosed format argument: {}", fmt_);
			return result;
		}

		std::size_t fmtLen;
		bool positional;
		if (closingBracePos == 2 && rest[1] >= '0' && rest[1] <= '9') {
			result = rest[1] - '0';
			fmtLen = 3;
			positional = true;
		} else {
			result = next_++;
			fmtLen = closingBracePos + 1;
			positional = false;
		}
		if (!result) {
			LogError("Unsupported format argument: {}", rest);
		} else if (*result >= len_) {
			LogError("Not enough format arguments, {} given for: {}", len_, fmt_);
			result = std::nullopt;
		} else {
			if (!args_[*result].HasFormatted()) {
				const auto fmtStr = positional ? "{}" : std::string_view(rest.data(), fmtLen);
				args_[*result].SetFormatted(fmt::format(fmt::runtime(fmtStr), std::get<int>(args_[*result].value())));
			}
			rest.remove_prefix(fmtLen);
		}
		return result;
	}

	size_t offset() const
	{
		return next_;
	}

private:
	std::string_view fmt_;
	DrawStringFormatArg *args_;
	std::size_t len_;
	std::size_t next_;
};

bool ContainsSmallFontTallCodepoints(std::string_view text)
{
	while (!text.empty()) {
		const char32_t next = ConsumeFirstUtf8CodePoint(&text);
		if (next == Utf8DecodeError)
			break;
		if (next == ZWSP)
			continue;
		if (IsSmallFontTallRow(GetUnicodeRow(next)))
			return true;
	}
	return false;
}

int GetLineHeight(std::string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, GameFontTables fontIndex)
{
	constexpr std::array<int, 6> LineHeights = { 12, 26, 38, 42, 50, 22 };
	if (fontIndex == GameFont12 && IsSmallFontTall()) {
		FmtArgParser fmtArgParser { fmt, args, argsLen };
		std::string_view rest = fmt;
		while (!rest.empty()) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(rest);
			if (fmtArgPos) {
				if (ContainsSmallFontTallCodepoints(args[*fmtArgPos].GetFormatted())) {
					return SmallFontTallLineHeight;
				}
				continue;
			}
			const char32_t cp = ConsumeFirstUtf8CodePoint(&rest);
			if (cp == Utf8DecodeError) break;
			if (cp == ZWSP) continue;
			if (IsSmallFontTallRow(GetUnicodeRow(cp))) return SmallFontTallLineHeight;
		}
	}
	return LineHeights[fontIndex];
}

Surface ClipSurface(const Surface &out, Rectangle rect)
{
	if (rect.size.height == 0) {
		return out.subregion(0, 0, std::min(rect.position.x + rect.size.width, out.w()), out.h());
	}
	return out.subregion(0, 0,
	    std::min(rect.position.x + rect.size.width, out.w()),
	    std::min(rect.position.y + rect.size.height, out.h()));
}

int AdjustSpacingToFitHorizontally(int &lineWidth, int maxSpacing, int charactersInLine, int availableWidth)
{
	if (lineWidth <= availableWidth || charactersInLine < 2)
		return maxSpacing;

	const int overhang = lineWidth - availableWidth;
	const int spacingRedux = (overhang + charactersInLine - 2) / (charactersInLine - 1);
	lineWidth -= spacingRedux * (charactersInLine - 1);
	return maxSpacing - spacingRedux;
}

void MaybeWrap(Point &characterPosition, int characterWidth, int rightMargin, int initialX, int lineHeight)
{
	if (characterPosition.x + characterWidth > rightMargin) {
		characterPosition.x = initialX;
		characterPosition.y += lineHeight;
	}
}

int GetLineStartX(UiFlags flags, const Rectangle &rect, int lineWidth)
{
	if (HasAnyOf(flags, UiFlags::AlignCenter)) {
		return std::max(rect.position.x, rect.position.x + (rect.size.width - lineWidth) / 2);
	}
	if (HasAnyOf(flags, UiFlags::AlignRight))
		return rect.position.x + rect.size.width - lineWidth;
	return rect.position.x;
}

uint32_t DoDrawString(const Surface &out, std::string_view text, Rectangle rect, Point &characterPosition,
    int lineWidth, int charactersInLine, int rightMargin, int bottomMargin, GameFontTables size, text_color color, bool outline,
    TextRenderOptions &opts)
{
	CurrentFont currentFont;
	int curSpacing = opts.spacing;
	if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
		curSpacing = AdjustSpacingToFitHorizontally(lineWidth, opts.spacing, charactersInLine, rect.size.width);
		if (curSpacing != opts.spacing && HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
			const int adjustedLineWidth = GetLineWidth(text, size, curSpacing, &charactersInLine);
			characterPosition.x = GetLineStartX(opts.flags, rect, adjustedLineWidth);
		}
	}

	char32_t next;
	std::string_view remaining = text;
	size_t cpLen;

	const auto maybeDrawCursor = [&]() {
		if (opts.cursorPosition == static_cast<int>(text.size() - remaining.size())) {
			Point position = characterPosition;
			MaybeWrap(position, 2, rightMargin, position.x, opts.lineHeight);
			if (GetAnimationFrame(2, 500) != 0) {
				OptionalClxSpriteList baseFont = LoadFont(size, color, 0);
				if (baseFont)
					DrawFont(out, position, (*baseFont)['|'], color, outline);
			}
			if (opts.renderedCursorPositionOut != nullptr) {
				*opts.renderedCursorPositionOut = position;
			}
		}
	};

	for (; !remaining.empty() && remaining[0] != '\0'
	     && (next = DecodeFirstUtf8CodePoint(remaining, &cpLen)) != Utf8DecodeError;
	     remaining.remove_prefix(cpLen)) {
		if (next == ZWSP)
			continue;

		if (!currentFont.load(size, color, next)) {
			next = U'?';
			if (!currentFont.load(size, color, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		const uint16_t width = (*currentFont.sprite)[frame].width();
		if (next == U'\n' || characterPosition.x + width > rightMargin) {
			if (next == '\n')
				maybeDrawCursor();
			const int nextLineY = characterPosition.y + opts.lineHeight;
			if (nextLineY >= bottomMargin)
				break;
			characterPosition.y = nextLineY;

			if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
				int nextLineWidth = GetLineWidth(remaining.substr(cpLen), size, opts.spacing, &charactersInLine);
				curSpacing = AdjustSpacingToFitHorizontally(nextLineWidth, opts.spacing, charactersInLine, rect.size.width);
			}

			if (HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
				lineWidth = width;
				if (remaining.size() > cpLen)
					lineWidth += curSpacing + GetLineWidth(remaining.substr(cpLen), size, curSpacing);
			}
			characterPosition.x = GetLineStartX(opts.flags, rect, lineWidth);

			if (next == U'\n')
				continue;
		}

		const ClxSprite glyph = (*currentFont.sprite)[frame];
		const auto byteIndex = static_cast<int>(text.size() - remaining.size());

		// Draw highlight
		if (byteIndex >= opts.highlightRange.begin && byteIndex < opts.highlightRange.end) {
			const bool lastInRange = static_cast<int>(byteIndex + cpLen) == opts.highlightRange.end;
			FillRect(out, characterPosition.x, characterPosition.y,
			    glyph.width() + (lastInRange ? 0 : curSpacing), glyph.height(),
			    opts.highlightColor);
		}

		DrawFont(out, characterPosition, glyph, color, outline);
		maybeDrawCursor();
		characterPosition.x += width + curSpacing;
	}
	maybeDrawCursor();
	return static_cast<uint32_t>(remaining.data() - text.data());
}

void OptionLanguageCodeChanged()
{
	UnloadFonts();
	LanguageInitialize();
	LoadLanguageArchive();
}

const auto OptionChangeHandlerResolution = (GetOptions().Language.code.SetValueChangedCallback(OptionLanguageCodeChanged), true);

} // namespace

void LoadSmallSelectionSpinner()
{
	pSPentSpn2Cels = LoadCel("data\\pentspn2", 12);
}

void UnloadFonts()
{
	Fonts.clear();
}

int GetLineWidth(std::string_view text, GameFontTables size, int spacing, int *charactersInLine)
{
	int lineWidth = 0;
	CurrentFont currentFont;
	uint32_t codepoints = 0;
	char32_t next;
	while (!text.empty()) {
		next = ConsumeFirstUtf8CodePoint(&text);
		if (next == Utf8DecodeError)
			break;
		if (next == ZWSP)
			continue;

		if (next == U'\n')
			break;

		if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
			next = U'?';
			if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		lineWidth += (*currentFont.sprite)[frame].width() + spacing;
		++codepoints;
	}
	if (charactersInLine != nullptr)
		*charactersInLine = codepoints;

	return lineWidth != 0 ? (lineWidth - spacing) : 0;
}

bool IsConsumed(std::string_view s) { return s.empty() || s[0] == '\0'; };

int GetLineWidth(std::string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, size_t argsOffset, GameFontTables size, int spacing, int *charactersInLine,
    std::optional<size_t> firstArgOffset)
{
	int lineWidth = 0;
	CurrentFont currentFont;

	uint32_t codepoints = 0;
	char32_t prev = U'\0';
	char32_t next;
	std::string_view remaining = fmt;
	FmtArgParser fmtArgParser { fmt, args, argsLen, argsOffset };
	size_t cpLen;

	// The current formatted argument value being processed.
	std::string_view curFormatted;

	// The string that we're currently processing: either `remaining` or `curFormatted`.
	std::string_view *str;

	if (firstArgOffset.has_value()) {
		curFormatted = args[argsOffset - 1].GetFormatted().substr(*firstArgOffset);
	}

	for (; !(IsConsumed(curFormatted) && IsConsumed(remaining));
	     str->remove_prefix(cpLen), prev = next) {
		const bool isProcessingFormatArgValue = !IsConsumed(curFormatted);
		str = isProcessingFormatArgValue ? &curFormatted : &remaining;
		next = DecodeFirstUtf8CodePoint(*str, &cpLen);
		if (next == Utf8DecodeError) break;

		// {{ and }} escapes in fmt.
		if (!isProcessingFormatArgValue && (prev == U'{' || prev == U'}') && prev == next) continue;
		// ZWSP are line-breaking opportunities that can otherwise be skipped for rendering as they have 0-width.
		if (next == ZWSP) continue;
		if (next == U'\n') break;

		if (!isProcessingFormatArgValue) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(*str);
			if (fmtArgPos.has_value()) {
				// `fmtArgParser` has already consumed `*str`. Ensure the loop doesn't consume any more.
				cpLen = 0;
				// The loop assigns `prev = next`.
				// We reset it to U'\0' to ensure that {{ and }} escapes are not processed accross
				// the boundary of the format string and a formatted value.
				next = U'\0';
				currentFont.clear();
				const DrawStringFormatArg &arg = args[*fmtArgPos];
				curFormatted = arg.GetFormatted();
				continue;
			}
		}

		if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
			next = U'?';
			if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		lineWidth += (*currentFont.sprite)[frame].width() + spacing;
		++codepoints;
	}
	if (charactersInLine != nullptr)
		*charactersInLine = codepoints;

	return lineWidth != 0 ? (lineWidth - spacing) : 0;
}

int GetLineHeight(std::string_view text, GameFontTables fontIndex)
{
	if (fontIndex == GameFont12 && IsSmallFontTall() && ContainsSmallFontTallCodepoints(text)) {
		return SmallFontTallLineHeight;
	}
	return LineHeights[fontIndex];
}

std::string WordWrapString(std::string_view text, unsigned width, GameFontTables size, int spacing)
{
	std::string output;
	if (text.empty() || text[0] == '\0')
		return output;

	output.reserve(text.size());
	const char *begin = text.data();
	const char *processedEnd = text.data();
	std::string_view::size_type lastBreakablePos = std::string_view::npos;
	std::size_t lastBreakableLen = 0;
	unsigned lineWidth = 0;
	CurrentFont currentFont;

	char32_t codepoint = U'\0'; // the current codepoint
	char32_t nextCodepoint;     // the next codepoint
	std::size_t nextCodepointLen;
	std::string_view remaining = text;
	nextCodepoint = DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen);
	do {
		codepoint = nextCodepoint;
		const std::size_t codepointLen = nextCodepointLen;
		if (codepoint == Utf8DecodeError)
			break;
		remaining.remove_prefix(codepointLen);
		nextCodepoint = !remaining.empty() ? DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen) : U'\0';

		if (codepoint == U'\n') { // Existing line break, scan next line
			lastBreakablePos = std::string_view::npos;
			lineWidth = 0;
			output.append(processedEnd, remaining.data());
			processedEnd = remaining.data();
			continue;
		}

		if (codepoint != ZWSP) {
			const uint8_t frame = codepoint & 0xFF;
			if (!currentFont.load(size, text_color::ColorDialogWhite, codepoint)) {
				codepoint = U'?';
				if (!currentFont.load(size, text_color::ColorDialogWhite, codepoint)) {
					app_fatal("Missing fonts");
				}
			}

			lineWidth += (*currentFont.sprite)[frame].width() + spacing;
		}

		if (IsBreakableWhitespace(codepoint)) {
			lastBreakablePos = remaining.data() - begin - codepointLen;
			lastBreakableLen = codepointLen;
			continue;
		}

		if (lineWidth - spacing <= width) {
			if (IsBreakAllowed(codepoint, nextCodepoint)) {
				lastBreakablePos = remaining.data() - begin;
				lastBreakableLen = 0;
			}

			continue; // String is still within the limit, continue to the next symbol
		}

		if (lastBreakablePos == std::string_view::npos) { // Single word longer than width
			lastBreakablePos = remaining.data() - begin - codepointLen;
			lastBreakableLen = 0;
		}

		// Break line and continue to next line
		const char *end = &text[lastBreakablePos];
		output.append(processedEnd, end);
		output += '\n';

		// Restart from the beginning of the new line.
		remaining = text.substr(lastBreakablePos + lastBreakableLen);
		processedEnd = remaining.data();
		lastBreakablePos = std::string_view::npos;
		lineWidth = 0;
		nextCodepoint = !remaining.empty() ? DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen) : U'\0';
	} while (!remaining.empty() && remaining[0] != '\0');
	output.append(processedEnd, remaining.data());
	return output;
}

/**
 * @todo replace Rectangle with cropped Surface
 */
uint32_t DrawString(const Surface &out, std::string_view text, const Rectangle &rect, TextRenderOptions opts)
{
	const GameFontTables size = GetFontSizeFromUiFlags(opts.flags);
	const text_color color = GetColorFromFlags(opts.flags);

	int charactersInLine = 0;
	int lineWidth = 0;
	if (HasAnyOf(opts.flags, (UiFlags::AlignCenter | UiFlags::AlignRight | UiFlags::KerningFitSpacing)))
		lineWidth = GetLineWidth(text, size, opts.spacing, &charactersInLine);

	Point characterPosition { GetLineStartX(opts.flags, rect, lineWidth), rect.position.y };
	const int initialX = characterPosition.x;

	const int rightMargin = rect.position.x + rect.size.width;
	const int bottomMargin = rect.size.height != 0 ? std::min(rect.position.y + rect.size.height + BaseLineOffset[size], out.h()) : out.h();

	if (opts.lineHeight == -1)
		opts.lineHeight = GetLineHeight(text, size);

	if (HasAnyOf(opts.flags, UiFlags::VerticalCenter)) {
		const int textHeight = static_cast<int>((c_count(text, '\n') + 1) * opts.lineHeight);
		characterPosition.y += std::max(0, (rect.size.height - textHeight) / 2);
	}

	characterPosition.y += BaseLineOffset[size];

	const bool outlined = HasAnyOf(opts.flags, UiFlags::Outlined);

	const Surface clippedOut = ClipSurface(out, rect);

	// Only draw the PentaCursor if the cursor is not at the end.
	if (HasAnyOf(opts.flags, UiFlags::PentaCursor) && static_cast<size_t>(opts.cursorPosition) == text.size()) {
		opts.cursorPosition = -1;
	}

	const uint32_t bytesDrawn = DoDrawString(clippedOut, text, rect, characterPosition,
	    lineWidth, charactersInLine, rightMargin, bottomMargin, size, color, outlined, opts);

	if (HasAnyOf(opts.flags, UiFlags::PentaCursor)) {
		const ClxSprite sprite = (*pSPentSpn2Cels)[PentSpn2Spin()];
		MaybeWrap(characterPosition, sprite.width(), rightMargin, initialX, opts.lineHeight);
		ClxDraw(clippedOut, characterPosition + Displacement { 0, opts.lineHeight - BaseLineOffset[size] }, sprite);
	}

	return bytesDrawn;
}

void DrawStringWithColors(const Surface &out, std::string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, const Rectangle &rect, TextRenderOptions opts)
{
	const GameFontTables size = GetFontSizeFromUiFlags(opts.flags);
	const text_color color = GetColorFromFlags(opts.flags);

	int charactersInLine = 0;
	int lineWidth = 0;
	if (HasAnyOf(opts.flags, (UiFlags::AlignCenter | UiFlags::AlignRight | UiFlags::KerningFitSpacing)))
		lineWidth = GetLineWidth(fmt, args, argsLen, 0, size, opts.spacing, &charactersInLine);

	Point characterPosition { GetLineStartX(opts.flags, rect, lineWidth), rect.position.y };
	const int initialX = characterPosition.x;

	const int rightMargin = rect.position.x + rect.size.width;
	const int bottomMargin = rect.size.height != 0 ? std::min(rect.position.y + rect.size.height + BaseLineOffset[size], out.h()) : out.h();

	if (opts.lineHeight == -1)
		opts.lineHeight = GetLineHeight(fmt, args, argsLen, size);

	if (HasAnyOf(opts.flags, UiFlags::VerticalCenter)) {
		const int textHeight = static_cast<int>((CountNewlines(fmt, args, argsLen) + 1) * opts.lineHeight);
		characterPosition.y += std::max(0, (rect.size.height - textHeight) / 2);
	}

	characterPosition.y += BaseLineOffset[size];

	const bool outlined = HasAnyOf(opts.flags, UiFlags::Outlined);

	const Surface clippedOut = ClipSurface(out, rect);

	CurrentFont currentFont;
	const int originalSpacing = opts.spacing;
	if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
		opts.spacing = AdjustSpacingToFitHorizontally(lineWidth, originalSpacing, charactersInLine, rect.size.width);
		if (opts.spacing != originalSpacing && HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
			const int adjustedLineWidth = GetLineWidth(fmt, args, argsLen, 0, size, opts.spacing, &charactersInLine);
			characterPosition.x = GetLineStartX(opts.flags, rect, adjustedLineWidth);
		}
	}

	char32_t prev = U'\0';
	char32_t next;
	std::string_view remaining = fmt;
	FmtArgParser fmtArgParser { fmt, args, argsLen };
	size_t cpLen;

	// The current formatted argument value being processed.
	std::string_view curFormatted;
	text_color curFormattedColor;

	// The string that we're currently processing: either `remaining` or `curFormatted`.
	std::string_view *str;

	for (; !(IsConsumed(curFormatted) && IsConsumed(remaining));
	     str->remove_prefix(cpLen), prev = next) {
		const bool isProcessingFormatArgValue = !IsConsumed(curFormatted);
		str = isProcessingFormatArgValue ? &curFormatted : &remaining;
		next = DecodeFirstUtf8CodePoint(*str, &cpLen);
		if (next == Utf8DecodeError) break;

		// {{ and }} escapes in fmt.
		if (!isProcessingFormatArgValue && (prev == U'{' || prev == U'}') && prev == next) continue;
		// ZWSP are line-breaking opportunities that can otherwise be skipped for rendering as they have 0-width.
		if (next == ZWSP) continue;

		if (!isProcessingFormatArgValue) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(*str);
			if (fmtArgPos.has_value()) {
				// `fmtArgParser` has already consumed `*str`. Ensure the loop doesn't consume any more.
				cpLen = 0;
				// The loop assigns `prev = next`.
				// We reset it to U'\0' to ensure that {{ and }} escapes are not processed accross
				// the boundary of the format string and a formatted value.
				next = U'\0';
				currentFont.clear();
				const DrawStringFormatArg &arg = args[*fmtArgPos];
				curFormatted = arg.GetFormatted();
				curFormattedColor = GetColorFromFlags(arg.GetFlags());
				continue;
			}
		}

		const text_color curColor = isProcessingFormatArgValue ? curFormattedColor : color;
		if (!currentFont.load(size, curColor, next)) {
			next = U'?';
			if (!currentFont.load(size, curColor, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		const uint16_t width = (*currentFont.sprite)[frame].width();
		if (next == U'\n' || characterPosition.x + width > rightMargin) {
			const int nextLineY = characterPosition.y + opts.lineHeight;
			if (nextLineY >= bottomMargin)
				break;
			characterPosition.y = nextLineY;

			if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
				int nextLineWidth = isProcessingFormatArgValue
				    ? GetLineWidth(remaining, args, argsLen, fmtArgParser.offset(), size, originalSpacing, &charactersInLine,
				          /*firstArgOffset=*/args[fmtArgParser.offset() - 1].GetFormatted().size() - (curFormatted.size() - cpLen))
				    : GetLineWidth(remaining.substr(cpLen), args, argsLen, fmtArgParser.offset(), size, originalSpacing, &charactersInLine);
				opts.spacing = AdjustSpacingToFitHorizontally(nextLineWidth, originalSpacing, charactersInLine, rect.size.width);
			}

			if (HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
				lineWidth = width;
				if (str->size() > cpLen) {
					lineWidth += opts.spacing
					    + (isProcessingFormatArgValue
					            ? GetLineWidth(remaining, args, argsLen, fmtArgParser.offset(), size, opts.spacing, &charactersInLine,
					                  /*firstArgOffset=*/args[fmtArgParser.offset() - 1].GetFormatted().size() - (curFormatted.size() - cpLen))
					            : GetLineWidth(remaining.substr(cpLen), args, argsLen, fmtArgParser.offset(), size, opts.spacing, &charactersInLine));
				}
			}
			characterPosition.x = GetLineStartX(opts.flags, rect, lineWidth);

			if (next == U'\n')
				continue;
		}

		DrawFont(clippedOut, characterPosition, (*currentFont.sprite)[frame], curColor, outlined);
		characterPosition.x += width + opts.spacing;
	}

	if (HasAnyOf(opts.flags, UiFlags::PentaCursor)) {
		const ClxSprite sprite = (*pSPentSpn2Cels)[PentSpn2Spin()];
		MaybeWrap(characterPosition, sprite.width(), rightMargin, initialX, opts.lineHeight);
		ClxDraw(clippedOut, characterPosition + Displacement { 0, opts.lineHeight - BaseLineOffset[size] }, sprite);
	}
}

uint8_t PentSpn2Spin()
{
	return GetAnimationFrame(8, 50);
}

bool IsBreakableWhitespace(char32_t c)
{
	return IsAnyOf(c, U' ', U'　', ZWSP);
}

} // namespace devilution
