#include <string>
#include <SDL.h>
#ifdef __SWITCH__
#include "platform/switch/network.h"
#endif

#include "devilution.h"

#if !defined(__APPLE__)
extern "C" const char *__asan_default_options()
{
	return "halt_on_error=0";
}
#endif

static std::string build_cmdline(int argc, char **argv)
{
	std::string str;
	for (int i = 1; i < argc; i++) {
		if (i != 1) {
			str += ' ';
		}
		str += argv[i];
	}
	return str;
}

int main(int argc, char **argv)
{
	auto cmdline = build_cmdline(argc, argv);

#ifdef __SWITCH__
	switch_enable_network();
#endif

	return dvl::WinMain(NULL, NULL, (char *)cmdline.c_str(), 0);
}
