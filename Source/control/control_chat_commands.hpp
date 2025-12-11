/**
 * @file control_chat_commands.hpp
 *
 * Interface of the chat
 */
#pragma once

#include <string_view>

namespace devilution {

bool CheckChatCommand(std::string_view text);

} // namespace devilution
