#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "utils/string_view_hash.hpp"

namespace devilution {

class Ini {
public:
	// A single value associated with a section and key.
	struct Value {
		// When setting a value, `nullopt` results
		// in preserving the existing comment if any.
		std::optional<std::string> comment;
		std::string value;
	};

	// All the values associated with a section and key.
	class Values {
	public:
		/**
		 * @brief Constructs an empty set of values.
		 *
		 * If passed to `set`, the key is deleted.
		 */
		Values();

		explicit Values(const Value &data);

		[[nodiscard]] std::span<const Value> get() const;
		[[nodiscard]] std::span<Value> get();
		void append(const Value &value);

	private:
		// Most keys only have a single value, so we use
		// a representation that avoids allocations in that case.
		std::variant<Value, std::vector<Value>> rep_;

		friend class Ini;
	};

	static std::expected<Ini, std::string> parse(std::string_view buffer);
	[[nodiscard]] std::string serialize() const;

	/** @return all the keys associated with this section in the ini */
	[[nodiscard]] std::vector<std::string> getKeys(std::string_view section) const;

	/** @return all the values associated with this section and key in the ini */
	[[nodiscard]] std::span<const Value> get(std::string_view section, std::string_view key) const;

	/** @return the default value if the ini value is unset or empty */
	[[nodiscard]] std::string_view getString(std::string_view section, std::string_view key, std::string_view defaultValue = {}) const;

	/** @return the default value if the ini value is unset or empty */
	[[nodiscard]] bool getBool(std::string_view section, std::string_view key, bool defaultValue) const;

	/** @return the default value if the ini value is unset or empty */
	[[nodiscard]] int getInt(std::string_view section, std::string_view key, int defaultValue) const;

	/** @return the default value if the ini value is unset or empty */
	[[nodiscard]] float getFloat(std::string_view section, std::string_view key, float defaultValue) const;

	void getUtf8Buf(std::string_view section, std::string_view key, std::string_view defaultValue, char *dst, size_t dstSize) const;

	void getUtf8Buf(std::string_view section, std::string_view key, char *dst, size_t dstSize) const
	{
		getUtf8Buf(section, key, /*defaultValue=*/ {}, dst, dstSize);
	}

	[[nodiscard]] bool changed() const { return changed_; }
	void markAsUnchanged() { changed_ = false; }

	// If values are empty, deletes the entry.
	void set(std::string_view section, std::string_view key, Values &&values);

	void set(std::string_view section, std::string_view key, std::span<const std::string> value);
	void set(std::string_view section, std::string_view key, std::string &&value);
	void set(std::string_view section, std::string_view key, std::string_view value);
	void set(std::string_view section, std::string_view key, const char *value)
	{
		set(section, key, std::string_view(value));
	}
	void set(std::string_view section, std::string_view key, bool value);
	void set(std::string_view section, std::string_view key, int value);
	void set(std::string_view section, std::string_view key, float value);

private:
	struct ValuesData {
		Values values;
		uint32_t index;
	};

	struct SectionData {
		std::string comment;
		ankerl::unordered_dense::map<std::string, ValuesData, StringViewHash, StringViewEquals> entries;
		uint32_t index;
	};

	struct FileData {
		ankerl::unordered_dense::map<std::string, SectionData, StringViewHash, StringViewEquals> sections;
	};

	explicit Ini(FileData &&data)
	    : data_(std::move(data))
	{
	}

	FileData data_;
	bool changed_ = false;
};

} // namespace devilution
