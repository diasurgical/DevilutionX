#include <gtest/gtest.h>

#include "utils/fixed_point.hpp"

namespace devilution {
namespace {

TEST(FixedPointTest, FromInt_RoundTripsThroughWhole)
{
	const Fixed10_6 value = Fixed10_6::fromInt(5);
	EXPECT_EQ(value.whole(), 5);
	EXPECT_EQ(value.fractional(), 0);
	EXPECT_EQ(value.raw(), 5 << 6);
}

TEST(FixedPointTest, FromRaw_SplitsWholeAndFractional)
{
	const Fixed10_6 value = Fixed10_6::fromRaw((3 << 6) + 17);
	EXPECT_EQ(value.whole(), 3);
	EXPECT_EQ(value.fractional(), 17);
}

TEST(FixedPointTest, Addition)
{
	const Fixed10_6 a = Fixed10_6::fromInt(2);
	const Fixed10_6 b = Fixed10_6::fromRaw(10);
	EXPECT_EQ((a + b).raw(), (2 << 6) + 10);
}

TEST(FixedPointTest, Subtraction)
{
	const Fixed10_6 a = Fixed10_6::fromInt(5);
	const Fixed10_6 b = Fixed10_6::fromInt(2);
	EXPECT_EQ((a - b), Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, Negation)
{
	const Fixed10_6 value = Fixed10_6::fromInt(4);
	EXPECT_EQ(-value, Fixed10_6::fromInt(-4));
}

TEST(FixedPointTest, MultiplyByScalar)
{
	const Fixed10_6 value = Fixed10_6::fromInt(3);
	EXPECT_EQ(value * 4, Fixed10_6::fromInt(12));
}

TEST(FixedPointTest, DivideByScalar)
{
	const Fixed10_6 value = Fixed10_6::fromInt(12);
	EXPECT_EQ(value / 4, Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, CompoundAssignment)
{
	Fixed10_6 value = Fixed10_6::fromInt(1);
	value += Fixed10_6::fromInt(2);
	value *= 3;
	EXPECT_EQ(value, Fixed10_6::fromInt(9));
}

TEST(FixedPointTest, Comparisons)
{
	const Fixed10_6 a = Fixed10_6::fromInt(1);
	const Fixed10_6 b = Fixed10_6::fromInt(2);
	EXPECT_LT(a, b);
	EXPECT_LE(a, a);
	EXPECT_GT(b, a);
	EXPECT_GE(b, b);
	EXPECT_NE(a, b);
	EXPECT_EQ(a, Fixed10_6::fromInt(1));
}

TEST(FixedPointTest, WiderStorageAvoidsOverflow)
{
	const Fixed26_6 value = Fixed26_6::fromInt(2000);
	EXPECT_EQ(value.whole(), 2000);
	EXPECT_EQ(value.raw(), 2000 << 6);
}

TEST(FixedPointTest, MultiplyByFixedPoint)
{
	const Fixed10_6 half = Fixed10_6::fromRaw(32); // 0.5
	EXPECT_EQ(Fixed10_6::fromInt(10) * half, Fixed10_6::fromInt(5));
	EXPECT_EQ(Fixed10_6::fromInt(3) * Fixed10_6::fromInt(4), Fixed10_6::fromInt(12));
}

TEST(FixedPointTest, MultiplyByFixedPointDoesNotOverflowWideStorage)
{
	const Fixed26_6 large = Fixed26_6::fromInt(2000);
	const Fixed26_6 doubled = Fixed26_6::fromInt(2);
	EXPECT_EQ(large * doubled, Fixed26_6::fromInt(4000));
}

TEST(FixedPointTest, WideningConversion)
{
	const Fixed10_6 narrow = Fixed10_6::fromInt(5);
	const Fixed26_6 widened { narrow };
	EXPECT_EQ(widened, Fixed26_6::fromInt(5));
}

} // namespace
} // namespace devilution
