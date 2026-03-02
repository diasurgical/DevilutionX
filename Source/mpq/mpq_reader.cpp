#include "mpq/mpq_reader.hpp"

#include <cstring>
#include <utility>

#include <mpqfs/mpqfs.h>

#include "utils/file_util.h"

namespace devilution {

// Helper: NUL-terminate a string_view into a stack buffer.
// Returns false if the name doesn't fit.
static bool CopyToPathBuf(std::string_view sv, char *buf, size_t bufSize)
{
	if (sv.size() >= bufSize) return false;
	std::memcpy(buf, sv.data(), sv.size());
	buf[sv.size()] = '\0';
	return true;
}

MpqArchive::MpqArchive(std::string path, mpqfs_archive_t *archive)
    : path_(std::move(path))
    , archive_(archive)
{
}

MpqArchive::MpqArchive(MpqArchive &&other) noexcept
    : path_(std::move(other.path_))
    , archive_(other.archive_)
{
	other.archive_ = nullptr;
}

MpqArchive &MpqArchive::operator=(MpqArchive &&other) noexcept
{
	if (this != &other) {
		mpqfs_close(archive_);
		path_ = std::move(other.path_);
		archive_ = other.archive_;
		other.archive_ = nullptr;
	}
	return *this;
}

MpqArchive::~MpqArchive()
{
	mpqfs_close(archive_);
}

std::optional<MpqArchive> MpqArchive::Open(const char *path, int32_t &error)
{
	if (!FileExists(path)) {
		error = 0;
		return std::nullopt;
	}
	mpqfs_archive_t *handle = mpqfs_open(path);
	if (!handle) {
		error = -1;
		return std::nullopt;
	}
	error = 0;
	return MpqArchive(path, handle);
}

std::optional<MpqArchive> MpqArchive::Clone(int32_t &error)
{
	mpqfs_archive_t *clone = mpqfs_clone(archive_);
	if (!clone) {
		error = -1;
		return std::nullopt;
	}
	error = 0;
	return MpqArchive(path_, clone);
}

const char *MpqArchive::ErrorMessage()
{
	const char *msg = mpqfs_last_error();
	return msg ? msg : "Unknown error";
}

bool MpqArchive::HasFile(std::string_view filename) const
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf)))
		return false;
	return mpqfs_has_file(archive_, buf);
}

size_t MpqArchive::GetFileSize(std::string_view filename) const
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf)))
		return 0;
	return mpqfs_file_size(archive_, buf);
}

uint32_t MpqArchive::FindHash(std::string_view filename) const
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf)))
		return UINT32_MAX;
	return mpqfs_find_hash(archive_, buf);
}

bool MpqArchive::HasFileHash(uint32_t hash) const
{
	return mpqfs_has_file_hash(archive_, hash);
}

size_t MpqArchive::GetFileSizeFromHash(uint32_t hash) const
{
	return mpqfs_file_size_from_hash(archive_, hash);
}

std::unique_ptr<std::byte[]> MpqArchive::ReadFile(
    std::string_view filename, std::size_t &fileSize, int32_t &error)
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf))) {
		error = -1;
		return nullptr;
	}

	const size_t size = mpqfs_file_size(archive_, buf);
	if (size == 0) {
		error = -1;
		return nullptr;
	}

	auto result = std::make_unique<std::byte[]>(size);
	const size_t read = mpqfs_read_file_into(archive_, buf,
	    result.get(), size);
	if (read == 0) {
		error = -1;
		return nullptr;
	}

	error = 0;
	fileSize = read;
	return result;
}

} // namespace devilution
