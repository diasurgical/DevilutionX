#pragma once

#include <charconv>
#include <expected>
#include <optional>
#include <ostream>

#include "parser.hpp"
#include "utils/parse_int.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

class DataFileField {
	GetFieldResult *state_;
	const char *end_;
	unsigned row_;
	unsigned column_;

public:
	enum class Error {
		NotANumber,
		OutOfRange,
		InvalidValue
	};

	static std::expected<void, Error> mapError(std::errc ec)
	{
		if (ec == std::errc())
			return {};
		switch (ec) {
		case std::errc::result_out_of_range:
			return std::unexpected { Error::OutOfRange };
		case std::errc::invalid_argument:
			return std::unexpected { Error::NotANumber };
		default:
			return std::unexpected { Error::InvalidValue };
		}
	}

	static std::expected<void, Error> mapError(ParseIntError ec)
	{
		switch (ec) {
		case ParseIntError::OutOfRange:
			return std::unexpected { Error::OutOfRange };
		case ParseIntError::ParseError:
			return std::unexpected { Error::NotANumber };
		default:
			return std::unexpected { Error::InvalidValue };
		}
	}

	DataFileField(GetFieldResult *state, const char *end, unsigned row, unsigned column)
	    : state_(state)
	    , end_(end)
	    , row_(row)
	    , column_(column)
	{
	}

	/**
	 * @brief Returns a view of the current field
	 *
	 * This method scans the current field if this is the first value access since the last
	 * advance. If you expect the field to contain a numeric value then calling parseInt first
	 * is more efficient, but calling the methods in either order is supported.
	 * @return The current field value (may be an empty string) or a zero length string_view
	 */
	[[nodiscard]] std::string_view value()
	{
		if (state_->status == GetFieldResult::Status::ReadyToRead) {
			*state_ = GetNextField(state_->next, end_);
		}
		return state_->value;
	}

	/**
	 * Convenience function to let DataFileField instances be used like other single-value STL containers
	 */
	[[nodiscard]] std::string_view operator*()
	{
		return this->value();
	}

	/**
	 * @brief Attempts to parse the current field as a numeric value using std::from_chars
	 *
	 * You can freely interleave this method with calls to operator*. If this is the first value
	 * access since the last advance this will scan the current field and store it for later
	 * use with operator* or repeated calls to parseInt (even with different types).
	 * @tparam T an Integral type supported by std::from_chars
	 * @param destination value to store the result of successful parsing
	 * @return an error code corresponding to the from_chars result if parsing failed
	 */
	template <typename T>
	[[nodiscard]] std::expected<void, Error> parseInt(T &destination)
	{
		std::from_chars_result result {};
		if (state_->status == GetFieldResult::Status::ReadyToRead) {
			const char *begin = state_->next;
			result = std::from_chars(begin, end_, destination);
			if (result.ec != std::errc::invalid_argument) {
				// from_chars was able to consume at least one character, consume the rest of the field
				*state_ = GetNextField(result.ptr, end_);
				// and prepend what was already parsed
				state_->value = { begin, (state_->value.data() - begin) + state_->value.size() };
			}
		} else {
			result = std::from_chars(state_->value.data(), end_, destination);
		}

		return mapError(result.ec);
	}

	[[nodiscard]] std::expected<void, Error> parseBool(bool &destination)
	{
		const std::string_view str = value();
		if (str == "true") {
			destination = true;
			return {};
		}
		if (str == "false") {
			destination = false;
			return {};
		}
		return std::unexpected(DataFileField::Error::InvalidValue);
	}

	template <typename T>
	[[nodiscard]] std::expected<void, Error> parseIntArray(T *destination, size_t n)
	{
		size_t i = 0;
		for (const std::string_view part : SplitByChar(value(), ',')) {
			if (i == n)
				return std::unexpected(Error::InvalidValue);
			const std::from_chars_result result
			    = std::from_chars(part.data(), part.data() + part.size(), destination[i]);
			if (result.ec != std::errc())
				return mapError(result.ec);
			++i;
		}
		if (i != n)
			return std::unexpected(Error::InvalidValue);
		return {};
	}

