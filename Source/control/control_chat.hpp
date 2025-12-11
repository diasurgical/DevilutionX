#pragma once

#include <string_view>

#include "engine/clx_sprite.hpp"

namespace devilution {

extern OptionalOwnedClxSpriteList talkButtons;
extern std::optional<TextInputState> ChatInputState;
extern char TalkMessage[MAX_SEND_STR_LEN];
extern bool TalkButtonsDown[3];
extern int sgbPlrTalkTbl;
extern bool WhisperList[MAX_PLRS];

bool CheckChatCommand(std::string_view text);

} // namespace devilution
