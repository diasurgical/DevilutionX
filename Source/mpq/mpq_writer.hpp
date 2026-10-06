/**
 * @file mpq/mpq_writer.hpp
 *
 * Interface of functions for creating and editing MPQ files.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Forward-declare so that we can avoid exposing mpqfs.h to all consumers.
struct mpqfs_writer;
typedef struct mpqfs_writer mpqfs_writer_t;

namespace devilution {

constexpr uint32_t MpqWriterHashTableSize = 2048;

class MpqWriter {
public:
	explicit MpqWriter(const char *path, bool carryForward = true);
	explicit MpqWriter(const std::string &path, bool carryForward = true)
	    : MpqWriter(path.c_str(), carryForward)
	{
	}
	MpqWriter(MpqWriter &&other) noexcept;
	MpqWriter &operator=(MpqWriter &&other) noexcept;
	~MpqWriter();

	MpqWriter(const MpqWriter &) = delete;
	MpqWriter &operator=(const MpqWriter &) = delete;

	bool HasFile(std::string_view name) const;
	void RemoveHashEntry(std::string_view filename);
	void RemoveHashEntries(bool (*fnGetName)(uint8_t, char *));
	bool WriteFile(std::string_view filename, const std::byte *data, size_t size);
	void RenameFile(std::string_view name, std::string_view newName);

private:
	std::string path_;
	mpqfs_writer_t *writer_ = nullptr;
};

} // namespace devilution