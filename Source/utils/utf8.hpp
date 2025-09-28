#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

constexpr char32_t Utf8DecodeError = 0xD83F;

/**
 * Decodes the first code point from UTF8-encoded input.
 *
 * Sets `len` to the length of the code point in bytes.
 * Returns `Utf8DecodeError` on error.
 */
char32_t DecodeFirstUtf8CodePoint(string_view input, std::size_t *len);

/**
 * Decodes and removes the first code point from UTF8-encoded input.
 */
inline char32_t ConsumeFirstUtf8CodePoint(string_view *input)
{
	std::size_t len;
	const char32_t result = DecodeFirstUtf8CodePoint(*input, &len);
	input->remove_prefix(len);
	return result;
}

/**
 * Returns true if the character is part of the Basic Latin set.
 *
 * This includes ASCII punctuation, symbols, math operators, digits, and both uppercase/lowercase latin alphabets
 */
constexpr bool IsBasicLatin(char x)
{
	return x >= '\x20' && x <= '\x7E';
}

/**
 * Returns true if this is a trailing byte in a UTF-8 code point encoding.
 *
 * Trailing bytes all begin with 10 as the most significant bits, meaning they generally fall in the range 0x80 to
 * 0xBF. Please note that certain 3 and 4 byte sequences use a narrower range for the second byte, this function is
 * not intended to guarantee the character is valid within the sequence (or that the sequence is well-formed).
 */
inline bool IsTrailUtf8CodeUnit(char x)
{
	// The following is equivalent to a bitmask test (x & 0xC0) == 0x80
	// On x86_64 architectures it ends up being one instruction shorter
	return static_cast<signed char>(x) < static_cast<signed char>('\xC0');
}

/**
 * @brief Returns the number of code units for a code point starting at *src;
 *
 * `src` must not be empty.
 * If `src` does not begin with a UTF-8 code point start byte, returns 1.
 */
inline size_t Utf8CodePointLen(const char *src)
{
	// This constant is effectively a lookup table for 2-bit keys, where
	// values represent code point length - 1.
	// `-1` is so that this method never returns 0, even for invalid values
	// (which could lead to infinite loops in some code).
	// Generated with:
	// ruby -e 'p "0000000000000000000000001111223".reverse.to_i(4).to_s(16)'
	return ((0x3a55000000000000ULL >> (2 * (static_cast<unsigned char>(*src) >> 3))) & 0x3) + 1;
}

/**
 * Returns the start byte index of the last code point in a UTF-8 string.
 */
inline std::size_t FindLastUtf8Symbols(string_view input)
{
	if (input.empty())
		return 0;

	std::size_t pos = input.size() - 1;
	while (pos > 0 && IsTrailUtf8CodeUnit(input[pos]))
		--pos;
	return pos;
}

/**
 * @brief Copy up to a given number of bytes from a UTF8 string, and zero terminate string
 * @param dest The destination buffer
 * @param source The source string
 * @param bytes Max number of bytes to copy
 */
void CopyUtf8(char *dest, string_view source, std::size_t bytes);

void AppendUtf8(char32_t codepoint, std::string &out);

/** @brief Truncates `str` to at most `len` at a code point boundary. */
std::string_view TruncateUtf8(std::string_view str, std::size_t len);

} // namespace devilution
