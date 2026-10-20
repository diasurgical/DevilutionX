/* -----------------------------------------------------------------------
 * The audio file for the "ShootFireballBow" sound effect is known to have
 * an invalid WAV format due to a missing pad byte after the data chunk.
 * Furthermore, the smpl chunk causes problems for playback when using
 * SDL3. The easiest way to correct it is to truncate the file after the
 * data chunk and adjust the RIFF chunk length accordingly.
 * ----------------------------------------------------------------------- */

#include "engine/fbbsanitizer.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "engine/assets.hpp"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

constexpr size_t UnsanitizedSize = 0x47b3;
constexpr uint32_t UnsanitizedHash = 0x18a5bac5;

constexpr size_t SanitizedSize = 0x467f;
constexpr size_t SanitizedRiffChunkLength = SanitizedSize - 8;
constexpr size_t RiffChunkLengthLSB = 0x4;
constexpr size_t RiffChunkLengthMSB = RiffChunkLengthLSB + 1;

char FbbAssetPath[] = "sfx\\misc\\fballbow.wav";

uint32_t ComputeHash(std::span<char, UnsanitizedSize> data)
{
	// FNV-1a
	constexpr uint32_t Prime = 16777619U;
	uint32_t hash = 2166136261U;
	for (const char index : data)
		hash = (hash ^ index) * Prime;
	return hash;
}

} // namespace

bool IsFbbAsset(std::string_view filename)
{
	return filename == FbbAssetPath;
}

void SanitizeFbbAsset()
{
	AssetRef ref = FindAsset(FbbAssetPath);
	if (!ref.ok() || ref.size() != UnsanitizedSize)
		return;

	std::array<char, UnsanitizedSize> data;
	{
		AssetHandle handle = OpenAsset(std::move(ref));
		if (!handle.ok() || !handle.read(data.data(), UnsanitizedSize))
			return;
	}

	const uint32_t hash = ComputeHash(data);
	if (hash != UnsanitizedHash)
		return;

	data[RiffChunkLengthLSB] = static_cast<char>(SanitizedRiffChunkLength);
	data[RiffChunkLengthMSB] = static_cast<char>(SanitizedRiffChunkLength >> 8);

	const std::string overrideDirectory = StrCat(paths::PrefPath(), "sfx", DIRECTORY_SEPARATOR_STR, "misc");
	RecursivelyCreateDir(overrideDirectory.c_str());

	const std::string overridePath = StrCat(overrideDirectory, DIRECTORY_SEPARATOR_STR, "fballbow.wav");
	FILE *outfile = OpenFile(overridePath.c_str(), "wb");
	if (outfile == nullptr) {
		LogError("Failed to open {} for writing: {}", overridePath, std::strerror(errno));
		return;
	}

	const size_t write = std::fwrite(data.data(), sizeof(char), SanitizedSize, outfile);
	if (write != SanitizedSize)
		LogError("Write failed {}: {}", overridePath, std::strerror(errno));
	std::fclose(outfile);
}

} // namespace devilution
