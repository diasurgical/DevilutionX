/**
 * @file chatlog.h
 *
 * Adds Chat log QoL feature
 */
#pragma once

#include "DiabloUI/ui_flags.hpp"
#include "engine/surface.hpp"
#include "player.h"

namespace devilution {

struct ColoredText {
	std::string text;
	UiFlags color;
};

struct MultiColoredText {
	std::string text;
	std::vector<ColoredText> colors;
};

extern bool ChatLogFlag;
extern std::vector<MultiColoredText> ChatLogLines;
extern unsigned int MessageCounter;

void ToggleChatLog();
void AddMessageToChatLog(std::string_view message, Player *player = nullptr, UiFlags flags = UiFlags::ColorWhite);
void DrawChatLog(const Surface &out);
void ChatLogScrollUp();
void ChatLogScrollDown();
void ChatLogScrollTop();
void ChatLogScrollBottom();

} // namespace devilution
