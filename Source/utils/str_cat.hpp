#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace devilution {

struct AsHexU8Pad2 {
	uint8_t value;
	bool uppercase = false;
};

struct AsHexU16Pad2 {
	uint16_t value;
	bool uppercase = false;
};

/** @brief Maximum number of digits for a value of type T.
 * If T is signed, also accounts for a potential '-'.
 */
template <typename T>
constexpr size_t MaxNumDigits = (8 * sizeof(T) * 28 / 93) + 1 + (std::is_signed_v<T> ? 1 : 0);

template <typename T>
struct LeftPadT {
	T value;
	unsigned length;
	char padChar;
};

/**
 * @brief Formats the value as a lowercase zero-padded hexadecimal with at least 2 hex digits (0-padded on the left).
 */
constexpr AsHexU8Pad2 AsHexPad2(uint8_t value, bool uppercase = false) { return { value, uppercase }; }

/**
 * @brief Formats the value as a lowercase zero-padded hexadecimal with at least 2 hex digits (0-padded on the left).
 */
constexpr AsHexU16Pad2 AsHexPad2(uint16_t value, bool uppercase = false) { return { value, uppercase }; }

/**
 * @brief Left-pads the value to the specified length with the specified character.
 */
template <typename T>
constexpr LeftPadT<T> LeftPad(T value, unsigned length, char padChar) { return { value, length, padChar }; }

/**
 * @brief Writes the integer to the given buffer.
 * @return char* end of the buffer
 */
char *BufCopy(char *out, long long value);
inline char *BufCopy(char *out, long value)
{
	return BufCopy(out, static_cast<long long>(value));
}
inline char *BufCopy(char *out, int value)
{
	return BufCopy(out, static_cast<long long>(value));
}
inline char *BufCopy(char *out, short value)
{
	return BufCopy(out, static_cast<long long>(value));
}

/**
 * @brief Writes the integer to the given buffer.
 * @return char* end of the buffer
 */
char *BufCopy(char *out, unsigned long long value);
inline char *BufCopy(char *out, unsigned long value)
{
	return BufCopy(out, static_cast<unsigned long long>(value));
}
inline char *BufCopy(char *out, unsigned int value)
{
	return BufCopy(out, static_cast<unsigned long long>(value));
}
inline char *BufCopy(char *out, unsigned short value)
{
	return BufCopy(out, static_cast<unsigned long long>(value));
}

char *BufCopy(char *out, AsHexU8Pad2 value);
char *BufCopy(char *out, AsHexU16Pad2 value);

template <typename T>
char *BufCopy(char *out, LeftPadT<T> value)
{
	char buf[MaxNumDigits<T>];
	const char *end = BufCopy(buf, value.value);
	const std::string_view str = std::string_view(buf, end - buf);
	for (size_t i = str.size(); i < value.length; ++i) {
		*out++ = value.padChar;
	}
	std::memcpy(out, str.data(), str.size());
	return out + str.size();
}

/**
 * @brief Appends the integer to the given string.
 */
void StrAppend(std::string &out, long long value);
inline void StrAppend(std::string &out, long value)
{
	StrAppend(out, static_cast<long long>(value));
}
inline void StrAppend(std::string &out, int value)
{
	StrAppend(out, static_cast<long long>(value));
}
inline void StrAppend(std::string &out, short value)
{
	StrAppend(out, static_cast<long long>(value));
}

/**
 * @brief Appends the integer to the given string.
 */
void StrAppend(std::string &out, unsigned long long value);
inline void StrAppend(std::string &out, unsigned long value)
{
	StrAppend(out, static_cast<unsigned long long>(value));
}
inline void StrAppend(std::string &out, unsigned int value)
{
	StrAppend(out, static_cast<unsigned long long>(value));
}
inline void StrAppend(std::string &out, unsigned short value)
{
	StrAppend(out, static_cast<unsigned long long>(value));
}

void StrAppend(std::string &out, AsHexU8Pad2 value);
void StrAppend(std::string &out, AsHexU16Pad2 value);

template <typename T>
void StrAppend(std::string &out, LeftPadT<T> value)
{
	char buf[MaxNumDigits<T>];
	const auto len = static_cast<size_t>(BufCopy(buf, value) - buf);
	out.append(buf, len);
}

/**
 * @brief Copies the given std::string_view to the given buffer.
 */
inline char *BufCopy(char *out, std::string_view value)
{
	std::memcpy(out, value.data(), value.size());
	return out + value.size();
}

/**
 * @brief Copies the given std::string_view to the given string.
 */
inline void StrAppend(std::string &out, std::string_view value)
{
	out.append(value);
}

/**
 * @brief Appends the given C string to the given buffer.
 *
 * `str` must be a null-terminated C string, `out` will not be null terminated.
 */
inline char *BufCopy(char *out, const char *str)
{
	return BufCopy(out, std::string_view(str != nullptr ? str : "(nullptr)"));
}

/**
 * @brief Appends the given C string to the given string.
 */
inline void StrAppend(std::string &out, const char *str)
{
	out.append(std::string_view(str != nullptr ? str : "(nullptr)"));
}

template <typename... Args>
typename std::enable_if<(sizeof...(Args) > 1), char *>::type
BufCopy(char *out, Args &&...args)
{
	return ((out = BufCopy(out, std::forward<Args>(args))), ...);
}

template <typename... Args>
typename std::enable_if<(sizeof...(Args) > 1), void>::type
StrAppend(std::string &out, Args &&...args)
{
	(StrAppend(out, std::forward<Args>(args)), ...);
}

template <typename... Args>
std::string StrCat(Args &&...args)
{
	std::string result;
	StrAppend(result, std::forward<Args>(args)...);
	return result;
}

} // namespace devilution
