#include <gtest/gtest.h>

#include "multi.h"

namespace devilution {

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
