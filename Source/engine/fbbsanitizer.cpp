#include "engine/fbbsanitizer.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "engine/assets.hpp"
#include "utils/endian_read.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

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

constexpr int SanitizedSize = 0x467f;
constexpr int SanitizedRiffChunkLength = SanitizedSize - 8;
constexpr int RiffChunkLengthLSB = 0x4;
constexpr int RiffChunkLengthMSB = RiffChunkLengthLSB + 1;

constexpr int UnsanitizedSize = 0x47b3;
constexpr int UnsanitizedRiffChunkLength = UnsanitizedSize - 8;
constexpr int DataChunkPosition = 0x24;
constexpr int DataChunkLength = 0x4653;

char fbbAssetPath[] = "sfx\\misc\\fballbow.wav";

/* -----------------------------------------------------------------------
 * Context structure shared by both SDL2 and SDL3 implementations.
 *
 * Wraps an underlying "raw" stream of bytes derived from the original
 * asset data for the "ShootFireballBow" sound effect. This audio file is
 * known to have an invalid WAV format due to a missing pad byte after the
 * data chunk. Furthermore, the smpl chunk causes problems for playback
 * when using SDL3. The easiest way to correct it is to truncate the file
 * after the data chunk and adjust the RIFF chunk length accordingly.
 * ----------------------------------------------------------------------- */

struct FbbSanitizerCtx {
	SDL_IOStream *rawStream;
	int position;
};

/* =======================================================================
 * SDL3 implementation
 * ======================================================================= */

#ifdef USE_SDL3

Sint64 SDLCALL FbbSanitizer_Size(void * /*userdata*/)
{
	return SanitizedSize;
}

Sint64 SDLCALL FbbSanitizer_Seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
	auto *ctx = static_cast<FbbSanitizerCtx *>(userdata);

	const Sint64 result = SDL_SeekIO(ctx->rawStream, offset, whence);
	if (result < 0)
		return result;

	int origin;
	switch (whence) {
	case SDL_IO_SEEK_SET: origin = 0; break;
	case SDL_IO_SEEK_CUR: origin = ctx->position; break;
	case SDL_IO_SEEK_END: origin = SanitizedSize; break;
	default:
		SDL_SetError("FbbSanitizer_Seek: unknown whence");
		return -1;
	}

	ctx->position = origin + static_cast<int>(offset);
	return ctx->position;
}

size_t SDLCALL FbbSanitizer_Read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
	auto *ctx = static_cast<FbbSanitizerCtx *>(userdata);

	const size_t maxSize = static_cast<size_t>(SanitizedSize - ctx->position);
	const size_t readSize = std::min(size, maxSize);
	const size_t read = SDL_ReadIO(ctx->rawStream, ptr, readSize);

	// If the RIFF chunk length was returned by the read operation, fix it here
	char *c = static_cast<char *>(ptr);
	const size_t lsbOffset = static_cast<size_t>(RiffChunkLengthLSB - ctx->position);
	const size_t msbOffset = static_cast<size_t>(RiffChunkLengthMSB - ctx->position);
	const int newPosition = ctx->position + static_cast<int>(read);
	if (ctx->position <= RiffChunkLengthLSB && newPosition > RiffChunkLengthLSB)
		c[lsbOffset] = SanitizedRiffChunkLength & 0xFF;
	if (ctx->position <= RiffChunkLengthMSB && newPosition > RiffChunkLengthMSB)
		c[msbOffset] = (SanitizedRiffChunkLength >> 8) & 0xFF;

	ctx->position += static_cast<int>(read);
	if (read < readSize)
		*status = SDL_GetIOStatus(ctx->rawStream);
	else if (readSize == maxSize)
		*status = SDL_IO_STATUS_EOF;
	return read;
}

