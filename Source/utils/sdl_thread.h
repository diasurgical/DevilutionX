#pragma once

#include <memory>

#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#include "appfat.h"
#include "utils/attributes.h"

namespace devilution {

namespace this_sdl_thread {
inline SDL_threadID get_id()
{
#if defined(__DJGPP__)
	return 1;
#else
	return SDL_GetThreadID(nullptr);
#endif
}
} // namespace this_sdl_thread

#if defined(__DJGPP__)
class SdlThread final {
public:
	SdlThread(int(SDLCALL *handler)(void *), void *data)
	{
		if (handler != nullptr) handler(data);
	}
	explicit SdlThread(void (*handler)(void))
	{
		if (handler != nullptr) handler();
	}
	SdlThread() = default;
	bool joinable() const
	{
		return false;
	}
	SDL_threadID get_id() const
	{
		return this_sdl_thread::get_id();
	}
	void join()
	{
	}
};
#else

class SdlThread final {
	static int SDLCALL ThreadTranslate(void *ptr);
	static void ThreadDeleter(SDL_Thread *thread);

	std::unique_ptr<SDL_Thread, void (*)(SDL_Thread *)> thread { nullptr, ThreadDeleter };

public:
	SdlThread(int(SDLCALL *handler)(void *), void *data)
#ifdef USE_SDL1
	    : thread(SDL_CreateThread(handler, data), ThreadDeleter)
#else
	    : thread(SDL_CreateThread(handler, nullptr, data), ThreadDeleter)
#endif
	{
		if (thread == nullptr)
			ErrSdl();
	}

	explicit SdlThread(void (*handler)(void))
	    : SdlThread(ThreadTranslate, (void *)handler)
	{
	}

	SdlThread() = default;

	bool joinable() const
	{
		return thread != nullptr;
	}

	SDL_threadID get_id() const
	{
		return SDL_GetThreadID(thread.get());
	}

	void join()
	{
		if (!joinable())
			return;
		if (get_id() == this_sdl_thread::get_id())
			app_fatal("Thread joined from within itself");

		SDL_WaitThread(thread.get(), nullptr);
		thread.release();
	}
};

#endif

} // namespace devilution
