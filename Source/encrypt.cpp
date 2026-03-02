/**
 * @file encrypt.cpp
 *
 * Implementation of functions for compression and decompressing MPQ data.
 */
#include <cstdint>
#include <cstring>
#include <memory>

#include <mpqfs/mpqfs.h>

#include "encrypt.h"

namespace devilution {

uint32_t PkwareCompress(std::byte *srcData, uint32_t size)
{
	size_t dstCap = size * 2 + 64;
	auto dst = std::make_unique<uint8_t[]>(dstCap);
	size_t dstSize = dstCap;

	int rc = mpqfs_pk_implode(
	    reinterpret_cast<const uint8_t *>(srcData), size,
	    dst.get(), &dstSize, /*dict_bits=*/6);

	if (rc == 0 && dstSize < size) {
		std::memcpy(srcData, dst.get(), dstSize);
		return static_cast<uint32_t>(dstSize);
	}

	// Compression didn't help — return original size.
	return size;
}

uint32_t PkwareDecompress(std::byte *inBuff, uint32_t recvSize, size_t maxBytes)
{
	auto out = std::make_unique<uint8_t[]>(maxBytes);
	size_t outSize = maxBytes;

	int rc = mpqfs_pk_explode(
	    reinterpret_cast<const uint8_t *>(inBuff), recvSize,
	    out.get(), &outSize);
	if (rc != 0)
		return 0;

	std::memcpy(inBuff, out.get(), outSize);
	return static_cast<uint32_t>(outSize);
}

} // namespace devilution
