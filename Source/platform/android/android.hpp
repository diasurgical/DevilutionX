#pragma once

namespace devilution {
namespace accessibility {

#ifdef __ANDROID__
bool InitializeScreenReaderAndroid();
void ShutDownScreenReaderAndroid();
void SpeakTextAndroid(const char *text);
bool IsScreenReaderEnabledAndroid();
#endif

} // namespace accessibility
} // namespace devilution
