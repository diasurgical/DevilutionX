#include <gtest/gtest.h>

#include "engine/displacement.hpp"

namespace devilution {

TEST(MathTest, WorldScreenTransformation)
{
	Displacement offset { 5, 2 };
	// Diablo renders tiles with the world origin translated to the top left of the screen, while the normal convention
	// has the screen origin at the bottom left. This means that we end up with negative offsets in screen space for
	// tiles in world space where x > y
	EXPECT_EQ(offset.worldToScreen(), Displacement(-96, -112));

	// Transformation should be reversable (as long as it's not truncating)
	EXPECT_EQ(offset.worldToScreen().screenToWorld(), offset);

	// Tiles with y >= x will still have a negative y coordinate in screen space
	offset = { 2, 5 };
	EXPECT_EQ(offset.worldToScreen(), Displacement(96, -112));

	// Most screen to world transformations will have a further displacement applied, this is a simple case of
	// selecting a tile on the edge of the world with the default origin
	const Displacement cursorPosition = { 342, -150 };
	EXPECT_EQ(cursorPosition.screenToWorld(), Displacement(0, 10));

	// Screen > World transforms lose information, so cannot be reversed exactly using ints
	EXPECT_EQ(cursorPosition.screenToWorld().worldToScreen(), Displacement(320, -160));
}

TEST(MathTest, NormalizeDisplacement)
{
	// Normalizing displacements transforms the value into 16 bit fixed point representations
	Displacement vector { 5, 0 };
	EXPECT_FLOAT_EQ(vector.magnitude(), 5);
	EXPECT_EQ(vector.normalized(), Displacement(1 << 16, 0)); // (1.0, 0.0)

	vector = { 3, 4 };
	EXPECT_FLOAT_EQ(vector.magnitude(), 5);
	EXPECT_EQ(vector.normalized(), Displacement(39321, 52428)); // ~(0.6, 0.8)

	vector = { -5, 2 };
	EXPECT_FLOAT_EQ(vector.magnitude(), 5.3851647f);
	EXPECT_EQ(vector.normalized(), Displacement(-60848, 24339)); // ~(-0.92, 0.37)
}

TEST(MathTest, MissileTransformation)
{
	// starting with a Displacement 2 world units West results in a vector pointing left of screen
	EXPECT_EQ(Displacement(2, -2).worldToNormalScreen(), Displacement(-65536, 0));

	// if it's not normalizing the vector then it's a problem :D
	EXPECT_EQ(Displacement(4, -4).worldToNormalScreen(), Displacement(-65536, 0));

	// Because of the isometric projection the y axis gets squashed
	EXPECT_EQ(Displacement(8, 1).worldToNormalScreen(), Displacement(-40235, -25865)); // ~(0.6, 0.8/2)

	// in elevation projection this would be a vector with x == y, isometric projection means y == x/2
	EXPECT_EQ(Displacement(8, 0).worldToNormalScreen(), Displacement(-46340, -23170)); // ~(0.7, 0.7/2)
}

} // namespace devilution
