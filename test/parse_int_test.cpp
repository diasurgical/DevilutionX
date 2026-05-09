#include <gtest/gtest.h>

#include "utils/parse_int.hpp"

namespace devilution {
TEST(ParseIntTest, ParseInt)
{
	ParseIntResult<int> result = ParseInt<int>("");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), ParseIntError::ParseError);

	result = ParseInt<int>("abcd");
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), ParseIntError::ParseError);

	result = ParseInt<int>("12");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), 12);

	result = ParseInt<int>(("99999999"), -5, 100);
	ASSERT_FALSE(result.has_value());
	EXPECT_EQ(result.error(), ParseIntError::OutOfRange);

	ParseIntResult<int8_t> shortResult = ParseInt<int8_t>(("99999999"));
	ASSERT_FALSE(shortResult.has_value());
	EXPECT_EQ(shortResult.error(), ParseIntError::OutOfRange);
}

TEST(ParseIntTest, ParseFixed6Fraction)
{
	EXPECT_EQ(ParseFixed6Fraction(""), 0);
	EXPECT_EQ(ParseFixed6Fraction("0"), 0);
	EXPECT_EQ(ParseFixed6Fraction("00781249"), 0);
	EXPECT_EQ(ParseFixed6Fraction("0078125"), 1);
	EXPECT_EQ(ParseFixed6Fraction("015625"), 1);
	EXPECT_EQ(ParseFixed6Fraction("03125"), 2);
	EXPECT_EQ(ParseFixed6Fraction("046875"), 3);
	EXPECT_EQ(ParseFixed6Fraction("0625"), 4);
	EXPECT_EQ(ParseFixed6Fraction("078125"), 5);
	EXPECT_EQ(ParseFixed6Fraction("09375"), 6);
	EXPECT_EQ(ParseFixed6Fraction("109375"), 7);
	EXPECT_EQ(ParseFixed6Fraction("125"), 8);
	EXPECT_EQ(ParseFixed6Fraction("140625"), 9);
	EXPECT_EQ(ParseFixed6Fraction("15625"), 10);
	EXPECT_EQ(ParseFixed6Fraction("171875"), 11);
	EXPECT_EQ(ParseFixed6Fraction("1875"), 12);
	EXPECT_EQ(ParseFixed6Fraction("203125"), 13);
	EXPECT_EQ(ParseFixed6Fraction("21875"), 14);
	EXPECT_EQ(ParseFixed6Fraction("234375"), 15);
	EXPECT_EQ(ParseFixed6Fraction("25"), 16);
	EXPECT_EQ(ParseFixed6Fraction("265625"), 17);
	EXPECT_EQ(ParseFixed6Fraction("28125"), 18);
	EXPECT_EQ(ParseFixed6Fraction("296875"), 19);
	EXPECT_EQ(ParseFixed6Fraction("3125"), 20);
	EXPECT_EQ(ParseFixed6Fraction("328125"), 21);
	EXPECT_EQ(ParseFixed6Fraction("34375"), 22);
	EXPECT_EQ(ParseFixed6Fraction("359375"), 23);
	EXPECT_EQ(ParseFixed6Fraction("375"), 24);
	EXPECT_EQ(ParseFixed6Fraction("390625"), 25);
	EXPECT_EQ(ParseFixed6Fraction("40625"), 26);
	EXPECT_EQ(ParseFixed6Fraction("421875"), 27);
	EXPECT_EQ(ParseFixed6Fraction("4375"), 28);
	EXPECT_EQ(ParseFixed6Fraction("453125"), 29);
	EXPECT_EQ(ParseFixed6Fraction("46875"), 30);
	EXPECT_EQ(ParseFixed6Fraction("484375"), 31);
	EXPECT_EQ(ParseFixed6Fraction("5"), 32);
	EXPECT_EQ(ParseFixed6Fraction("515625"), 33);
	EXPECT_EQ(ParseFixed6Fraction("53125"), 34);
	EXPECT_EQ(ParseFixed6Fraction("546875"), 35);
	EXPECT_EQ(ParseFixed6Fraction("5625"), 36);
	EXPECT_EQ(ParseFixed6Fraction("578125"), 37);
	EXPECT_EQ(ParseFixed6Fraction("59375"), 38);
	EXPECT_EQ(ParseFixed6Fraction("609375"), 39);
	EXPECT_EQ(ParseFixed6Fraction("625"), 40);
	EXPECT_EQ(ParseFixed6Fraction("640625"), 41);
	EXPECT_EQ(ParseFixed6Fraction("65625"), 42);
	EXPECT_EQ(ParseFixed6Fraction("671875"), 43);
	EXPECT_EQ(ParseFixed6Fraction("6875"), 44);
	EXPECT_EQ(ParseFixed6Fraction("703125"), 45);
	EXPECT_EQ(ParseFixed6Fraction("71875"), 46);
	EXPECT_EQ(ParseFixed6Fraction("734375"), 47);
	EXPECT_EQ(ParseFixed6Fraction("75"), 48);
	EXPECT_EQ(ParseFixed6Fraction("765625"), 49);
	EXPECT_EQ(ParseFixed6Fraction("78125"), 50);
	EXPECT_EQ(ParseFixed6Fraction("796875"), 51);
	EXPECT_EQ(ParseFixed6Fraction("8125"), 52);
	EXPECT_EQ(ParseFixed6Fraction("828125"), 53);
	EXPECT_EQ(ParseFixed6Fraction("84375"), 54);
	EXPECT_EQ(ParseFixed6Fraction("859375"), 55);
	EXPECT_EQ(ParseFixed6Fraction("875"), 56);
	EXPECT_EQ(ParseFixed6Fraction("890625"), 57);
	EXPECT_EQ(ParseFixed6Fraction("90625"), 58);
	EXPECT_EQ(ParseFixed6Fraction("921875"), 59);
	EXPECT_EQ(ParseFixed6Fraction("9375"), 60);
	EXPECT_EQ(ParseFixed6Fraction("953125"), 61);
	EXPECT_EQ(ParseFixed6Fraction("96875"), 62);
	EXPECT_EQ(ParseFixed6Fraction("984375"), 63);
	EXPECT_EQ(ParseFixed6Fraction("99218749"), 63);
	EXPECT_EQ(ParseFixed6Fraction("9921875"), 64);
}

