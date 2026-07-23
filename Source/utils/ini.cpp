#include "utils/ini.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "utils/algorithm/container.hpp"
#include "utils/str_cat.hpp"
#include "utils/string_view_hash.hpp"
#include "utils/utf8.hpp"

namespace devilution {

// We avoid including the "appfat.h" to avoid depending on SDL in tests.
[[noreturn]] extern void app_fatal(std::string_view str);

namespace {

template <typename T>
bool OrderByValueIndex(const std::pair<std::string, T> &a, const std::pair<std::string, T> &b)
{
	return a.second.index < b.second.index;
};

// Returns a pointer to the first non-leading whitespace.
// Only ' ' and '\t' are considered whitespace.
// Requires: begin <= end.
const char *SkipLeadingWhitespace(const char *begin, const char *end)
{
	while (begin != end && (*begin == ' ' || *begin == '\t')) {
		++begin;
	}
	return begin;
}

// Returns a pointer to the last non-whitespace.
// Only ' ' and '\t' are considered whitespace.
// Requires: begin <= end.
const char *SkipTrailingWhitespace(const char *begin, const char *end)
{
	while (begin != end && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
		--end;
	}
	return end;
}

// Skips UTF-8 byte order mark.
// See https://en.wikipedia.org/wiki/Byte_order_mark
const char *SkipUtf8Bom(const char *begin, const char *end)
{
	if (end - begin >= 3 && begin[0] == '\xEF' && begin[1] == '\xBB' && begin[2] == '\xBF') {
		return begin + 3;
	}
	return begin;
}

} // namespace

Ini::Values::Values()
    : rep_(std::vector<Value> {})
{
}

Ini::Values::Values(const Value &data)
    : rep_(data)
{
}

std::vector<std::string> Ini::getKeys(std::string_view section) const
{
	const auto sectionIt = data_.sections.find(section);
	if (sectionIt == data_.sections.end()) return {};

	std::vector<std::pair<std::string, ValuesData>> entries(sectionIt->second.entries.begin(), sectionIt->second.entries.end());
	c_sort(entries, OrderByValueIndex<ValuesData>);

	std::vector<std::string> keys;
	keys.reserve(entries.size());
	for (const auto &[key, _] : entries) {
		keys.push_back(key);
	}
	return keys;
}

std::span<const Ini::Value> Ini::Values::get() const
{
	if (std::holds_alternative<Ini::Value>(rep_)) {
		return { &std::get<Ini::Value>(rep_), 1 };
	}
	return std::get<std::vector<Ini::Value>>(rep_);
}

std::span<Ini::Value> Ini::Values::get()
{
	if (std::holds_alternative<Ini::Value>(rep_)) {
		return { &std::get<Ini::Value>(rep_), 1 };
	}
	return std::get<std::vector<Ini::Value>>(rep_);
}

void Ini::Values::append(const Ini::Value &value)
{
	if (std::holds_alternative<Value>(rep_)) {
		rep_ = std::vector<Value> { std::get<Value>(rep_), value };
		return;
	}
	std::get<std::vector<Value>>(rep_).push_back(value);
}

std::expected<Ini, std::string> Ini::parse(std::string_view buffer)
{
	Ini::FileData fileData;
	ankerl::unordered_dense::map<std::string, ValuesData, StringViewHash, StringViewEquals> *sectionEntries = nullptr;
	const char *eof = buffer.data() + buffer.size();
	const char *lineBegin = SkipUtf8Bom(buffer.data(), eof);
	size_t lineNum = 0;

	const char *commentBegin = nullptr;
	const char *nextLineBegin;
	for (; lineBegin < eof; lineBegin = nextLineBegin) {
		++lineNum;
		const char *lineEnd = static_cast<const char *>(memchr(lineBegin, '\n', eof - lineBegin));
		if (lineEnd == nullptr) {
			lineEnd = eof;
			nextLineBegin = eof;
		} else {
			nextLineBegin = lineEnd + 1;
			if (lineBegin + 1 <= lineEnd && *(lineEnd - 1) == '\r') --lineEnd;
		}
		const char *keyBegin = SkipLeadingWhitespace(lineBegin, lineEnd);
		if (keyBegin == lineEnd) continue;

		if (*keyBegin == ';') {
			if (commentBegin == nullptr) commentBegin = lineBegin;
			continue;
		}
		std::string_view comment;
		if (commentBegin != nullptr) {
			comment = std::string_view(commentBegin, lineBegin - commentBegin);
		}

		if (*keyBegin == '[') {
			const char *keyEnd = ++keyBegin;
			while (keyEnd < lineEnd && *keyEnd != ']') {
				++keyEnd;
			}
			if (keyEnd == lineEnd) {
				return std::unexpected(std::format("line {}: unclosed section name {}", lineNum, std::string_view(keyBegin, keyEnd - keyBegin)));
			}
			if (const char *after = SkipTrailingWhitespace(keyEnd + 1, lineEnd); after != lineEnd) {
				return std::unexpected(std::format("line {}: content after section [{}]: {}", lineNum, std::string_view(keyBegin, keyEnd - keyBegin), std::string_view(after, lineEnd - after)));
			}
			const std::string_view sectionName = std::string_view(keyBegin, keyEnd - keyBegin);
			auto it = fileData.sections.find(sectionName);
			if (it == fileData.sections.end()) {
				it = fileData.sections.emplace_hint(it, sectionName,
				    SectionData {
				        .comment = std::string(comment),
				        .entries = {},
				        .index = static_cast<uint32_t>(fileData.sections.size()),
				    });
			}
			sectionEntries = &it->second.entries;
			commentBegin = nullptr;
			continue;
		}

		if (sectionEntries == nullptr) return std::unexpected(std::format("line {}: key not in any section", lineNum));
		const char *eqPos = static_cast<const char *>(memchr(keyBegin, '=', lineEnd - keyBegin));
		if (eqPos == nullptr) {
			return std::unexpected(std::format("line {}: key {} has no value", lineNum, std::string_view(keyBegin, lineEnd - keyBegin)));
		}
		const char *keyEnd = SkipTrailingWhitespace(keyBegin, eqPos);
		const std::string_view key = std::string_view(keyBegin, keyEnd - keyBegin);
		const char *valueBegin = SkipLeadingWhitespace(eqPos + 1, lineEnd);
		const std::string_view value = std::string_view(valueBegin, lineEnd - valueBegin);
		if (const auto it = sectionEntries->find(key); it != sectionEntries->end()) {
			it->second.values.append(Value { std::string(comment), std::string(value) });
		} else {
			sectionEntries->emplace_hint(it, key,
			    ValuesData {
			        .values = Values { Value {
			            .comment = std::string(comment),
			            .value = std::string(value),
			        } },
			        .index = static_cast<uint32_t>(sectionEntries->size()),
			    });
		}
		commentBegin = nullptr;
	}
	return Ini(std::move(fileData));
}

std::span<const Ini::Value> Ini::get(std::string_view section, std::string_view key) const
{
	const auto sectionIt = data_.sections.find(section);
	if (sectionIt == data_.sections.end()) return {};
	const auto it = sectionIt->second.entries.find(key);
	if (it == sectionIt->second.entries.end()) return {};
	return it->second.values.get();
}

std::string_view Ini::getString(std::string_view section, std::string_view key, std::string_view defaultValue) const
{
	const std::span<const Ini::Value> xs = get(section, key);
	if (xs.empty() || xs.back().value.empty()) return defaultValue;
	return xs.back().value;
}

int Ini::getInt(std::string_view section, std::string_view key, int defaultValue) const
{
	const std::span<const Ini::Value> xs = get(section, key);
	if (xs.empty() || xs.back().value.empty()) return defaultValue;
	const std::string_view str = xs.back().value;
	int value;
	const std::from_chars_result result = std::from_chars(str.data(), str.data() + str.size(), value);
	if (result.ec != std::errc()) {
		app_fatal(std::format("ini: Failed to parse {}.{}={} as int", section, key, str));
		return defaultValue;
	}
	return value;
}

bool Ini::getBool(std::string_view section, std::string_view key, bool defaultValue) const
{
	const std::span<const Ini::Value> xs = get(section, key);
	if (xs.empty() || xs.back().value.empty()) return defaultValue;
	const std::string_view str = xs.back().value;
	if (str == "0") return false;
	if (str == "1") return true;
	app_fatal(std::format("ini: Failed to parse {}.{}={} as bool", section, key, str));
}

float Ini::getFloat(std::string_view section, std::string_view key, float defaultValue) const
{
	const std::span<const Ini::Value> xs = get(section, key);
	if (xs.empty() || xs.back().value.empty()) return defaultValue;
	const std::string &str = xs.back().value;

#if __cpp_lib_to_chars >= 201611L
	float value;
	const std::from_chars_result result = std::from_chars(str.data(), str.data() + str.size(), value);
	if (result.ec != std::errc()) {
		app_fatal(std::format("ini: Failed to parse {}.{}={} as float", section, key, str));
		return defaultValue;
	}
	return value;
#else
	return strtof(str.data(), nullptr);
#endif
}

void Ini::getUtf8Buf(std::string_view section, std::string_view key, std::string_view defaultValue, char *dst, size_t dstSize) const
{
	CopyUtf8(dst, getString(section, key, defaultValue), dstSize);
}

void Ini::set(std::string_view section, std::string_view key, Ini::Values &&values)
{
	const std::span<Value> updated = values.get();

	auto sectionIt = data_.sections.find(section);
	if (sectionIt == data_.sections.end()) {
		// Deleting a key from a non-existing section
		if (updated.empty()) return;

		// Adding a new section and key
		data_.sections.emplace_hint(sectionIt, section,
		    SectionData {
		        .comment = {},
		        .entries = { { std::string(key), ValuesData { .values = std::move(values), .index = 0 } } },
		        .index = static_cast<uint32_t>(data_.sections.size()),
		    });
		changed_ = true;
		return;
	}
	const auto it = sectionIt->second.entries.find(key);
	if (it == sectionIt->second.entries.end()) {
		// Deleting a non-existing key
		if (updated.empty()) return;

		// Adding a new key to an existing section
		sectionIt->second.entries.emplace(key,
		    ValuesData {
		        .values = std::move(values),
		        .index = static_cast<uint32_t>(sectionIt->second.entries.size()),
		    });
		changed_ = true;
		return;
	}

	// Deleting an existing key
	if (updated.empty()) {
		sectionIt->second.entries.erase(it);
		if (sectionIt->second.entries.empty()) data_.sections.erase(sectionIt);
		changed_ = true;
		return;
	}

	// Overriding an existing key
	const std::span<Value> original = it->second.values.get();
	if (original.size() == updated.size()) {
		bool equal = true;
		for (size_t i = 0; i < original.size(); ++i) {
			if (original[i].value != updated[i].value) {
				equal = false;
				break;
			}
		}
		if (equal) return;
	}

	// Preserve existing comments where not overriden.
	for (size_t i = 0, n = std::min(original.size(), updated.size()); i < n; ++i) {
		if (!updated[i].comment.has_value() && original[i].comment.has_value()) {
			updated[i].comment = std::move(original[i].comment);
		}
	}
	it->second.values = std::move(values);
	changed_ = true;
}

void Ini::set(std::string_view section, std::string_view key, std::span<const std::string> strings)
{
	if (strings.empty()) {
		set(section, key, Values {});
	} else if (strings.size() == 1) {
		set(section, key, Values { Value { .comment = {}, .value = strings[0] } });
	} else {
		Values values;
		auto &items = std::get<std::vector<Value>>(values.rep_);
		items.reserve(strings.size());
		for (const std::string &str : strings) {
			items.push_back(Value { .comment = {}, .value = str });
		}
		set(section, key, std::move(values));
	}
}

void Ini::set(std::string_view section, std::string_view key, std::string &&value)
{
	set(section, key, Values { Value { .comment = {}, .value = std::move(value) } });
}

void Ini::set(std::string_view section, std::string_view key, std::string_view value)
{

	set(section, key, std::string(value));
}

void Ini::set(std::string_view section, std::string_view key, int value)
{
	set(section, key, StrCat(value));
}

void Ini::set(std::string_view section, std::string_view key, bool value)
{
	set(section, key, std::string(value ? "1" : "0"));
}

void Ini::set(std::string_view section, std::string_view key, float value)
{
#ifndef PS2
	constexpr size_t BufSize = 64;
	char buf[BufSize] {};
	const std::to_chars_result result = std::to_chars(buf, buf + BufSize, value);
	if (result.ec != std::errc()) {
		app_fatal("float->string failed"); // should never happen
	}
	set(section, key, std::string_view(buf, result.ptr - buf));
#endif
}

namespace {

// Appends a possibly multi-line comment, converting \n to \r\n.
void AppendComment(std::string_view comment, std::string &out)
{
	bool prevR = false;
	for (const char c : comment) {
		if (c == '\r') {
			prevR = true;
		} else {
			if (c == '\n' && !prevR) out += '\r';
			prevR = false;
		}
		out += c;
	}
}

void AppendSection(std::string_view sectionName, std::string &out)
{
	out.append("[").append(sectionName).append("]\r\n");
}

void AppendKeyValue(std::string_view key, std::string_view value, std::string &out)
{
	out.append(key).append("=").append(value).append("\r\n");
}

} // namespace

std::string Ini::serialize() const
{
	std::string result;
	std::vector<std::pair<std::string, SectionData>> sections(data_.sections.begin(), data_.sections.end());
	c_sort(sections, OrderByValueIndex<SectionData>);

	std::vector<std::pair<std::string, ValuesData>> entries;
	for (auto &[sectionName, section] : sections) {
		if (!result.empty()) result.append("\r\n");
		if (!section.comment.empty()) AppendComment(section.comment, result);
		AppendSection(sectionName, result);
		entries.assign(section.entries.begin(), section.entries.end());
		c_sort(entries, OrderByValueIndex<ValuesData>);
		for (const auto &[key, entry] : entries) {
			for (const auto &[comment, value] : entry.values.get()) {
				if (comment.has_value() && !comment->empty()) AppendComment(*comment, result);
				AppendKeyValue(key, value, result);
			}
		}
	}
	return result;
}

} // namespace devilution
