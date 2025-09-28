#include "utils/str_cat.hpp"

#include <fmt/format.h>

namespace devilution {
namespace {

char HexDigit(uint8_t v) { return "0123456789abcdef"[v]; }

} // namespace

char *BufCopy(char *out, long long value)
{
	const fmt::format_int formatted { value };
	std::memcpy(out, formatted.data(), formatted.size());
	return out + formatted.size();
}
char *BufCopy(char *out, unsigned long long value)
{
	const fmt::format_int formatted { value };
	std::memcpy(out, formatted.data(), formatted.size());
	return out + formatted.size();
}
char *BufCopy(char *out, AsHexU8Pad2 value)
{
	*out++ = HexDigit(value.value >> 4);
	*out++ = HexDigit(value.value & 0xf);
	return out;
}
char *BufCopy(char *out, AsHexU16Pad2 value)
{
	if (value.value > 0xff) {
		if (value.value > 0xfff) {
			out = BufCopy(out, AsHexU8Pad2 { static_cast<uint8_t>(value.value >> 8) });
		} else {
			*out++ = HexDigit(value.value >> 8);
		}
	}
	return BufCopy(out, AsHexU8Pad2 { static_cast<uint8_t>(value.value & 0xff) });
}

void StrAppend(std::string &out, long long value)
{
	const fmt::format_int formatted { value };
	out.append(formatted.data(), formatted.size());
}
void StrAppend(std::string &out, unsigned long long value)
{
	const fmt::format_int formatted { value };
	out.append(formatted.data(), formatted.size());
}
void StrAppend(std::string &out, AsHexU8Pad2 value)
{
	char hex[2];
	BufCopy(hex, value);
	out.append(hex, 2);
}

void StrAppend(std::string &out, AsHexU16Pad2 value)
{
	char hex[4];
	const auto len = static_cast<size_t>(BufCopy(hex, value) - hex);
	out.append(hex, len);
}

} // namespace devilution
