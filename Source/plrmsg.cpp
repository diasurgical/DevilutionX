/**
 * @file plrmsg.cpp
 *
 * Implementation of functionality for printing the ingame chat messages.
 */
#include "plrmsg.h"

#include <algorithm>
#include <cstdint>

#include <fmt/format.h>

#include "bannedwords.hpp"
#include "control.h"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "qol/chatlog.h"
#include "qol/stash.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

struct PlayerMessage {
	/** Time message was recived */
	Uint32 time;
	/** The default text color */
	UiFlags style;
	/** The text message to display on screen */
	std::string text;
	/** First portion of text that should be rendered in gold */
	string_view from;
	/** The line height of the text */
	int lineHeight;
};

std::array<PlayerMessage, 8> Messages;

int CountLinesOfText(string_view text)
{
	return 1 + std::count(text.begin(), text.end(), '\n');
}

PlayerMessage &GetNextMessage()
{
	std::move_backward(Messages.begin(), Messages.end() - 1, Messages.end()); // Push back older messages

	return Messages.front();
}

} // namespace

void plrmsg_delay(bool delay)
{
	static uint32_t plrmsgTicks;

	if (delay) {
		plrmsgTicks = -SDL_GetTicks();
		return;
	}

	plrmsgTicks += SDL_GetTicks();
	for (PlayerMessage &message : Messages)
		message.time += plrmsgTicks;
}

void EventPlrMsg(string_view text, UiFlags style)
{
	PlayerMessage &message = GetNextMessage();

	message.style = style;
	message.time = SDL_GetTicks();
	message.text = std::string(text);
	message.from = string_view(message.text.data(), 0);
	message.lineHeight = GetLineHeight(message.text, GameFont12) + 3;
	AddMessageToChatLog(text);
}

void SendPlrMsg(Player &player, string_view text)
{
	PlayerMessage &message = GetNextMessage();

	std::string from = fmt::format(fmt::runtime(_("{:s} (lvl {:d}): ")), player._pName, player._pLevel);

	message.style = UiFlags::ColorWhite;
	message.time = SDL_GetTicks();
	message.text = from + CensorMessage(std::string(text));
	message.from = string_view(message.text.data(), from.size());
	message.lineHeight = GetLineHeight(message.text, GameFont12) + 3;
	AddMessageToChatLog(text, &player);
}

void InitPlrMsg()
{
	Messages = {};
}

void DrawPlrMsg(const Surface &out)
{
	if (ChatLogFlag)
		return;

	int x = 10;
	int y = GetMainPanel().position.y - 13;
	int width = gnScreenWidth - 20;

	if (!talkflag && IsLeftPanelOpen()) {
		x += GetLeftPanel().position.x + GetLeftPanel().size.width;
		width -= GetLeftPanel().size.width;
	}
	if (!talkflag && IsRightPanelOpen())
		width -= gnScreenWidth - GetRightPanel().position.x;

	if (width < 300)
		return;

	width = std::min(540, width);

	for (PlayerMessage &message : Messages) {
		if (message.text.empty())
			break;
		if (!talkflag && SDL_GetTicks() - message.time >= 10000)
			break;

		std::string text = WordWrapString(message.text, width);
		int chatlines = CountLinesOfText(text);
		y -= message.lineHeight * chatlines;

		DrawHalfTransparentRectTo(out, x - 3, y, width + 6, message.lineHeight * chatlines);
		DrawString(out, text, { { x, y }, { width, 0 } }, message.style, 1, message.lineHeight);
		DrawString(out, message.from, { { x, y }, { width, 0 } }, UiFlags::ColorWhitegold, 1, message.lineHeight);
	}
}

} // namespace devilution
