/**
 * @file encrypt.cpp
 *
 * Implementation of functions for compression and decompressing MPQ data.
 */
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <mpqfs/mpqfs.h>

#include "encrypt.h"

namespace devilution {

uint32_t PkwareCompress(std::byte *srcData, uint32_t size)
{
	// Stack buffer covers typical network messages and MPQ sectors (≤ 4096).
	// Falls back to heap for larger payloads.
	constexpr size_t StackBufSize = (4096 * 2) + 64;
	uint8_t stackBuf[StackBufSize];

	size_t dstCap = (static_cast<size_t>(size) * 2) + 64;
	uint8_t *dst;
	std::unique_ptr<uint8_t[]> heapBuf;

	if (dstCap <= StackBufSize) {
		dst = stackBuf;
	} else {
		heapBuf = std::make_unique<uint8_t[]>(dstCap);
		dst = heapBuf.get();
	}

	size_t dstSize = dstCap;
	int rc = mpqfs_pk_implode(
	    reinterpret_cast<const uint8_t *>(srcData), size,
	    dst, &dstSize, /*dict_bits=*/6);

	if (rc == 0 && dstSize < size) {
		std::memcpy(srcData, dst, dstSize);
		return static_cast<uint32_t>(dstSize);
	}

	// Compression didn't help — return original size.
	return size;
}

uint32_t PkwareDecompress(std::byte *inBuff, uint32_t recvSize, size_t maxBytes)
{
	// Stack buffer covers most decompressed network payloads.
	// Falls back to heap for larger buffers.
	constexpr size_t StackBufSize = 8192;
	uint8_t stackBuf[StackBufSize];

	uint8_t *out;
	std::unique_ptr<uint8_t[]> heapBuf;

	if (maxBytes <= StackBufSize) {
		out = stackBuf;
	} else {
		heapBuf = std::make_unique<uint8_t[]>(maxBytes);
		out = heapBuf.get();
	}

	size_t outSize = maxBytes;
	int rc = mpqfs_pk_explode(
	    reinterpret_cast<const uint8_t *>(inBuff), recvSize,
	    out, &outSize);
	if (rc != 0)
		return 0;

	std::memcpy(inBuff, out, outSize);
	return static_cast<uint32_t>(outSize);
}

} // namespace devilution