#include "mpq/mpq_reader.hpp"

#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include <mpqfs/mpqfs.h>

#include "utils/str_cat.hpp"

namespace devilution {

namespace {

// Helper: NUL-terminate a string_view into a stack buffer.
// Returns false if the name doesn't fit.
bool CopyToPathBuf(std::string_view sv, char *buf, size_t bufSize)
{
	if (sv.size() >= bufSize) return false;
	std::memcpy(buf, sv.data(), sv.size());
	buf[sv.size()] = '\0';
	return true;
}

// mpqfs_error_message() only describes the general category of failure; for
// I/O errors the specific OS error is only available via errno immediately
// after the failing call, so it's folded in here for diagnostics.
std::string FormatMpqfsError(mpqfs_error_code code)
{
	if (code == MPQFS_ERR_IO) {
		return StrCat(mpqfs_error_message(code), ": ", std::strerror(errno));
	}
	return mpqfs_error_message(code);
}

} // namespace

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

tl::expected<MpqArchive, std::string> MpqArchive::Open(const char *path)
{
	mpqfs_archive_t *handle = nullptr;
	const mpqfs_error_code code = mpqfs_open(path, &handle);
	if (code != MPQFS_OK) {
		return tl::make_unexpected(FormatMpqfsError(code));
	}
	return MpqArchive(path, handle);
}

tl::expected<MpqArchive, std::string> MpqArchive::Clone()
{
	mpqfs_archive_t *clone = nullptr;
	const mpqfs_error_code code = mpqfs_clone(archive_, &clone);
	if (code != MPQFS_OK) {
		return tl::make_unexpected(FormatMpqfsError(code));
	}
	return MpqArchive(path_, clone);
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
	size_t size = 0;
	if (mpqfs_file_size(archive_, buf, &size) != MPQFS_OK)
		return 0;
	return size;
}

uint32_t MpqArchive::FindHash(std::string_view filename) const
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf))) {
		return std::numeric_limits<uint32_t>::max();
	}
	return mpqfs_find_hash(archive_, buf);
}

bool MpqArchive::HasFileHash(uint32_t hash) const
{
	return mpqfs_has_file_hash(archive_, hash);
}

size_t MpqArchive::GetFileSizeFromHash(uint32_t hash) const
{
	size_t size = 0;
	if (mpqfs_file_size_from_hash(archive_, hash, &size) != MPQFS_OK)
		return 0;
	return size;
}

std::unique_ptr<std::byte[]> MpqArchive::ReadFile(
    std::string_view filename, std::size_t &fileSize, int32_t &error)
{
	char buf[256];
	if (!CopyToPathBuf(filename, buf, sizeof(buf))) {
		error = -1;
		return nullptr;
	}

	size_t size = 0;
	mpqfs_error_code code = mpqfs_file_size(archive_, buf, &size);
	if (code != MPQFS_OK) {
		error = static_cast<int32_t>(code);
		return nullptr;
	}

	auto result = std::make_unique<std::byte[]>(size);
	size_t bytesRead = 0;
	code = mpqfs_read_file_into(archive_, buf, result.get(), size, &bytesRead);
	if (code != MPQFS_OK) {
		error = static_cast<int32_t>(code);
		return nullptr;
	}

	error = 0;
	fileSize = bytesRead;
	return result;
}

} // namespace devilution
