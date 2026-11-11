#include "mpq/mpq_sdl_ds.hpp"

#include <cstdio>

#include <mpqfs/mpqfs.h>

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>
#ifndef USE_SDL1
#include <SDL_rwops.h>
#endif
#include "utils/sdl_compat.h"
#endif

namespace devilution {

namespace {

size_t SDL_IOStream_Read(void *ds, void *buffer, size_t size)
{
	SDL_IOStream *stream = static_cast<SDL_IOStream *>(ds);
	return SDL_ReadIO(stream, buffer, size);
}

size_t SDL_IOStream_Write(void *ds, const void *buffer, size_t size)
{
	SDL_IOStream *stream = static_cast<SDL_IOStream *>(ds);
	return SDL_WriteIO(stream, buffer, size);
}

int SDL_IOStream_Seek(void *ds, long offset, int origin)
{
	SDL_IOStream *stream = static_cast<SDL_IOStream *>(ds);
	const Sint64 sdlOffset = static_cast<Sint64>(offset);
	SDL_IOWhence whence;
	switch (origin) {
	case SEEK_SET: whence = SDL_IO_SEEK_SET; break;
	case SEEK_CUR: whence = SDL_IO_SEEK_CUR; break;
	case SEEK_END: whence = SDL_IO_SEEK_END; break;
	default:
		SDL_SetError("SDL_IOStream_Seek: unknown origin");
		return -1;
	}
	const Sint64 result = SDL_SeekIO(stream, sdlOffset, whence);
	return result >= 0 ? 0 : -1;
}

int SDL_IOStream_Close(void *ds)
{
	SDL_IOStream *stream = static_cast<SDL_IOStream *>(ds);
	return SDL_CloseIO(stream) ? 0 : EOF;
}

} // namespace

void *OpenAsMpqfsDataSource(const char *path, const char *mode, mpqfs_io_t &io)
{
	io.read = SDL_IOStream_Read;
	io.write = SDL_IOStream_Write;
	io.seek = SDL_IOStream_Seek;
	io.close = SDL_IOStream_Close;

	SDL_IOStream *stream = SDL_IOFromFile(path, mode);
	return static_cast<void *>(stream);
}

} // namespace devilution
