#include "utils/sdl_thread.h"

namespace devilution {

#if !defined(__DJGPP__) && !defined(__EMSCRIPTEN__)
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
