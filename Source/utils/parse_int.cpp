#include "parse_int.hpp"

#include <algorithm>

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

uint8_t ParseFixed6Fraction(string_view str, const char **endOfParse)
{
	unsigned numDigits = 0;
	uint32_t decimalFraction = 0;

	// Read at most 7 digits, at that threshold we're able to determine an exact rounding for 6 bit fixed point numbers
	while (!str.empty() && numDigits < 7) {
		if (str[0] < '0' || str[0] > '9') {
			break;
		}
		decimalFraction = decimalFraction * 10 + str[0] - '0';
		++numDigits;
		str.remove_prefix(1);
	}
	if (endOfParse != nullptr) {
		// to mimic the behaviour of std::from_chars consume all remaining digits in case the value was overly precise.
		*endOfParse = std::find_if_not(str.data(), str.data() + str.size(), [](char character) { return character >= '0' && character <= '9'; });
	}
	// to ensure rounding to nearest we normalise all values to 7 decimal places
	while (numDigits < 7) {
		decimalFraction *= 10;
		++numDigits;
	}
	// we add half the step between representable values to use integer truncation as a substitute for rounding to nearest.
	return (decimalFraction + 78125) / 156250;
}

} // namespace devilution