	template <typename T, size_t N>
	[[nodiscard]] std::expected<void, Error> parseIntArray(T (&destination)[N])
	{
		return parseIntArray<T>(destination, N);
	}

	template <typename T, size_t N>
	[[nodiscard]] std::expected<void, Error> parseIntArray(std::array<T, N> &destination)
	{
		return parseIntArray<T>(destination.data(), N);
	}

	template <typename T, typename ParseFn>
	[[nodiscard]] std::expected<void, std::string> parseEnumArray(T *destination, size_t n, std::optional<T> fillMissing, ParseFn &&parseFn)
	{
		size_t i = 0;
		const std::string_view str = value();
		if (!str.empty()) {
			for (const std::string_view part : SplitByChar(str, ',')) {
				if (i == n)
					return std::unexpected(StrCat("Too many values, max: ", n));
				auto result = parseFn(part);
				if (!result.has_value()) {
					return std::unexpected(std::move(result).error());
				}
				destination[i++] = *result;
			}
		}
		if (i != n) {
			if (!fillMissing.has_value()) {
				return std::unexpected(StrCat("Too few values, expected ", n, " got ", i));
			}
			while (i < n) {
				destination[i++] = *fillMissing;
			}
		}
		return {};
	}

	template <typename T, size_t N, typename ParseFn>
	[[nodiscard]] std::expected<void, std::string> parseEnumArray(T (&destination)[N], std::optional<T> fillMissing, ParseFn &&parseFn)
	{
		return parseEnumArray<T, ParseFn>(destination, N, std::move(fillMissing), std::forward<ParseFn>(parseFn));
	}

	template <typename T, size_t N, typename ParseFn>
	[[nodiscard]] std::expected<void, std::string> parseIntArray(std::array<T, N> &destination, std::optional<T> fillMissing, ParseFn &&parseFn)
	{
		return parseEnumArray<T, ParseFn>(destination.data(), N, std::move(fillMissing), std::forward<ParseFn>(parseFn));
	}

	template <typename T, typename ParseFn>
	[[nodiscard]] std::expected<void, std::string> parseEnumList(T &destination, ParseFn &&parseFn)
	{
		destination = {};
		const std::string_view str = value();
		if (str.empty())
			return {};
		for (const std::string_view part : SplitByChar(str, ',')) {
			auto result = parseFn(part);
			if (!result.has_value())
				return std::unexpected(std::move(result).error());
			destination |= result.value();
		}
		return {};
	}

	template <typename T>
	[[nodiscard]] std::expected<T, Error> asInt()
	{
		T value = 0;
		return parseInt(value).transform([value]() { return value; });
	}

	/**
	 * @brief Attempts to parse the current field as a fixed point value with 6 bits for the fraction
	 *
	 * You can freely interleave this method with calls to operator*. If this is the first value
	 * access since the last advance this will scan the current field and store it for later
	 * use with operator* or repeated calls to parseInt/Fixed6 (even with different types).
	 * @tparam T an Integral type supported by std::from_chars
	 * @param destination value to store the result of successful parsing
	 * @return an error code equivalent to what you'd get from from_chars if parsing failed
	 */
	template <typename T>
	[[nodiscard]] std::expected<void, Error> parseFixed6(T &destination)
	{
		ParseIntResult<T> parseResult;
		if (state_->status == GetFieldResult::Status::ReadyToRead) {
			const char *begin = state_->next;
			// first read, consume digits
			parseResult = ParseFixed6<T>({ begin, static_cast<size_t>(end_ - begin) }, &state_->next);
			// then read the remainder of the field
			*state_ = GetNextField(state_->next, end_);
			// and prepend what was already parsed
			state_->value = { begin, (state_->value.data() - begin) + state_->value.size() };
		} else {
			parseResult = ParseFixed6<T>(state_->value);
		}

		if (parseResult.has_value()) {
			destination = parseResult.value();
			return {};
		} else {
			return mapError(parseResult.error());
		}
	}

