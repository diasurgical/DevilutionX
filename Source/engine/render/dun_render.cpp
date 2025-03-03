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

#include <SDL_endian.h>

#include <climits>
#include <cstddef>
#include <cstdint>

#include "engine/render/blit_impl.hpp"
#include "levels/dun_tile.hpp"
#include "lighting.h"
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
constexpr int_fast16_t Width = DunFrameWidth;

/** Height of a tile rendering primitive (except triangles). */
constexpr int_fast16_t Height = DunFrameHeight;

/** Height of the lower triangle of a triangular or a trapezoid tile. */
constexpr int_fast16_t LowerHeight = DunFrameHeight / 2;

/** Height of the upper triangle of a triangular tile. */
constexpr int_fast16_t TriangleUpperHeight = DunFrameHeight / 2 - 1;

/** Height of the upper rectangle of a trapezoid tile. */
constexpr int_fast16_t TrapezoidUpperHeight = DunFrameHeight / 2;

constexpr int_fast16_t TriangleHeight = DunFrameTriangleHeight;

/** For triangles, for each pixel drawn vertically, this many pixels are drawn horizontally. */
constexpr int_fast16_t XStep = 2;

#ifdef DEBUG_STR
std::pair<std::string_view, UiFlags> GetTileDebugStr(TileType tile)
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
	// clang-format on
}
#endif // DEBUG_RENDER_COLOR

// How many pixels to increment the transparent (Left) or opaque (Right)
// prefix width after each line (drawing bottom-to-top).
template <MaskType Mask>
constexpr int8_t PrefixIncrement = 0;
template <>
constexpr int8_t PrefixIncrement<MaskType::Left> = 2;
template <>
constexpr int8_t PrefixIncrement<MaskType::Right> = -2;

// Initial value for the prefix.
template <MaskType Mask>
int8_t InitialPrefix = PrefixIncrement<Mask> >= 0 ? -32 : 64;

// The initial value for the prefix at y-th line (counting from the bottom).
template <MaskType Mask>
DVL_ALWAYS_INLINE int8_t InitPrefix(int8_t y)
{
	return InitialPrefix<Mask> + PrefixIncrement<Mask> * y;
}

enum class LightType : uint8_t {
	FullyDark,
	PartiallyLit,
	FullyLit,
	PerPixel,
};

template <LightType Light>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl);

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque<LightType::FullyDark>(uint8_t *DVL_RESTRICT dst, [[maybe_unused]] const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	BlitFillDirect(dst, n, 0);
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque<LightType::FullyLit>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
#ifndef DEBUG_RENDER_COLOR
	BlitPixelsDirect(dst, src, n);
#else
	BlitFillDirect(dst, n, DBGCOLOR);
#endif
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque<LightType::PartiallyLit>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
#ifndef DEBUG_RENDER_COLOR
	BlitPixelsWithMap(dst, src, n, tbl);
#else
	BlitFillDirect(dst, n, tbl[DBGCOLOR]);
#endif
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque<LightType::PerPixel>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
#ifndef DEBUG_RENDER_COLOR
	BlitPixelsWithLightmap(dst, src, n);
#else
	BlitFillDirect(dst, n, tbl[DBGCOLOR]);
#endif
}

#ifndef DEBUG_RENDER_COLOR
template <LightType Light>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl);

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent<LightType::FullyDark>(uint8_t *DVL_RESTRICT dst, [[maybe_unused]] const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	BlitFillBlended(dst, n, 0);
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent<LightType::FullyLit>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	BlitPixelsBlended(dst, src, n);
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent<LightType::PartiallyLit>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
	BlitPixelsBlendedWithMap(dst, src, n, tbl);
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent<LightType::PerPixel>(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
	BlitPixelsBlendedWithLightmap(dst, src, n);
}
#else // DEBUG_RENDER_COLOR
template <LightType Light>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl)
{
	for (size_t i = 0; i < n; i++) {
		dst[i] = paletteTransparencyLookup[dst[i]][tbl[DBGCOLOR + 4]];
	}
}
#endif

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentOrOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if constexpr (Transparent) {
		RenderLineTransparent<Light>(dst, src, width, tbl);
	} else {
		RenderLineOpaque<Light>(dst, src, width, tbl);
	}
}

