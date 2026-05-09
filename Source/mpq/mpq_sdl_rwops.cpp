#include "mpq/mpq_sdl_rwops.hpp"

#include <cstdlib>
#include <cstring>

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

constexpr size_t MaxMpqPathSize = 256;

/* -----------------------------------------------------------------------
 * Context structure shared by both SDL2 and SDL3 implementations.
 *
 * Wraps an mpqfs_stream_t (sector-based, on-demand decompression) and,
 * for the threadsafe variant, an independently cloned archive so that
 * reads don't race with the main thread's archive FILE*.
 * ----------------------------------------------------------------------- */

struct MpqStreamCtx {
	mpqfs_stream_t *stream;      /* Sector-based stream (owned)           */
	mpqfs_archive_t *ownedClone; /* Non-null if we cloned for threadsafe  */
};

static void DestroyCtx(MpqStreamCtx *ctx)
{
	if (ctx == nullptr)
		return;
	mpqfs_stream_close(ctx->stream);
	if (ctx->ownedClone != nullptr)
		mpqfs_close(ctx->ownedClone);
	delete ctx;
}

/* -----------------------------------------------------------------------
 * Helper: create the MpqStreamCtx, optionally cloning the archive for
 * thread-safety.  Tries hash-based open first, falls back to filename.
 * ----------------------------------------------------------------------- */

static MpqStreamCtx *CreateCtx(mpqfs_archive_t *archive,
    uint32_t hashIndex,
    const char *filename,
    bool threadsafe)
{
	mpqfs_archive_t *target = archive;
	mpqfs_archive_t *clone = nullptr;

	if (threadsafe) {
		clone = mpqfs_clone(archive);
		if (clone == nullptr)
			return nullptr;
		target = clone;
	}

	mpqfs_stream_t *stream = nullptr;

	/* Try the fast hash-based path first (avoids re-hashing). */
	if (hashIndex != UINT32_MAX)
		stream = mpqfs_stream_open_from_hash(target, hashIndex);

	/* Fall back to filename-based open (needed for encrypted files and
	 * when hashIndex is not available). */
	if (stream == nullptr)
		stream = mpqfs_stream_open(target, filename);

	if (stream == nullptr) {
		if (clone != nullptr)
			mpqfs_close(clone);
		return nullptr;
	}

	auto *ctx = new (std::nothrow) MpqStreamCtx { stream, clone };
	if (ctx == nullptr) {
		mpqfs_stream_close(stream);
		if (clone != nullptr)
			mpqfs_close(clone);
		return nullptr;
	}

	return ctx;
}

/* =======================================================================
 * SDL3 implementation
 * ======================================================================= */

#ifdef USE_SDL3

static Sint64 SDLCALL MpqStream_Size(void *userdata)
{
	auto *ctx = static_cast<MpqStreamCtx *>(userdata);
	return static_cast<Sint64>(mpqfs_stream_size(ctx->stream));
}

static Sint64 SDLCALL MpqStream_Seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
	auto *ctx = static_cast<MpqStreamCtx *>(userdata);
	int w;
	switch (whence) {
	case SDL_IO_SEEK_SET: w = SEEK_SET; break;
	case SDL_IO_SEEK_CUR: w = SEEK_CUR; break;
	case SDL_IO_SEEK_END: w = SEEK_END; break;
	default:
		SDL_SetError("MpqStream_Seek: unknown whence");
		return -1;
	}
	int64_t pos = mpqfs_stream_seek(ctx->stream, offset, w);
	if (pos < 0) {
		SDL_SetError("MpqStream_Seek: seek failed");
		return -1;
	}
	return static_cast<Sint64>(pos);
}

static size_t SDLCALL MpqStream_Read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
	auto *ctx = static_cast<MpqStreamCtx *>(userdata);
	size_t n = mpqfs_stream_read(ctx->stream, ptr, size);
	if (n == static_cast<size_t>(-1)) {
		if (status != nullptr)
			*status = SDL_IO_STATUS_ERROR;
		SDL_SetError("MpqStream_Read: read failed");
		return 0;
	}
	if (n == 0 && size > 0) {
		if (status != nullptr)
			*status = SDL_IO_STATUS_EOF;
	} else {
		if (status != nullptr)
			*status = SDL_IO_STATUS_READY;
	}
	return n;
}

static size_t SDLCALL MpqStream_Write(void * /*userdata*/, const void * /*ptr*/,
    size_t /*size*/, SDL_IOStatus *status)
{
	if (status != nullptr)
		*status = SDL_IO_STATUS_ERROR;
	SDL_SetError("MpqStream_Write: read-only stream");
	return 0;
}

static bool SDLCALL MpqStream_Close(void *userdata)
{
	auto *ctx = static_cast<MpqStreamCtx *>(userdata);
	DestroyCtx(ctx);
	return true;
}

