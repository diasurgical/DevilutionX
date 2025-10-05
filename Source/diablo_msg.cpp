/**
 * @file diablo_msg.cpp
 *
 * Implementation of in-game message functions.
 */

#include <algorithm>
#include <cstdint>
#include <deque>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "diablo_msg.hpp"

#include "DiabloUI/ui_flags.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/info_box.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/language.h"
#include "utils/str_split.hpp"

namespace devilution {

namespace {

struct MessageEntry {
	std::string text;
	uint32_t duration; // Duration in milliseconds
};

std::deque<MessageEntry> DiabloMessages;
uint32_t msgStartTime;
std::vector<std::string> TextLines;
int OuterHeight;
int LineWidth;
int LineHeight;

void InitDiabloMsg()
{
	TextLines.clear();
	if (DiabloMessages.empty())
		return;

	LineWidth = 418;
	const std::string_view text = DiabloMessages.front().text;
	const std::string wrapped = WordWrapString(text, LineWidth, GameFont12);
	for (const std::string_view line : SplitByChar(wrapped, '\n')) {
		LineWidth = std::max(LineWidth, GetLineWidth(text, GameFont12));
		TextLines.emplace_back(line);
	}

	msgStartTime = SDL_GetTicks();
	LineHeight = GetLineHeight(text, GameFont12);
	OuterHeight = static_cast<int>((TextLines.size()) * LineHeight) + 42;
}

} // namespace

/** Maps from error_id to error message. */
const char *const MsgStrings[] = {
	"",
	N_("Game saved"),
	N_("No multiplayer functions in demo"),
	N_("Direct Sound Creation Failed"),
	N_("Not available in shareware version"),
	N_("Not enough space to save"),
	N_("No Pause in town"),
	N_("Copying to a hard disk is recommended"),
	N_("Multiplayer sync problem"),
	N_("No pause in multiplayer"),
	N_("Loading..."),
	N_("Saving..."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Some are weakened as one grows strong"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "New strength is forged through destruction"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Those who defend seldom attack"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "The sword of justice is swift and sharp"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "While the spirit is vigilant the body thrives"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "The powers of mana refocused renews"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Time cannot diminish the power of steel"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Magic is not always what it seems to be"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "What once was opened now is closed"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Intensity comes at the cost of wisdom"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Arcane power brings destruction"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "That which cannot be held cannot be harmed"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Crimson and Azure become as the sun"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Knowledge and wisdom at the cost of self"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Drink and be refreshed"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Wherever you go, there you are"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Energy comes at the cost of wisdom"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Riches abound when least expected"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Where avarice fails, patience gains reward"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Blessed by a benevolent companion!"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "The hands of men may be guided by fate"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Strength is bolstered by heavenly faith"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "The essence of life flows from within"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "The way is made clear when viewed from above"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Salvation comes at the cost of wisdom"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Mysteries are revealed in the light of reason"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Those who are last may yet be first"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Generosity brings its own rewards"),
	N_("You must be at least level 8 to use this."),
	N_("You must be at least level 13 to use this."),
	N_("You must be at least level 17 to use this."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Arcane knowledge gained!"),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "That which does not kill you..."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Knowledge is power."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Give and you shall receive."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Some experience is gained by touch."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "There's no place like home."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "Spiritual energy is restored."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "You feel more agile."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "You feel stronger."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "You feel wiser."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "You feel refreshed."),
	N_(/* TRANSLATORS: Shrine Text. Keep atmospheric. :) */ "That which can break will."),
};

void InitDiabloMsg(diablo_message e, uint32_t duration /*= 3500*/)
{
	InitDiabloMsg(LanguageTranslate(MsgStrings[e]), duration);
}

void InitDiabloMsg(std::string_view msg, uint32_t duration /*= 3500*/)
{
	if (c_find_if(DiabloMessages, [&msg](const MessageEntry &entry) { return entry.text == msg; })
	    != DiabloMessages.end())
		return;

	DiabloMessages.push_back({ std::string(msg), duration });
	if (DiabloMessages.size() == 1) {
		InitDiabloMsg();
	}
}

bool IsDiabloMsgAvailable()
{
	return !DiabloMessages.empty();
}

void CancelCurrentDiabloMsg()
{
	if (!DiabloMessages.empty()) {
		DiabloMessages.pop_front();
		InitDiabloMsg();
	}
}

void ClrDiabloMsg()
{
	DiabloMessages.clear();
}

void DrawDiabloMsg(const Surface &out)
{
	const ClxSpriteList sprites = *pSTextSlidCels;
	const ClxSprite borderCornerTopLeftSprite = sprites[0];
	const ClxSprite borderCornerBottomLeftSprite = sprites[1];
	const ClxSprite borderCornerBottomRightSprite = sprites[2];
	const ClxSprite borderCornerTopRightSprite = sprites[3];
	const ClxSprite borderTopSprite = sprites[4];
	const ClxSprite borderLeftSprite = sprites[5];
	const ClxSprite borderBottomSprite = sprites[6];
	const ClxSprite borderRightSprite = sprites[7];

	const int borderPartWidth = 12;
	const int borderPartHeight = 12;

	const int textPaddingX = 5;
	const int borderThickness = 3;

	const int outerHeight = std::min<int>(out.h(), OuterHeight);
	const int outerWidth = std::min<int>(out.w(), LineWidth + 2 * borderThickness + textPaddingX);
	const int innerWidth = outerWidth - 2 * borderThickness;
	const int lineWidth = innerWidth - textPaddingX;
	const int innerHeight = outerHeight - 2 * borderThickness;

	const Point topLeft { (out.w() - outerWidth) / 2, (out.h() - outerHeight) / 2 };

	const int innerXBegin = topLeft.x + borderThickness;
	const int innerXEnd = innerXBegin + innerWidth;
	const int innerYBegin = topLeft.y + borderThickness;
	const int innerYEnd = innerYBegin + innerHeight;
	const int borderRightX = innerXEnd - (borderPartWidth - borderThickness);
	const int borderBottomY = innerYEnd - (borderPartHeight - borderThickness);

	RenderClxSprite(out, borderCornerTopLeftSprite, topLeft);
	RenderClxSprite(out, borderCornerBottomLeftSprite, { topLeft.x, borderBottomY });
	RenderClxSprite(out, borderCornerBottomRightSprite, { borderRightX, borderBottomY });
	RenderClxSprite(out, borderCornerTopRightSprite, { borderRightX, topLeft.y });

	const Surface horizontalBorderOut = out.subregionX(topLeft.x, outerWidth - borderPartWidth);
	for (int x = borderPartWidth; x < horizontalBorderOut.w(); x += borderPartWidth) {
		RenderClxSprite(horizontalBorderOut, borderTopSprite, { x, topLeft.y });
		RenderClxSprite(horizontalBorderOut, borderBottomSprite, { x, borderBottomY });
	}

	const Surface verticalBorderOut = out.subregionY(topLeft.y, outerHeight - borderPartHeight);
	for (int y = borderPartHeight; y < verticalBorderOut.h(); y += borderPartHeight) {
		RenderClxSprite(verticalBorderOut, borderLeftSprite, { topLeft.x, y });
		RenderClxSprite(verticalBorderOut, borderRightSprite, { borderRightX, y });
	}
	DrawHalfTransparentRectTo(out, innerXBegin, innerYBegin, innerWidth, innerHeight);

	const int textX = innerXBegin + textPaddingX;
	int textY = innerYBegin + (innerHeight - LineHeight * static_cast<int>(TextLines.size())) / 2;
	for (const std::string &line : TextLines) {
		DrawString(out, line, { { textX, textY }, { lineWidth, LineHeight } },
		    { .flags = UiFlags::AlignCenter, .lineHeight = LineHeight });
		textY += LineHeight;
	}

	// Calculate the time the current message has been displayed
	const uint32_t messageElapsedTime = SDL_GetTicks() - msgStartTime;

	// Check if the current message's duration has passed
	if (!DiabloMessages.empty() && messageElapsedTime >= DiabloMessages.front().duration) {
		DiabloMessages.pop_front();
		InitDiabloMsg();
	}
}

} // namespace devilution