size_t SDLCALL FbbSanitizer_Write(void * /*userdata*/, const void * /*ptr*/,
    size_t /*size*/, SDL_IOStatus *status)
{
	if (status != nullptr)
		*status = SDL_IO_STATUS_READONLY;
	SDL_SetError("FbbSanitizer_Write: read-only stream");
	return 0;
}

bool SDLCALL FbbSanitizer_Close(void *userdata)
{
	if (userdata == nullptr)
		return true;
	auto *ctx = static_cast<FbbSanitizerCtx *>(userdata);
	bool closed = SDL_CloseIO(ctx->rawStream);
	delete ctx;
	return closed;
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
Sint64 SDLCALL FbbSanitizer_Size(SDL_RWops * /*rw*/)
{
	return SanitizedSize;
}
#endif

OffsetType SDLCALL FbbSanitizer_Seek(SDL_RWops *rw, OffsetType offset, int whence)
{
	auto *ctx = static_cast<FbbSanitizerCtx *>(rw->hidden.unknown.data1);

	const OffsetType result = SDL_SeekIO(ctx->rawStream, offset, whence);
	if (result < 0)
		return result;

	int origin;
	switch (whence) {
	case SDL_IO_SEEK_SET: origin = 0; break;
	case SDL_IO_SEEK_CUR: origin = ctx->position; break;
	case SDL_IO_SEEK_END: origin = SanitizedSize; break;
	default:
		SDL_SetError("FbbSanitizer_Seek: unknown whence");
		return -1;
	}

	ctx->position = origin + static_cast<int>(offset);
	return ctx->position;
}

SizeType SDLCALL FbbSanitizer_Read(SDL_RWops *rw, void *ptr,
    SizeType size, SizeType maxnum)
{
	auto *ctx = static_cast<FbbSanitizerCtx *>(rw->hidden.unknown.data1);

	const SizeType remaining = static_cast<SizeType>(SanitizedSize - ctx->position) / size;
	const SizeType readnum = std::min(maxnum, remaining);
	const size_t readSize = static_cast<size_t>(size * readnum);
	const size_t read = SDL_ReadIO(ctx->rawStream, ptr, readSize);
	if (static_cast<SizeType>(read) % size != 0) {
		SDL_SetError("FbbSanitizer_Read: failed to read expected number of bytes");
		return 0;
	}

	// If the RIFF chunk length was returned by the read operation, fix it here
	char *c = static_cast<char *>(ptr);
	const size_t lsbOffset = static_cast<size_t>(RiffChunkLengthLSB - ctx->position);
	const size_t msbOffset = static_cast<size_t>(RiffChunkLengthMSB - ctx->position);
	const int newPosition = ctx->position + static_cast<int>(read);
	if (ctx->position <= RiffChunkLengthLSB && newPosition > RiffChunkLengthLSB)
		c[lsbOffset] = SanitizedRiffChunkLength & 0xFF;
	if (ctx->position <= RiffChunkLengthMSB && newPosition > RiffChunkLengthMSB)
		c[msbOffset] = (SanitizedRiffChunkLength >> 8) & 0xFF;

	ctx->position = newPosition;
	return static_cast<SizeType>(read) / size;
}

SizeType SDLCALL FbbSanitizer_Write(SDL_RWops * /*rw*/, const void * /*ptr*/,
    SizeType /*size*/, SizeType /*num*/)
{
	SDL_SetError("FbbSanitizer_Write: read-only stream");
	return 0;
}

int SDLCALL FbbSanitizer_Close(SDL_RWops *rw)
{
	if (rw == nullptr)
		return 0;
	auto *ctx = static_cast<FbbSanitizerCtx *>(rw->hidden.unknown.data1);
	int closed = SDL_CloseIO(ctx->rawStream);
	SDL_FreeRW(rw);
	return closed;
}

#endif /* !USE_SDL3 */

} // namespace

bool IsFbbAsset(std::string_view filename)
{
	return filename == fbbAssetPath;
}

