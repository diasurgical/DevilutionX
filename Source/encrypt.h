/**
 * @file encrypt.h
 *
 * Interface of functions for compression and decompressing MPQ data.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace devilution {

uint32_t PkwareCompress(std::byte *srcData, uint32_t size);
uint32_t PkwareDecompress(std::byte *inBuff, uint32_t recvSize, size_t maxBytes);

} // namespace devilution
