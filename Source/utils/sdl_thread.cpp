#include "utils/sdl_thread.h"

namespace devilution {

#ifndef __DJGPP__
int SDLCALL SdlThread::ThreadTranslate(void *ptr)
{
	auto handler = (void (*)())ptr;

	handler();

	return 0;
}

void SdlThread::ThreadDeleter(SDL_Thread *thread)
{
	if (thread != nullptr)
		app_fatal("Joinable thread destroyed");
}
#endif

} // namespace devilution
