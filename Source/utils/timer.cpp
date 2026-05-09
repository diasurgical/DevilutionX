#include "engine/demomode.h"

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

namespace devilution {

uint32_t GetMillisecondsSinceStartup()
{
	return (demo::IsRunning() || demo::IsRecording()) ? demo::SimulateMillisecondsSinceStartup() : SDL_GetTicks();
}

} // namespace devilution