#else /* SDL1 or SDL2 */

/* =======================================================================
 * SDL1 / SDL2 implementation
 *
 * SDL1 uses int for offsets and sizes; SDL2 uses Sint64 and size_t.
 * SDL1 has no ->size callback and no SDL_RWOPS_UNKNOWN.
 * SDL1's SDL_SetError returns void; SDL2's returns int.
 * ======================================================================= */

#ifndef USE_SDL1
using OffsetType = Sint64;
using SizeType = size_t;
#else
using OffsetType = int;
using SizeType = int;
#endif

#ifndef USE_SDL1
static Sint64 SDLCALL MpqStream_Size(SDL_RWops *rw)
{
	auto *ctx = static_cast<MpqStreamCtx *>(rw->hidden.unknown.data1);
	return static_cast<Sint64>(mpqfs_stream_size(ctx->stream));
}
#endif

static OffsetType SDLCALL MpqStream_Seek(SDL_RWops *rw, OffsetType offset, int whence)
{
	auto *ctx = static_cast<MpqStreamCtx *>(rw->hidden.unknown.data1);

	int w;
	switch (whence) {
	case SDL_IO_SEEK_SET: w = SEEK_SET; break;
	case SDL_IO_SEEK_CUR: w = SEEK_CUR; break;
	case SDL_IO_SEEK_END: w = SEEK_END; break;
	default:
		SDL_SetError("MpqStream_Seek: unknown whence");
		return -1;
	}

	int64_t pos = mpqfs_stream_seek(ctx->stream, offset, w);
	if (pos < 0) {
		SDL_SetError("MpqStream_Seek: seek failed");
		return -1;
	}

	return static_cast<OffsetType>(pos);
}

static SizeType SDLCALL MpqStream_Read(SDL_RWops *rw, void *ptr,
    SizeType size, SizeType maxnum)
{
	auto *ctx = static_cast<MpqStreamCtx *>(rw->hidden.unknown.data1);

	size_t totalBytes = static_cast<size_t>(size) * static_cast<size_t>(maxnum);
	size_t n = mpqfs_stream_read(ctx->stream, ptr, totalBytes);
	if (n == static_cast<size_t>(-1))
		return 0;

	/* Return number of whole objects read. */
	return (size > 0) ? static_cast<SizeType>(n / static_cast<size_t>(size)) : 0;
}

static SizeType SDLCALL MpqStream_Write(SDL_RWops * /*rw*/, const void * /*ptr*/,
    SizeType /*size*/, SizeType /*num*/)
{
	SDL_SetError("MpqStream_Write: read-only stream");
	return 0;
}

static int SDLCALL MpqStream_Close(SDL_RWops *rw)
{
	if (rw != nullptr) {
		auto *ctx = static_cast<MpqStreamCtx *>(rw->hidden.unknown.data1);
		DestroyCtx(ctx);
		SDL_FreeRW(rw);
	}
	return 0;
}

#endif /* !USE_SDL3 */

} // namespace

SdlRwopsType *SDL_RWops_FromMpqFile(MpqArchive &archive,
    uint32_t hashIndex,
    std::string_view filename,
    bool threadsafe)
{
	/* NUL-terminate the filename for the C API. */
	char pathBuf[MaxMpqPathSize];
	if (filename.size() >= sizeof(pathBuf))
		return nullptr;
	std::memcpy(pathBuf, filename.data(), filename.size());
	pathBuf[filename.size()] = '\0';

	MpqStreamCtx *ctx = CreateCtx(archive.handle(), hashIndex, pathBuf, threadsafe);
	if (ctx == nullptr)
		return nullptr;

#ifdef USE_SDL3
	SDL_IOStreamInterface iface = {};
	iface.version = sizeof(iface);
	iface.size = MpqStream_Size;
	iface.seek = MpqStream_Seek;
	iface.read = MpqStream_Read;
	iface.write = MpqStream_Write;
	iface.close = MpqStream_Close;

	SdlRwopsType *rwops = SDL_OpenIO(&iface, ctx);
	if (rwops == nullptr) {
		DestroyCtx(ctx);
		return nullptr;
	}

	return rwops;
#else
	SDL_RWops *rwops = SDL_AllocRW();
	if (rwops == nullptr) {
		DestroyCtx(ctx);
		return nullptr;
	}

#ifndef USE_SDL1
	rwops->type = SDL_RWOPS_UNKNOWN;
	rwops->size = MpqStream_Size;
#else
	rwops->type = 0;
#endif
	rwops->seek = MpqStream_Seek;
	rwops->read = MpqStream_Read;
	rwops->write = MpqStream_Write;
	rwops->close = MpqStream_Close;
	rwops->hidden.unknown.data1 = ctx;

	return rwops;
#endif
}

} // namespace devilution