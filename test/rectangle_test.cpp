#include <gtest/gtest.h>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"

namespace devilution {
namespace {

TEST(RectangleTest, Contains_LargerSize)
{
	const RectangleOf<uint8_t> rect { { 0, 0 }, { 10, 20 } };
	EXPECT_TRUE(rect.contains(Point(9, 9)));
	EXPECT_FALSE(rect.contains(Point(-1, -1)));
	EXPECT_FALSE(rect.contains(Point(257, 257)));
}

TEST(RectangleTest, Contains_UnsignedRectangle_SignedPointSameSize)
{
	const RectangleOf<uint8_t> rect { { 0, 0 }, { 255, 255 } };
	EXPECT_TRUE(rect.contains(PointOf<int8_t>(5, 5)));
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(-1, -1)));
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(-2, -2)));
}

TEST(RectangleTest, Contains_SignedRectangle_UnsignedPointSameSize)
{
	const RectangleOf<int8_t> rect { { -10, -10 }, { 127, 127 } };
	EXPECT_TRUE(rect.contains(PointOf<uint8_t>(0, 0)));
	EXPECT_FALSE(rect.contains(PointOf<uint8_t>(255, 255)));
}

} // namespace
} // namespace devilution
