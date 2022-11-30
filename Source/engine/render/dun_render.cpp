/**
 * @file dun_render.cpp
 *
 * Implementation of functionality for rendering the level tiles.
 */

// Debugging variables
// #define DEBUG_STR
// #define DEBUG_RENDER_COLOR
// #define DEBUG_RENDER_OFFSET_X 5
// #define DEBUG_RENDER_OFFSET_Y 5

#include "engine/render/dun_render.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>

#include "lighting.h"
#include "utils/stdcompat/algorithm.hpp"
#ifdef _DEBUG
#include "miniwin/misc_msg.h"
#endif
#include "options.h"
#include "utils/attributes.h"
#ifdef DEBUG_STR
#include "engine/render/text_render.hpp"
#endif
#if defined(DEBUG_STR) || defined(DUN_RENDER_STATS)
#include "utils/str_cat.hpp"
#endif

namespace devilution {

namespace {

/** Width of a tile rendering primitive. */
constexpr int_fast16_t Width = TILE_WIDTH / 2;

/** Height of a tile rendering primitive (except triangles). */
constexpr int_fast16_t Height = TILE_HEIGHT;

/** Height of the lower triangle of a triangular or a trapezoid tile. */
constexpr int_fast16_t LowerHeight = TILE_HEIGHT / 2;

/** Height of the upper triangle of a triangular tile. */
constexpr int_fast16_t TriangleUpperHeight = TILE_HEIGHT / 2 - 1;

/** Height of the upper rectangle of a trapezoid tile. */
constexpr int_fast16_t TrapezoidUpperHeight = TILE_HEIGHT / 2;

constexpr int_fast16_t TriangleHeight = LowerHeight + TriangleUpperHeight;

/** For triangles, for each pixel drawn vertically, this many pixels are drawn horizontally. */
constexpr int_fast16_t XStep = 2;

int_fast16_t GetTileHeight(TileType tile)
{
	if (tile == TileType::LeftTriangle || tile == TileType::RightTriangle)
		return TriangleHeight;
	return Height;
}

#ifdef DEBUG_STR
std::pair<string_view, UiFlags> GetTileDebugStr(TileType tile)
{
	// clang-format off
	switch (tile) {
		case TileType::Square: return {"S", UiFlags::AlignCenter | UiFlags::VerticalCenter};
		case TileType::TransparentSquare: return {"T", UiFlags::AlignCenter | UiFlags::VerticalCenter};
		case TileType::LeftTriangle: return {"<", UiFlags::AlignRight | UiFlags::VerticalCenter};
		case TileType::RightTriangle: return {">", UiFlags::VerticalCenter};
		case TileType::LeftTrapezoid: return {"\\", UiFlags::AlignCenter};
		case TileType::RightTrapezoid: return {"/", UiFlags::AlignCenter};
		default: return {"", {}};
	}
	// clang-format on
}
#endif

#ifdef DEBUG_RENDER_COLOR
int DBGCOLOR = 0;

int GetTileDebugColor(TileType tile)
{
	// clang-format off
	switch (tile) {
		case TileType::Square: return PAL16_YELLOW + 5;
		case TileType::TransparentSquare: return PAL16_ORANGE + 5;
		case TileType::LeftTriangle: return PAL16_GRAY + 5;
		case TileType::RightTriangle: return PAL16_BEIGE;
		case TileType::LeftTrapezoid: return PAL16_RED + 5;
		case TileType::RightTrapezoid: return PAL16_BLUE + 5;
		default: return 0;
	}
}
#endif // DEBUG_RENDER_COLOR

// Masks are defined by 2 template variables:
//
// 1. `OpaquePrefix`: Whether the line starts with opaque pixels
//    followed by blended pixels or the other way around.
// 2. `PrefixIncrement`: The change to the prefix when going
//    up 1 line.
//
// The Left mask can only be applied to LeftTrapezoid and TransparentSquare.
// The Right mask can only be applied to RightTrapezoid and TransparentSquare.
// The Left/RightFoliage masks can only be applied to TransparentSquare.

// True if the given OpaquePrefix and PrefixIncrement represent foliage.
// For foliage, we skip transparent pixels instead of blending them.
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool IsFoliage = PrefixIncrement != 0 && (OpaquePrefix == (PrefixIncrement > 0));

// True for foliage:
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool SkipTransparentPixels = IsFoliage<OpaquePrefix, PrefixIncrement>;

// True if the entire lower half of the mask is transparent.
// True for Transparent, LeftFoliage, and RightFoliage.
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool LowerHalfTransparent = (OpaquePrefix == (PrefixIncrement >= 0));

// The initial value for the prefix:
template <int8_t PrefixIncrement>
DVL_ALWAYS_INLINE int8_t InitPrefix()
{
	return PrefixIncrement >= 0 ? -32 : 64;
}

// The initial value for the prefix at y-th line (counting from the bottom).
template <int8_t PrefixIncrement>
DVL_ALWAYS_INLINE int8_t InitPrefix(int8_t y)
{
	return InitPrefix<PrefixIncrement>() + PrefixIncrement * y;
}

#ifdef DEBUG_STR
template <bool OpaquePrefix, int8_t PrefixIncrement>
std::string prefixDebugString(int8_t prefix) {
	std::string out(32, OpaquePrefix ? '0' : '1');
	const uint8_t clamped = clamp<int8_t>(prefix, 0, 32);
	out.replace(0, clamped, clamped, OpaquePrefix ? '1' : '0');
	StrAppend(out, " prefix=", prefix, " OpaquePrefix=", OpaquePrefix, " PrefixIncrement=", PrefixIncrement);
	return out;
}
#endif

enum class MaskType {
	Invalid,
	Solid,
	Transparent,
	Right,
	Left,
	RightFoliage,
	LeftFoliage,
};

enum class LightType : uint8_t {
	FullyDark,
	PartiallyLit,
	FullyLit,
};

template <LightType Light>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
	if (Light == LightType::FullyDark) {
		memset(dst, 0, n);
	} else if (Light == LightType::FullyLit) {
#ifndef DEBUG_RENDER_COLOR
		memcpy(dst, src, n);
#else
		memset(dst, DBGCOLOR, n);
#endif
	} else { // Partially lit
#ifndef DEBUG_RENDER_COLOR
		while (n-- != 0) {
			*dst++ = tbl[*src++];
		}
#else
		memset(dst, tbl[DBGCOLOR], n);
#endif
	}
}

template <LightType Light>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
#ifndef DEBUG_RENDER_COLOR
	if (Light == LightType::FullyDark) {
		while (n-- != 0) {
			*dst = paletteTransparencyLookup[0][*dst];
			++dst;
		}
	} else if (Light == LightType::FullyLit) {
		while (n-- != 0) {
			*dst = paletteTransparencyLookup[*dst][*src];
			++dst;
			++src;
		}
	} else { // Partially lit
		while (n-- != 0) {
			*dst = paletteTransparencyLookup[*dst][tbl[*src]];
			++dst;
			++src;
		}
	}
#else
	for (size_t i = 0; i < n; i++) {
		dst[i] = paletteTransparencyLookup[dst[i]][tbl[DBGCOLOR + 4]];
	}
#endif
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentOrOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if (Transparent) {
		RenderLineTransparent<Light>(dst, src, width, tbl);
	} else {
		RenderLineOpaque<Light>(dst, src, width, tbl);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentAndOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t prefixWidth, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if (OpaquePrefix) {
		RenderLineOpaque<Light>(dst, src, prefixWidth, tbl);
		if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
			RenderLineTransparent<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	} else {
		if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
			RenderLineTransparent<Light>(dst, src, prefixWidth, tbl);
		RenderLineOpaque<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLine(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl, int8_t prefix)
{
	if (PrefixIncrement == 0) {
		RenderLineTransparentOrOpaque<Light, OpaquePrefix>(dst, src, n, tbl);
	} else {
		RenderLineTransparentAndOpaque<Light, OpaquePrefix, PrefixIncrement>(dst, src, clamp<int8_t>(prefix, 0, n), n, tbl);
	}
}

struct Clip {
	int_fast16_t top;
	int_fast16_t bottom;
	int_fast16_t left;
	int_fast16_t right;
	int_fast16_t width;
	int_fast16_t height;
};

DVL_ALWAYS_INLINE Clip CalculateClip(int_fast16_t x, int_fast16_t y, int_fast16_t w, int_fast16_t h, const Surface &out)
{
	Clip clip;
	clip.top = y + 1 < h ? h - (y + 1) : 0;
	clip.bottom = y + 1 > out.h() ? (y + 1) - out.h() : 0;
	clip.left = x < 0 ? -x : 0;
	clip.right = x + w > out.w() ? x + w - out.w() : 0;
	clip.width = w - clip.left - clip.right;
	clip.height = h - clip.top - clip.bottom;
	return clip;
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareLowerHalf(uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl) {
	for (auto i = 0; i < LowerHeight; ++i, dst -= dstPitch) {
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, Width, tbl);
		src += Width;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareUpperHalf(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl) {
	uint_fast8_t prefixWidth = PrefixIncrement < 0 ? 32 : 0;
	for (auto i = 0; i < TrapezoidUpperHeight; ++i, dst -= dstPitch) {
		RenderLineTransparentAndOpaque<Light, OpaquePrefix, PrefixIncrement>(dst, src, prefixWidth, Width, tbl);
		if (PrefixIncrement != 0)
			prefixWidth += PrefixIncrement;
		src += Width;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	if (PrefixIncrement == 0) {
		// Fast path for MaskType::Solid and MaskType::Transparent
		for (auto i = 0; i < Height; ++i, dst -= dstPitch) {
			RenderLineTransparentOrOpaque<Light, OpaquePrefix>(dst, src, Width, tbl);
			src += Width;
		}
	} else {
		RenderSquareLowerHalf<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
		RenderSquareUpperHalf<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareClipped(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	src += clip.bottom * Height + clip.left;
	for (auto i = 0; i < clip.height; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix - (clip.left));
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquare(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.height == Height) {
		RenderSquareFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
	} else {
		RenderSquareClipped<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	int8_t prefix = InitPrefix<PrefixIncrement>();
	for (auto i = 0; i < Height; ++i, dst -= dstPitch + Width) {
		uint_fast8_t drawWidth = Width;
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, v, tbl, prefix - (Width - drawWidth));
				src += v;
			} else {
				v = -v;
			}
			dst += v;
			drawWidth -= v;
		}
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
// NOLINTNEXTLINE(readability-function-cognitive-complexity): Actually complex and has to be fast.
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareClipped(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const auto skipRestOfTheLine = [&src](int_fast16_t remainingWidth) {
		while (remainingWidth > 0) {
			const auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				src += v;
				remainingWidth -= v;
			} else {
				remainingWidth -= -v;
			}
		}
		assert(remainingWidth == 0);
	};

	// Skip the bottom clipped lines.
	for (auto i = 0; i < clip.bottom; ++i) {
		skipRestOfTheLine(Width);
	}

	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	for (auto i = 0; i < clip.height; ++i, dst -= dstPitch + clip.width) {
		auto drawWidth = clip.width;

		// Skip initial src if clipping on the left.
		// Handles overshoot, i.e. when the RLE segment goes into the unclipped area.
		auto remainingLeftClip = clip.left;
		while (remainingLeftClip > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				if (v > remainingLeftClip) {
					const auto overshoot = v - remainingLeftClip;
					RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src + remainingLeftClip, overshoot, tbl, prefix - (Width - remainingLeftClip));
					dst += overshoot;
					drawWidth -= overshoot;
				}
				src += v;
			} else {
				v = -v;
				if (v > remainingLeftClip) {
					const auto overshoot = v - remainingLeftClip;
					dst += overshoot;
					drawWidth -= overshoot;
				}
			}
			remainingLeftClip -= v;
		}

		// Draw the non-clipped segment
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				if (v > drawWidth) {
					RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, drawWidth, tbl, prefix - (Width - drawWidth));
					src += v;
					dst += drawWidth;
					drawWidth -= v;
					break;
				}
				RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, v, tbl, prefix - (Width - drawWidth));
				src += v;
			} else {
				v = -v;
				if (v > drawWidth) {
					dst += drawWidth;
					drawWidth -= v;
					break;
				}
			}
			dst += v;
			drawWidth -= v;
		}

		// Skip the rest of src line if clipping on the right
		assert(drawWidth <= 0);
		skipRestOfTheLine(clip.right + drawWidth);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquare(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.height == Height) {
		RenderTransparentSquareFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
	} else {
		RenderTransparentSquareClipped<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

/** Vertical clip for the lower and upper triangles of a diamond tile (L/RTRIANGLE).*/
struct DiamondClipY {
	int_fast16_t lowerBottom;
	int_fast16_t lowerTop;
	int_fast16_t upperBottom;
	int_fast16_t upperTop;
};

template <int_fast16_t UpperHeight = TriangleUpperHeight>
DVL_ALWAYS_INLINE DiamondClipY CalculateDiamondClipY(const Clip &clip)
{
	DiamondClipY result;
	if (clip.bottom > LowerHeight) {
		result.lowerBottom = LowerHeight;
		result.upperBottom = clip.bottom - LowerHeight;
		result.lowerTop = result.upperTop = 0;
	} else if (clip.top > UpperHeight) {
		result.upperTop = UpperHeight;
		result.lowerTop = clip.top - UpperHeight;
		result.upperBottom = result.lowerBottom = 0;
	} else {
		result.upperTop = clip.top;
		result.lowerBottom = clip.bottom;
		result.lowerTop = result.upperBottom = 0;
	}
	return result;
}

DVL_ALWAYS_INLINE std::size_t CalculateTriangleSourceSkipLowerBottom(int_fast16_t numLines)
{
	return XStep * numLines * (numLines + 1) / 2 + 2 * ((numLines + 1) / 2);
}

DVL_ALWAYS_INLINE std::size_t CalculateTriangleSourceSkipUpperBottom(int_fast16_t numLines)
{
	return 2 * TriangleUpperHeight * numLines - numLines * (numLines - 1) + 2 * ((numLines + 1) / 2);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLower(uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	dst += XStep * (LowerHeight - 1);
	for (auto i = 1; i <= LowerHeight; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipVertical(int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1) - clipLeft;
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		const auto startX = Width - XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		if (width > skip)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst + skip, src + skip, width - skip, tbl, prefix - (skip));
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipRightAndVertical(int_fast16_t clipRight, int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		if (width > clipRight)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width - clipRight, tbl, prefix);
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	int8_t prefix = InitPrefix<PrefixIncrement>(LowerHeight);
	dst += 2 * XStep;
	for (auto i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderLeftTriangleLowerClipVertical<Light, OpaquePrefix, PrefixIncrement>(prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderLeftTriangleLowerClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipLeft, prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		const auto startX = XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		if (width > skip)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst + skip, src + skip, width - skip, tbl, prefix - (skip));
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipRightAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderLeftTriangleLowerClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipRight, prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		if (width <= clipRight)
			break;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width - clipRight, tbl, prefix);
		src += width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangle(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderLeftTriangleFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTriangleClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTriangleClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTriangleClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLower(uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	for (auto i = 1; i <= LowerHeight; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipVertical(int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		if (width > clipLeft)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src + clipLeft, width - clipLeft, tbl, prefix - clipLeft);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipRightAndVertical(int_fast16_t clipRight, int8_t &prefix, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, int dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		if (width > skip)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width - skip, tbl, prefix);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	int8_t prefix = InitPrefix<PrefixIncrement>(LowerHeight);
	for (auto i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderRightTriangleLowerClipVertical<Light, OpaquePrefix, PrefixIncrement>(prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width, tbl, prefix);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderRightTriangleLowerClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipLeft, prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		if (width <= clipLeft)
			break;
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src + clipLeft, width - clipLeft, tbl, prefix - clipLeft);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipRightAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderRightTriangleLowerClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipRight, prefix, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		if (width > skip)
			RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, width - skip, tbl, prefix);
		src += width + 2 * (i % 2);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangle(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderRightTriangleFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTriangleClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTriangleClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTriangleClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	dst += XStep;
	RenderSquareUpperHalf<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipVertical<Light, OpaquePrefix, PrefixIncrement>(prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, Width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderLeftTriangleLowerClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipLeft, prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clipLeft;
	dst += XStep + clipLeft;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix - clipLeft);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipRightAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	const int_fast16_t clipRight = clip.right;
	RenderLeftTriangleLowerClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipRight, prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoid(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderLeftTrapezoidFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTrapezoidClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTrapezoidClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidFull(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	RenderSquareUpperHalf<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipVertical<Light, OpaquePrefix, PrefixIncrement>(prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, Width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderRightTriangleLowerClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipLeft, prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clipLeft;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix - clipLeft);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipRightAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	const int_fast16_t clipRight = clip.right;
	RenderRightTriangleLowerClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clipRight, prefix, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoid(uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderRightTrapezoidFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTrapezoidClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTrapezoidClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileType(TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::Square:
		RenderSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTriangle:
		RenderLeftTriangle<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTriangle:
		RenderRightTriangle<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareDispatch(uint8_t lightTableIndex, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (lightTableIndex == LightsMax) {
		RenderTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else if (lightTableIndex == 0) {
		RenderTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquare(TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquare(TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquareDispatch(uint8_t lightTableIndex, TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (lightTableIndex == LightsMax) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else if (lightTableIndex == 0) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidOrTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquareDispatch(uint8_t lightTableIndex, TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (lightTableIndex == LightsMax) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else if (lightTableIndex == 0) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidOrTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileDispatch(uint8_t lightTableIndex, TileType tile, uint8_t *DVL_RESTRICT dst, int dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (lightTableIndex == LightsMax) {
		RenderTileType<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else if (lightTableIndex == 0) {
		RenderTileType<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderTileType<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	}
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT MaskType GetMask(TileType tile, uint16_t levelPieceId, ArchType archType, bool transparency, bool foliage)
{
#ifdef _DEBUG
	if ((SDL_GetModState() & KMOD_ALT) != 0) {
		return MaskType::Solid;
	}
#endif

	if (transparency) {
		if (archType == ArchType::None) {
			return MaskType::Transparent;
		}
		if (archType == ArchType::Left && tile != TileType::LeftTriangle) {
			if (TileHasAny(levelPieceId, TileProperties::TransparentLeft)) {
				return MaskType::Left;
			}
		}
		if (archType == ArchType::Right && tile != TileType::RightTriangle) {
			if (TileHasAny(levelPieceId, TileProperties::TransparentRight)) {
				return MaskType::Right;
			}
		}
	} else if (archType != ArchType::None && foliage) {
		if (tile != TileType::TransparentSquare)
			return MaskType::Invalid;
		if (archType == ArchType::Left)
			return MaskType::LeftFoliage;
		if (archType == ArchType::Right)
			return MaskType::RightFoliage;
	}
	return MaskType::Solid;
}

// Blit with left and vertical clipping.
void RenderBlackTileClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, int sx, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = clipY.lowerBottom + 1; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto w = 2 * XStep * i;
		const auto curX = sx + TILE_WIDTH / 2 - XStep * i;
		if (curX >= 0) {
			memset(dst, 0, w);
		} else if (-curX <= w) {
			memset(dst - curX, 0, w + curX);
		}
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = clipY.upperBottom; i < upperMax; ++i, dst -= dstPitch - XStep) {
		const auto w = 2 * XStep * (TriangleUpperHeight - i);
		const auto curX = sx + TILE_WIDTH / 2 - XStep * (TriangleUpperHeight - i);
		if (curX >= 0) {
			memset(dst, 0, w);
		} else if (-curX <= w) {
			memset(dst - curX, 0, w + curX);
		} else {
			break;
		}
	}
}

// Blit with right and vertical clipping.
void RenderBlackTileClipRightAndVertical(uint8_t *DVL_RESTRICT dst, int dstPitch, int_fast16_t maxWidth, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = clipY.lowerBottom + 1; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto width = 2 * XStep * i;
		const auto endX = TILE_WIDTH / 2 + XStep * i;
		const auto skip = endX > maxWidth ? endX - maxWidth : 0;
		if (width > skip)
			memset(dst, 0, width - skip);
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		const auto width = TILE_WIDTH - 2 * XStep * i;
		const auto endX = TILE_WIDTH / 2 + XStep * (TriangleUpperHeight - i + 1);
		const auto skip = endX > maxWidth ? endX - maxWidth : 0;
		if (width <= skip)
			break;
		memset(dst, 0, width - skip);
	}
}

// Blit with vertical clipping only.
void RenderBlackTileClipY(uint8_t *DVL_RESTRICT dst, int dstPitch, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		memset(dst, 0, 2 * XStep * i);
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		memset(dst, 0, TILE_WIDTH - 2 * XStep * i);
	}
}

// Blit a black tile without clipping (must be fully in bounds).
void RenderBlackTileFull(uint8_t *DVL_RESTRICT dst, int dstPitch)
{
	dst += XStep * (LowerHeight - 1);
	// Tile is fully in bounds, can use constant loop boundaries.
	// Lower triangle (drawn bottom to top):
	for (unsigned i = 1; i <= LowerHeight; ++i, dst -= dstPitch + XStep) {
		memset(dst, 0, 2 * XStep * i);
	}
	dst += 2 * XStep;
	// Upper triangle (drawn bottom to to top):
	for (unsigned i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch - XStep) {
		memset(dst, 0, TILE_WIDTH - 2 * XStep * i);
	}
}

} // namespace

#ifdef DUN_RENDER_STATS
std::unordered_map<DunRenderType, size_t, DunRenderTypeHash> DunRenderStats;

string_view TileTypeToString(TileType tileType)
{
	// clang-format off
	switch (tileType) {
	case TileType::Square: return "Square";
	case TileType::TransparentSquare: return "TransparentSquare";
	case TileType::LeftTriangle: return "LeftTriangle";
	case TileType::RightTriangle: return "RightTriangle";
	case TileType::LeftTrapezoid: return "LeftTrapezoid";
	case TileType::RightTrapezoid: return "RightTrapezoid";
	default: return "???";
	}
	// clang-format on
}

string_view MaskTypeToString(uint8_t maskType)
{
	// clang-format off
	switch (static_cast<MaskType>(maskType)) {
	case MaskType::Invalid: return "Invalid";
	case MaskType::Solid: return "Solid";
	case MaskType::Transparent: return "Transparent";
	case MaskType::Right: return "Right";
	case MaskType::Left: return "Left";
	case MaskType::RightFoliage: return "RightFoliage";
	case MaskType::LeftFoliage: return "LeftFoliage";
	default: return "???";
	}
	// clang-format on
}
#endif

void RenderTile(const Surface &out, Point position,
    LevelCelBlock levelCelBlock, uint16_t levelPieceId,
    uint8_t lightTableIndex, ArchType archType,
    bool transparency, bool foliage)
{
	const TileType tile = levelCelBlock.type();

#ifdef DEBUG_RENDER_OFFSET_X
	position.x += DEBUG_RENDER_OFFSET_X;
#endif
#ifdef DEBUG_RENDER_OFFSET_Y
	position.y += DEBUG_RENDER_OFFSET_Y;
#endif
#ifdef DEBUG_RENDER_COLOR
	DBGCOLOR = GetTileDebugColor(tile);
#endif

	Clip clip = CalculateClip(position.x, position.y, Width, GetTileHeight(tile), out);
	if (clip.width <= 0 || clip.height <= 0)
		return;

	MaskType maskType = GetMask(tile, levelPieceId, archType, transparency, foliage);
	const uint8_t *tbl = &LightTables[256 * lightTableIndex];
	const auto *pFrameTable = reinterpret_cast<const uint32_t *>(pDungeonCels.get());
	const auto *src = reinterpret_cast<const uint8_t *>(&pDungeonCels[pFrameTable[levelCelBlock.frame()]]);
	uint8_t *dst = out.at(static_cast<int>(position.x + clip.left), static_cast<int>(position.y - clip.bottom));
	const auto dstPitch = out.pitch();

#ifdef DUN_RENDER_STATS
	++DunRenderStats[DunRenderType { tile, static_cast<uint8_t>(maskType) }];
#endif

	switch (maskType) {
	case MaskType::Invalid:
		break;
	case MaskType::Solid:
		RenderTileDispatch</*OpaquePrefix=*/false, /*PrefixIncrement=*/0>(lightTableIndex, tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Transparent:
		RenderTileDispatch</*OpaquePrefix=*/true, /*PrefixIncrement=*/0>(lightTableIndex, tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Left:
		RenderLeftTrapezoidOrTransparentSquareDispatch</*OpaquePrefix=*/false, /*PrefixIncrement=*/2>(lightTableIndex, tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Right:
		RenderRightTrapezoidOrTransparentSquareDispatch</*OpaquePrefix=*/true, /*PrefixIncrement=*/-2>(lightTableIndex, tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::LeftFoliage:
		RenderTransparentSquareDispatch</*OpaquePrefix=*/true, /*PrefixIncrement=*/2>(lightTableIndex, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::RightFoliage:
		RenderTransparentSquareDispatch</*OpaquePrefix=*/false, /*PrefixIncrement=*/-2>(lightTableIndex, dst, dstPitch, src, tbl, clip);
		break;
	}

#ifdef DEBUG_STR
	const std::pair<string_view, UiFlags> debugStr = GetTileDebugStr(tile);
	DrawString(out, debugStr.first, Rectangle { Point { position.x + 2, position.y - 29 }, Size { 28, 28 } }, debugStr.second);
#endif
}

void world_draw_black_tile(const Surface &out, int sx, int sy)
{
#ifdef DEBUG_RENDER_OFFSET_X
	sx += DEBUG_RENDER_OFFSET_X;
#endif
#ifdef DEBUG_RENDER_OFFSET_Y
	sy += DEBUG_RENDER_OFFSET_Y;
#endif
	auto clip = CalculateClip(sx, sy, TILE_WIDTH, TriangleHeight, out);
	if (clip.width <= 0 || clip.height <= 0)
		return;

	auto clipY = CalculateDiamondClipY(clip);
	uint8_t *dst = out.at(sx, static_cast<int>(sy - clip.bottom));
	if (clip.width == TILE_WIDTH) {
		if (clip.height == TriangleHeight) {
			RenderBlackTileFull(dst, out.pitch());
		} else {
			RenderBlackTileClipY(dst, out.pitch(), clipY);
		}
	} else {
		if (clip.right == 0) {
			RenderBlackTileClipLeftAndVertical(dst, out.pitch(), sx, clipY);
		} else {
			RenderBlackTileClipRightAndVertical(dst, out.pitch(), clip.width, clipY);
		}
	}
}

} // namespace devilution
