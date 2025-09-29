/**
 * @file error.cpp
 *
 * Implementation of in-game message functions.
 */

#include <cstdint>
#include <deque>

#include "error.h"

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/info_box.hpp"
#include "stores.h"
#include "utils/language.h"

namespace devilution {

namespace {

struct MessageEntry {
	std::string text;
	uint32_t duration; // Duration in milliseconds
};

std::deque<MessageEntry> DiabloMessages;
uint32_t msgStartTime = 0;
std::vector<std::string> TextLines;
int ErrorWindowHeight = 54;
const int LineHeight = 12;
const int LineWidth = 418;

void InitNextLines()
{
	TextLines.clear();

	const std::string paragraphs = WordWrapString(DiabloMessages.front().text, LineWidth, GameFont12, 1);

	size_t previous = 0;
	while (true) {
		size_t next = paragraphs.find('\n', previous);
		TextLines.emplace_back(paragraphs.substr(previous, next - previous));
		if (next == std::string::npos)
			break;
		previous = next + 1;
	}

	ErrorWindowHeight = std::max(54, static_cast<int>((TextLines.size() * LineHeight) + 42));
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

void InitDiabloMsg(string_view msg, uint32_t duration /*= 3500*/)
{
	if (DiabloMessages.size() >= MAX_SEND_STR_LEN)
		return;

	if (std::find_if(DiabloMessages.begin(), DiabloMessages.end(),
	        [&msg](const MessageEntry &entry) { return entry.text == msg; })
	    != DiabloMessages.end())
		return;

	DiabloMessages.push_back({ std::string(msg), duration });
	if (DiabloMessages.size() == 1) {
		InitNextLines();
		msgStartTime = SDL_GetTicks();
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
		if (!DiabloMessages.empty()) {
			InitNextLines();
			msgStartTime = SDL_GetTicks();
		}
	}
}

void ClrDiabloMsg()
{
	DiabloMessages.clear();
}

void DrawDiabloMsg(const Surface &out)
{
	auto &uiRectanglePosition = GetUIRectangle().position;
	int dialogStartY = ((gnScreenHeight - GetMainPanel().size.height) / 2) - (ErrorWindowHeight / 2) + 9;

	ClxDraw(out, { uiRectanglePosition.x + 101, dialogStartY }, (*pSTextSlidCels)[0]);
	ClxDraw(out, { uiRectanglePosition.x + 101, dialogStartY + ErrorWindowHeight - 6 }, (*pSTextSlidCels)[1]);
	ClxDraw(out, { uiRectanglePosition.x + 527, dialogStartY + ErrorWindowHeight - 6 }, (*pSTextSlidCels)[2]);
	ClxDraw(out, { uiRectanglePosition.x + 527, dialogStartY }, (*pSTextSlidCels)[3]);

	int sx = uiRectanglePosition.x + 109;
	for (int i = 0; i < 35; i++) {
		ClxDraw(out, { sx, dialogStartY }, (*pSTextSlidCels)[4]);
		ClxDraw(out, { sx, dialogStartY + ErrorWindowHeight - 6 }, (*pSTextSlidCels)[6]);
		sx += 12;
	}
	int drawnYborder = 12;
	while ((drawnYborder + 12) < ErrorWindowHeight) {
		ClxDraw(out, { uiRectanglePosition.x + 101, dialogStartY + drawnYborder }, (*pSTextSlidCels)[5]);
		ClxDraw(out, { uiRectanglePosition.x + 527, dialogStartY + drawnYborder }, (*pSTextSlidCels)[7]);
		drawnYborder += 12;
	}

	DrawHalfTransparentRectTo(out, uiRectanglePosition.x + 104, dialogStartY - 8, 432, ErrorWindowHeight);

	int lineNumber = 0;
	for (auto &line : TextLines) {
		DrawString(out, line, { { uiRectanglePosition.x + 109, dialogStartY + 12 + lineNumber * LineHeight }, { LineWidth, LineHeight } }, { UiFlags::AlignCenter, 1, LineHeight });
		lineNumber += 1;
	}

	// Calculate the time the current message has been displayed
	uint32_t currentTime = SDL_GetTicks();
	uint32_t messageElapsedTime = currentTime - msgStartTime;

	// Check if the current message's duration has passed
	if (!DiabloMessages.empty() && messageElapsedTime >= DiabloMessages.front().duration) {
		DiabloMessages.pop_front();
		if (!DiabloMessages.empty()) {
			InitNextLines();
			msgStartTime = currentTime;
		}
	}
}

} // namespace devilution
