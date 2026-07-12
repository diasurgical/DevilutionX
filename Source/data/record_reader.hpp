#pragma once

#include <array>
#include <expected>
#include <string_view>
#include <type_traits>

#include <function_ref.hpp>

#include "data/file.hpp"
#include "data/iterators.hpp"

namespace devilution {

/**
 * @brief A record reader that treats every error as fatal.
 */
class RecordReader {
public:
	RecordReader(DataFileRecord &record, std::string_view filename)
	    : it_(record.begin())
	    , end_(record.end())
	    , filename_(filename)
	{
	}

	template <typename T>
	typename std::enable_if_t<std::is_integral_v<T>, void>
	readInt(std::string_view name, T &out)
	{
		DataFileField field = nextField();
		failOnError(field.parseInt(out), name, field);
	}

	template <typename T>
	typename std::enable_if_t<std::is_integral_v<T>, void>
	readOptionalInt(std::string_view name, T &out)
	{
		DataFileField field = nextField();
		if (field.value().empty()) return;
		failOnError(field.parseInt(out), name, field);
	}

	template <typename T, size_t N>
	void readIntArray(std::string_view name, T (&out)[N])
	{
		DataFileField field = nextField();
		failOnError(field.parseIntArray(out), name, field);
	}

	template <typename T, size_t N, typename F>
	void readEnumArray(std::string_view name, std::optional<T> fillMissing, T (&out)[N], F &&parseFn)
	{
		DataFileField field = nextField();
		failOnError(field.parseEnumArray(out, fillMissing, parseFn), name, field, DataFileField::Error::InvalidValue);
	}

	template <typename T, size_t N>
	void readIntArray(std::string_view name, std::array<T, N> &out)
	{
		DataFileField field = nextField();
		failOnError(field.parseIntArray(out), name, field);
	}

	template <typename T>
	typename std::enable_if_t<std::is_integral_v<T>, void>
	readFixed6(std::string_view name, T &out)
	{
		DataFileField field = nextField();
		failOnError(field.parseFixed6(out), name, field);
	}

	void readBool(std::string_view name, bool &out)
	{
		DataFileField field = nextField();
		failOnError(field.parseBool(out), name, field);
	}

	void readString(std::string_view name, std::string &out)
	{
		advance();
		out = (*it_).value();
	}

	template <typename T, typename F>
	void read(std::string_view name, T &out, F &&parseFn)
	{
		DataFileField field = nextField();
		std::expected<T, std::string> result = parseFn(field.value());
		failOnError(result, name, field, DataFileField::Error::InvalidValue);
		out = *std::move(result);
	}

	template <typename T, typename F>
	void readEnumList(std::string_view name, T &out, F &&parseFn)
	{
		DataFileField field = nextField();
		failOnError(field.parseEnumList(out, std::forward<F>(parseFn)),
		    name, field, DataFileField::Error::InvalidValue);
	}

	std::string_view value()
	{
		advance();
		needsIncrement_ = false;
		return (*it_).value();
	}

	void advance();

	DataFileField nextField()
	{
		advance();
		return *it_;
	}

private:
	template <typename T>
	void failOnError(const std::expected<T, DataFileField::Error> &result, std::string_view name, const DataFileField &field)
	{
		if (!result.has_value()) {
			DataFile::reportFatalFieldError(result.error(), filename_, name, field);
		}
	}

	template <typename T>
	void failOnError(const std::expected<T, std::string> &result, std::string_view name, const DataFileField &field, DataFileField::Error error)
	{
		if (!result.has_value()) {
			DataFile::reportFatalFieldError(error, filename_, name, field, result.error());
		}
	}

	FieldIterator it_;
	const FieldIterator end_;
	std::string_view filename_;
	bool needsIncrement_ = false;
};

} // namespace devilution
