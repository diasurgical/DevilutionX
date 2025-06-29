#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include <SDL.h>

#include "utils/static_vector.hpp"
#include "utils/str_cat.hpp"

#define DEVILUTIONX_PRINT_PALETTE_BLENDING_TREE_GRAPHVIZ 0

#if DEVILUTIONX_PRINT_PALETTE_BLENDING_TREE_GRAPHVIZ
#include <cstdio>
#endif

namespace devilution {

[[nodiscard]] inline uint32_t GetColorDistance(const SDL_Color &a, const std::array<uint8_t, 3> &b)
{
	const int diffr = a.r - b[0];
	const int diffg = a.g - b[1];
	const int diffb = a.b - b[2];
	return (diffr * diffr) + (diffg * diffg) + (diffb * diffb);
}

[[nodiscard]] inline uint32_t GetColorDistanceToPlane(int x1, int x2)
{
	// Our planes are axis-aligned, so a distance from a point to a plane
	// can be calculated based on just the axis coordinate.
	const int delta = x1 - x2;
	return static_cast<uint32_t>(delta * delta);
}

template <size_t N>
uint8_t GetColorComponent(const SDL_Color &);
template <>
inline uint8_t GetColorComponent<0>(const SDL_Color &c) { return c.r; }
template <>
inline uint8_t GetColorComponent<1>(const SDL_Color &c) { return c.g; }
template <>
inline uint8_t GetColorComponent<2>(const SDL_Color &c) { return c.b; }

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

	[[nodiscard]] static constexpr uint8_t getColorCoordinate(const SDL_Color &color)
	{
		return GetColorComponent<Coord>(color);
	}

	[[nodiscard]] uint8_t leafIndexForColor(const SDL_Color &color, uint8_t result = 0)
	{
		const bool isLeft = getColorCoordinate(color) < pivot;
		if constexpr (RemainingDepth == 1) {
			return (2 * result) + (isLeft ? 0 : 1);
		} else {
			return (2 * child(isLeft).leafIndexForColor(color, result)) + (isLeft ? 0 : 1);
		}
	}

	[[nodiscard]] PaletteKdTreeNode<0> &leafByIndex(uint8_t index)
	{
		if constexpr (RemainingDepth == 1) {
			return child(index % 2 == 0);
		} else {
			return child(index % 2 == 0).leafByIndex(index / 2);
		}
	}

	[[maybe_unused]] void toGraphvizDot(size_t id, std::span<const uint8_t, 256> values, std::string &dot) const
	{
		StrAppend(dot, "  node_", id, " [label=\"");
		if (Coord == 0) {
			dot += 'r';
		} else if (Coord == 1) {
			dot += 'g';
		} else {
			dot += 'b';
		}
		StrAppend(dot, ": ", pivot, "\"]\n");

		const size_t leftId = (2 * id) + 1;
		const size_t rightId = (2 * id) + 2;
		left.toGraphvizDot(leftId, values, dot);
		right.toGraphvizDot(rightId, values, dot);
		StrAppend(dot, "  node_", id, " -- node_", leftId, "\n");
		StrAppend(dot, "  node_", id, " -- node_", rightId, "\n");
	}
};

/**
 * @brief A leaf node in the k-d tree.
 */
template <>
struct PaletteKdTreeNode</*RemainingDepth=*/0> {
	// We use inclusive indices to allow for representing the full [0, 255] range.
	// An empty node is represented as [1, 0].
	uint8_t valuesBegin;
	uint8_t valuesEndInclusive;

	[[maybe_unused]] void toGraphvizDot(size_t id, std::span<const uint8_t, 256> values, std::string &dot) const
	{
		StrAppend(dot, "  node_", id, " [shape=box label=\"");
		const uint8_t *it = values.data() + valuesBegin;
		const uint8_t *const end = values.data() + valuesEndInclusive;
		while (it <= end) {
			StrAppend(dot, static_cast<int>(*it), ", ");
			++it;
		}
		if (valuesBegin <= valuesEndInclusive) {
			dot[dot.size() - 2] = '\"';
			dot[dot.size() - 1] = ']';
			dot += "\n";
		} else {
			StrAppend(dot, "\"]\n");
		}
	}
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
	explicit PaletteKdTree(const SDL_Color palette[256], int skipFrom, int skipTo)
	    : palette_(palette)
	{
		populatePivots(skipFrom, skipTo);
		StaticVector<uint8_t, 256> leafValues[NumLeaves];
		for (int i = 0; i < 256; ++i) {
			if (i >= skipFrom && i <= skipTo) continue;
			leafValues[tree_.leafIndexForColor(palette[i])].emplace_back(i);
		}

		size_t totalLen = 0;
		for (uint8_t leafIndex = 0; leafIndex < NumLeaves; ++leafIndex) {
			PaletteKdTreeNode<0> &leaf = tree_.leafByIndex(leafIndex);
			const std::span<const uint8_t> values = leafValues[leafIndex];
			if (values.empty()) {
				leaf.valuesBegin = 1;
				leaf.valuesEndInclusive = 0;
			} else {
				leaf.valuesBegin = static_cast<uint8_t>(totalLen);
				leaf.valuesEndInclusive = static_cast<uint8_t>(totalLen - 1 + values.size());
				std::copy(values.begin(), values.end(), values_.data() + totalLen);
				totalLen += values.size();
			}
		}

#if DEVILUTIONX_PRINT_PALETTE_BLENDING_TREE_GRAPHVIZ
		// To generate palette.dot.svg, run:
		// dot -O -Tsvg palette.dot
		FILE *out = std::fopen("palette.dot", "w");
		std::string dot = toGraphvizDot();
		std::fwrite(dot.data(), dot.size(), 1, out);
		std::fclose(out);
#endif
	}

