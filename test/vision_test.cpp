#include <gtest/gtest.h>

#include "vision.hpp"

namespace devilution {

namespace {

const uint8_t ENV_WIDTH = 25;
const uint8_t ENV_HEIGHT = 25;

// Real environment
char env[ENV_WIDTH][ENV_HEIGHT];
// Visible environment
char vis[ENV_WIDTH][ENV_HEIGHT];
// Observer position in the center of the environment
const Point pos(ENV_WIDTH / 2, ENV_HEIGHT / 2);
// Walls (box) around the observer with the specified radius
const int box_radius = 4;
// Objects around the observer: point, visible-to-observer flag
const std::pair<Point, bool> objects[] = {
	{ { 15, 12 }, true },
	{ { 13, 15 }, true },
	{ { 10, 11 }, true },
	{ { 11, 13 }, true },
	{ { 9, 15 }, false }, // Invisible to the observer, because of the {11,13}
};

// Build walls around with diagonally adjacent corners
void buildWallsAround(char env[ENV_WIDTH][ENV_HEIGHT], Point p, int radius)
{
	for (int i = -radius + 1; i < radius; i++) {
		env[p.x + radius][p.y + i] = '#';
		env[p.x - radius][p.y + i] = '#';
		env[p.x - i][p.y + radius] = '#';
		env[p.x - i][p.y - radius] = '#';
	}
}

void initEnvironment()
{
	memset(env, ' ', sizeof(env));
	memset(vis, ' ', sizeof(vis));

	// Build walls around with diagonally adjacent corners
	buildWallsAround(env, pos, box_radius);

	// Place objects
	for (const auto &o : objects) {
		env[o.first.x][o.first.y] = '#';
	}

	// Place observer
	env[pos.x][pos.y] = 'x';
}

void doVision()
{
	initEnvironment();

	auto markVisibleFn = [](Point p) {
		if (env[p.x][p.y] == ' ')
			// Mark as hit by the ray
			vis[p.x][p.y] = '.';
		else
			// Copy visible object
			vis[p.x][p.y] = env[p.x][p.y];
	};
	auto markTransparentFn = [](Point p) {};
	auto passesLightFn = [](Point p) {
		return env[p.x][p.y] != '#';
	};
	auto inBoundsFn = [](Point p) { return true; };

	DoVision(pos, 15, markVisibleFn, markTransparentFn, passesLightFn, inBoundsFn);
}

[[maybe_unused]]
void dumpVisibleEnv()
{
	char buf[4096];
	int sz = 0;
	for (int i = 0; i < ENV_HEIGHT; i++) {
		for (int j = 0; j < ENV_WIDTH; j++) {
			sz += snprintf(buf + sz, sizeof(buf) - sz, "%c ", vis[i][j]);
		}
		sz += snprintf(buf + sz, sizeof(buf) - sz, "\n");
	}
#ifdef _WIN32
	_write(2, buf, sz);
#else
	write(2, buf, sz);
#endif
}

// This test case checks the visibility of surrounding objects
TEST(VisionTest, VisibleObjects)
{
	doVision();

	for (const auto &o : objects) {
		if (o.second)
			// Visible object
			EXPECT_EQ(vis[o.first.x][o.first.y], '#') << "Expext visible wall or object";
		else
			// Invisible object
			EXPECT_EQ(vis[o.first.x][o.first.y], ' ') << "Expect invisible tile";
	}
}

// This test case checks the visibility of objects in a straight line
// of sight parallel to the X or Y coordinate lines:
// https://github.com/diasurgical/DevilutionX/pull/7901
TEST(VisionTest, VisibilityInStraightLineOfSight)
{
	doVision();

	Displacement displacements[] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };

	for (auto &d : displacements) {
		Point p = pos;
		bool found = false;

		// Move along the XY coordinate lines until a visible object is hit
		while (p.x >= 0 && p.y >= 0 && p.x < ENV_WIDTH && p.y < ENV_HEIGHT) {
			p += d;

			if (vis[p.x][p.y] == '#') {
				found = true;
				break;
			}
		}
		EXPECT_TRUE(found) << "Expect visible wall or object in a straight line of sight";
	}
}

// This test case checks that nothing is visible through the
// diagonally adjacent tiles:
// https://github.com/diasurgical/DevilutionX/pull/7920
TEST(VisionTest, NoVisibilityThroughAdjacentTiles)
{
	char mask[ENV_WIDTH][ENV_HEIGHT];

	doVision();

	memset(mask, ' ', sizeof(mask));
	buildWallsAround(mask, pos, box_radius);

	enum State {
		BehindWall = 0,
		HitWall,
		OnWall,
		InsideBox,
	} state
	    = BehindWall;

	// Goes over each tile and compares the mask with the visible
	// environment that is behind the wall
	for (int i = 0; i < ENV_HEIGHT; i++) {
		EXPECT_EQ(state, BehindWall);
		for (int j = 0; j < ENV_WIDTH; j++) {
			if (state == BehindWall) {
				// Mask and environment are compared strictly behind
				// the wall
				EXPECT_EQ(mask[i][j], vis[i][j]) << "Expect no \"leaked\" light through adjacent tiles";
			}

			if (mask[i][j] == '#') {
				if (state == BehindWall)
					state = HitWall;
				else if (state == HitWall)
					state = OnWall;
				else if (state == InsideBox)
					state = BehindWall;
			} else {
				if (state == HitWall)
					state = InsideBox;
				else if (state == OnWall)
					state = BehindWall;
			}
		}
	}
}

} // namespace
} // namespace devilution
