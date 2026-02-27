#include <array>
#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

#include "multi.h"

namespace devilution {

TEST(ComputeModListHash, EmptyListProducesZero)
{
	// An empty mod list produces zero (XOR identity with no contributors).
	EXPECT_EQ(ComputeModListHash({}), 0U);
}

TEST(ComputeModListHash, Deterministic)
{
	const std::array<std::string_view, 2> mods = { "mod-a", "mod-b" };
	EXPECT_EQ(ComputeModListHash(mods), ComputeModListHash(mods));
}

TEST(ComputeModListHash, DifferentModsProduceDifferentHashes)
{
	const std::array<std::string_view, 1> modsA = { "mod-a" };
	const std::array<std::string_view, 1> modsB = { "mod-b" };
	EXPECT_NE(ComputeModListHash(modsA), ComputeModListHash(modsB));
}

TEST(ComputeModListHash, OrderDoesNotMatter)
{
	const std::array<std::string_view, 2> ab = { "mod-a", "mod-b" };
	const std::array<std::string_view, 2> ba = { "mod-b", "mod-a" };
	EXPECT_EQ(ComputeModListHash(ab), ComputeModListHash(ba));
}

TEST(ComputeModListHash, DifferentModNamesDifferentHashes)
{
	// ["ab", "c"] must not collide with ["a", "bc"].
	const std::array<std::string_view, 2> splitFirst = { "ab", "c" };
	const std::array<std::string_view, 2> splitSecond = { "a", "bc" };
	EXPECT_NE(ComputeModListHash(splitFirst), ComputeModListHash(splitSecond));
}

TEST(ComputeModListHash, NoModsDifferFromSomeMods)
{
	const std::array<std::string_view, 1> oneMod = { "any-mod" };
	EXPECT_NE(ComputeModListHash({}), ComputeModListHash(oneMod));
}

TEST(MultiplayerLogging, NormalExitReason)
{
	EXPECT_EQ("normal exit", DescribeLeaveReason(net::leaveinfo_t::LEAVE_EXIT));
}

TEST(MultiplayerLogging, DiabloEndingReason)
{
	EXPECT_EQ("Diablo defeated", DescribeLeaveReason(net::leaveinfo_t::LEAVE_ENDING));
}

TEST(MultiplayerLogging, DropReason)
{
	EXPECT_EQ("connection timeout", DescribeLeaveReason(net::leaveinfo_t::LEAVE_DROP));
}

TEST(MultiplayerLogging, CustomReasonCode)
{
	constexpr net::leaveinfo_t CustomCode = static_cast<net::leaveinfo_t>(0xDEADBEEF);
	EXPECT_EQ("code 0xDEADBEEF", DescribeLeaveReason(CustomCode));
}

} // namespace devilution
