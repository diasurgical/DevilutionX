/**
 * @file encrypt.cpp
 *
 * Implementation of functions for compression and decompressing MPQ data.
 */
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>

#include <pkware.h>

#include "encrypt.h"

namespace devilution {

namespace {

struct TDataInfo {
	std::byte *srcData;
	uint32_t srcOffset;
	uint32_t srcSize;
	std::byte *destData;
	uint32_t destOffset;
	size_t destSize;
	bool error;
};

unsigned int PkwareBufferRead(char *buf, unsigned int *size, void *param) // NOLINT(readability-non-const-parameter)
{
	auto *pInfo = reinterpret_cast<TDataInfo *>(param);

	uint32_t sSize;
	if (*size >= pInfo->srcSize - pInfo->srcOffset) {
		sSize = pInfo->srcSize - pInfo->srcOffset;
	} else {
		sSize = *size;
	}

	memcpy(buf, pInfo->srcData + pInfo->srcOffset, sSize);
	pInfo->srcOffset += sSize;

	return sSize;
}

void PkwareBufferWrite(char *buf, unsigned int *size, void *param) // NOLINT(readability-non-const-parameter)
{
	auto *pInfo = reinterpret_cast<TDataInfo *>(param);

	pInfo->error = pInfo->error || pInfo->destOffset + *size > pInfo->destSize;
	if (pInfo->error) {
		return;
	}

	memcpy(pInfo->destData + pInfo->destOffset, buf, *size);
	pInfo->destOffset += *size;
}

} // namespace

uint32_t PkwareCompress(std::byte *srcData, uint32_t size)
{
	const std::unique_ptr<char[]> ptr = std::make_unique<char[]>(CMP_BUFFER_SIZE);

	unsigned destSize = 2 * size;
	if (destSize < 2 * 4096)
		destSize = 2 * 4096;

	const std::unique_ptr<std::byte[]> destData { new std::byte[destSize] };

	TDataInfo param;
	param.srcData = srcData;
	param.srcOffset = 0;
	param.srcSize = size;
	param.destData = destData.get();
	param.destOffset = 0;
	param.destSize = destSize;
	param.error = false;

	unsigned type = 0;
	unsigned dsize = 4096;
	implode(PkwareBufferRead, PkwareBufferWrite, ptr.get(), &param, &type, &dsize);

	if (param.destOffset < size) {
		memcpy(srcData, destData.get(), param.destOffset);
		size = param.destOffset;
	}

	return size;
}

uint32_t PkwareDecompress(std::byte *inBuff, uint32_t recvSize, size_t maxBytes)
{
	const std::unique_ptr<char[]> ptr = std::make_unique<char[]>(CMP_BUFFER_SIZE);
	const std::unique_ptr<std::byte[]> outBuff { new std::byte[maxBytes] };

	TDataInfo info;
	info.srcData = inBuff;
	info.srcOffset = 0;
	info.srcSize = recvSize;
	info.destData = outBuff.get();
	info.destOffset = 0;
	info.destSize = maxBytes;
	info.error = false;

	explode(PkwareBufferRead, PkwareBufferWrite, ptr.get(), &info);
	if (info.error) {
		return 0;
	}

	memcpy(inBuff, outBuff.get(), info.destOffset);
	return info.destOffset;
}

} // namespace devilution
