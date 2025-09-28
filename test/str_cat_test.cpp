#include <cstdint>

#include <gtest/gtest.h>

#include "utils/str_cat.hpp"

namespace devilution {
namespace {

TEST(StrCatTest, StrCatBasicTest)
{
	EXPECT_EQ(StrCat("a", "b", "c", 5), "abc5");
}

TEST(StrCatTest, LeftPadTest)
{
	EXPECT_EQ(StrCat(LeftPad(static_cast<uint8_t>(5), 2, '0')), "05");
}

TEST(StrCatTest, BufCopyBasicTest)
{
	char buf[5];
	char *end = BufCopy(buf, "a", "b", "c", 5);
	EXPECT_EQ(std::string_view(buf, end - buf), "abc5");
}

} // namespace
} // namespace devilution
