#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include "engine/points_in_rectangle_range.hpp"
#include "engine/world_tile.hpp"
#include "levels/gendung.h"

namespace devilution {

TEST(DrlgTest, RectangleRangeIterator)
{
	constexpr WorldTileRectangle topLeftArea { { 1, 1 }, 1 };
	constexpr WorldTileRectangle bottomRightArea { { 3, 3 }, 1 };
	// Dungeon generation depends on the iteration order remaining unchanged to ensure we generate the same layout as vanilla Diablo/Hellfire
	std::array<std::array<int, 5>, 5> region {};

	int counter = 0;
	// Iterate over a 9 tile area in the top left of the region.
	for (const WorldTilePosition position : PointsInRectangle(topLeftArea)) {
		region[position.x][position.y] = ++counter;
	}

	EXPECT_EQ(counter, 9) << "Iterating over a 9 tile range should return exactly 9 points";
	EXPECT_EQ(region[2][2], 9) << "Iterating over a 9 tile range should return exactly 9 points";

	EXPECT_EQ(region[0][0], 1) << "Default order should be row-major (where x defines the column, y defines the row)";
	EXPECT_EQ(region[1][0], 2) << "Default order should be row-major (where x defines the column, y defines the row)";
	EXPECT_EQ(region[2][0], 3) << "Default order should be row-major (where x defines the column, y defines the row)";
	EXPECT_EQ(region[0][2], 7) << "Default order should be row-major (where x defines the column, y defines the row)";

	EXPECT_EQ(region[0][3], 0) << "Iterator should not return out of bounds points";
	EXPECT_EQ(region[3][0], 0) << "Iterator should not return out of bounds points";

	region = {};
	counter = 0;

	const auto rowMajorRange = PointsInRectangle(bottomRightArea);
	std::for_each(rowMajorRange.rbegin(), rowMajorRange.rend(), [&region, &counter](Point position) { region[position.x][position.y] = ++counter; });
	EXPECT_EQ(region[4][4], 1) << "Reverse iterators are required";
	EXPECT_EQ(region[2][4], 3) << "Reverse iterators are required";
	EXPECT_EQ(region[4][2], 7) << "Reverse iterators are required";
	EXPECT_EQ(region[2][2], 9) << "Reverse iterators are required";

	region = {};
	counter = 0;

	for (const WorldTilePosition position : PointsInRectangleColMajor(topLeftArea)) {
		region[position.x][position.y] = ++counter;
	}

	EXPECT_EQ(counter, 9) << "Iterating over a 9 tile range should return exactly 9 points";
	EXPECT_EQ(region[2][2], 9) << "Iterating over a 9 tile range should return exactly 9 points";

	EXPECT_EQ(region[0][0], 1) << "col-major iterator must use col-major order (where x defines the column, y defines the row)";
	EXPECT_EQ(region[0][1], 2) << "col-major iterator must use col-major order (where x defines the column, y defines the row)";
	EXPECT_EQ(region[0][2], 3) << "col-major iterator must use col-major order (where x defines the column, y defines the row)";
	EXPECT_EQ(region[2][0], 7) << "col-major iterator must use col-major order (where x defines the column, y defines the row)";

	EXPECT_EQ(region[0][3], 0) << "Iterator should not return out of bounds points";
	EXPECT_EQ(region[3][0], 0) << "Iterator should not return out of bounds points";

	region = {};
	counter = 0;

	const auto colMajorRange = PointsInRectangleColMajor(bottomRightArea);
	std::for_each(colMajorRange.rbegin(), colMajorRange.rend(), [&region, &counter](Point position) { region[position.x][position.y] = ++counter; });
	EXPECT_EQ(region[4][4], 1) << "Reverse iterators are required";
	EXPECT_EQ(region[4][2], 3) << "Reverse iterators are required";
	EXPECT_EQ(region[2][4], 7) << "Reverse iterators are required";
	EXPECT_EQ(region[2][2], 9) << "Reverse iterators are required";
}

TEST(DrlgTest, ThemeRoomSize)
{
	memset(dungeon, 0, sizeof(dungeon));

	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(8, 8)) << "All floor theme area should be 8x8";

	dungeon[9][9] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(7, 7)) << "Corners shrink the chosen dimensions";
	dungeon[9][5] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(7, 3)) << "Minimum dimensions are determined by corners outside the min area";
	dungeon[9][4] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(7, 8)) << "Walls below the min size let larger opposing dimensions get picked";
	dungeon[9][5] = 0;
	dungeon[9][4] = 0;
	dungeon[9][9] = 0;

	// Time for some unusual cases
	dungeon[7][2] = 1;
	dungeon[5][9] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(5, 7)) << "Search space terminates at width 8 due to the wall being in the first three rows";

	dungeon[6][4] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(4, 7)) << "Smallest width now defined by row 5, height still extends due to minSize";
	dungeon[6][4] = 0;

	dungeon[5][9] = 0;
	dungeon[7][2] = 0;

	dungeon[7][0] = 1;
	dungeon[6][6] = 1;
	dungeon[8][5] = 1;
	EXPECT_EQ(GetSizeForThemeRoom(), WorldTileSize(4, 4)) << "Search is terminated by the 0 width row 7, inset corner gives a larger height than otherwise expected";
}

} // namespace devilution
