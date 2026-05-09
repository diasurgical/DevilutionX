#include "mpq/mpq_writer.hpp"

#include <cstring>
#include <utility>

#include <mpqfs/mpqfs.h>

#include "mpq/mpq_common.hpp"
#include "utils/file_util.h"
#include "utils/log.hpp"

namespace devilution {

MpqWriter::MpqWriter(const char *path, bool carryForward)
    : path_(path)
{
	const std::string dir = std::string(Dirname(path));
	if (!dir.empty()) {
		RecursivelyCreateDir(dir.c_str());
	}
	LogVerbose("Opening {}", path);

	// If the file already exists and we need to preserve its contents,
	// rename it to a temp path so we can read from it after the writer
	// truncates the original path.
	std::string tmpPath;
	mpqfs_archive_t *oldArchive = nullptr;
	if (carryForward && FileExists(path)) {
		tmpPath = std::string(path) + ".tmp";
		// Remove any stale temp file, then rename old → tmp.
		::devilution::RemoveFile(tmpPath.c_str());
		::devilution::RenameFile(path, tmpPath.c_str());
		oldArchive = mpqfs_open(tmpPath.c_str());
		// If it fails to open (e.g. corrupt), we proceed without
		// carry-forward — the file will be recreated from scratch.
	}

	writer_ = mpqfs_writer_create(path, MpqWriterHashTableSize);
	if (writer_ == nullptr) {
		LogError("Failed to create MPQ archive: {}", mpqfs_last_error());
		if (oldArchive != nullptr)
			mpqfs_close(oldArchive);
		if (!tmpPath.empty())
			::devilution::RemoveFile(tmpPath.c_str());
		return;
	}

	if (oldArchive != nullptr) {
		if (!mpqfs_writer_carry_forward_all(writer_, oldArchive)) {
			LogError("Failed to carry forward files from existing archive: {}", mpqfs_last_error());
		}
		mpqfs_close(oldArchive);
	}

	if (!tmpPath.empty())
		::devilution::RemoveFile(tmpPath.c_str());
}

MpqWriter::MpqWriter(MpqWriter &&other) noexcept
    : path_(std::move(other.path_))
    , writer_(other.writer_)
{
	other.writer_ = nullptr;
}

MpqWriter &MpqWriter::operator=(MpqWriter &&other) noexcept
{
	if (this != &other) {
		if (writer_ != nullptr)
			mpqfs_writer_discard(writer_);
		path_ = std::move(other.path_);
		writer_ = other.writer_;
		other.writer_ = nullptr;
	}
	return *this;
}

MpqWriter::~MpqWriter()
{
	if (writer_ == nullptr)
		return;

	LogVerbose("Closing {}", path_);

	if (!mpqfs_writer_close(writer_)) {
		LogError("Failed to close MPQ archive: {}", mpqfs_last_error());
	}
}

bool MpqWriter::HasFile(std::string_view name) const
{
	if (writer_ == nullptr)
		return false;

	char buf[MaxMpqPathSize];
	if (name.size() >= sizeof(buf))
		return false;
	std::memcpy(buf, name.data(), name.size());
	buf[name.size()] = '\0';

	return mpqfs_writer_has_file(writer_, buf);
}

void MpqWriter::RemoveHashEntry(std::string_view filename)
{
	if (writer_ == nullptr)
		return;

	char buf[MaxMpqPathSize];
	if (filename.size() >= sizeof(buf))
		return;
	std::memcpy(buf, filename.data(), filename.size());
	buf[filename.size()] = '\0';

	mpqfs_writer_remove_file(writer_, buf);
}

void MpqWriter::RemoveHashEntries(bool (*fnGetName)(uint8_t, char *))
{
	char pszFileName[MaxMpqPathSize];
	for (uint8_t i = 0; fnGetName(i, pszFileName); i++) {
		RemoveHashEntry(pszFileName);
	}
}

bool MpqWriter::WriteFile(std::string_view filename, const std::byte *data, size_t size)
{
	if (writer_ == nullptr)
		return false;

	char buf[MaxMpqPathSize];
	if (filename.size() >= sizeof(buf))
		return false;
	std::memcpy(buf, filename.data(), filename.size());
	buf[filename.size()] = '\0';

	// Remove any previous entry with this name so that
	// only the latest write is included in the archive.
	mpqfs_writer_remove_file(writer_, buf);

	if (!mpqfs_writer_add_file(writer_, buf, data, size)) {
		LogError("Failed to write file '{}' to MPQ: {}", filename, mpqfs_last_error());
		return false;
	}
	return true;
}

void MpqWriter::RenameFile(std::string_view name, std::string_view newName)
{
	if (writer_ == nullptr)
		return;

	char oldBuf[MaxMpqPathSize];
	char newBuf[MaxMpqPathSize];
	if (name.size() >= sizeof(oldBuf) || newName.size() >= sizeof(newBuf))
		return;
	std::memcpy(oldBuf, name.data(), name.size());
	oldBuf[name.size()] = '\0';
	std::memcpy(newBuf, newName.data(), newName.size());
	newBuf[newName.size()] = '\0';

	mpqfs_writer_rename_file(writer_, oldBuf, newBuf);
}

} // namespace devilution