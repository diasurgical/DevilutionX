#include "utils/palette_kd_tree.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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

#include "utils/static_vector.hpp"
#include "utils/str_cat.hpp"

#if DEVILUTIONX_PRINT_PALETTE_BLENDING_TREE_GRAPHVIZ
#include <cstdio>
#endif

namespace devilution {
namespace {

template <size_t N>
uint8_t GetColorComponent(const SDL_Color &);
template <>
inline uint8_t GetColorComponent<0>(const SDL_Color &c) { return c.r; }
template <>
inline uint8_t GetColorComponent<1>(const SDL_Color &c) { return c.g; }
template <>
inline uint8_t GetColorComponent<2>(const SDL_Color &c) { return c.b; }

template <size_t RemainingDepth>
[[nodiscard]] PaletteKdTreeNode<0> &LeafByIndex(PaletteKdTreeNode<RemainingDepth> &node, uint8_t index)
{
	if constexpr (RemainingDepth == 1) {
		return node.child(index % 2 == 0);
	} else {
		return LeafByIndex(node.child(index % 2 == 0), index / 2);
	}
}

template <size_t RemainingDepth>
[[nodiscard]] uint8_t LeafIndexForColor(const PaletteKdTreeNode<RemainingDepth> &node, const SDL_Color &color, uint8_t result = 0)
{
	const bool isLeft = GetColorComponent<PaletteKdTreeNode<RemainingDepth>::Coord>(color) < node.pivot;
	if constexpr (RemainingDepth == 1) {
		return (2 * result) + (isLeft ? 0 : 1);
	} else {
		return (2 * LeafIndexForColor(node.child(isLeft), color, result)) + (isLeft ? 0 : 1);
	}
}

struct MedianInfo {
	std::array<uint16_t, 256> counts = {};
	uint16_t numValues = 0;
};

[[nodiscard]] static uint8_t GetMedian(const MedianInfo &medianInfo)
{
	const std::span<const uint16_t, 256> counts = medianInfo.counts;
	const uint_fast16_t numValues = medianInfo.numValues;
	const auto medianTarget = static_cast<uint_fast16_t>((medianInfo.numValues + 1) / 2);
	uint_fast16_t partialSum = 0;
	uint_fast16_t i = 0;
	for (; partialSum < medianTarget && partialSum != numValues; ++i) {
		partialSum += counts[i];
	}

	// Special cases:
	// 1. If the elements are empty, this will return 0.
	// 2. If all the elements are the same, this will be `value + 1` (rolling over to 0 if value is 256).
	//    This means all the elements will be on one side of the pivot (left unless the value is 255).
	return static_cast<uint8_t>(i);
}

template <size_t RemainingDepth, size_t N>
void MaybeAddToSubdivisionForMedian(
    const PaletteKdTreeNode<RemainingDepth> &node,
    const SDL_Color palette[256], unsigned paletteIndex,
    std::span<MedianInfo, N> medianInfos)
{
	const uint8_t color = GetColorComponent<PaletteKdTreeNode<RemainingDepth>::Coord>(palette[paletteIndex]);
	if constexpr (N == 1) {
		MedianInfo &medianInfo = medianInfos[0];
		++medianInfo.counts[color];
		++medianInfo.numValues;
	} else {
		const bool isLeft = color < node.pivot;
		MaybeAddToSubdivisionForMedian(node.child(isLeft),
		    palette,
		    paletteIndex,
		    isLeft
		        ? medianInfos.template subspan<0, N / 2>()
		        : medianInfos.template subspan<N / 2, N / 2>());
	}
}

template <size_t RemainingDepth, size_t N>
void SetPivotsRecursively(
    PaletteKdTreeNode<RemainingDepth> &node,
    std::span<MedianInfo, N> medianInfos)
{
	if constexpr (N == 1) {
		node.pivot = GetMedian(medianInfos[0]);
	} else {
		SetPivotsRecursively(node.left, medianInfos.template subspan<0, N / 2>());
		SetPivotsRecursively(node.right, medianInfos.template subspan<N / 2, N / 2>());
	}
}

template <size_t TargetDepth>
void PopulatePivotsForTargetDepth(PaletteKdTreeNode<PaletteKdTreeDepth> &root,
    const SDL_Color palette[256], int skipFrom, int skipTo)
{
	constexpr size_t NumSubdivisions = 1U << TargetDepth;
	std::array<MedianInfo, NumSubdivisions> subdivisions = {};
	const std::span<MedianInfo, NumSubdivisions> subdivisionsSpan { subdivisions };
	for (int i = 0; i < 256; ++i) {
		if (i >= skipFrom && i <= skipTo) continue;
		MaybeAddToSubdivisionForMedian(root, palette, i, subdivisionsSpan);
	}
	SetPivotsRecursively(root, subdivisionsSpan);
}

template <size_t... TargetDepths>
void PopulatePivotsImpl(PaletteKdTreeNode<PaletteKdTreeDepth> &root,
    const SDL_Color palette[256], int skipFrom, int skipTo, std::index_sequence<TargetDepths...> intSeq) // NOLINT(misc-unused-parameters)
{
	(PopulatePivotsForTargetDepth<TargetDepths>(root, palette, skipFrom, skipTo), ...);
}

void PopulatePivots(PaletteKdTreeNode<PaletteKdTreeDepth> &root,
    const SDL_Color palette[256], int skipFrom, int skipTo)
{
	PopulatePivotsImpl(root, palette, skipFrom, skipTo, std::make_index_sequence<PaletteKdTreeDepth> {});
}

} // namespace

PaletteKdTree::PaletteKdTree(const SDL_Color palette[256], int skipFrom, int skipTo)
{
	PopulatePivots(tree_, palette, skipFrom, skipTo);
	StaticVector<uint8_t, 256> leafValues[NumLeaves];
	for (int i = 0; i < 256; ++i) {
		if (i >= skipFrom && i <= skipTo) continue;
		leafValues[LeafIndexForColor(tree_, palette[i])].emplace_back(i);
	}

	size_t totalLen = 0;
	for (uint8_t leafIndex = 0; leafIndex < NumLeaves; ++leafIndex) {
		PaletteKdTreeNode<0> &leaf = LeafByIndex(tree_, leafIndex);
		const std::span<const uint8_t> values = leafValues[leafIndex];
		if (values.empty()) {
			leaf.valuesBegin = 1;
			leaf.valuesEndInclusive = 0;
		} else {
			leaf.valuesBegin = static_cast<uint8_t>(totalLen);
			leaf.valuesEndInclusive = static_cast<uint8_t>(totalLen - 1 + values.size());

			for (size_t i = 0; i < values.size(); ++i) {
				const uint8_t value = values[i];
				values_[totalLen + i] = std::make_pair(RGB { palette[value].r, palette[value].g, palette[value].b }, value);
			}
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

std::string PaletteKdTree::toGraphvizDot() const
{
	std::string dot = "graph palette_tree {\n  rankdir=LR\n";
	tree_.toGraphvizDot(0, values_, dot);
	dot.append("}\n");
	return dot;
}

void PaletteKdTreeNode<0>::toGraphvizDot(
    size_t id, std::span<const std::pair<PaletteKdTreeNode<0>::RGB, uint8_t>, 256> values, std::string &dot) const
{
	StrAppend(dot, "  node_", id, R"( [shape=plain label=<
  <table border="0" cellborder="0" cellspacing="0" cellpadding="2" style="ROUNDED">
    <tr>)");
	const std::pair<RGB, uint8_t> *const end = values.data() + valuesEndInclusive;
	for (const std::pair<RGB, uint8_t> *it = values.data() + valuesBegin; it <= end; ++it) {
		const auto &[rgb, paletteIndex] = *it;
		StrAppend(dot, R"(<td balign="left" bgcolor="#)",
		    AsHexPad2(rgb[0]), AsHexPad2(rgb[1]), AsHexPad2(rgb[2]), "\">");
		const bool useWhiteText = rgb[0] + rgb[1] + rgb[2] < 350;
		if (useWhiteText) StrAppend(dot, R"(<font color="white">)");
		StrAppend(dot,
		    static_cast<int>(rgb[0]), " ",
		    static_cast<int>(rgb[1]), " ",
		    static_cast<int>(rgb[2]), R"(<br/>)",
		    static_cast<int>(paletteIndex));
		if (useWhiteText) StrAppend(dot, "</font>");
		StrAppend(dot, "</td>");
	}
	if (valuesBegin > valuesEndInclusive) StrAppend(dot, "<td></td>");
	StrAppend(dot, "</tr>\n  </table>>]\n");
}

} // namespace devilution