	[[nodiscard]] uint8_t findNearestNeighbor(const RGB &rgb) const
	{
		uint8_t best;
		uint32_t bestDiff = std::numeric_limits<uint32_t>::max();
		findNearestNeighborVisit(tree_, rgb, bestDiff, best);
		return best;
	}

	[[maybe_unused]] [[nodiscard]] std::string toGraphvizDot() const
	{
		std::string dot = "graph palette_tree {\n  rankdir=LR\n";
		tree_.toGraphvizDot(0, values_, dot);
		dot.append("}\n");
		return dot;
	}

private:
	[[nodiscard]] static uint8_t getMedian(std::span<const uint8_t> elements)
	{
		uint8_t min = 255;
		uint8_t max = 0;
		uint_fast16_t count[256] = {};
		for (const uint8_t x : elements) {
			min = std::min(x, min);
			max = std::max(x, max);
			++count[x];
		}

		const auto medianTarget = static_cast<uint_fast16_t>((elements.size() + 1) / 2);
		uint_fast16_t partialSum = count[min];
		for (uint_fast16_t i = min + 1; i <= max; ++i) {
			if (partialSum >= medianTarget) return i;
			partialSum += count[i];
		}

		// Can't find a helpful pivot so return 255 so that
		// NN lookups through this node mostly go to the left child.
		return 255;
	}

	template <size_t RemainingDepth, size_t N>
	void maybeAddToSubdivisionForMedian(
	    const PaletteKdTreeNode<RemainingDepth> &node, unsigned paletteIndex,
	    std::span<StaticVector<uint8_t, 256>, N> out)
	{
		const uint8_t color = node.getColorCoordinate(palette_[paletteIndex]);
		if constexpr (N == 1) {
			out[0].emplace_back(color);
		} else {
			const bool isLeft = color < node.pivot;
			maybeAddToSubdivisionForMedian(node.child(isLeft),
			    paletteIndex,
			    isLeft
			        ? out.template subspan<0, N / 2>()
			        : out.template subspan<N / 2, N / 2>());
		}
	}

	template <size_t RemainingDepth, size_t N>
	void setPivotsRecursively(
	    PaletteKdTreeNode<RemainingDepth> &node,
	    std::span<StaticVector<uint8_t, 256>, N> values)
	{
		if constexpr (N == 1) {
			node.pivot = getMedian(values[0]);
		} else {
			setPivotsRecursively(node.left, values.template subspan<0, N / 2>());
			setPivotsRecursively(node.right, values.template subspan<N / 2, N / 2>());
		}
	}

	template <size_t TargetDepth>
	void populatePivotsForTargetDepth(int skipFrom, int skipTo)
	{
		constexpr size_t NumSubdivisions = 1U << TargetDepth;
		std::array<StaticVector<uint8_t, 256>, NumSubdivisions> subdivisions;
		const std::span<StaticVector<uint8_t, 256>, NumSubdivisions> subdivisionsSpan { subdivisions };
		for (int i = 0; i < 256; ++i) {
			if (i >= skipFrom && i <= skipTo) continue;
			maybeAddToSubdivisionForMedian(tree_, i, subdivisionsSpan);
		}
		setPivotsRecursively(tree_, subdivisionsSpan);
	}

	template <size_t... TargetDepths>
	void populatePivotsImpl(int skipFrom, int skipTo, std::index_sequence<TargetDepths...> intSeq) // NOLINT(misc-unused-parameters)
	{
		(populatePivotsForTargetDepth<TargetDepths>(skipFrom, skipTo), ...);
	}

	void populatePivots(int skipFrom, int skipTo)
	{
		populatePivotsImpl(skipFrom, skipTo, std::make_index_sequence<PaletteKdTreeDepth> {});
	}

	template <size_t RemainingDepth>
	void findNearestNeighborVisit(const PaletteKdTreeNode<RemainingDepth> &node, const RGB &rgb,
	    uint32_t &bestDiff, uint8_t &best) const
	{
		const uint8_t coord = rgb[PaletteKdTreeNode<RemainingDepth>::Coord];
		findNearestNeighborVisit(node.child(coord < node.pivot), rgb, bestDiff, best);

		// To see if we need to check a node's subtree, we compare the distance from the query
		// to the current best candidate vs the distance to the edge of the half-space represented
		// by the node.
		if (bestDiff == std::numeric_limits<uint32_t>::max()
		    || GetColorDistanceToPlane(node.pivot, coord) < GetColorDistance(palette_[best], rgb)) {
			findNearestNeighborVisit(node.child(coord >= node.pivot), rgb, bestDiff, best);
		}
	}

	void findNearestNeighborVisit(const PaletteKdTreeNode<0> &node, const RGB &rgb,
	    uint32_t &bestDiff, uint8_t &best) const
	{
		const uint8_t *it = values_.data() + node.valuesBegin;
		const uint8_t *const end = values_.data() + node.valuesEndInclusive;
		while (it <= end) {
			const uint8_t paletteIndex = *it++;
			const uint32_t diff = GetColorDistance(palette_[paletteIndex], rgb);
			if (diff < bestDiff) {
				best = paletteIndex;
				bestDiff = diff;
			}
		}
	}

	const SDL_Color *palette_;
	PaletteKdTreeNode<PaletteKdTreeDepth> tree_;
	std::array<uint8_t, 256> values_;
};

} // namespace devilution
