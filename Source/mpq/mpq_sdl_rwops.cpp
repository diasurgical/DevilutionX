#include "mpq/mpq_sdl_rwops.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>

#include "utils/sdl_compat.h"
#endif

namespace devilution {

namespace {

struct Data {
	// File information:
	std::optional<MpqArchive> ownedArchive;
	MpqArchive *mpqArchive;
	uint32_t fileNumber;
	size_t blockSize;
	size_t lastBlockSize;
	uint32_t numBlocks;
	size_t size;

	// State:
	size_t position;
	bool blockRead;
	std::unique_ptr<uint8_t[]> blockData;
};

#ifdef USE_SDL3
Data *GetData(void *userdata) { return reinterpret_cast<Data *>(userdata); }
#else
Data *GetData(struct SDL_RWops *context)
{
	return reinterpret_cast<Data *>(context->hidden.unknown.data1);
}

void SetData(struct SDL_RWops *context, Data *data)
{
	context->hidden.unknown.data1 = data;
}
#endif

#ifndef USE_SDL1
using OffsetType = Sint64;
using SizeType = size_t;
#else
using OffsetType = int;
using SizeType = int;
#endif

extern "C" {

#ifndef USE_SDL1
static Sint64 MpqFileRwSize(
#ifdef USE_SDL3
    void *
#else
    struct SDL_RWops *
#endif
        context)
{
	return static_cast<Sint64>(GetData(context)->size);
}
#endif

#ifdef USE_SDL3
static Sint64 MpqFileRwSeek(void *context, Sint64 offset, SDL_IOWhence whence)
#else
static OffsetType MpqFileRwSeek(struct SDL_RWops *context, OffsetType offset, int whence)
#endif
{
	Data &data = *GetData(context);
	OffsetType newPosition;
	switch (whence) {
	case SDL_IO_SEEK_SET:
		newPosition = offset;
		break;
	case SDL_IO_SEEK_CUR:
		newPosition = static_cast<OffsetType>(data.position + offset);
		break;
	case SDL_IO_SEEK_END:
		newPosition = static_cast<OffsetType>(data.size + offset);
		break;
	default:
		return -1;
	}

	if (newPosition == static_cast<OffsetType>(data.position))
		return newPosition;

	if (newPosition > static_cast<OffsetType>(data.size)) {
		SDL_SetError("MpqFileRwSeek beyond EOF (%d > %u)", static_cast<int>(newPosition), static_cast<unsigned>(data.size));
		return -1;
	}

	if (newPosition < 0) {
		SDL_SetError("MpqFileRwSeek beyond BOF (%d < 0)", static_cast<int>(newPosition));
		return -1;
	}

	if (data.position / data.blockSize != static_cast<size_t>(newPosition) / data.blockSize)
		data.blockRead = false;

	data.position = static_cast<size_t>(newPosition);

	return newPosition;
}

#ifdef USE_SDL3
static SizeType MpqFileRwRead(void *context, void *ptr, size_t size, SDL_IOStatus *status)
#else
static SizeType MpqFileRwRead(struct SDL_RWops *context, void *ptr, SizeType size, SizeType maxnum)
#endif
{
#ifdef USE_SDL3
	const size_t maxnum = 1;
#endif
	Data &data = *GetData(context);
	const size_t totalSize = size * maxnum;
	size_t remainingSize = totalSize;

	auto *out = static_cast<uint8_t *>(ptr);

	if (data.blockData == nullptr) {
		data.blockData = std::unique_ptr<uint8_t[]> { new uint8_t[data.blockSize] };
	}

	auto blockNumber = static_cast<uint32_t>(data.position / data.blockSize);
	while (remainingSize > 0) {
		if (data.position == data.size) {
#ifdef USE_SDL3
			*status = SDL_IO_STATUS_EOF;
#endif
			break;
		}

		const size_t currentBlockSize = blockNumber + 1 == data.numBlocks ? data.lastBlockSize : data.blockSize;

		if (!data.blockRead) {
			const int32_t error = data.mpqArchive->ReadBlock(data.fileNumber, blockNumber, data.blockData.get(), currentBlockSize);
			if (error != 0) {
				SDL_SetError("MpqFileRwRead ReadBlock: %s", MpqArchive::ErrorMessage(error));
				return 0;
			}
			data.blockRead = true;
		}

		const size_t blockPosition = data.position - (blockNumber * data.blockSize);
		const size_t remainingBlockSize = currentBlockSize - blockPosition;

		if (remainingSize < remainingBlockSize) {
			std::memcpy(out, data.blockData.get() + blockPosition, remainingSize);
			data.position += remainingSize;
#ifdef USE_SDL3
			return size;
#else
			return maxnum;
#endif
		}

		std::memcpy(out, data.blockData.get() + blockPosition, remainingBlockSize);
		out += remainingBlockSize;
		data.position += remainingBlockSize;
		remainingSize -= remainingBlockSize;
		++blockNumber;
		data.blockRead = false;
	}

#ifdef USE_SDL3
	return static_cast<SizeType>(totalSize - remainingSize);
#else
	return static_cast<SizeType>((totalSize - remainingSize) / size);
#endif
}

#ifdef USE_SDL3
static bool MpqFileRwClose(void *context)
#else
static int MpqFileRwClose(struct SDL_RWops *context)
#endif
{
	Data *data = GetData(context);
	data->mpqArchive->CloseBlockOffsetTable(data->fileNumber);
	delete data;
#ifdef USE_SDL3
	return true;
#else
	delete context;
	return 0;
#endif
}

} // extern "C"

} // namespace

SDL_IOStream *SDL_RWops_FromMpqFile(MpqArchive &mpqArchive, uint32_t fileNumber, std::string_view filename, bool threadsafe)
{
#ifdef USE_SDL3
	SDL_IOStreamInterface interface;
	SDL_INIT_INTERFACE(&interface);
	SDL_IOStreamInterface *result = &interface;
#else
	auto result = std::make_unique<SDL_RWops>();
	std::memset(result.get(), 0, sizeof(*result));
#endif

#ifndef USE_SDL1
	result->size = &MpqFileRwSize;
#ifndef USE_SDL3
	result->type = SDL_RWOPS_UNKNOWN;
#endif
#else
	result->type = 0;
#endif

	result->seek = &MpqFileRwSeek;
	result->read = &MpqFileRwRead;
	result->write = nullptr;
	result->close = &MpqFileRwClose;
#ifdef USE_SDL3
	result->flush = nullptr;
#endif

	auto data = std::make_unique<Data>();
	int32_t error = 0;

	if (threadsafe) {
		data->ownedArchive = mpqArchive.Clone(error);
		if (error != 0) {
			SDL_SetError("MpqFileRwRead Clone: %s", MpqArchive::ErrorMessage(error));
			return nullptr;
		}
		data->mpqArchive = &*data->ownedArchive;
	} else {
		data->mpqArchive = &mpqArchive;
	}
	data->fileNumber = fileNumber;
	MpqArchive &archive = *data->mpqArchive;

	error = archive.OpenBlockOffsetTable(fileNumber, filename);
	if (error != 0) {
		SDL_SetError("MpqFileRwRead OpenBlockOffsetTable: %s", MpqArchive::ErrorMessage(error));
		return nullptr;
	}

	data->size = archive.GetUnpackedFileSize(fileNumber, error);
	if (error != 0) {
		SDL_SetError("MpqFileRwRead GetUnpackedFileSize: %s", MpqArchive::ErrorMessage(error));
		return nullptr;
	}

	const std::uint32_t numBlocks = archive.GetNumBlocks(fileNumber, error);
	if (error != 0) {
		SDL_SetError("MpqFileRwRead GetNumBlocks: %s", MpqArchive::ErrorMessage(error));
		return nullptr;
	}
	data->numBlocks = numBlocks;

	const size_t blockSize = archive.GetBlockSize(fileNumber, 0, error);
	if (error != 0) {
		SDL_SetError("MpqFileRwRead GetBlockSize: %s", MpqArchive::ErrorMessage(error));
		return nullptr;
	}
	data->blockSize = blockSize;

	if (numBlocks > 1) {
		data->lastBlockSize = archive.GetBlockSize(fileNumber, numBlocks - 1, error);
		if (error != 0) {
			SDL_SetError("MpqFileRwRead GetBlockSize: %s", MpqArchive::ErrorMessage(error));
			return nullptr;
		}
	} else {
		data->lastBlockSize = blockSize;
	}

	data->position = 0;
	data->blockRead = false;

#ifdef USE_SDL3
	return SDL_OpenIO(&interface, data.release());
#else
	SetData(result.get(), data.release());
	return result.release();
#endif
}

} // namespace devilution
