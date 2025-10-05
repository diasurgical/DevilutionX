#pragma once

#include <memory>

#ifdef USE_SDL3
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL_mutex.h>
#include <SDL_version.h>
#endif

#include "appfat.h"

namespace devilution {

/*
 * RAII wrapper for SDL_mutex. Satisfies std's "Lockable" (SDL 2) or "BasicLockable" (SDL 1)
 * requirements so it can be used with std::lock_guard and friends.
 */
#ifdef __DJGPP__
class SdlMutex final {
public:
	SdlMutex() noexcept { }
	~SdlMutex() noexcept { }

	SdlMutex(const SdlMutex &) = delete;
	SdlMutex(SdlMutex &&) = delete;
	SdlMutex &operator=(const SdlMutex &) = delete;
	SdlMutex &operator=(SdlMutex &&) = delete;

	void lock() noexcept { }
	void unlock() noexcept { }

	void *get() noexcept { return nullptr; } // Dummy
};
#else
class SdlMutex final {
public:
	SdlMutex()
	    : mutex_(SDL_CreateMutex())
	{
		if (mutex_ == nullptr)
			ErrSdl();
	}

	~SdlMutex()
	{
		SDL_DestroyMutex(mutex_);
	}

	SdlMutex(const SdlMutex &) = delete;
	SdlMutex(SdlMutex &&) = delete;
	SdlMutex &operator=(const SdlMutex &) = delete;
	SdlMutex &operator=(SdlMutex &&) = delete;

	void lock() noexcept // NOLINT(readability-identifier-naming)
	{
#ifdef USE_SDL3
		SDL_LockMutex(mutex_);
#else
		int err = SDL_LockMutex(mutex_);
		if (err == -1) ErrSdl();
#endif
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	[[nodiscard]] bool try_lock() noexcept // NOLINT(readability-identifier-naming)
	{
		const bool ok =
#ifdef USE_SDL3
		    SDL_TryLockMutex(mutex_);
#else
		    SDL_TryLockMutex(mutex_) == 0;
#endif
		return ok;
	}
#endif

	void unlock() noexcept // NOLINT(readability-identifier-naming)
	{
#ifdef USE_SDL3
		SDL_UnlockMutex(mutex_);
#else
		int err = SDL_UnlockMutex(mutex_);
		if (err == -1) ErrSdl();
#endif
	}

#ifdef USE_SDL3
	SDL_Mutex *get() { return mutex_; }
#else
	SDL_mutex *get() { return mutex_; }
#endif

private:
#ifdef USE_SDL3
	SDL_Mutex *mutex_;
#else
	SDL_mutex *mutex_;
#endif
};
#endif

} // namespace devilution