	template <typename T>
	[[nodiscard]] std::expected<T, Error> asFixed6()
	{
		T value = 0;
		return parseFixed6(value).transform([value]() { return value; });
	}

	/**
	 * Returns the current row number
	 */
	[[nodiscard]] unsigned row() const
	{
		return row_;
	}

	/**
	 * Returns the current column/field number (from the start of the row/record)
	 */
	[[nodiscard]] unsigned column() const
	{
		return column_;
	}

	/**
	 * Allows accessing the value of this field in a const context
	 *
	 * This requires an actual non-const value access to happen first before it returns
	 * any useful results, intended for use in error reporting (or test output).
	 */
	[[nodiscard]] std::string_view currentValue() const
	{
		return state_->value;
	}
};

/**
 * @brief Show the field value along with the row/column number (mainly used in test failure messages)
 * @param stream output stream, expected to have overloads for unsigned, std::string_view, and char*
 * @param field Object to display
 * @return the stream, to allow chaining
 */
inline std::ostream &operator<<(std::ostream &stream, const DataFileField &field)
{
	return stream << "\"" << field.currentValue() << "\" (at row " << field.row() << ", column " << field.column() << ")";
}

class FieldIterator {
	GetFieldResult *state_;
	const char *const end_;
	const unsigned row_;
	unsigned column_ = 0;

public:
	using iterator_category = std::input_iterator_tag;
	using value_type = DataFileField;

	FieldIterator()
	    : state_(nullptr)
	    , end_(nullptr)
	    , row_(0)
	{
	}

	FieldIterator(GetFieldResult *state, const char *end, unsigned row)
	    : state_(state)
	    , end_(end)
	    , row_(row)
	{
		state_->status = GetFieldResult::Status::ReadyToRead;
	}

	[[nodiscard]] bool operator==(const FieldIterator &rhs) const
	{
		if (state_ == nullptr && rhs.state_ == nullptr)
			return true;

		return state_ != nullptr && rhs.state_ != nullptr && state_->next == rhs.state_->next;
	}

	[[nodiscard]] bool operator!=(const FieldIterator &rhs) const
	{
		return !(*this == rhs);
	}

	/**
	 * Advances to the next field in the current record
	 */
	FieldIterator &operator++()
	{
		return *this += 1;
	}

	/**
	 * @brief Advances by the specified number of fields
	 *
	 * if a non-zero increment is provided and advancing the iterator causes it to reach the end
	 * of the record the iterator is invalidated. It will compare equal to an end iterator and
	 * cannot be used for value access or any further parsing
	 * @param increment how many fields to advance (can be 0)
	 * @return self-reference
	 */
	FieldIterator &operator+=(unsigned increment)
	{
		if (increment == 0)
			return *this;

		if (state_->status == GetFieldResult::Status::ReadyToRead) {
			// We never read the value and no longer need it, discard it so that we end up
			//  advancing past the field delimiter (as if a value access had happened)
			*state_ = DiscardField(state_->next, end_);
		}

		if (state_->endOfRecord()) {
			state_ = nullptr;
		} else {
			unsigned fieldsSkipped = 0;
			// By this point we've already advanced past the end of this field (either because the
			//  last value access found the end of the field by necessity or we discarded it a few
			//  lines up), so we only need to advance further if an increment greater than 1 was
			//  provided.
			*state_ = DiscardMultipleFields(state_->next, end_, increment - 1, &fieldsSkipped);
			// As we've consumed the current field by this point we need to increment the internal
			//  column counter one extra time so we have an accurate value.
			column_ += fieldsSkipped + 1;
			// We use Status::ReadyToRead as a marker so we only read the next value on the next
			//  value access, this allows consumers to choose the most efficient method (e.g. if
			//  they want the value as an int) or even repeated advances without using a value.
			state_->status = GetFieldResult::Status::ReadyToRead;
		}
		return *this;
	}

