#pragma once
#ifdef _DEBUG

#include <expected>
#include <string>
#include <string_view>

#include <sol/forward.hpp>

namespace devilution {

std::expected<std::string, std::string> RunLuaReplLine(std::string_view code);

sol::environment &GetLuaReplEnvironment();

void LuaReplShutdown();

} // namespace devilution
#endif // _DEBUG
