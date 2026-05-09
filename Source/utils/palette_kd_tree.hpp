#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#elif defined(USE_SDL1)
#include <SDL_video.h>
#else
#include <SDL_pixels.h>
#endif

#include "utils/str_cat.hpp"

#define DEVILUTIONX_PRINT_PALETTE_BLENDING_TREE_GRAPHVIZ 0 // NOLINT(modernize-macro-to-enum)

namespace devilution {

/**
 * @brief Depth (number of levels) of the tree.
 */
constexpr size_t PaletteKdTreeDepth = 5;

/**
 * @brief A node in the k-d tree.
 *
 * @tparam RemainingDepth distance to the leaf nodes.
 */
template <size_t RemainingDepth>
struct PaletteKdTreeNode {
	using RGB = std::array<uint8_t, 3>;

	static constexpr unsigned Coord = (PaletteKdTreeDepth - RemainingDepth) % 3;

	PaletteKdTreeNode<RemainingDepth - 1> left;
	PaletteKdTreeNode<RemainingDepth - 1> right;
	uint8_t pivot;

	[[nodiscard]] const PaletteKdTreeNode<RemainingDepth - 1> &child(bool isLeft) const
	{
		return isLeft ? left : right;
	}
	[[nodiscard]] PaletteKdTreeNode<RemainingDepth - 1> &child(bool isLeft)
	{
		return isLeft ? left : right;
	}

	[[maybe_unused]] void toGraphvizDot(size_t id, std::span<const std::pair<RGB, uint8_t>, 256> values, std::string &dot) const
	{
		StrAppend(dot, "  node_", id, " [label=\"", "rgb"[Coord], ": ", pivot, "\"]\n");
		const size_t leftId = (2 * id) + 1;
		const size_t rightId = (2 * id) + 2;
		left.toGraphvizDot(leftId, values, dot);
		right.toGraphvizDot(rightId, values, dot);
		StrAppend(dot, "  node_", id, " -- node_", leftId,
		    "\n  node_", id, " -- node_", rightId, "\n");
	}
};

/**
 * @brief A leaf node in the k-d tree.
 */
template <>
struct PaletteKdTreeNode</*RemainingDepth=*/0> {
	using RGB = std::array<uint8_t, 3>;

	// We use inclusive indices to allow for representing the full [0, 255] range.
	// An empty node is represented as [1, 0].
	uint8_t valuesBegin;
	uint8_t valuesEndInclusive;

	void toGraphvizDot(size_t id, std::span<const std::pair<RGB, uint8_t>, 256> values, std::string &dot) const;
};

/**
 * @brief A kd-tree used to find the nearest neighbor in the color space.
 *
 * Each level splits the space in half by red, green, and blue respectively.
 */
class PaletteKdTree {
private:
	using RGB = std::array<uint8_t, 3>;
	static constexpr unsigned NumLeaves = 1U << PaletteKdTreeDepth;

public:
	PaletteKdTree() = default;

	/**
	 * @brief Constructs a PaletteKdTree
	 *
	 * The palette is used as points in the tree.
	 * Colors between skipFrom and skipTo (inclusive) are skipped.
	 */
	PaletteKdTree(const SDL_Color palette[256], int skipFrom, int skipTo);

	struct VisitState {
		uint8_t best;
		uint32_t bestDiff;
	};

	[[nodiscard]] uint8_t findNearestNeighbor(const RGB &rgb) const
	{
		VisitState visitState;
		visitState.bestDiff = std::numeric_limits<uint32_t>::max();
		findNearestNeighborVisit(tree_, rgb, visitState);
		return visitState.best;
	}

	[[maybe_unused]] [[nodiscard]] std::string toGraphvizDot() const;

private:
	[[nodiscard]] static constexpr uint32_t getColorDistance(const std::array<uint8_t, 3> &a, const std::array<uint8_t, 3> &b)
	{
		const int diffr = a[0] - b[0];
		const int diffg = a[1] - b[1];
		const int diffb = a[2] - b[2];
		return (diffr * diffr) + (diffg * diffg) + (diffb * diffb);
	}

	[[nodiscard]] static constexpr uint32_t getColorDistanceToPlane(int x1, int x2)
	{
		// Our planes are axis-aligned, so a distance from a point to a plane
		// can be calculated based on just the axis coordinate.
		const int delta = x1 - x2;
		return static_cast<uint32_t>(delta * delta);
	}

	template <size_t RemainingDepth>
	void findNearestNeighborVisit(const PaletteKdTreeNode<RemainingDepth> &node, const RGB &rgb,
	    VisitState &visitState) const
	{
		const uint8_t coord = rgb[PaletteKdTreeNode<RemainingDepth>::Coord];
		findNearestNeighborVisit(node.child(coord < node.pivot), rgb, visitState);

		// To see if we need to check a node's subtree, we compare the distance from the query
		// to the current best candidate vs the distance to the edge of the half-space represented
		// by the node.
		if (getColorDistanceToPlane(node.pivot, coord) < visitState.bestDiff) {
			findNearestNeighborVisit(node.child(coord >= node.pivot), rgb, visitState);
		}
	}

	void findNearestNeighborVisit(const PaletteKdTreeNode<0> &node, const RGB &rgb,
	    VisitState &visitState) const
	{
		// Nodes are almost never empty.
		// Separating the empty check from the loop makes this faster,
		// probaly because of better branch prediction.
		if (node.valuesBegin > node.valuesEndInclusive) return;

		const std::pair<RGB, uint8_t> *it = values_.data() + node.valuesBegin;
		const std::pair<RGB, uint8_t> *const end = values_.data() + node.valuesEndInclusive;
		do {
			const auto &[paletteColor, paletteIndex] = *it++;
			const uint32_t diff = getColorDistance(paletteColor, rgb);
			if (diff < visitState.bestDiff) {
				visitState.best = paletteIndex;
				visitState.bestDiff = diff;
			}
		} while (it <= end);
	}

	PaletteKdTreeNode<PaletteKdTreeDepth> tree_;
	std::array<std::pair<RGB, uint8_t>, 256> values_;
};

} // namespace devilution
