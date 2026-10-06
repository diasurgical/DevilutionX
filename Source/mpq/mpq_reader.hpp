#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

// Forward-declare so that we can avoid exposing mpqfs.h to all consumers.
struct mpqfs_archive;
typedef struct mpqfs_archive mpqfs_archive_t;

namespace devilution {

class MpqArchive {
public:
	static std::expected<MpqArchive, std::string> Open(const char *path);
	std::expected<MpqArchive, std::string> Clone();

	MpqArchive(MpqArchive &&other) noexcept;
	MpqArchive &operator=(MpqArchive &&other) noexcept;
	~MpqArchive();

	MpqArchive(const MpqArchive &) = delete;
	MpqArchive &operator=(const MpqArchive &) = delete;

	bool HasFile(std::string_view filename) const;
	size_t GetFileSize(std::string_view filename) const;

	// Hash-based lookup: resolve once, reuse the index.
	uint32_t FindHash(std::string_view filename) const;
	bool HasFileHash(uint32_t hash) const;
	size_t GetFileSizeFromHash(uint32_t hash) const;

	std::unique_ptr<std::byte[]> ReadFile(
	    std::string_view filename,
	    std::size_t &fileSize,
	    int32_t &error);

	mpqfs_archive_t *handle() const { return archive_; }

private:
	MpqArchive(std::string path, mpqfs_archive_t *archive);

	std::string path_;
	mpqfs_archive_t *archive_ = nullptr;
};

} // namespace devilution
