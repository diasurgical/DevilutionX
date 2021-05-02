/**
 * @file encrypt.cpp
 *
 * Implementation of functions for compression and decompressing MPQ data.
 */
#include <memory>

#include "encrypt.h"

#include "engine.h"
#include "pkware.h"

namespace devilution {

static uint32_t hashtable[5][256];

void Decrypt(uint32_t *castBlock, uint32_t size, uint32_t key)
{
	uint32_t seed = 0xEEEEEEEE;
	for (uint32_t i = 0; i < (size >> 2); i++) {
		uint32_t t = SDL_SwapLE32(*castBlock);
		seed += hashtable[4][(key & 0xFF)];
		t ^= seed + key;
		*castBlock = t;
		seed += t + (seed << 5) + 3;
		castBlock++;
		key = (((key << 0x15) ^ 0xFFE00000) + 0x11111111) | (key >> 0x0B);
	}
}

void Encrypt(uint32_t *castBlock, uint32_t size, uint32_t key)
{
	uint32_t seed = 0xEEEEEEEE;
	for (unsigned i = 0; i < (size >> 2); i++) {
		uint32_t ch = *castBlock;
		uint32_t t = ch;
		seed += hashtable[4][(key & 0xFF)];
		t ^= seed + key;
		*castBlock = SDL_SwapLE32(t);
		castBlock++;
		seed += ch + (seed << 5) + 3;
		key = (((key << 0x15) ^ 0xFFE00000) + 0x11111111) | (key >> 0x0B);
	}
}

uint32_t Hash(const char *s, int type)
{
	uint32_t seed1 = 0x7FED7FED;
	uint32_t seed2 = 0xEEEEEEEE;
	while (s != nullptr && *s) {
		int8_t ch = *s++;
		ch = toupper(ch);
		seed1 = hashtable[type][ch] ^ (seed1 + seed2);
		seed2 += ch + seed1 + (seed2 << 5) + 3;
	}
	return seed1;
}

void InitHash()
{
	uint32_t seed = 0x00100001;

	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 5; j++) {
			seed = (125 * seed + 3) % 0x2AAAAB;
			uint32_t ch = (seed & 0xFFFF);
			seed = (125 * seed + 3) % 0x2AAAAB;
			hashtable[j][i] = ch << 16 | (seed & 0xFFFF);
		}
	}
}

static unsigned int PkwareBufferRead(char *buf, unsigned int *size, void *param)
{
	TDataInfo *pInfo = (TDataInfo *)param;

	uint32_t sSize;
	if (*size >= pInfo->size - pInfo->srcOffset) {
		sSize = pInfo->size - pInfo->srcOffset;
	} else {
		sSize = *size;
	}

	memcpy(buf, pInfo->srcData + pInfo->srcOffset, sSize);
	pInfo->srcOffset += sSize;

	return sSize;
}

static void PkwareBufferWrite(char *buf, unsigned int *size, void *param)
{
	TDataInfo *pInfo = (TDataInfo *)param;

	memcpy(pInfo->destData + pInfo->destOffset, buf, *size);
	pInfo->destOffset += *size;
}

uint32_t PkwareCompress(uint8_t *srcData, uint32_t size)
{
	std::unique_ptr<char[]> ptr { new char[CMP_BUFFER_SIZE] };

	unsigned destSize = 2 * size;
	if (destSize < 2 * 4096)
		destSize = 2 * 4096;

	std::unique_ptr<uint8_t[]> destData { new uint8_t[destSize] };

	TDataInfo param;
	param.srcData = srcData;
	param.srcOffset = 0;
	param.destData = destData.get();
	param.destOffset = 0;
	param.size = size;

	unsigned type = 0;
	unsigned dsize = 4096;
	implode(PkwareBufferRead, PkwareBufferWrite, ptr.get(), &param, &type, &dsize);

	if (param.destOffset < size) {
		memcpy(srcData, destData.get(), param.destOffset);
		size = param.destOffset;
	}

	return size;
}

void PkwareDecompress(uint8_t *inBuff, int recvSize, int maxBytes)
{
	TDataInfo info;

	std::unique_ptr<char[]> ptr { new char[CMP_BUFFER_SIZE] };
	std::unique_ptr<uint8_t[]> outBuff { new uint8_t[maxBytes] };

	info.srcData = inBuff;
	info.srcOffset = 0;
	info.destData = outBuff.get();
	info.destOffset = 0;
	info.size = recvSize;

	explode(PkwareBufferRead, PkwareBufferWrite, ptr.get(), &info);
	memcpy(inBuff, outBuff.get(), info.destOffset);
}

} // namespace devilution
