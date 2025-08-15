#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>

#include <expected.hpp>
#include <function_ref.hpp>

#include "iterators.hpp"

namespace devilution {

struct ColumnDefinition {
	enum class Error {
		UnknownColumn
	};

	uint8_t type = std::numeric_limits<uint8_t>::max();

	// The number of fields between this column and the last one identified as important (or from start of the record if this is the first column we care about)
	unsigned skipLength = 0;

	bool operator==(const ColumnDefinition &other) const = default;

	template <typename T>
	explicit operator T() const
	{
		return static_cast<T>(type);
	}
};

/**
 * @brief Container for a tab-delimited file following the TSV-like format described in txtdata/Readme.md
 */
class DataFile {
	std::unique_ptr<const char[]> data_;
	std::string_view content_;

	const char *body_;

	DataFile() = delete;

	/**
	 * @brief Creates a view over a sequence of utf8 code units, skipping over the BOM if present
	 * @param data pointer to the raw data backing the view (this container will take ownership to ensure the lifetime of the view)
	 * @param size total number of bytes/code units including the BOM if present
	 */
	DataFile(std::unique_ptr<const char[]> &&data, size_t size)
	    : data_(std::move(data))
	    , content_(data_.get(), size)
	{
		constexpr std::string_view utf8BOM = "\xef\xbb\xbf";
		if (this->content_.starts_with(utf8BOM))
			this->content_.remove_prefix(utf8BOM.size());

		body_ = this->content_.data();
	}

public:
	enum class Error {
		NotFound,
		OpenFailed,
		BadRead,
		NoContent,
		NotEnoughColumns
	};

	/**
	 * @brief Attempts to load a data file (using the same mechanism as other runtime assets)
	 *
	 * @param path file to load including the /txtdata/ prefix
	 * @return an object containing an owned pointer to an in-memory copy of the file
	 *         or an error code describing the reason for failure.
	 */
	static tl::expected<DataFile, Error> load(std::string_view path);

	static DataFile loadOrDie(std::string_view path);

	static void reportFatalError(Error code, std::string_view fileName);
	static void reportFatalFieldError(DataFileField::Error code, std::string_view fileName, std::string_view fieldName, const DataFileField &field, std::string_view details = {});

	void resetHeader()
	{
		body_ = content_.data();
	}

	/**
	 * @brief Attempts to parse the first row/record in the file, populating the range defined by [begin, end) using the provided mapping function
	 *
	 * This method will also set an internal marker so that future uses of the begin iterator skip the header line.
	 * @param begin Start of the destination range
	 * @param end End of the destination range
	 * @param mapper Function that maps from a string_view to a unique numeric identifier for the column
	 * @return If the file ends after the header or not enough columns were defined this function returns an error code describing the failure.
	 */
	[[nodiscard]] tl::expected<void, DataFile::Error> parseHeader(ColumnDefinition *begin, ColumnDefinition *end, tl::function_ref<tl::expected<uint8_t, ColumnDefinition::Error>(std::string_view)> mapper);

	/**
	 * @brief Templated version of parseHeader(uint8_t) to allow using directly with enum definitions of columns
	 * @tparam T An enum or any type that defines operator uint8_t()
	 * @param begin Start of the destination range
	 * @param end End of the destination range
	 * @param typedMapper Function that maps from a string_view to a unique T value
	 * @return A void success result or an error code as described above
	 */
	template <typename T>
	[[nodiscard]] tl::expected<void, DataFile::Error> parseHeader(ColumnDefinition *begin, ColumnDefinition *end, std::function<tl::expected<T, ColumnDefinition::Error>(std::string_view)> typedMapper)
	{
		return parseHeader(begin, end, [typedMapper](std::string_view label) { return typedMapper(label).transform([](T value) { return static_cast<uint8_t>(value); }); });
	}

	[[nodiscard]] tl::expected<void, DataFile::Error> skipHeader();

	void skipHeaderOrDie(std::string_view path);

	[[nodiscard]] RecordIterator begin() const
	{
		return { body_, data() + size(), body_ != data() };
	}

	[[nodiscard]] RecordIterator end() const
	{
		return {};
	}

	// Assumes a header
	[[nodiscard]] size_t numRecords() const;

	[[nodiscard]] const char *data() const
	{
		return content_.data();
	}

	[[nodiscard]] size_t size() const
	{
		return content_.size();
	}
};
} // namespace devilution
