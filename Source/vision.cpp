#include "vision.hpp"

#include <cstdint>
#include <iterator>

#include <function_ref.hpp>

#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {
namespace {
/*
 * XY points of vision rays are cast to trace the visibility of the
 * surrounding environment. The table represents N rays of M points in
 * one quadrant (0°-90°) of a circle, so rays for other quadrants will
 * be created by mirroring. Zero points at the end will be trimmed and
 * ignored. A similar table can be recreated using Bresenham's line
 * drawing algorithm, which is suitable for integer arithmetic:
 * https://en.wikipedia.org/wiki/Bresenham's_line_algorithm
 */
const DisplacementOf<int8_t> VisionRays[23][15] = {
	// clang-format off
	{ { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 6, 0 }, { 7, 0 }, { 8, 0 }, { 9, 0 }, { 10,  0 }, { 11,  0 }, { 12,  0 }, { 13,  0 }, { 14,  0 }, { 15,  0 } },
	{ { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 6, 0 }, { 7, 0 }, { 8, 1 }, { 9, 1 }, { 10,  1 }, { 11,  1 }, { 12,  1 }, { 13,  1 }, { 14,  1 }, { 15,  1 } },
	{ { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 7, 1 }, { 8, 1 }, { 9, 1 }, { 10,  1 }, { 11,  1 }, { 12,  2 }, { 13,  2 }, { 14,  2 }, { 15,  2 } },
	{ { 1, 0 }, { 2, 0 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 7, 1 }, { 8, 2 }, { 9, 2 }, { 10,  2 }, { 11,  2 }, { 12,  2 }, { 13,  3 }, { 14,  3 }, { 15,  3 } },
	{ { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 2 }, { 7, 2 }, { 8, 2 }, { 9, 3 }, { 10,  3 }, { 11,  3 }, { 12,  3 }, { 13,  4 }, { 14,  4 }, {  0,  0 } },
	{ { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 2 }, { 6, 2 }, { 7, 3 }, { 8, 3 }, { 9, 3 }, { 10,  4 }, { 11,  4 }, { 12,  4 }, { 13,  5 }, { 14,  5 }, {  0,  0 } },
	{ { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 }, { 6, 3 }, { 7, 3 }, { 8, 3 }, { 9, 4 }, { 10,  4 }, { 11,  5 }, { 12,  5 }, { 13,  6 }, { 14,  6 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 1 }, { 3, 2 }, { 4, 2 }, { 5, 3 }, { 6, 3 }, { 7, 4 }, { 8, 4 }, { 9, 5 }, { 10,  5 }, { 11,  6 }, { 12,  6 }, { 13,  7 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 1 }, { 3, 2 }, { 4, 2 }, { 5, 3 }, { 6, 4 }, { 7, 4 }, { 8, 5 }, { 9, 6 }, { 10,  6 }, { 11,  7 }, { 12,  7 }, { 12,  8 }, { 13,  8 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 2 }, { 3, 2 }, { 4, 3 }, { 5, 4 }, { 6, 5 }, { 7, 5 }, { 8, 6 }, { 9, 7 }, { 10,  7 }, { 10,  8 }, { 11,  8 }, { 12,  9 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 5 }, { 7, 6 }, { 8, 7 }, { 9, 8 }, { 10,  9 }, { 11,  9 }, { 11, 10 }, {  0,  0 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 6 }, { 7, 7 }, { 8, 8 }, { 9, 9 }, { 10, 10 }, { 11, 11 }, {  0,  0 }, {  0,  0 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 }, { 8, 9 }, {  9, 10 }, {  9, 11 }, { 10, 11 }, {  0,  0 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 2, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 5, 7 }, { 6, 8 }, { 7, 9 }, {  7, 10 }, {  8, 10 }, {  8, 11 }, {  9, 12 }, {  0,  0 }, {  0,  0 } },
	{ { 1, 1 }, { 1, 2 }, { 2, 3 }, { 2, 4 }, { 3, 5 }, { 4, 6 }, { 4, 7 }, { 5, 8 }, { 6, 9 }, {  6, 10 }, {  7, 11 }, {  7, 12 }, {  8, 12 }, {  8, 13 }, {  0,  0 } },
	{ { 1, 1 }, { 1, 2 }, { 2, 3 }, { 2, 4 }, { 3, 5 }, { 3, 6 }, { 4, 7 }, { 4, 8 }, { 5, 9 }, {  5, 10 }, {  6, 11 }, {  6, 12 }, {  7, 13 }, {  0,  0 }, {  0,  0 } },
	{ { 0, 1 }, { 1, 2 }, { 1, 3 }, { 2, 4 }, { 2, 5 }, { 3, 6 }, { 3, 7 }, { 3, 8 }, { 4, 9 }, {  4, 10 }, {  5, 11 }, {  5, 12 }, {  6, 13 }, {  6, 14 }, {  0,  0 } },
	{ { 0, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 2, 5 }, { 2, 6 }, { 3, 7 }, { 3, 8 }, { 3, 9 }, {  4, 10 }, {  4, 11 }, {  4, 12 }, {  5, 13 }, {  5, 14 }, {  0,  0 } },
	{ { 0, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 2, 6 }, { 2, 7 }, { 2, 8 }, { 3, 9 }, {  3, 10 }, {  3, 11 }, {  3, 12 }, {  4, 13 }, {  4, 14 }, {  0,  0 } },
	{ { 0, 1 }, { 0, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 7 }, { 2, 8 }, { 2, 9 }, {  2, 10 }, {  2, 11 }, {  2, 12 }, {  3, 13 }, {  3, 14 }, {  3, 15 } },
	{ { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 7 }, { 1, 8 }, { 1, 9 }, {  1, 10 }, {  1, 11 }, {  2, 12 }, {  2, 13 }, {  2, 14 }, {  2, 15 } },
	{ { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 }, { 1, 8 }, { 1, 9 }, {  1, 10 }, {  1, 11 }, {  1, 12 }, {  1, 13 }, {  1, 14 }, {  1, 15 } },
	{ { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 7 }, { 0, 8 }, { 0, 9 }, {  0, 10 }, {  0, 11 }, {  0, 12 }, {  0, 13 }, {  0, 14 }, {  0, 15 } },
	// clang-format on
};
} // namespace

