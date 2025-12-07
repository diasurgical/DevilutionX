#include "utils/utf8.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <SheenBidi/SheenBidi.h>

namespace devilution {

char32_t DecodeFirstUtf8CodePoint(std::string_view input, std::size_t *len)
{
	SBUInteger index = 0;
	const SBCodepoint result = SBCodepointDecodeNextFromUTF8(
	    reinterpret_cast<const SBUInt8 *>(input.data()), static_cast<SBUInteger>(input.size()), &index);
	*len = index;
	return result;
}

std::string_view TruncateUtf8(std::string_view str, std::size_t len)
{
	if (str.size() > len) {
		std::size_t truncIndex = len;
		while (truncIndex > 0 && IsTrailUtf8CodeUnit(str[truncIndex]))
			truncIndex--;
		str.remove_suffix(str.size() - truncIndex);
	}
	return str;
}

void CopyUtf8(char *dest, std::string_view source, std::size_t bytes)
{
	source = TruncateUtf8(source, bytes - 1);
	// source.empty() can mean source.data() == nullptr.
	// It is UB to pass a null pointer to memcpy, so we guard against it.
	if (!source.empty()) {
		std::memcpy(dest, source.data(), source.size());
	}
	dest[source.size()] = '\0';
}

void AppendUtf8(char32_t codepoint, std::string &out)
{
	if (codepoint <= 0x7F) {
		out += static_cast<char>(codepoint);
		return;
	}

	char buf[4];
	if (codepoint <= 0x7FF) {
		buf[0] = static_cast<char>(0xC0 | (codepoint >> 6 & 0x3F));
		buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
		out.append(buf, 2);
	} else if (codepoint <= 0xFFFF) {
		buf[0] = static_cast<char>(0xE0 | (codepoint >> 12 & 0x3F));
		buf[1] = static_cast<char>(0x80 | (codepoint >> 6 & 0x3F));
		buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
		out.append(buf, 3);
	} else {
		buf[0] = static_cast<char>(0xF0 | (codepoint >> 18 & 0x3F));
		buf[1] = static_cast<char>(0x80 | (codepoint >> 12 & 0x3F));
		buf[2] = static_cast<char>(0x80 | (codepoint >> 6 & 0x3F));
		buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
		out.append(buf, 4);
	}
}

} // namespace devilution
