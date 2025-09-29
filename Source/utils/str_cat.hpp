#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct AsHexU8Pad2 {
	uint8_t value;
};

struct AsHexU16Pad2 {
	uint16_t value;
};

/**
 * @brief Formats the value as a lowercase zero-padded hexadecimal with at least 2 hex digits (0-padded on the left).
 */
constexpr AsHexU8Pad2 AsHexPad2(uint8_t value) { return { value }; }

/**
 * @brief Formats the value as a lowercase zero-padded hexadecimal with at least 2 hex digits (0-padded on the left).
 */
constexpr AsHexU16Pad2 AsHexPad2(uint16_t value) { return { value }; }

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

/**
 *
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

/**
 * @brief Copies the given string_view to the given buffer.
 */
inline char *BufCopy(char *out, string_view value)
{
	std::memcpy(out, value.data(), value.size());
	return out + value.size();
}

char *BufCopy(char *out, AsHexU8Pad2 value);
char *BufCopy(char *out, AsHexU16Pad2 value);

/**
 * @brief Copies the given string_view to the given string.
 */
inline void StrAppend(std::string &out, string_view value)
{
	AppendStrView(out, value);
}

/**
 * @brief Appends the given C string to the given buffer.
 *
 * `str` must be a null-terminated C string, `out` will not be null terminated.
 */
inline char *BufCopy(char *out, const char *str)
{
	return BufCopy(out, string_view(str != nullptr ? str : "(nullptr)"));
}

/**
 * @brief Appends the given C string to the given string.
 */
inline void StrAppend(std::string &out, const char *str)
{
	AppendStrView(out, string_view(str != nullptr ? str : "(nullptr)"));
}

void StrAppend(std::string &out, AsHexU8Pad2 value);
void StrAppend(std::string &out, AsHexU16Pad2 value);

#if __cplusplus >= 201703L
template <typename... Args>
typename std::enable_if<(sizeof...(Args) > 1), char *>::type
BufCopy(char *out, Args &&...args)
{
	return ((out = BufCopy(out, std::forward<Args>(args))), ...);
}
#else
template <typename Arg, typename... Args>
inline typename std::enable_if<(sizeof...(Args) > 0), char *>::type
BufCopy(char *out, Arg &&arg, Args &&...args)
{
	return BufCopy(BufCopy(out, std::forward<Arg>(arg)), std::forward<Args>(args)...);
}
#endif

#if __cplusplus >= 201703L
template <typename... Args>
typename std::enable_if<(sizeof...(Args) > 1), void>::type
StrAppend(std::string &out, Args &&...args)
{
	(StrAppend(out, std::forward<Args>(args)), ...);
}
#else
template <typename Arg, typename... Args>
typename std::enable_if<(sizeof...(Args) > 0), void>::type
StrAppend(std::string &out, Arg &&arg, Args &&...args)
{
	StrAppend(out, std::forward<Arg>(arg));
	StrAppend(out, std::forward<Args>(args)...);
}
#endif

template <typename... Args>
std::string StrCat(Args &&...args)
{
	std::string result;
	StrAppend(result, std::forward<Args>(args)...);
	return result;
}

} // namespace devilution
