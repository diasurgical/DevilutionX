#pragma once

#include <string_view>

namespace devilution {

#ifdef SCREEN_READER_INTEGRATION
void InitializeScreenReader();
void ShutDownScreenReader();
void SpeakText(std::string_view text, bool force = false);
#else
constexpr void InitializeScreenReader()
{
}

constexpr void ShutDownScreenReader()
{
}

constexpr void SpeakText(std::string_view text, bool force = false)
{
}
#endif

} // namespace devilution