TEST(ParseInt, ParseFixed6)
{
	ParseIntResult<int> result = ParseFixed6<int>("");
	ASSERT_FALSE(result.has_value()) << "Empty strings are not valid fixed point values.";
	EXPECT_EQ(result.error(), ParseIntError::ParseError) << "ParseFixed6 should give a ParseError code when parsing an empty string.";

	result = ParseFixed6<int>("abcd");
	ASSERT_FALSE(result.has_value()) << "Non-numeric strings should not be parsed as a fixed-point value.";
	EXPECT_EQ(result.error(), ParseIntError::ParseError) << "ParseFixed6 should give a ParseError code when parsing a non-numeric string.";

	result = ParseFixed6<int>(".");
	ASSERT_FALSE(result.has_value()) << "To match std::from_chars ParseFixed6 should fail to parse a decimal string with no digits.";
	EXPECT_EQ(result.error(), ParseIntError::ParseError) << "Decimal strings with no digits are reported as ParseError codes.";

	result = ParseFixed6<int>("1.");
	ASSERT_TRUE(result.has_value()) << "A trailing decimal point is permitted for fixed point values";
	EXPECT_EQ(result.value(), 1 << 6);

	result = ParseFixed6<int>(".5");
	ASSERT_TRUE(result.has_value()) << "A fixed point value with no integer part is accepted";
	EXPECT_EQ(result.value(), 32);

	const std::string_view badString { "-." };
	const char *endOfParse = nullptr;
	result = ParseFixed6<int>(badString, &endOfParse);
	ASSERT_FALSE(result.has_value()) << "To match std::from_chars ParseFixed6 should fail to parse a decimal string with no digits, even if it starts with a minus sign.";
	EXPECT_EQ(result.error(), ParseIntError::ParseError) << "Decimal strings with no digits are reported as ParseError codes.";
	EXPECT_EQ(endOfParse, badString.data()) << "Failed fixed point parsing should set the end pointer to match the start of the string even though it read multiple characters";

	result = ParseFixed6<int>("-1.");
	ASSERT_TRUE(result.has_value()) << "negative fixed point values are handled when reading into signed types";
	EXPECT_EQ(result.value(), -1 << 6);

	result = ParseFixed6<int>("-1.25");
	ASSERT_TRUE(result.has_value()) << "negative fixed point values are handled when reading into signed types";
	EXPECT_EQ(result.value(), -((1 << 6) + 16)) << "and the fraction part is combined with the integer part respecting the sign";

	result = ParseFixed6<int>("-.25");
	ASSERT_TRUE(result.has_value()) << "negative fixed point values with no integer digits are handled when reading into signed types";
	EXPECT_EQ(result.value(), -16) << "and the fraction part is used respecting the sign";

	result = ParseFixed6<int>("-0.25");
	ASSERT_TRUE(result.has_value()) << "negative fixed point values with an explicit -0 integer part are handled when reading into signed types";
	EXPECT_EQ(result.value(), -16) << "and the fraction part is used respecting the sign";

	ParseIntResult<unsigned> unsignedResult = ParseFixed6<unsigned>("-1.");
	ASSERT_FALSE(unsignedResult.has_value()) << "negative fixed point values are not permitted when reading into unsigned types";
	EXPECT_EQ(unsignedResult.error(), ParseIntError::ParseError) << "Attempting to parse a negative value into an unsigned type is a ParseError, not an OutOfRange value";
}
} // namespace devilution
