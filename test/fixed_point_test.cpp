#include <gtest/gtest.h>

#include <type_traits>

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

TEST(FixedPointTest, MultiplyWithoutWideningMatchesOperatorStarWhenItFits)
{
	const Fixed10_6 half = Fixed10_6::fromRaw(32); // 0.5
	EXPECT_EQ(Fixed10_6::fromInt(10).multiplyWithoutWidening(half), Fixed10_6::fromInt(5));
	EXPECT_EQ(Fixed10_6::fromInt(3).multiplyWithoutWidening(Fixed10_6::fromInt(4)), Fixed10_6::fromInt(12));
}

TEST(FixedPointTest, DivideByFixedPoint)
{
	const Fixed10_6 half = Fixed10_6::fromRaw(32); // 0.5
	EXPECT_EQ(Fixed10_6::fromInt(10) / half, Fixed10_6::fromInt(20));
	EXPECT_EQ(Fixed10_6::fromInt(12) / Fixed10_6::fromInt(4), Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, DivideByFixedPointDoesNotOverflowWideStorage)
{
	const Fixed26_6 large = Fixed26_6::fromInt(2000);
	const Fixed26_6 tiny = Fixed26_6::fromRaw(1); // smallest positive representable value
	// Scaling `large` up by 2^FractionalBits before dividing would overflow a narrower
	// intermediate; this only works out to fromInt(2000 * 64) if the wide intermediate holds.
	EXPECT_EQ(large / tiny, Fixed26_6::fromInt(2000 * 64));
}

TEST(FixedPointTest, DivideWithoutWideningMatchesOperatorSlashWhenItFits)
{
	const Fixed10_6 half = Fixed10_6::fromRaw(32); // 0.5
	EXPECT_EQ(Fixed10_6::fromInt(10).divideWithoutWidening(half), Fixed10_6::fromInt(20));
	EXPECT_EQ(Fixed10_6::fromInt(12).divideWithoutWidening(Fixed10_6::fromInt(4)), Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, WideningConversion)
{
	const Fixed10_6 narrow = Fixed10_6::fromInt(5);
	const Fixed26_6 widened { narrow };
	EXPECT_EQ(widened, Fixed26_6::fromInt(5));
}

TEST(FixedPointTest, WideningConversionIsImplicit)
{
	static_assert(std::is_convertible_v<Fixed10_6, Fixed26_6>, "Widening a Fixed10_6 to a Fixed26_6 should not require an explicit cast");
	const Fixed26_6 widened = Fixed10_6::fromInt(5); // would not compile if the conversion were explicit-only
	EXPECT_EQ(widened, Fixed26_6::fromInt(5));
}

TEST(FixedPointTest, NarrowingConversionIsExplicitOnly)
{
	static_assert(!std::is_convertible_v<Fixed26_6, Fixed10_6>, "Narrowing a Fixed26_6 to a Fixed10_6 should require an explicit cast");
	const Fixed10_6 narrowed(Fixed26_6::fromInt(5)); // fine: explicit construction
	EXPECT_EQ(narrowed, Fixed10_6::fromInt(5));
}

TEST(FixedPointTest, SameTypeArithmeticPromotesLikeC)
{
	// Adding (or multiplying, etc.) two Fixed10_6 values mimics `int16_t + int16_t`,
	// which promotes to `int`, not `int16_t`.
	const Fixed10_6 a = Fixed10_6::fromInt(1);
	const Fixed10_6 b = Fixed10_6::fromInt(2);
	static_assert(std::is_same_v<decltype(a + b)::StorageType, decltype(int16_t {} + int16_t {})>);
	static_assert(std::is_same_v<decltype(a * b)::StorageType, decltype(int16_t {} * int16_t {})>);
	EXPECT_EQ(a + b, Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, MixedWidthArithmeticPromotesToTheWiderType)
{
	const Fixed10_6 a = Fixed10_6::fromInt(1);
	const Fixed26_6 b = Fixed26_6::fromInt(2);
	static_assert(std::is_same_v<decltype(a + b)::StorageType, decltype(int16_t {} + int32_t {})>);
	EXPECT_EQ(a + b, Fixed26_6::fromInt(3));
	EXPECT_EQ(b + a, Fixed26_6::fromInt(3)); // commutative, regardless of argument order
}

TEST(FixedPointTest, CompoundAssignmentNarrowsBackToTheLeftHandSideType)
{
	// x += y behaves like `int16_t x; x += int32_t_y;`: computed in the promoted type,
	// then narrowed back down to store into x.
	Fixed10_6 value = Fixed10_6::fromInt(1);
	value += Fixed26_6::fromInt(2);
	static_assert(std::is_same_v<decltype(value)::StorageType, int16_t>);
	EXPECT_EQ(value, Fixed10_6::fromInt(3));
}

TEST(FixedPointTest, ComparisonWithIntTreatsItAsWholeUnits)
{
	const Fixed10_6 value = Fixed10_6::fromInt(3);
	EXPECT_TRUE(value == 3);
	EXPECT_TRUE(3 == value);
	EXPECT_TRUE(value != 4);
	EXPECT_TRUE(4 != value);
	EXPECT_TRUE(value < 4);
	EXPECT_TRUE(2 < value);
	EXPECT_TRUE(value <= 3);
	EXPECT_TRUE(3 <= value);
	EXPECT_TRUE(value > 2);
	EXPECT_TRUE(4 > value);
	EXPECT_TRUE(value >= 3);
	EXPECT_TRUE(3 >= value);

	const Fixed10_6 fractional = Fixed10_6::fromRaw((3 << 6) + 1);
	EXPECT_FALSE(fractional == 3); // has a fractional part, so it's not exactly 3
	EXPECT_TRUE(fractional > 3);
}

} // namespace
} // namespace devilution
