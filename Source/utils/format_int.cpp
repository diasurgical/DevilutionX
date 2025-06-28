#include "utils/format_int.hpp"

#include <cstdint>
#include <string_view>

#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {

std::string FormatInteger(int n)
{
	constexpr size_t GroupSize = 3;

	char buf[40];
	char *begin = buf;
	const char *end = BufCopy(buf, n);
	const size_t len = end - begin;

	std::string out;
	const size_t prefixLen = n < 0 ? 1 : 0;
	const size_t numLen = len - prefixLen;
	if (numLen <= GroupSize) {
		out.append(begin, len);
		return out;
	}

	const std::string_view separator = _(/* TRANSLATORS: Thousands separator */ ",");
	out.reserve(len + separator.size() * (numLen - 1) / GroupSize);
	if (n < 0) {
		out += '-';
		++begin;
	}

	size_t mlen = numLen % GroupSize;
	if (mlen == 0)
		mlen = GroupSize;
	out.append(begin, mlen);
	begin += mlen;
	for (; begin != end; begin += GroupSize) {
		out.append(separator);
		out.append(begin, GroupSize);
	}

	return out;
}

std::string FormatInteger(uint32_t n)
{
	constexpr size_t GroupSize = 3;

	char buf[40];
	char *begin = buf;
	const char *end = BufCopy(buf, n);
	const size_t len = end - begin;

	std::string out;
	const size_t numLen = len;
	if (numLen <= GroupSize) {
		out.append(begin, len);
		return out;
	}

	const std::string_view separator = _(/* TRANSLATORS: Thousands separator */ ",");
	out.reserve(len + separator.size() * (numLen - 1) / GroupSize);

	size_t mlen = numLen % GroupSize;
	if (mlen == 0)
		mlen = GroupSize;
	out.append(begin, mlen);
	begin += mlen;
	for (; begin != end; begin += GroupSize) {
		out.append(separator);
		out.append(begin, GroupSize);
	}

	return out;
}

} // namespace devilution