bool FbbAssetNeedsSanitizer()
{
	SDL_IOStream *stream = OpenAssetAsSdlRwOps(fbbAssetPath);
	if (stream == nullptr)
		return false;

	Sint64 size = SDL_GetIOSize(stream);
	if (static_cast<int>(size) != UnsanitizedSize) {
		SDL_CloseIO(stream);
		return false;
	}

	char buf[12];
	const size_t headerRead = SDL_ReadIO(stream, static_cast<void *>(buf), 12);
	if (headerRead != 12) {
		SDL_CloseIO(stream);
		return false;
	}

	std::string_view riffChunkType { buf, 4 };
	const int riffChunkLength = LoadLE32(&buf[4]);
	std::string_view waveChunkType { &buf[8], 4 };
	if (riffChunkType != "RIFF" || riffChunkLength != UnsanitizedRiffChunkLength || waveChunkType != "WAVE") {
		SDL_CloseIO(stream);
		return false;
	}

	Sint64 position = SDL_SeekIO(stream, DataChunkPosition, SDL_IO_SEEK_SET);
	if (position != DataChunkPosition) {
		SDL_CloseIO(stream);
		return false;
	}

	const size_t dataChunkRead = SDL_ReadIO(stream, static_cast<void *>(buf), 8);
	if (dataChunkRead != 8) {
		SDL_CloseIO(stream);
		return false;
	}

	std::string_view dataChunkType { buf, 4 };
	const int dataChunkLength = LoadLE32(&buf[4]);
	SDL_CloseIO(stream);
	return dataChunkType == "data" && dataChunkLength == DataChunkLength;
}

SDL_IOStream *OpenSanitizedFbbAsSdlRwOps(bool threadsafe)
{
	SDL_IOStream *rawStream = OpenAssetAsSdlRwOps(fbbAssetPath, threadsafe);
	if (rawStream == nullptr)
		return nullptr;

	auto *ctx = new (std::nothrow) FbbSanitizerCtx { rawStream, 0 };
	if (ctx == nullptr) {
		SDL_CloseIO(rawStream);
		return nullptr;
	}

#ifdef USE_SDL3
	SDL_IOStreamInterface iface = {};
	iface.version = sizeof(iface);
	iface.size = FbbSanitizer_Size;
	iface.seek = FbbSanitizer_Seek;
	iface.read = FbbSanitizer_Read;
	iface.write = FbbSanitizer_Write;
	iface.close = FbbSanitizer_Close;

	SdlRwopsType *rwops = SDL_OpenIO(&iface, ctx);
	if (rwops == nullptr) {
		SDL_CloseIO(rawStream);
		delete ctx;
		return nullptr;
	}

	return rwops;
#else
	SDL_IOStream *rwops = SDL_AllocRW();
	if (rwops == nullptr) {
		SDL_CloseIO(rawStream);
		delete ctx;
		return nullptr;
	}

#ifndef USE_SDL1
	rwops->type = SDL_RWOPS_UNKNOWN;
	rwops->size = FbbSanitizer_Size;
#else
	rwops->type = 0;
#endif
	rwops->seek = FbbSanitizer_Seek;
	rwops->read = FbbSanitizer_Read;
	rwops->write = FbbSanitizer_Write;
	rwops->close = FbbSanitizer_Close;
	rwops->hidden.unknown.data1 = ctx;

	return rwops;
#endif
}

ArraySharedPtr<std::uint8_t> LoadSanitizedFbbAsset(size_t &size)
{
	SDL_IOStream *stream = OpenSanitizedFbbAsSdlRwOps();
	if (stream == nullptr)
		return nullptr;

	const size_t readSize = static_cast<size_t>(SanitizedSize);
	auto waveFile = MakeArraySharedPtr<std::uint8_t>(readSize);
	const size_t read = SDL_ReadIO(stream, static_cast<void *>(waveFile.get()), readSize);
	SDL_CloseIO(stream);
	if (read != readSize)
		return nullptr;
	size = readSize;
	return waveFile;
}

} // namespace devilution