template <LightType Light, bool OpaqueFirst>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentAndOpaque(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t prefixWidth, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if constexpr (OpaqueFirst) {
		RenderLineOpaque<Light>(dst, src, prefixWidth, tbl);
		RenderLineTransparent<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	} else {
		RenderLineTransparent<Light>(dst, src, prefixWidth, tbl);
		RenderLineOpaque<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLine(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl, int8_t prefix)
{
	if constexpr (Mask == MaskType::Solid || Mask == MaskType::Transparent) {
		RenderLineTransparentOrOpaque<Light, /*Transparent=*/Mask == MaskType::Transparent>(dst, src, n, tbl);
	} else if (prefix >= static_cast<int8_t>(n)) {
		// We std::clamp the prefix to (0, n] and avoid calling `RenderLineTransparent/Opaque` with width=0.
		if constexpr (Mask == MaskType::Right) {
			RenderLineOpaque<Light>(dst, src, n, tbl);
		} else {
			RenderLineTransparent<Light>(dst, src, n, tbl);
		}
	} else if (prefix <= 0) {
		if constexpr (Mask == MaskType::Left) {
			RenderLineOpaque<Light>(dst, src, n, tbl);
		} else {
			RenderLineTransparent<Light>(dst, src, n, tbl);
		}
	} else {
		RenderLineTransparentAndOpaque<Light, /*OpaqueFirst=*/Mask == MaskType::Right>(dst, src, prefix, n, tbl);
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

DVL_ALWAYS_INLINE bool IsFullyDark(const uint8_t *DVL_RESTRICT tbl)
{
	return tbl == FullyDarkLightTable;
}

DVL_ALWAYS_INLINE bool IsFullyLit(const uint8_t *DVL_RESTRICT tbl)
{
	return tbl == FullyLitLightTable;
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	for (auto i = 0; i < Height; ++i, dst -= dstPitch) {
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, Width, tbl);
		src += Width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareClipped(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	src += clip.bottom * Height + clip.left;
	for (auto i = 0; i < clip.height; ++i, dst -= dstPitch) {
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, clip.width, tbl);
		src += Width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquare(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.height == Height) {
		RenderSquareFull<Light, Transparent>(dst, dstPitch, src, tbl);
	} else {
		RenderSquareClipped<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, unsigned height)
{
	int8_t prefix = InitialPrefix<Mask>;
	DVL_ASSUME(height >= 16);
	DVL_ASSUME(height <= 32);
	for (unsigned i = 0; i < height; ++i, dst -= dstPitch + Width) {
		uint_fast8_t drawWidth = Width;
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				RenderLine<Light, Mask>(dst, src, v, tbl, prefix - (Width - drawWidth));
				src += v;
			} else {
				v = -v;
			}
			dst += v;
			drawWidth -= v;
		}
		prefix += PrefixIncrement<Mask>;
	}
}

template <LightType Light, MaskType Mask>
// NOLINTNEXTLINE(readability-function-cognitive-complexity): Actually complex and has to be fast.
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareClipped(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
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

	int8_t prefix = InitPrefix<Mask>(clip.bottom);
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
					RenderLine<Light, Mask>(dst, src + remainingLeftClip, overshoot, tbl, prefix - (Width - remainingLeftClip));
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
					RenderLine<Light, Mask>(dst, src, drawWidth, tbl, prefix - (Width - drawWidth));
					src += v;
					dst += drawWidth;
					drawWidth -= v;
					break;
				}
				RenderLine<Light, Mask>(dst, src, v, tbl, prefix - (Width - drawWidth));
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
		prefix += PrefixIncrement<Mask>;
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquare(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.bottom == 0 && clip.top == 0) {
		RenderTransparentSquareFull<Light, Mask>(dst, dstPitch, src, tbl, clip.height);
	} else {
		RenderTransparentSquareClipped<Light, Mask>(dst, dstPitch, src, tbl, clip);
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
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT DiamondClipY CalculateDiamondClipY(const Clip &clip)
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

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT std::size_t CalculateTriangleSourceSkipLowerBottom(int_fast16_t numLines)
{
	return XStep * numLines * (numLines + 1) / 2;
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT std::size_t CalculateTriangleSourceSkipUpperBottom(int_fast16_t numLines)
{
	return 2 * TriangleUpperHeight * numLines - numLines * (numLines - 1);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTriangleLower(uint8_t *DVL_RESTRICT &dst, ptrdiff_t dstLineOffset, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 0 * dstLineOffset, src + 0, 2, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 1 * dstLineOffset, src + 2, 4, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 2 * dstLineOffset, src + 6, 6, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 3 * dstLineOffset, src + 12, 8, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 4 * dstLineOffset, src + 20, 10, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 5 * dstLineOffset, src + 30, 12, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 6 * dstLineOffset, src + 42, 14, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 7 * dstLineOffset, src + 56, 16, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 8 * dstLineOffset, src + 72, 18, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 9 * dstLineOffset, src + 90, 20, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 10 * dstLineOffset, src + 110, 22, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 11 * dstLineOffset, src + 132, 24, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 12 * dstLineOffset, src + 156, 26, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 13 * dstLineOffset, src + 182, 28, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 14 * dstLineOffset, src + 210, 30, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 15 * dstLineOffset, src + 240, 32, tbl);
	src += 272;
	dst -= 16 * dstLineOffset;
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTriangleLower<LightType::FullyDark, /*Transparent=*/false>(uint8_t *DVL_RESTRICT &dst, ptrdiff_t dstLineOffset, const uint8_t *DVL_RESTRICT &src, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	unsigned width = XStep;
	for (unsigned i = 0; i < LowerHeight; ++i) {
		BlitFillDirect(dst, width, 0);
		dst -= dstLineOffset;
		width += XStep;
	}
	src += 272;
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTriangleUpper(uint8_t *DVL_RESTRICT dst, ptrdiff_t dstLineOffset, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 0 * dstLineOffset, src + 0, 30, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 1 * dstLineOffset, src + 30, 28, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 2 * dstLineOffset, src + 58, 26, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 3 * dstLineOffset, src + 84, 24, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 4 * dstLineOffset, src + 108, 22, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 5 * dstLineOffset, src + 130, 20, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 6 * dstLineOffset, src + 150, 18, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 7 * dstLineOffset, src + 168, 16, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 8 * dstLineOffset, src + 184, 14, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 9 * dstLineOffset, src + 198, 12, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 10 * dstLineOffset, src + 210, 10, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 11 * dstLineOffset, src + 220, 8, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 12 * dstLineOffset, src + 228, 6, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 13 * dstLineOffset, src + 234, 4, tbl);
	RenderLineTransparentOrOpaque<Light, Transparent>(dst - 14 * dstLineOffset, src + 238, 2, tbl);
}

template <>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTriangleUpper<LightType::FullyDark, /*Transparent=*/false>(uint8_t *DVL_RESTRICT dst, ptrdiff_t dstLineOffset, [[maybe_unused]] const uint8_t *DVL_RESTRICT src, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	unsigned width = Width - XStep;
	for (unsigned i = 0; i < TriangleUpperHeight; ++i) {
		BlitFillDirect(dst, width, 0);
		dst -= dstLineOffset;
		width -= XStep;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLower(uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	dst += XStep * (LowerHeight - 1);
	RenderTriangleLower<Light, Transparent>(dst, dstPitch + XStep, src, tbl);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipVertical(const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1) - clipLeft;
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto width = XStep * i;
		const auto startX = Width - XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		if (width > skip)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst + skip, src + skip, width - skip, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipRightAndVertical(int_fast16_t clipRight, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto width = XStep * i;
		if (width > clipRight)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - clipRight, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, Transparent>(dst, dstPitch, src, tbl);
	dst += 2 * XStep;
	RenderTriangleUpper<Light, Transparent>(dst, dstPitch - XStep, src, tbl);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderLeftTriangleLowerClipVertical<Light, Transparent>(clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderLeftTriangleLowerClipLeftAndVertical<Light, Transparent>(clipLeft, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		const auto width = Width - XStep * i;
		const auto startX = XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst + skip, src + skip, width > skip ? width - skip : 0, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipRightAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderLeftTriangleLowerClipRightAndVertical<Light, Transparent>(clipRight, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		const auto width = Width - XStep * i;
		if (width <= clipRight)
			break;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - clipRight, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangle(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderLeftTriangleFull<Light, Transparent>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTriangleClipVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTriangleClipLeftAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTriangleClipRightAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLower(uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderTriangleLower<Light, Transparent>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipVertical(const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		if (width > clipLeft)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src + clipLeft, width - clipLeft, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipRightAndVertical(int_fast16_t clipRight, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		if (width > skip)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - skip, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, Transparent>(dst, dstPitch, src, tbl);
	RenderTriangleUpper<Light, Transparent>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderRightTriangleLowerClipVertical<Light, Transparent>(clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderRightTriangleLowerClipLeftAndVertical<Light, Transparent>(clipLeft, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		if (width <= clipLeft)
			break;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src + clipLeft, width - clipLeft, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipRightAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderRightTriangleLowerClipRightAndVertical<Light, Transparent>(clipRight, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width > skip ? width - skip : 0, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangle(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderRightTriangleFull<Light, Transparent>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTriangleClipVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTriangleClipLeftAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTriangleClipRightAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalf(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	if constexpr (Mask == MaskType::Left || Mask == MaskType::Right) {
		// The first line is always fully opaque.
		// We handle it specially to avoid calling the blitter with width=0.
		const uint8_t *srcEnd = src + Width * TrapezoidUpperHeight;
		RenderLineOpaque<Light>(dst, src, Width, tbl);
		src += Width;
		dst -= dstPitch;
		uint8_t prefixWidth = (PrefixIncrement<Mask> < 0 ? 32 : 0) + PrefixIncrement<Mask>;
		do {
			RenderLineTransparentAndOpaque<Light, /*OpaqueFirst=*/Mask == MaskType::Right>(dst, src, prefixWidth, Width, tbl);
			prefixWidth += PrefixIncrement<Mask>;
			src += Width;
			dst -= dstPitch;
		} while (src != srcEnd);
	} else { // Mask == MaskType::Solid || Mask == MaskType::Transparent
		const uint8_t *srcEnd = src + Width * TrapezoidUpperHeight;
		do {
			RenderLineTransparentOrOpaque<Light, /*Transparent=*/Mask == MaskType::Transparent>(dst, src, Width, tbl);
			src += Width;
			dst -= dstPitch;
		} while (src != srcEnd);
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipVertical(const Clip &clip, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<Mask>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, Mask>(dst, src, Width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement<Mask>;
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipLeftAndVertical(const Clip &clip, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<Mask>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, Mask>(dst, src, clip.width, tbl, prefix - clip.left);
		src += Width;
		prefix += PrefixIncrement<Mask>;
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipRightAndVertical(const Clip &clip, const DiamondClipY &clipY, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<Mask>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, Mask>(dst, src, clip.width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement<Mask>;
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, /*Transparent=*/Mask == MaskType::Transparent>(dst, dstPitch, src, tbl);
	dst += XStep;
	RenderTrapezoidUpperHalf<Light, Mask>(dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	RenderTrapezoidUpperHalfClipVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipLeftAndVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clip.left, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clip.left;
	dst += XStep + clip.left;
	RenderTrapezoidUpperHalfClipLeftAndVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipRightAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipRightAndVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clip.right, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	RenderTrapezoidUpperHalfClipRightAndVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoid(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderLeftTrapezoidFull<Light, Mask>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTrapezoidClipVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTrapezoidClipLeftAndVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidClipRightAndVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidFull(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, /*Transparent=*/Mask == MaskType::Transparent>(dst, dstPitch, src, tbl);
	RenderTrapezoidUpperHalf<Light, Mask>(dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	RenderTrapezoidUpperHalfClipVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipLeftAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipLeftAndVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clip.left, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clip.left;
	RenderTrapezoidUpperHalfClipLeftAndVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipRightAndVertical(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipRightAndVertical<Light, /*Transparent=*/Mask == MaskType::Transparent>(clip.right, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	RenderTrapezoidUpperHalfClipRightAndVertical<Light, Mask>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoid(uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderRightTrapezoidFull<Light, Mask>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTrapezoidClipVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTrapezoidClipLeftAndVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidClipRightAndVertical<Light, Mask>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileType(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::Square:
		RenderSquare<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, Transparent ? MaskType::Transparent : MaskType::Solid>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTriangle:
		RenderLeftTriangle<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTriangle:
		RenderRightTriangle<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, Transparent ? MaskType::Transparent : MaskType::Solid>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, Transparent ? MaskType::Transparent : MaskType::Solid>(dst, dstPitch, src, tbl, clip);
		break;
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquare(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, Mask>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, Mask>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <LightType Light, MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquare(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, Mask>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, Mask>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquareDispatch(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (*GetOptions().Graphics.perPixelLighting) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::PerPixel, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyDark(tbl)) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyDark, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyLit(tbl)) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyLit, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidOrTransparentSquare<LightType::PartiallyLit, Mask>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <MaskType Mask>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquareDispatch(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (*GetOptions().Graphics.perPixelLighting) {
		RenderRightTrapezoidOrTransparentSquare<LightType::PerPixel, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyDark(tbl)) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyDark, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyLit(tbl)) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyLit, Mask>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidOrTransparentSquare<LightType::PartiallyLit, Mask>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <bool Transparent>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileDispatch(TileType tile, uint8_t *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (*GetOptions().Graphics.perPixelLighting) {
		RenderTileType<LightType::PerPixel, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyDark(tbl)) {
		RenderTileType<LightType::FullyDark, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	} else if (IsFullyLit(tbl)) {
		RenderTileType<LightType::FullyLit, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderTileType<LightType::PartiallyLit, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	}
}

} // namespace

#ifdef DUN_RENDER_STATS
ankerl::unordered_dense::map<DunRenderType, size_t, DunRenderTypeHash> DunRenderStats;

std::string_view TileTypeToString(TileType tileType)
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

std::string_view MaskTypeToString(MaskType maskType)
{
	// clang-format off
	switch (maskType) {
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

DVL_ATTRIBUTE_HOT void RenderTileFrame(const Surface &out, const Point &position, TileType tile, const uint8_t *src, int_fast16_t height,
    MaskType maskType, const uint8_t *tbl)
{
#ifdef DEBUG_RENDER_OFFSET_X
	position.x += DEBUG_RENDER_OFFSET_X;
#endif
#ifdef DEBUG_RENDER_OFFSET_Y
	position.y += DEBUG_RENDER_OFFSET_Y;
#endif
	const Clip clip = CalculateClip(position.x, position.y, DunFrameWidth, height, out);
	if (clip.width <= 0 || clip.height <= 0) return;

	uint8_t *dst = out.at(static_cast<int>(position.x + clip.left), static_cast<int>(position.y - clip.bottom));
	const uint16_t dstPitch = out.pitch();

#ifdef DUN_RENDER_STATS
	++DunRenderStats[DunRenderType { tile, maskType }];
#endif

	switch (maskType) {
	case MaskType::Solid:
		RenderTileDispatch</*Transparent=*/false>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Transparent:
		RenderTileDispatch</*Transparent=*/true>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Left:
		RenderLeftTrapezoidOrTransparentSquareDispatch<MaskType::Left>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Right:
		RenderRightTrapezoidOrTransparentSquareDispatch<MaskType::Right>(tile, dst, dstPitch, src, tbl, clip);
		break;
	}

#ifdef DEBUG_STR
	const auto [debugStr, flags] = GetTileDebugStr(tile);
	DrawString(out, debugStr, Rectangle { Point { position.x + 2, position.y - 29 }, Size { 28, 28 } }, { .flags = flags });
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
	const Clip clipLeft = CalculateClip(sx, sy, Width, TriangleHeight, out);
	if (clipLeft.height <= 0) return;
	Clip clipRight;
	clipRight.top = clipLeft.top;
	clipRight.bottom = clipLeft.bottom;
	clipRight.left = (sx + Width) < 0 ? -(sx + Width) : 0;
	clipRight.right = sx + Width + Width > out.w() ? sx + Width + Width - out.w() : 0;
	clipRight.width = Width - clipRight.left - clipRight.right;
	clipRight.height = clipLeft.height;

	const uint16_t dstPitch = out.pitch();
	if (clipLeft.width > 0) {
		uint8_t *dst = out.at(static_cast<int>(sx + clipLeft.left), static_cast<int>(sy - clipLeft.bottom));
		RenderLeftTriangle<LightType::FullyDark, /*Transparent=*/false>(dst, dstPitch, nullptr, nullptr, clipLeft);
	}
	if (clipRight.width > 0) {
		uint8_t *dst = out.at(static_cast<int>(sx + Width + clipRight.left), static_cast<int>(sy - clipRight.bottom));
		RenderRightTriangle<LightType::FullyDark, /*Transparent=*/false>(dst, dstPitch, nullptr, nullptr, clipRight);
	}
}

} // namespace devilution
