#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mpq/mpq_common.hpp"

// Forward-declare so that we can avoid exposing libmpq.
struct mpq_archive;
using mpq_archive_s = struct mpq_archive;

namespace devilution {

class MpqArchive {
public:
	// If the file does not exist, returns nullopt without an error.
	static std::optional<MpqArchive> Open(const char *path, int32_t &error);

	std::optional<MpqArchive> Clone(int32_t &error);

	static const char *ErrorMessage(int32_t errorCode);

	MpqArchive(MpqArchive &&other) noexcept
	    : path_(std::move(other.path_))
	    , archive_(other.archive_)
	    , tmp_buf_(std::move(other.tmp_buf_))
	{
		other.archive_ = nullptr;
	}

	MpqArchive &operator=(MpqArchive &&other) noexcept;

	~MpqArchive();

	// Returns false if the file does not exit.
	bool GetFileNumber(MpqFileHash fileHash, uint32_t &fileNumber);

	std::unique_ptr<std::byte[]> ReadFile(std::string_view filename, std::size_t &fileSize, int32_t &error);

	// Returns error code.
	int32_t ReadBlock(uint32_t fileNumber, uint32_t blockNumber, uint8_t *out, size_t outSize);

	std::size_t GetUnpackedFileSize(uint32_t fileNumber, int32_t &error);

	uint32_t GetNumBlocks(uint32_t fileNumber, int32_t &error);

	int32_t OpenBlockOffsetTable(uint32_t fileNumber, std::string_view filename);

	int32_t CloseBlockOffsetTable(uint32_t fileNumber);

	// Requires the block offset table to be open
	std::size_t GetBlockSize(uint32_t fileNumber, uint32_t blockNumber, int32_t &error);

	bool HasFile(std::string_view filename) const;

private:
	MpqArchive(std::string path, mpq_archive_s *archive)
	    : path_(std::move(path))
	    , archive_(archive)
	{
	}

	std::vector<std::uint8_t> &GetTemporaryBuffer(std::size_t size)
	{
		if (tmp_buf_.size() < size)
			tmp_buf_.resize(size);
		return tmp_buf_;
	}

	std::string path_;
	mpq_archive_s *archive_;
	std::vector<std::uint8_t> tmp_buf_;
};

} // namespace devilution