void DoVision(Point position, uint8_t radius,
    tl::function_ref<void(Point)> markVisibleFn,
    tl::function_ref<void(Point)> markTransparentFn,
    tl::function_ref<bool(Point)> passesLightFn,
    tl::function_ref<bool(Point)> inBoundsFn)
{
	markVisibleFn(position);

	// Adjustment to a ray length to ensure all rays lie on an
	// accurate circle
	constexpr uint8_t RayLenAdj[23] = { 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 4, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0 };
	static_assert(std::size(RayLenAdj) == std::size(VisionRays));

	// Four quadrants on a circle
	constexpr Displacement Quadrants[] = { { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };

	// Loop over quadrants and mirror rays for each one
	for (const auto &quadrant : Quadrants) {
		// Cast a ray for a quadrant
		for (unsigned int j = 0; j < std::size(VisionRays); j++) {
			const int rayLen = radius - RayLenAdj[j];
			for (int k = 0; k < rayLen; k++) {
				const auto &relRayPoint = VisionRays[j][k];
				// Calculate the next point on a ray in the quadrant
				const Point rayPoint = position + relRayPoint * quadrant;
				if (!inBoundsFn(rayPoint)) break;

				// We've cast an approximated ray on an integer 2D
				// grid, so we need to check if a ray can pass through
				// the diagonally adjacent tiles. For example, consider
				// this case:
				//
				//        #?
				//       ↗ #
				//     x
				//
				// The ray is cast from the observer 'x', and reaches
				// the '?', but diagonally adjacent tiles '#' do not
				// pass the light, so the '?' should not be visible
				// for the 2D observer.
				//
				// The trick is to perform two additional visibility
				// checks for the diagonally adjacent tiles, but only
				// for the rays that are not parallel to the X or Y
				// coordinate lines. Parallel rays, which have a 0 in
				// one of their coordinate components, do not require
				// any additional adjacent visibility checks, and the
				// tile, hit by the ray, is always considered visible.
				//
				if (relRayPoint.deltaX > 0 && relRayPoint.deltaY > 0) {
					const Displacement adjacent1 = { -quadrant.deltaX, 0 };
					const Displacement adjacent2 = { 0, -quadrant.deltaY };

					// If diagonally adjacent tiles do not pass the
					// light further, we are done with this ray.
					const bool passesLight = (passesLightFn(rayPoint + adjacent1) || passesLightFn(rayPoint + adjacent2));
					if (!passesLight) break;
				}
				markVisibleFn(rayPoint);

				// If the tile does not pass the light further, we are
				// done with this ray.
				const bool passesLight = passesLightFn(rayPoint);
				if (!passesLight) break;

				markTransparentFn(rayPoint);
			}
		}
	}
}

} // namespace devilution
