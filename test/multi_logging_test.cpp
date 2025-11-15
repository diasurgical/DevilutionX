#include <gtest/gtest.h>

#include "multi.h"
#include "storm/storm_net.hpp"

namespace devilution {

TEST(MultiplayerLogging, RequestedExitReason)
{
	EXPECT_EQ("requested exit", DescribeLeaveReason(0));
}

TEST(MultiplayerLogging, DiabloEndingReason)
{
	EXPECT_EQ("Diablo defeated", DescribeLeaveReason(LEAVE_ENDING));
}

TEST(MultiplayerLogging, DropReason)
{
	EXPECT_EQ("connection timeout", DescribeLeaveReason(LEAVE_DROP));
}

TEST(MultiplayerLogging, CustomReasonCode)
{
	constexpr uint32_t CustomCode = 0xDEADBEEF;
	EXPECT_EQ("code 0xDEADBEEF", DescribeLeaveReason(CustomCode));
}

} // namespace devilution