	/**
	 * @brief Returns a view of the current field
	 *
	 * The returned value is a thin wrapper over the current state of this iterator (or last
	 * successful read if incrementing this iterator would result in it reaching the end state).
	 */
	[[nodiscard]] value_type operator*()
	{
		return { state_, end_, row_, column_ };
	}

	/**
	 * @brief Returns the current row number
	 */
	[[nodiscard]] unsigned row() const
	{
		return row_;
	}
	/**
	 * @brief Returns the current column/field number (from the start of the row/record)
	 */
	[[nodiscard]] unsigned column() const
	{
		return column_;
	}
};

class DataFileRecord {
	GetFieldResult *state_;
	const char *const end_;
	const unsigned row_;

public:
	DataFileRecord(GetFieldResult *state, const char *end, unsigned row)
	    : state_(state)
	    , end_(end)
	    , row_(row)
	{
	}

	[[nodiscard]] FieldIterator begin()
	{
		return { state_, end_, row_ };
	}

	[[nodiscard]] FieldIterator end() const
	{
		return {};
	}

	[[nodiscard]] unsigned row() const
	{
		return row_;
	}
};

class RecordIterator {
	GetFieldResult state_;
	const char *const end_;
	unsigned row_ = 0;

public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = DataFileRecord;

	RecordIterator()
	    : state_(nullptr, GetFieldResult::Status::EndOfFile)
	    , end_(nullptr)
	{
	}

	RecordIterator(const char *begin, const char *end, bool skippedHeader)
	    : state_(begin)
	    , end_(end)
	    , row_(skippedHeader ? 1 : 0)
	{
	}

	[[nodiscard]] bool operator==(const RecordIterator &rhs) const
	{
		return state_.next == rhs.state_.next;
	}

	[[nodiscard]] bool operator!=(const RecordIterator &rhs) const
	{
		return !(*this == rhs);
	}

	RecordIterator &operator++()
	{
		return *this += 1;
	}

	RecordIterator &operator+=(unsigned increment)
	{
		if (increment == 0)
			return *this;

		if (!state_.endOfRecord()) {
			// The field iterator either hasn't been used or hasn't consumed the entire record
			state_ = DiscardRemainingFields(state_.next, end_);
		}

		if (state_.endOfFile()) {
			state_.next = nullptr;
		} else {
			unsigned recordsSkipped = 0;
			// By this point we've already advanced past the end of this record (either because the
			//  last value access found the end of the record by necessity or we discarded any
			//  leftovers a few lines up), so we only need to advance further if an increment
			//  greater than 1 was provided.
			state_ = DiscardMultipleRecords(state_.next, end_, increment - 1, &recordsSkipped);
			// As we've consumed the current record by this point we need to increment the internal
			//  row counter one extra time so we have an accurate value.
			row_ += recordsSkipped + 1;
			// We use Status::ReadyToRead as a marker in case the DataFileField iterator is never
			//  used, so the next call to operator+= will advance past the current record
			state_.status = GetFieldResult::Status::ReadyToRead;
		}
		return *this;
	}

	[[nodiscard]] DataFileRecord operator*()
	{
		return { &state_, end_, row_ };
	}

	/**
	 * @brief Exposes the current location of this input iterator.
	 *
	 * This is only expected to be used internally so the DataFile instance knows where the header
	 * ends and the body begins. You probably don't want to use this directly.
	 */
	[[nodiscard]] const char *data() const
	{
		return state_.next;
	}

	/**
	 * @brief Returns the current row/record number (from the start of the file)
	 *
	 * The header row is always considered row 0, however if you've called DataFile.parseHeader()
	 * before calling DataFile.begin() then you'll get row 1 as the first record of the range.
	 */
	[[nodiscard]] unsigned row() const
	{
		return row_;
	}
};
} // namespace devilution
