/**
 * @file dc_save_codec.hpp
 * @brief Dreamcast save container helpers
 *
 * Save reads and writes use the zlib-based container format.
 */

#pragma once

#ifdef __DREAMCAST__

#include <cstddef>
#include <memory>

namespace devilution {
namespace dc {

bool WriteCompressedFile(const char *path, const std::byte *data, size_t size);
std::unique_ptr<std::byte[]> ReadCompressedFile(const char *path, size_t &outSize);

bool WriteToVmu(const char *vmuPath, const char *filename,
    const std::byte *data, size_t size);
std::unique_ptr<std::byte[]> ReadFromVmu(const char *vmuPath, const char *filename,
    size_t &outSize);

bool VmuFileExists(const char *vmuPath, const char *filename);

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
