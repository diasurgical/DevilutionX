#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include <SDL.h>

#include "utils/algorithm/container.hpp"
#include "utils/static_vector.hpp"

namespace devilution {

[[nodiscard]] inline uint32_t GetColorDistance(const SDL_Color &a, const std::array<uint8_t, 3> &b)
{
	const int diffr = a.r - b[0];
	const int diffg = a.g - b[1];
	const int diffb = a.b - b[2];
	return (diffr * diffr) + (diffg * diffg) + (diffb * diffb);
}

/**
 * @brief A 3-level kd-tree used to find the nearest neighbor in the color space.
 *
 * Each level splits the space in half by red, green, and blue respectively.
 */
class PaletteKdTree {
	using RGB = std::array<uint8_t, 3>;

public:
	explicit PaletteKdTree(const SDL_Color palette[256])
	    : palette_(palette)
	    , pivots_(getPivots(palette))
	{
		for (unsigned i = 0; i < 256; ++i) {
			const SDL_Color &color = palette[i];
			auto &level1 = color.r < pivots_[0] ? tree_.first : tree_.second;
			auto &level2 = color.g < pivots_[1] ? level1.first : level1.second;
			auto &level3 = color.b < pivots_[2] ? level2.first : level2.second;
			level3.emplace_back(i);
		}

		// Uncomment the loop below to print the node distribution:
		// for (const bool r : { false, true }) {
		// 	for (const bool g : { false, true }) {
		// 		for (const bool b : { false, true }) {
		// 			printf("r%d.g%d.b%d: %d\n",
		// 			    static_cast<int>(r), static_cast<int>(g), static_cast<int>(b),
		// 			    static_cast<int>(getLeaf(r, g, b).size()));
		// 		}
		// 	}
		// }
	}

	[[nodiscard]] uint8_t findNearestNeighbor(const RGB &rgb) const
	{
		const bool compR = rgb[0] < pivots_[0];
		const bool compG = rgb[1] < pivots_[1];
		const bool compB = rgb[2] < pivots_[2];

		// Conceptually, we visit the tree recursively.
		// As the tree only has 3 levels, we fully unroll
		// the recursion here.
		uint8_t best;
		uint32_t bestDiff = std::numeric_limits<uint32_t>::max();
		checkLeaf(compR, compG, compB, rgb, best, bestDiff);
		if (shouldCheckNode(best, bestDiff, /*coord=*/2, rgb)) {
			checkLeaf(compR, compG, !compB, rgb, best, bestDiff);
		}
		if (shouldCheckNode(best, bestDiff, /*coord=*/1, rgb)) {
			checkLeaf(compR, !compG, compB, rgb, best, bestDiff);
			if (shouldCheckNode(best, bestDiff, /*coord=*/2, rgb)) {
				checkLeaf(compR, !compG, !compB, rgb, best, bestDiff);
			}
		}
		if (shouldCheckNode(best, bestDiff, /*coord=*/0, rgb)) {
			checkLeaf(!compR, compG, compB, rgb, best, bestDiff);
			if (shouldCheckNode(best, bestDiff, /*coord=*/1, rgb)) {
				checkLeaf(!compR, !compG, compB, rgb, best, bestDiff);
				if (shouldCheckNode(best, bestDiff, /*coord=*/2, rgb)) {
					checkLeaf(!compR, !compG, !compB, rgb, best, bestDiff);
				}
			}
			if (shouldCheckNode(best, bestDiff, /*coord=*/2, rgb)) {
				checkLeaf(!compR, compG, !compB, rgb, best, bestDiff);
			}
		}
		return best;
	}

private:
	static uint8_t getMedian(std::span<uint8_t, 256> elements)
	{
		const auto middleItr = elements.begin() + (elements.size() / 2);
		std::nth_element(elements.begin(), middleItr, elements.end());
		if (elements.size() % 2 == 0) {
			const auto leftMiddleItr = std::max_element(elements.begin(), middleItr);
			return (*leftMiddleItr + *middleItr) / 2;
		}
		return *middleItr;
	}

	static std::array<uint8_t, 3> getPivots(const SDL_Color palette[256])
	{
		std::array<std::array<uint8_t, 256>, 3> coords;
		for (unsigned i = 0; i < 256; ++i) {
			coords[0][i] = palette[i].r;
			coords[1][i] = palette[i].g;
			coords[2][i] = palette[i].b;
		}
		return { getMedian(coords[0]), getMedian(coords[1]), getMedian(coords[2]) };
	}

	void checkLeaf(bool compR, bool compG, bool compB, const RGB &rgb, uint8_t &best, uint32_t &bestDiff) const
	{
		const std::span<const uint8_t> leaf = getLeaf(compR, compG, compB);
		uint8_t leafBest;
		uint32_t leafBestDiff = bestDiff;
		for (const uint8_t paletteIndex : leaf) {
			const uint32_t diff = GetColorDistance(palette_[paletteIndex], rgb);
			if (diff < leafBestDiff) {
				leafBest = paletteIndex;
				leafBestDiff = diff;
			}
		}
		if (leafBestDiff < bestDiff) {
			best = leafBest;
			bestDiff = leafBestDiff;
		}
	}

	[[nodiscard]] bool shouldCheckNode(uint8_t best, uint32_t bestDiff, unsigned coord, const RGB &rgb) const
	{
		// To see if we need to check a node's subtree, we compare the distance from the query
		// to the current best candidate vs the distance to the edge of the half-space represented
		// by the node.
		if (bestDiff == std::numeric_limits<uint32_t>::max()) return true;
		const int delta = static_cast<int>(pivots_[coord]) - static_cast<int>(rgb[coord]);
		return delta * delta < GetColorDistance(palette_[best], rgb);
	}

	[[nodiscard]] std::span<const uint8_t> getLeaf(bool r, bool g, bool b) const
	{
		const auto &level1 = r ? tree_.first : tree_.second;
		const auto &level2 = g ? level1.first : level1.second;
		const auto &level3 = b ? level2.first : level2.second;
		return { level3 };
	}

	const SDL_Color *palette_;
	std::array<uint8_t, 3> pivots_;
	std::pair<
	    // r0
	    std::pair<
	        // r0.g0.b{0, 1}
	        std::pair<StaticVector<uint8_t, 256>, StaticVector<uint8_t, 256>>,
	        // r0.g1.b{0, 1}
	        std::pair<StaticVector<uint8_t, 256>, StaticVector<uint8_t, 256>>>,
	    // r1
	    std::pair<
	        // r1.g0.b{0, 1}
	        std::pair<StaticVector<uint8_t, 256>, StaticVector<uint8_t, 256>>,
	        // r1.g1.b{0, 1}
	        std::pair<StaticVector<uint8_t, 256>, StaticVector<uint8_t, 256>>>>
	    tree_;
};

} // namespace devilution
