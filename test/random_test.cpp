#include <gtest/gtest.h>

#include "engine/random.hpp"

namespace devilution {

// These tests use ASSERT_EQ as the PRNG is expected to depend on state from the last call, so one failing assertion
// means the rest of the results can't be trusted.
TEST(RandomTest, RandomEngineParams)
{
	// The core diablo random number generator is an LCG with Borland constants.
	// This RNG must be available for network/save compatibility for things such as level generation.

	constexpr uint32_t multiplicand = 22695477;
	constexpr uint32_t increment = 1;

	SetRndSeed(0);

	// Starting from a seed of 0 means the multiplicand is dropped and the state advances by increment only
	ASSERT_EQ(GenerateRandomNumber(), increment) << "Increment factor is incorrect";

	// LCGs use a formula of mult * seed + inc. Using a long form in the code to document the expected factors.
	ASSERT_EQ(GenerateRandomNumber(), (multiplicand * 1) + increment) << "Multiplicand factor is incorrect";

	// C++11 defines the default seed for a LCG engine as 1. The ten thousandth value is commonly used for sanity checking
	// a sequence, so as we've had one round since state 1 we need to discard another 9998 values to get to the 10000th state.
	// To make off by one errors more visible test the 9999th value as well as 10000th
	DiscardRandomValues(9997);

	uint32_t expectedState = 3495122800U;
	EXPECT_EQ(GenerateRandomNumber(), expectedState) << "Wrong engine state after 9999 invocations";
	expectedState = 3007658545U;
	ASSERT_EQ(GenerateRandomNumber(), expectedState) << "Wrong engine state after 10000 invocations";
}

TEST(RandomTest, AbsDistribution)
{
	// The default distribution for RNG calls is the absolute value of the generated value interpreted as a signed int
	// This relies on undefined behaviour when called on std::numeric_limits<int_t>::min(). See C17 7.22.6.1
	// The current behaviour is this returns the same value (the most negative number of the type).
	SetRndSeed(1457187811); // yields -2147483648
	ASSERT_EQ(AdvanceRndSeed(), std::numeric_limits<int32_t>::min()) << "Invalid distribution";
	SetRndSeed(3604671459U); // yields 0
	ASSERT_EQ(AdvanceRndSeed(), 0) << "Invalid distribution";

	SetRndSeed(0); // yields +1
	ASSERT_EQ(AdvanceRndSeed(), 1) << "Invalid distribution";
	SetRndSeed(2914375622U); // yields -1
	ASSERT_EQ(AdvanceRndSeed(), 1) << "Invalid distribution";

	SetRndSeed(3604671460U); // yields +22695477
	ASSERT_EQ(AdvanceRndSeed(), 22695477) << "Invalid distribution";
	SetRndSeed(3604671458U); // yields -22695477
	ASSERT_EQ(AdvanceRndSeed(), 22695477) << "Invalid distribution";

	SetRndSeed(1902003768); // yields +429496729
	ASSERT_EQ(AdvanceRndSeed(), 429496729) << "Invalid distribution";
	SetRndSeed(1012371854); // yields -429496729
	ASSERT_EQ(AdvanceRndSeed(), 429496729) << "Invalid distribution";

	SetRndSeed(189776845); // yields +1212022642
	ASSERT_EQ(AdvanceRndSeed(), 1212022642) << "Invalid distribution";
	SetRndSeed(2724598777U); // yields -1212022642
	ASSERT_EQ(AdvanceRndSeed(), 1212022642) << "Invalid distribution";

	SetRndSeed(76596137); // yields +2147483646
	ASSERT_EQ(AdvanceRndSeed(), 2147483646) << "Invalid distribution";
	SetRndSeed(2837779485U); // yields -2147483646
	ASSERT_EQ(AdvanceRndSeed(), 2147483646) << "Invalid distribution";

	SetRndSeed(766891974); // yields +2147483647
	ASSERT_EQ(AdvanceRndSeed(), 2147483647) << "Invalid distribution";
	SetRndSeed(2147483648U); // yields -2147483647
	ASSERT_EQ(AdvanceRndSeed(), 2147483647) << "Invalid distribution";
}

// The bounded distribution function used by Diablo performs a modulo operation on the result of the AbsDistribution
// tested above, with a shift operation applied before the mod when the bound is < 65535.
//
// The current implementation does not allow testing these operations independantly, hopefully this can be changed
// with confidence from these tests.
//
// The result of a mod b when a is negative was implementation defined before C++11, current behaviour is to
// preserve the sign of a. See C++17 [expr.mul] and C++03 TR1 [expr.mul]
TEST(RandomTest, ModDistributionInvalidRange)
{
	// Calling the modulo distribution with an upper bound <= 0 returns 0 without advancing the engine
	auto initialSeed = 44444444;
	SetRndSeed(initialSeed);
	EXPECT_EQ(GenerateRnd(0), 0) << "A distribution with an upper bound of 0 must return 0";
	EXPECT_EQ(GetLCGEngineState(), initialSeed) << "Distribution with invalid range must not advance the engine state";
	EXPECT_EQ(GenerateRnd(-1), 0) << "A distribution with a negative upper bound must return 0";
	EXPECT_EQ(GetLCGEngineState(), initialSeed) << "Distribution with invalid range must not advance the engine state";
	EXPECT_EQ(GenerateRnd(std::numeric_limits<int32_t>::min()), 0)
	    << "A distribution with a negative upper bound must return 0";
	EXPECT_EQ(GetLCGEngineState(), initialSeed) << "Distribution with invalid range must not advance the engine state";
}

TEST(RandomTest, ShiftModDistributionSingleRange)
{
	// All results mod 1 = 0;
	auto initialSeed = ::testing::UnitTest::GetInstance()->random_seed();
	SetRndSeed(initialSeed);
	for (auto i = 0; i < 100; i++)
		ASSERT_EQ(GenerateRnd(1), 0) << "Interval [0, 1) must return 0";
	ASSERT_NE(GetLCGEngineState(), initialSeed) << "Interval of 1 element must still advance the engine state";

	// Just in case -0 has a distinct representation? Shouldn't be possible but cheap to test.
	SetRndSeed(1457187811);
	ASSERT_EQ(GenerateRnd(1), 0) << "Interval [0, 1) must return 0 when AbsDistribution yields INT_MIN";
}

// When called with an upper bound less than or equal to 0x7FFF this distribution function discards the low 16 bits of the output
// from the default distribution by performing a shift right of 16 bits. This relies on implementation defined
// behaviour for negative numbers, the expectation is shift right uses sign carry. See C++17 [expr.shift]
TEST(RandomTest, ShiftModDistributionSignCarry)
{
	// This distribution is used when the upper bound is a value in [1, 32768)
	// Using an upper bound of 1 means the result always maps to 0, see RandomTest_ShiftModDistributionSingleRange

	// The only negative value returned from AbsDistribution is -2147483648
	// A sign-preserving shift right of 16 bits gives a result of -32768.
	// This is greater in magnitude than the limit so always results in a division.
	SetRndSeed(1457187811); // Test mod when a division occurs
	ASSERT_EQ(GenerateRnd(32767), -1) << "Distribution must map negative numbers using sign carry shifts";
}

// The Diablo LCG implementation attempts to improve the quality of generated numbers that would only use the low
// bits of the LCG output but due to applying this after taking the absolute value this introduces bias. This may
// be an inconsistency with the implementation in devilutionx, see the comment for RandomTest_ShiftModDistributionHighBits
TEST(RandomTest, ShiftModDistributionLowBits)
{
	constexpr auto maxBound = 32767;

	// All the following seeds generate values less than 2^16, so after shifting they give a 0 value
	SetRndSeed(3604671459U); // yields 0
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 0";
	SetRndSeed(0); // yields 1
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 1";
	SetRndSeed(2914375622U); // yields -1
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -1";
	SetRndSeed(538964771); // yields 64
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 64";
	SetRndSeed(2375410851U); // yields -64
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -64";
	SetRndSeed(1229260608); // yields 65
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 65";
	SetRndSeed(1685115014); // yields -65
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -65";
	SetRndSeed(1768225379); // yields 128
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 128";
	SetRndSeed(1146150243); // yields -128
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -128";
	SetRndSeed(1480523688); // yields 7625
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 7625";
	SetRndSeed(1433851934); // yields -7625
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -7625";
	SetRndSeed(2382565573U); // yields 32458
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 32458";
	SetRndSeed(531810049); // yields -32458
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -32458";
	SetRndSeed(1625910243); // yields 32768
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 32768";
	SetRndSeed(1288465379); // yields -32768
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -32768";

	// -1 from the max bound for the next two to ensure it's not due to a mod with no remainder
	SetRndSeed(2561524649U); // yields 65534
	ASSERT_EQ(GenerateRnd(maxBound - 1), 0) << "Invalid distribution when generator yields 65534";
	SetRndSeed(352850973U); // yields -65534
	ASSERT_EQ(GenerateRnd(maxBound - 1), 0) << "Invalid distribution when generator yields -65534";

	SetRndSeed(3251820486U); // yields 65535
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 65535";
	SetRndSeed(3957522432U); // yields -65535
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -65535";
}

// The highest value GenerateRnd can return is 32767 (0x7FFF). I suspect this is the Borland rand() implementation
// Diablo appears to have reimplemented this method incorrectly as that function was implemented as the following:
// uint seed = mult * seed + inc
// return (seed >> 16) & 0x7FFF
// i.e., no cast from unsigned to signed, no modulo when building the return value.
TEST(RandomTest, ShiftModDistributionHighBits)
{
	constexpr auto maxBound = 32767;
	SetRndSeed(3267226595U); // yields 65536
	ASSERT_EQ(GenerateRnd(maxBound), 1) << "Invalid distribution when generator yields 65536";
	SetRndSeed(3942116323U); // yields -65536
	ASSERT_EQ(GenerateRnd(maxBound), 1) << "Invalid distribution when generator yields -65536";
	SetRndSeed(4279561187U); // yields 131072
	ASSERT_EQ(GenerateRnd(maxBound), 2) << "Invalid distribution when generator yields 131072";
	SetRndSeed(2929781731U); // yields -131072
	ASSERT_EQ(GenerateRnd(maxBound), 2) << "Invalid distribution when generator yields -131072";
	SetRndSeed(659483619); // yields 262144
	ASSERT_EQ(GenerateRnd(maxBound), 4) << "Invalid distribution when generator yields 262144";
	SetRndSeed(2254892003U); // yields -262144
	ASSERT_EQ(GenerateRnd(maxBound), 4) << "Invalid distribution when generator yields -262144";
	SetRndSeed(3604671458U); // yields 22695477
	ASSERT_EQ(GenerateRnd(maxBound), 346) << "Invalid distribution when generator yields 22695477";
	SetRndSeed(3604671460U); // yields -22695477
	ASSERT_EQ(GenerateRnd(maxBound), 346) << "Invalid distribution when generator yields -22695477";
	SetRndSeed(1012371854); // yields 429496729
	ASSERT_EQ(GenerateRnd(maxBound), 6553) << "Invalid distribution when generator yields 429496729";
	SetRndSeed(1902003768); // yields -429496729
	ASSERT_EQ(GenerateRnd(maxBound), 6553) << "Invalid distribution when generator yields -429496729";
	SetRndSeed(189776845); // yields 1212022642
	ASSERT_EQ(GenerateRnd(maxBound), 18493) << "Invalid distribution when generator yields 1212022642";
	SetRndSeed(2724598777); // yields -1212022642
	ASSERT_EQ(GenerateRnd(maxBound), 18493) << "Invalid distribution when generator yields -1212022642";
	SetRndSeed(76596137); // yields 2147483646
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 2147483646";
	SetRndSeed(2837779485); // yields -2147483646
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -2147483646";
	SetRndSeed(766891974); // yields 2147483647
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields 2147483647";
	SetRndSeed(2147483648); // yields -2147483647
	ASSERT_EQ(GenerateRnd(maxBound), 0) << "Invalid distribution when generator yields -2147483647";
}

TEST(RandomTest, ModDistributionSignPreserving)
{
	// This distribution is used when the upper bound is a value in [32768, 2147483647]

	// Sign preserving modulo when no division is performed cannot be tested in isolation with the current implementation.
	SetRndSeed(1457187811);
	ASSERT_EQ(GenerateRnd(32768), 0) << "Distribution must map negative numbers using sign preserving modulo";
	SetRndSeed(1457187811);
	ASSERT_EQ(GenerateRnd(32769), -2) << "Distribution must map negative numbers using sign preserving modulo";
	SetRndSeed(1457187811);
	ASSERT_EQ(GenerateRnd(65535), -32768) << "Distribution must map negative numbers using sign preserving modulo";
	SetRndSeed(1457187811);
	ASSERT_EQ(GenerateRnd(std::numeric_limits<int32_t>::max()), -1)
	    << "Distribution must map negative numbers using sign preserving modulo";
}

TEST(RandomTest, NegativeReturnValues)
{
	// The bug in vanilla RNG stemming from mod instead of bitmasking means that negative values are possible for
	// non-power of 2 arguments to GenerateRnd
	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(3), -2) << "Unexpected return value for a limit of 3";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(5), -3) << "Unexpected return value for a limit of 5";
	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(6), -2) << "Unexpected return value for a limit of 6";
	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(7), -1) << "Unexpected return value for a limit of 7";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(11), -10) << "Unexpected return value for a limit of 11";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(17), -9) << "Unexpected return value for a limit of 17";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(19), -12) << "Unexpected return value for a limit of 19";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(22), -10) << "Unexpected return value for a limit of 22";
	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(23), -16) << "Unexpected return value for a limit of 23";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(25), -18) << "Unexpected return value for a limit of 25";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(27), -17) << "Unexpected return value for a limit of 27";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(29), -27) << "Unexpected return value for a limit of 29";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(31), -1) << "Unexpected return value for a limit of 31";

	for (const int i : { 9, 10, 12, 13, 14, 15, 18, 20, 21, 24, 26, 28, 30 }) {
		SetRndSeed(1457187811);
		EXPECT_EQ(GenerateRnd(i), -8) << "Unexpected return value for a limit of " << i;
	}

	// The following values are/were used throughout the code
	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(37), -23) << "Unexpected return value for a limit of 37";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(38), -12) << "Unexpected return value for a limit of 38";

	SetRndSeed(1457187811);
	// commonly used for dungeon megatile coordinates
	EXPECT_EQ(GenerateRnd(40), -8) << "Unexpected return value for a limit of 40";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(41), -9) << "Unexpected return value for a limit of 41";

	SetRndSeed(1457187811);
	EXPECT_EQ(GenerateRnd(52), -8) << "Unexpected return value for a limit of 52";

	SetRndSeed(1457187811);
	// commonly used for dungeon tile coordinates
	EXPECT_EQ(GenerateRnd(80), -48) << "Unexpected return value for a limit of 80";

	SetRndSeed(1457187811);
	// commonly used for percentage rolls (typically in a context that 0 and -68 lead to the same outcome)
	EXPECT_EQ(GenerateRnd(100), -68) << "Unexpected return value for a limit of 100";

	for (int i = 1; i < 32768; i *= 2) {
		SetRndSeed(1457187811);
		EXPECT_EQ(GenerateRnd(i), 0) << "Expect powers of 2 such as " << i << " to cleanly divide the int_min RNG value ";
	}
}
} // namespace devilution
