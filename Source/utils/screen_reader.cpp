#include "utils/screen_reader.hpp"

#include <string>
#include <string_view>

#ifdef _WIN32
#include "utils/file_util.h"
#include <Tolk.h>
#else
#include <speech-dispatcher/libspeechd.h>
#endif

namespace devilution {

#ifndef _WIN32
SPDConnection *Speechd;
#endif

void InitializeScreenReader()
{
#ifdef _WIN32
	Tolk_Load();
#else
	Speechd = spd_open("DevilutionX", "DevilutionX", NULL, SPD_MODE_SINGLE);
#endif
}

void ShutDownScreenReader()
{
#ifdef _WIN32
	Tolk_Unload();
#else
	spd_close(Speechd);
#endif
}

void SpeakText(std::string_view text, bool force)
{
	static std::string SpokenText;

	if (!force && SpokenText == text)
		return;

	SpokenText = text;

#ifdef _WIN32
	const auto textUtf16 = ToWideChar(SpokenText);
	if (textUtf16 != nullptr)
		Tolk_Output(textUtf16.get(), true);
#else
	spd_say(Speechd, SPD_TEXT, SpokenText.c_str());
#endif
}

} // namespace devilution
