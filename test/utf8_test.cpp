
#include <gtest/gtest.h>

#include "utils/utf8.hpp"

namespace devilution {
namespace {

TEST(DecodeFirstUtf8CodePointTest, OneByteCodePoint)
{
	size_t len;
	const char32_t cp = DecodeFirstUtf8CodePoint("a", &len);
	EXPECT_EQ(cp, U'a');
	EXPECT_EQ(len, 1);
}

TEST(DecodeFirstUtf8CodePointTest, TwoByteCodePoint)
{
	size_t len;
	const char32_t cp = DecodeFirstUtf8CodePoint("ж", &len);
	EXPECT_EQ(cp, U'ж');
	EXPECT_EQ(len, 2);
}

TEST(DecodeFirstUtf8CodePointTest, ThreeByteCodePoint)
{
	size_t len;
	const char32_t cp = DecodeFirstUtf8CodePoint("€", &len);
	EXPECT_EQ(cp, U'€');
	EXPECT_EQ(len, 3);
}

TEST(DecodeFirstUtf8CodePointTest, FourByteCodePoint)
{
	size_t len;
	const char32_t cp = DecodeFirstUtf8CodePoint("💡", &len);
	EXPECT_EQ(cp, U'💡');
	EXPECT_EQ(len, 4);
}

TEST(DecodeFirstUtf8CodePointTest, InvalidCodePoint)
{
	size_t len;
	const char32_t cp = DecodeFirstUtf8CodePoint("\xc3\x28", &len);
	EXPECT_EQ(cp, Utf8DecodeError);
	EXPECT_EQ(len, 1);
}

TEST(AppendUtf8Test, OneByteCodePoint)
{
	std::string s = "x";
	AppendUtf8(U'a', s);
	EXPECT_EQ(s, "xa");
}

TEST(AppendUtf8Test, TwoByteCodePoint)
{
	std::string s = "x";
	AppendUtf8(U'Á', s);
	EXPECT_EQ(s, "xÁ");
}

TEST(AppendUtf8Test, ThreeByteCodePoint)
{
	std::string s;
	AppendUtf8(U'€', s);
	EXPECT_EQ(s, "€");
}

TEST(AppendUtf8Test, FourByteCodePoint)
{
	std::string s;
	AppendUtf8(U'💡', s);
	EXPECT_EQ(s, "💡");
}

TEST(Utf8CodeUnits, ValidCodePoints)
{
	// Working backwards on this loop to avoid triggering signed integer overflow on platforms where char has an
	// underlying type of signed char
	for (char x = '\x7F'; static_cast<signed char>(x) >= '\x00'; x--) {
		EXPECT_FALSE(IsTrailUtf8CodeUnit(x)) << "Basic Latin and ASCII Control characters are not trail code units";
	}

	for (char x = '\x80'; x <= '\xBF'; x++) {
		EXPECT_TRUE(IsTrailUtf8CodeUnit(x)) << "Bytes in the range 0x80 to 0xBF are potentially valid trail code units";
	}

	for (char x = '\xC2'; x <= '\xF4'; x++) {
		EXPECT_FALSE(IsTrailUtf8CodeUnit(x)) << "Bytes in the range 0xC2 to 0xF4 are never valid trail code units";
	}
}

TEST(Utf8CodeUnits, InvalidCodePoints)
{
	for (char x = '\xC0'; x <= '\xC1'; x++) {
		EXPECT_FALSE(IsTrailUtf8CodeUnit(x)) << "Bytes in the range 0xC0 to oxC1 are not trail code units";
	}

	for (char x = '\xFF'; x >= '\xF5'; x--) {
		EXPECT_FALSE(IsTrailUtf8CodeUnit(x)) << "Bytes in the range 0xF5 to 0xFF are not trail code units";
	}
}

TEST(Utf8CodeUnits, BasicLatin)
{
	for (char x = '\x00'; x < '\x20'; x++) {
		EXPECT_FALSE(IsBasicLatin(x)) << "ASCII Control characters are not Basic Latin symbols";
	}

	for (char x = '\x20'; x <= '\x7E'; x++) {
		EXPECT_TRUE(IsBasicLatin(x)) << "Basic Latin symbols are denoted by the range 0x20 to 0x7E inclusive";
	}

	EXPECT_FALSE(IsBasicLatin('\x7F')) << "ASCII Control character DEL is not a Basic Latin symbol";

	// Tests '\xFF' separately to avoid infinite loop on platforms with unsigned char.
	for (char x = '\x80'; x < '\xFF'; x++) {
		EXPECT_FALSE(IsBasicLatin(x)) << "Multibyte Utf8 code units are not Basic Latin symbols";
	}
	EXPECT_FALSE(IsBasicLatin('\xFF')) << "Multibyte Utf8 code units are not Basic Latin symbols";
}

} // namespace
} // namespace devilution
