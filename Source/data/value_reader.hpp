#pragma once

#include <array>
#include <string_view>
#include <type_traits>

#include <expected.hpp>
#include <function_ref.hpp>

#include "data/file.hpp"
#include "data/iterators.hpp"

namespace devilution {

/**
 * @brief A value reader.
 */
class ValueReader {
public:
	ValueReader(DataFile &dataFile, std::string_view filename);

	DataFileField getValueField(std::string_view expectedKey);

	template <typename T, typename F>
	void readValue(std::string_view expectedKey, T &outValue, F &&readFn)
	{
		DataFileField valueField = getValueField(expectedKey);
		if (const tl::expected<void, devilution::DataFileField::Error> result = readFn(valueField, outValue);
		    !result.has_value()) {
			DataFile::reportFatalFieldError(result.error(), filename_, "Value", valueField);
		}
		++it_;
	}

	template <typename T, typename F>
	void read(std::string_view expectedKey, T &outValue, F &&parseFn)
	{
		readValue(expectedKey, outValue, [&parseFn](DataFileField &valueField, T &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
			const auto result = parseFn(valueField.value());
			if (!result.has_value()) {
				return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
			}

			outValue = result.value();
			return {};
		});
	}

	template <typename T, typename F>
	void readEnumList(std::string_view expectedKey, T &outValue, F &&parseFn)
	{
		readValue(expectedKey, outValue, [&parseFn](DataFileField &valueField, T &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
			const auto result = valueField.parseEnumList(outValue, std::forward<F>(parseFn));
			if (!result.has_value()) {
				return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
			}

			return {};
		});
	}

	template <typename T>
	typename std::enable_if_t<std::is_integral_v<T>, void>
	readInt(std::string_view expectedKey, T &outValue)
	{
		readValue(expectedKey, outValue, [](DataFileField &valueField, T &outValue) {
			return valueField.parseInt(outValue);
		});
	}

	template <typename T>
	typename std::enable_if_t<std::is_integral_v<T>, void>
	readDecimal(std::string_view expectedKey, T &outValue)
	{
		readValue(expectedKey, outValue, [](DataFileField &valueField, T &outValue) {
			return valueField.parseFixed6(outValue);
		});
	}

	void readString(std::string_view expectedKey, std::string &outValue)
	{
		readValue(expectedKey, outValue, [](DataFileField &valueField, std::string &outValue) {
			outValue = valueField.value();
			return tl::expected<void, devilution::DataFileField::Error> {};
		});
	}

	void readChar(std::string_view expectedKey, char &outValue)
	{
		readValue(expectedKey, outValue, [](DataFileField &valueField, char &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
			if (valueField.value().size() != 1) {
				return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
			}

			outValue = valueField.value().at(0);
			return {};
		});
	}

private:
	RecordIterator it_;
	const RecordIterator end_;
	std::string_view filename_;
};

} // namespace devilution
