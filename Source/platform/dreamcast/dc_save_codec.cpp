/**
 * @file dc_save_codec.cpp
 * @brief Dreamcast save compression with zlib
 */

#ifdef __DREAMCAST__

#include "dc_save_codec.hpp"

#include "utils/log.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <zlib.h>

#include <dc/fs_vmu.h>
#include <dc/vmu_pkg.h>
#include <kos/fs.h>

namespace devilution {
namespace dc {

namespace {

constexpr size_t MaxSaveDataSize = 512 * 1024;
constexpr size_t SaveHeaderSize = 16;
constexpr uint8_t SaveFormatVersion = 1;
constexpr char SaveMagic[4] = { 'D', 'X', 'Z', '1' };

enum class SaveCodec : uint8_t {
	Raw = 0,
	Zlib = 1,
};

uint32_t ReadU32LE(const std::byte *data)
{
	const auto *bytes = reinterpret_cast<const uint8_t *>(data);
	return static_cast<uint32_t>(bytes[0])
	    | (static_cast<uint32_t>(bytes[1]) << 8)
	    | (static_cast<uint32_t>(bytes[2]) << 16)
	    | (static_cast<uint32_t>(bytes[3]) << 24);
}

void WriteU32LE(std::byte *destination, uint32_t value)
{
	auto *bytes = reinterpret_cast<uint8_t *>(destination);
	bytes[0] = static_cast<uint8_t>(value & 0xFF);
	bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
	bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

bool IsSaveContainer(const std::byte *data, size_t size)
{
	if (size < SaveHeaderSize)
		return false;

	const auto *bytes = reinterpret_cast<const char *>(data);
	return bytes[0] == SaveMagic[0]
	    && bytes[1] == SaveMagic[1]
	    && bytes[2] == SaveMagic[2]
	    && bytes[3] == SaveMagic[3]
	    && static_cast<uint8_t>(bytes[4]) == SaveFormatVersion;
}

bool DecodeZlib(
    const std::byte *input,
    size_t inputSize,
    std::byte *output,
    size_t outputCapacity,
    size_t &decodedSize)
{
	if (input == nullptr || output == nullptr || outputCapacity == 0)
		return false;
	if (inputSize > static_cast<size_t>(std::numeric_limits<uLong>::max()))
		return false;
	if (outputCapacity > static_cast<size_t>(std::numeric_limits<uLong>::max()))
		return false;

	uLongf size = static_cast<uLongf>(outputCapacity);
	const int rc = uncompress(
	    reinterpret_cast<Bytef *>(output),
	    &size,
	    reinterpret_cast<const Bytef *>(input),
	    static_cast<uLong>(inputSize));
	if (rc != Z_OK)
		return false;
	if (size != outputCapacity)
		return false;

	decodedSize = static_cast<size_t>(size);
	return true;
}

std::unique_ptr<std::byte[]> BuildSaveContainer(const std::byte *data, size_t size, size_t &outSize)
{
	outSize = 0;
	if (data == nullptr || size == 0 || size > MaxSaveDataSize)
		return nullptr;
	if (size > static_cast<size_t>(std::numeric_limits<uLong>::max()))
		return nullptr;

	SaveCodec codec = SaveCodec::Raw;
	size_t payloadSize = size;
	std::unique_ptr<std::byte[]> compressed;
	uLongf compressedSize = compressBound(static_cast<uLong>(size));

	if (compressedSize > 0) {
		compressed = std::make_unique<std::byte[]>(compressedSize);
		const int rc = compress2(
		    reinterpret_cast<Bytef *>(compressed.get()),
		    &compressedSize,
		    reinterpret_cast<const Bytef *>(data),
		    static_cast<uLong>(size),
		    Z_BEST_SPEED);
		if (rc == Z_OK && compressedSize < size) {
			codec = SaveCodec::Zlib;
			payloadSize = static_cast<size_t>(compressedSize);
		}
	}

	if (payloadSize > std::numeric_limits<uint32_t>::max())
		return nullptr;
	if (size > std::numeric_limits<uint32_t>::max())
		return nullptr;

	const size_t containerSize = SaveHeaderSize + payloadSize;
	auto container = std::make_unique<std::byte[]>(containerSize);

	auto *bytes = reinterpret_cast<char *>(container.get());
	bytes[0] = SaveMagic[0];
	bytes[1] = SaveMagic[1];
	bytes[2] = SaveMagic[2];
	bytes[3] = SaveMagic[3];
	bytes[4] = static_cast<char>(SaveFormatVersion);
	bytes[5] = static_cast<char>(codec);
	bytes[6] = 0;
	bytes[7] = 0;
	WriteU32LE(container.get() + 8, static_cast<uint32_t>(payloadSize));
	WriteU32LE(container.get() + 12, static_cast<uint32_t>(size));

	if (codec == SaveCodec::Zlib) {
		std::memcpy(container.get() + SaveHeaderSize, compressed.get(), payloadSize);
		LogVerbose("[DC Save] zlib compressed {} -> {} bytes ({:.1f}%)",
		    size, payloadSize, 100.0f * payloadSize / size);
	} else {
		std::memcpy(container.get() + SaveHeaderSize, data, payloadSize);
		LogVerbose("[DC Save] Stored {} bytes as raw payload", size);
	}

	outSize = containerSize;
	return container;
}

std::unique_ptr<std::byte[]> DecodeSaveContainer(
    const std::byte *data,
    size_t size,
    size_t &outSize,
    const char *sourceTag)
{
	outSize = 0;
	if (!IsSaveContainer(data, size)) {
		LogError("[DC Save] Invalid save container format in {}", sourceTag);
		return nullptr;
	}

	const uint8_t codec = static_cast<uint8_t>(reinterpret_cast<const char *>(data)[5]);
	const uint32_t payloadSize = ReadU32LE(data + 8);
	const uint32_t originalSize = ReadU32LE(data + 12);

	if (originalSize == 0 || originalSize > MaxSaveDataSize) {
		LogError("[DC Save] Invalid container original size {} in {}", originalSize, sourceTag);
		return nullptr;
	}
	if (payloadSize > size - SaveHeaderSize) {
		LogError("[DC Save] Invalid container payload size {} in {}", payloadSize, sourceTag);
		return nullptr;
	}

	const std::byte *payload = data + SaveHeaderSize;
	auto decoded = std::make_unique<std::byte[]>(originalSize);

	if (codec == static_cast<uint8_t>(SaveCodec::Raw)) {
		if (payloadSize < originalSize) {
			LogError("[DC Save] Raw payload too small in {}", sourceTag);
			return nullptr;
		}
		std::memcpy(decoded.get(), payload, originalSize);
		outSize = originalSize;
		return decoded;
	}

	if (codec == static_cast<uint8_t>(SaveCodec::Zlib)) {
		size_t decodedSize = 0;
		if (!DecodeZlib(payload, payloadSize, decoded.get(), originalSize, decodedSize)) {
			LogError("[DC Save] zlib decode failed for {}", sourceTag);
			return nullptr;
		}
		outSize = decodedSize;
		return decoded;
	}

	LogError("[DC Save] Unknown container codec {} in {}", codec, sourceTag);
	return nullptr;
}

bool ReadFileBytes(const char *path, std::unique_ptr<std::byte[]> &buffer, size_t &size)
{
	buffer.reset();
	size = 0;

	FILE *file = std::fopen(path, "rb");
	if (file == nullptr)
		return false;

	if (std::fseek(file, 0, SEEK_END) != 0) {
		std::fclose(file);
		return false;
	}
	const long fileLen = std::ftell(file);
	if (fileLen <= 0) {
		std::fclose(file);
		return false;
	}
	if (std::fseek(file, 0, SEEK_SET) != 0) {
		std::fclose(file);
		return false;
	}

	size = static_cast<size_t>(fileLen);
	buffer = std::make_unique<std::byte[]>(size);
	if (std::fread(buffer.get(), size, 1, file) != 1) {
		std::fclose(file);
		buffer.reset();
		size = 0;
		return false;
	}

	std::fclose(file);
	return true;
}

} // namespace

bool WriteCompressedFile(const char *path, const std::byte *data, size_t size)
{
	if (data == nullptr || size == 0 || size > MaxSaveDataSize || size > std::numeric_limits<uint32_t>::max()) {
		LogError("[DC Save] Refusing to write invalid save payload ({} bytes) to {}", size, path);
		return false;
	}

	size_t containerSize = 0;
	auto container = BuildSaveContainer(data, size, containerSize);
	if (!container || containerSize == 0) {
		LogError("[DC Save] Failed to create save container for {}", path);
		return false;
	}

	FILE *file = std::fopen(path, "wb");
	if (file == nullptr) {
		LogError("[DC Save] Failed to open {} for writing", path);
		return false;
	}

	if (std::fwrite(container.get(), containerSize, 1, file) != 1) {
		LogError("[DC Save] Failed to write container data to {}", path);
		std::fclose(file);
		return false;
	}

	std::fclose(file);
	return true;
}

std::unique_ptr<std::byte[]> ReadCompressedFile(const char *path, size_t &outSize)
{
	std::unique_ptr<std::byte[]> fileBytes;
	size_t fileSize = 0;
	if (!ReadFileBytes(path, fileBytes, fileSize)) {
		outSize = 0;
		return nullptr;
	}
	return DecodeSaveContainer(fileBytes.get(), fileSize, outSize, path);
}

// Blank 32x32 icon (4bpp = 512 bytes) for VMU file display.
static uint8_t g_blankIcon[512] = { 0 };

bool WriteToVmu(const char *vmuPath, const char *filename,
    const std::byte *data, size_t size)
{
	if (data == nullptr || size == 0 || size > MaxSaveDataSize)
		return false;

	size_t payloadSize = 0;
	auto payload = BuildSaveContainer(data, size, payloadSize);
	if (!payload || payloadSize == 0) {
		LogError("[DC VMU] Failed to create save container for {}", filename);
		return false;
	}

	vmu_pkg_t pkg;
	std::memset(&pkg, 0, sizeof(pkg));
	std::strncpy(pkg.desc_short, "DevilutionX", sizeof(pkg.desc_short) - 1);
	std::strncpy(pkg.desc_long, "DevilutionX Save Data", sizeof(pkg.desc_long) - 1);
	std::strncpy(pkg.app_id, "DevilutionX", sizeof(pkg.app_id) - 1);
	pkg.icon_cnt = 1;
	pkg.icon_anim_speed = 0;
	pkg.icon_data = g_blankIcon;
	pkg.eyecatch_type = VMUPKG_EC_NONE;

	const std::string fullPath = std::string(vmuPath) + filename;
	fs_unlink(fullPath.c_str());

	file_t fd = fs_open(fullPath.c_str(), O_WRONLY);
	if (fd == FILEHND_INVALID) {
		LogError("[DC VMU] Cannot open {} for writing", fullPath);
		return false;
	}

	fs_vmu_set_header(fd, &pkg);
	const ssize_t written = fs_write(fd, payload.get(), payloadSize);
	const int closeRet = fs_close(fd);

	if (written < 0 || written != static_cast<ssize_t>(payloadSize)) {
		LogError("[DC VMU] Short write to {}: {} of {}", fullPath, written, payloadSize);
		return false;
	}
	if (closeRet < 0) {
		LogError("[DC VMU] Close failed for {} (VMU full?)", fullPath);
		return false;
	}

	LogVerbose("[DC VMU] Saved {} ({} -> {} bytes on VMU)", fullPath, size, payloadSize);
	return true;
}

std::unique_ptr<std::byte[]> ReadFromVmu(const char *vmuPath, const char *filename,
    size_t &outSize)
{
	outSize = 0;
	const std::string fullPath = std::string(vmuPath) + filename;

	file_t fd = fs_open(fullPath.c_str(), O_RDONLY);
	if (fd == FILEHND_INVALID)
		return nullptr;

	size_t total = fs_total(fd);
	if (total == static_cast<size_t>(-1) || total < sizeof(uint32_t)) {
		fs_close(fd);
		return nullptr;
	}

	auto rawBuf = std::make_unique<std::byte[]>(total);
	const ssize_t bytesRead = fs_read(fd, rawBuf.get(), total);
	fs_close(fd);

	if (bytesRead < static_cast<ssize_t>(sizeof(uint32_t)))
		return nullptr;

	return DecodeSaveContainer(rawBuf.get(), static_cast<size_t>(bytesRead), outSize, fullPath.c_str());
}

bool VmuFileExists(const char *vmuPath, const char *filename)
{
	const std::string fullPath = std::string(vmuPath) + filename;
	file_t fd = fs_open(fullPath.c_str(), O_RDONLY);
	if (fd == FILEHND_INVALID)
		return false;
	fs_close(fd);
	return true;
}

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
