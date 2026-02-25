#include "mpq/mpq_sdl_rwops.hpp"

#include <cstring>

#include <mpqfs/mpqfs.h>

namespace devilution {

namespace {
constexpr size_t MaxMpqPathSize = 256;
} // namespace

SdlRwopsType *SDL_RWops_FromMpqFile(MpqArchive &archive,
    uint32_t hashIndex,
    std::string_view filename,
    bool threadsafe)
{
	char pathBuf[MaxMpqPathSize];
	if (filename.size() >= sizeof(pathBuf))
		return nullptr;
	std::memcpy(pathBuf, filename.data(), filename.size());
	pathBuf[filename.size()] = '\0';

	if (threadsafe) {
		if (hashIndex != UINT32_MAX) {
#ifdef USE_SDL3
			SdlRwopsType *result = mpqfs_open_io_threadsafe_from_hash(archive.handle(), hashIndex);
#else
			SdlRwopsType *result = mpqfs_open_rwops_threadsafe_from_hash(archive.handle(), hashIndex);
#endif
			if (result != nullptr)
				return result;
		}
#ifdef USE_SDL3
		return mpqfs_open_io_threadsafe(archive.handle(), pathBuf);
#else
		return mpqfs_open_rwops_threadsafe(archive.handle(), pathBuf);
#endif
	}

	// Non-threadsafe path: use hash-based open if available to avoid re-hashing.
	// This may fail for encrypted files (which need the filename for key derivation),
	// so we fall back to filename-based open on failure.
	if (hashIndex != UINT32_MAX) {
#ifdef USE_SDL3
		SdlRwopsType *result = mpqfs_open_io_from_hash(archive.handle(), hashIndex);
#else
		SdlRwopsType *result = mpqfs_open_rwops_from_hash(archive.handle(), hashIndex);
#endif
		if (result != nullptr)
			return result;
	}

#ifdef USE_SDL3
	return mpqfs_open_io(archive.handle(), pathBuf);
#else
	return mpqfs_open_rwops(archive.handle(), pathBuf);
#endif
}

} // namespace devilution
