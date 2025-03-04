/**
 * @file clx_render.cpp
 *
 * CL2 rendering.
 */
#include "clx_render.hpp"

#include <algorithm>
#include <cstdint>

#include "engine/point.hpp"
#include "engine/render/blit_impl.hpp"
#include "engine/surface.hpp"
#include "utils/attributes.h"
#include "utils/clx_decode.hpp"
#include "utils/static_vector.hpp"

#ifdef DEBUG_CLX
#include <fmt/format.h>

#include "utils/str_cat.hpp"
#endif

namespace devilution {
namespace {

/**
 * CL2 is similar to CEL, with the following differences:
 *
 * 1. Transparent runs can cross line boundaries.
 * 2. Control bytes are different, and the [0x80, 0xBE] control byte range
 *    indicates a fill-N command.
 */

struct BlitCommandInfo {
	const uint8_t *srcEnd;
	unsigned length;
};

BlitCommandInfo ClxBlitInfo(const uint8_t *src)
{
	const uint8_t control = *src;
	if (!IsClxOpaque(control))
		return { src + 1, control };
	if (IsClxOpaqueFill(control)) {
		const uint8_t width = GetClxOpaqueFillWidth(control);
		return { src + 2, width };
	}
	const uint8_t width = GetClxOpaquePixelsWidth(control);
	return { src + 1 + width, width };
}

struct ClipX {
	int_fast16_t left;
	int_fast16_t right;
	int_fast16_t width;
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT ClipX CalculateClipX(int_fast16_t x, std::size_t w, const Surface &out)
{
	ClipX clip;
	clip.left = static_cast<int_fast16_t>(x < 0 ? -x : 0);
	clip.right = static_cast<int_fast16_t>(static_cast<int_fast16_t>(x + w) > out.w() ? x + w - out.w() : 0);
	clip.width = static_cast<int_fast16_t>(w - clip.left - clip.right);
	return clip;
}

// Source data for rendering backwards: first line of input -> last line of output.
struct RenderSrc {
	const uint8_t *begin;
	const uint8_t *end;
	uint_fast16_t width;
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT const uint8_t *SkipRestOfLineWithOverrun(
    const uint8_t *src, int_fast16_t srcWidth, SkipSize &skipSize)
{
	int_fast16_t remainingWidth = srcWidth - skipSize.xOffset;
	while (remainingWidth > 0) {
		const auto [srcEnd, length] = ClxBlitInfo(src);
		src = srcEnd;
		remainingWidth -= length;
	}
	skipSize = GetSkipSize(remainingWidth, srcWidth);
	return src;
}

// Returns the horizontal overrun.
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT int_fast16_t SkipLinesForRenderBackwardsWithOverrun(
    Point &position, RenderSrc &src, int_fast16_t dstHeight)
{
	SkipSize skipSize { 0, 0 };
	while (position.y >= dstHeight && src.begin != src.end) {
		src.begin = SkipRestOfLineWithOverrun(
		    src.begin, static_cast<int_fast16_t>(src.width), skipSize);
		position.y -= static_cast<int>(skipSize.wholeLines);
	}
	return skipSize.xOffset;
}

template <typename BlitFn>
void DoRenderBackwardsClipY(
    const Surface &out, Point position, RenderSrc src, BlitFn &&blitFn)
{
	// Skip the bottom clipped lines.
	int_fast16_t xOffset = SkipLinesForRenderBackwardsWithOverrun(position, src, out.h());
	if (src.begin >= src.end)
		return;

	auto *dst = &out[position];
	const auto *dstBegin = out.begin();
	const int dstPitch = out.pitch();
	while (src.begin != src.end && dst >= dstBegin) {
		auto remainingWidth = static_cast<int_fast16_t>(src.width) - xOffset;
		dst += xOffset;
		while (remainingWidth > 0) {
			uint8_t v = *src.begin++;
			if (IsClxOpaque(v)) {
				if (IsClxOpaqueFill(v)) {
					v = GetClxOpaqueFillWidth(v);
					const uint8_t color = *src.begin++;
					blitFn(v, color, dst);
				} else {
					v = GetClxOpaquePixelsWidth(v);
					blitFn(v, dst, src.begin);
					src.begin += v;
				}
			}
			dst += v;
			remainingWidth -= v;
		}

		const SkipSize skipSize = GetSkipSize(remainingWidth, static_cast<int_fast16_t>(src.width));
		xOffset = skipSize.xOffset;
		dst -= skipSize.wholeLines * dstPitch + src.width - remainingWidth;
	}
}

template <typename BlitFn>
void DoRenderBackwardsClipXY(
    const Surface &out, Point position, RenderSrc src, ClipX clipX, BlitFn &&blitFn)
{
	// Skip the bottom clipped lines.
	int_fast16_t xOffset = SkipLinesForRenderBackwardsWithOverrun(position, src, out.h());
	if (src.begin >= src.end)
		return;

	position.x += static_cast<int>(clipX.left);
	auto *dst = &out[position];
	const auto *dstBegin = out.begin();
	const int dstPitch = out.pitch();

	while (src.begin != src.end && dst >= dstBegin) {
		// Skip initial src if clipping on the left.
		// Handles overshoot, i.e. when the RLE segment goes into the unclipped area.
		int_fast16_t remainingWidth = clipX.width;
		int_fast16_t remainingLeftClip = clipX.left - xOffset;
		if (remainingLeftClip < 0) {
			dst += std::min<unsigned>(remainingWidth, -remainingLeftClip);
			remainingWidth += remainingLeftClip;
		}
		while (remainingLeftClip > 0) {
			auto [srcEnd, length] = ClxBlitInfo(src.begin);
			if (static_cast<int_fast16_t>(length) > remainingLeftClip) {
				const uint8_t control = *src.begin;
				const auto overshoot = static_cast<int>(length - remainingLeftClip);
				length = std::min<unsigned>(remainingWidth, overshoot);
				if (IsClxOpaque(control)) {
					if (IsClxOpaqueFill(control)) {
						blitFn(length, src.begin[1], dst);
					} else {
						blitFn(length, dst, src.begin + 1 + remainingLeftClip);
					}
				}
				dst += length;
				remainingWidth -= overshoot;
				src.begin = srcEnd;
				break;
			}
			src.begin = srcEnd;
			remainingLeftClip -= length;
		}
		while (remainingWidth > 0) {
			auto [srcEnd, length] = ClxBlitInfo(src.begin);
			const uint8_t control = *src.begin;
			const unsigned unclippedLength = length;
			length = std::min<unsigned>(remainingWidth, length);
			if (IsClxOpaque(control)) {
				if (IsClxOpaqueFill(control)) {
					blitFn(length, src.begin[1], dst);
				} else {
					blitFn(length, dst, src.begin + 1);
				}
			}
			src.begin = srcEnd;
			dst += length;
			remainingWidth -= unclippedLength; // result can be negative
		}

		// `remainingWidth` can be negative, in which case it is the amount of pixels
		// that the source has overran the line.
		remainingWidth += clipX.right;
		SkipSize skipSize;
		if (remainingWidth > 0) {
			skipSize.xOffset = static_cast<int_fast16_t>(src.width) - remainingWidth;
			src.begin = SkipRestOfLineWithOverrun(
			    src.begin, static_cast<int_fast16_t>(src.width), skipSize);
		} else {
			skipSize = GetSkipSize(remainingWidth, static_cast<int_fast16_t>(src.width));
		}
		xOffset = skipSize.xOffset;
		dst -= dstPitch * skipSize.wholeLines + clipX.width;
	}
}

template <typename BlitFn>
void DoRenderBackwards(
    const Surface &out, Point position, const uint8_t *src, size_t srcSize,
    unsigned srcWidth, unsigned srcHeight, BlitFn &&blitFn)
{
	if (position.y < 0 || position.y + 1 >= static_cast<int>(out.h() + srcHeight))
		return;
	const ClipX clipX = CalculateClipX(position.x, srcWidth, out);
	if (clipX.width <= 0)
		return;
	RenderSrc srcForBackwards { src, src + srcSize, static_cast<uint_fast16_t>(srcWidth) };
	if (static_cast<std::size_t>(clipX.width) == srcWidth) {
		DoRenderBackwardsClipY(
		    out, position, srcForBackwards, std::forward<BlitFn>(blitFn));
	} else {
		DoRenderBackwardsClipXY(
		    out, position, srcForBackwards, clipX, std::forward<BlitFn>(blitFn));
	}
}

constexpr size_t MaxOutlinePixels = 4096;
constexpr size_t MaxOutlineSpriteWidth = 253;
using OutlinePixels = StaticVector<PointOf<uint8_t>, MaxOutlinePixels>;
using OutlineRowSolidRuns = StaticVector<std::pair<uint8_t, uint8_t>, MaxOutlineSpriteWidth / 2 + 1>;

struct OutlinePixelsCacheEntry {
	OutlinePixels outlinePixels;
	const void *spriteData = nullptr;
	bool skipColorIndexZero;
};
OutlinePixelsCacheEntry OutlinePixelsCache;

void PopulateOutlinePixelsForRow(
    const OutlineRowSolidRuns &runs,
    const bool *DVL_RESTRICT below,
    bool *DVL_RESTRICT cur,
    bool *DVL_RESTRICT above,
    uint8_t y,
    OutlinePixels &result)
{
	DVL_ASSUME(!runs.empty());
	for (const auto &[begin, end] : runs) {
		if (!cur[static_cast<uint8_t>(begin - 1)]) {
			result.emplace_back(static_cast<uint8_t>(begin - 1), y);
			cur[static_cast<uint8_t>(begin - 1)] = true;
		}
		if (!cur[end]) {
			result.emplace_back(end, y);
			cur[end] = true;
		}
		for (uint8_t x = begin; x < end; ++x) {
			if (!below[x]) {
				result.emplace_back(x, static_cast<uint8_t>(y + 1));
			}
			if (!above[x]) {
				result.emplace_back(x, static_cast<uint8_t>(y - 1));
				above[x] = true;
			}
		}
	}
}

void AppendOutlineRowSolidRuns(uint8_t x, uint8_t w, OutlineRowSolidRuns &solidRuns)
{
	if (solidRuns.empty() || solidRuns.back().second != x) {
		solidRuns.emplace_back(x, x + w);
	} else {
		solidRuns.back().second = static_cast<uint8_t>(x + w);
	}
}

template <bool SkipColorIndexZero>
void GetOutline(ClxSprite sprite, OutlinePixels &result) // NOLINT(readability-function-cognitive-complexity)
{
	const unsigned width = sprite.width();
	assert(width < MaxOutlineSpriteWidth);

	int x = 1;
	auto y = static_cast<uint8_t>(sprite.height());

	bool rows[3][MaxOutlineSpriteWidth + 2] = { {}, {}, {} };
	bool *rowAbove = rows[0];
	bool *row = rows[1];
	bool *rowBelow = rows[2];

	OutlineRowSolidRuns solidRuns[2];
	OutlineRowSolidRuns *solidRunAbove = &solidRuns[0];
	OutlineRowSolidRuns *solidRun = &solidRuns[1];

	const uint8_t *src = sprite.pixelData();
	const uint8_t *const end = src + sprite.pixelDataSize();
	while (src < end) {
		while (x <= static_cast<int>(width)) {
			const auto v = static_cast<uint8_t>(*src++);
			uint8_t w;
			if (IsClxOpaque(v)) {
				if constexpr (SkipColorIndexZero) {
					if (IsClxOpaqueFill(v)) {
						w = GetClxOpaqueFillWidth(v);
						const auto color = static_cast<uint8_t>(*src++);
						if (color != 0) {
							AppendOutlineRowSolidRuns(x, w, *solidRunAbove);
						}
					} else {
						w = GetClxOpaquePixelsWidth(v);
						bool prevZero = solidRunAbove->empty() || solidRunAbove->back().second != x;
						for (unsigned i = 0; i < w; ++i) {
							const auto color = static_cast<uint8_t>(src[i]);
							if (color == 0) {
								if (!prevZero) ++solidRunAbove->back().second;
								prevZero = true;
							} else {
								if (prevZero) solidRunAbove->emplace_back(x + i, x + i);
								++solidRunAbove->back().second;
								prevZero = false;
							}
						}
						src += w;
					}
				} else {
					if (IsClxOpaqueFill(v)) {
						w = GetClxOpaqueFillWidth(v);
						++src;
					} else {
						w = GetClxOpaquePixelsWidth(v);
						src += w;
					}
					AppendOutlineRowSolidRuns(x, w, *solidRunAbove);
				}
			} else {
				w = v;
			}
			x += w;
		}

		for (const auto &[xBegin, xEnd] : *solidRunAbove) {
			std::fill(rowAbove + xBegin, rowAbove + xEnd, true);
		}

		if (!solidRun->empty()) {
			PopulateOutlinePixelsForRow(*solidRun, rowBelow, row, rowAbove, static_cast<uint8_t>(y + 1), result);
		}

		// (0, 1, 2) => (2, 0, 1)
		std::swap(row, rowBelow);
		std::swap(row, rowAbove);
		std::fill_n(rowAbove, width, false);

		std::swap(solidRunAbove, solidRun);
		solidRunAbove->clear();

		if (x > static_cast<int>(width + 1)) {
			// Transparent overrun.
			const unsigned numWholeTransparentLines = (x - 1) / width;
			if (numWholeTransparentLines > 1) {
				if (!solidRun->empty()) {
					PopulateOutlinePixelsForRow(*solidRun, rowBelow, row, rowAbove, y, result);
				}
				solidRun->clear();
				std::fill_n(row, width, false);
			}
			if (numWholeTransparentLines > 2) std::fill_n(rowBelow, width, false);
			y -= static_cast<uint8_t>(numWholeTransparentLines);
			x = static_cast<int>((x - 1) % width) + 1;
		} else {
			--y;
			x = 1;
		}
	}
	std::fill_n(rowAbove, width, false);
	if (!solidRun->empty()) {
		PopulateOutlinePixelsForRow(*solidRun, rowBelow, row, rowAbove, static_cast<uint8_t>(y + 1), result);
	}
}

template <bool SkipColorIndexZero>
void UpdateOutlinePixelsCache(ClxSprite sprite)
{
	if (OutlinePixelsCache.spriteData == sprite.pixelData()
	    && OutlinePixelsCache.skipColorIndexZero == SkipColorIndexZero) {
		return;
	}
	OutlinePixelsCache.skipColorIndexZero = SkipColorIndexZero;
	OutlinePixelsCache.spriteData = sprite.pixelData();
	OutlinePixelsCache.outlinePixels.clear();
	GetOutline<SkipColorIndexZero>(sprite, OutlinePixelsCache.outlinePixels);
}

template <bool SkipColorIndexZero>
void RenderClxOutline(const Surface &out, Point position, ClxSprite sprite, uint8_t color)
{
	UpdateOutlinePixelsCache<SkipColorIndexZero>(sprite);
	--position.x;
	position.y -= sprite.height();
	if (position.x >= 0 && position.x + sprite.width() + 2 < out.w()
	    && position.y >= 0 && position.y + sprite.height() + 2 < out.h()) {
		for (const auto &[x, y] : OutlinePixelsCache.outlinePixels) {
			*out.at(position.x + x, position.y + y) = color;
		}
	} else {
		for (const auto &[x, y] : OutlinePixelsCache.outlinePixels) {
			out.SetPixel(Point(position.x + x, position.y + y), color);
		}
	}
}

void ClxApplyTrans(ClxSprite sprite, const uint8_t *trn)
{
	// A bit of a hack but this is the only place in the code where we need mutable sprites.
	auto *dst = const_cast<uint8_t *>(sprite.pixelData());
	uint16_t remaining = sprite.pixelDataSize();
	while (remaining != 0) {
		uint8_t val = *dst++;
		--remaining;
		if (!IsClxOpaque(val))
			continue;
		if (IsClxOpaqueFill(val)) {
			--remaining;
			*dst = trn[*dst];
			dst++;
		} else {
			val = GetClxOpaquePixelsWidth(val);
			remaining -= val;
			while (val-- > 0) {
				*dst = trn[*dst];
				dst++;
			}
		}
	}
}

} // namespace

void ClxApplyTrans(ClxSpriteList list, const uint8_t *trn)
{
	for (ClxSprite sprite : list) {
		ClxApplyTrans(sprite, trn);
	}
}

void ClxApplyTrans(ClxSpriteSheet sheet, const uint8_t *trn)
{
	for (ClxSpriteList list : sheet) {
		ClxApplyTrans(list, trn);
	}
}

bool IsPointWithinClx(Point position, ClxSprite clx)
{
	const uint8_t *src = clx.pixelData();
	const uint8_t *end = src + clx.pixelDataSize();
	const uint16_t width = clx.width();

	int xCur = 0;
	int yCur = clx.height() - 1;
	while (src < end) {
		if (yCur != position.y) {
			SkipSize skipSize {};
			skipSize.xOffset = xCur;
			src = SkipRestOfLineWithOverrun(src, width, skipSize);
			yCur -= skipSize.wholeLines;
			xCur = skipSize.xOffset;
			if (yCur < position.y)
				return false;
			continue;
		}

		while (xCur < width) {
			uint8_t val = *src++;
			if (!IsClxOpaque(val)) {
				// ignore transparent
				xCur += val;
				if (xCur > position.x)
					return false;
				continue;
			}

			if (IsClxOpaqueFill(val)) {
				val = GetClxOpaqueFillWidth(val);
				uint8_t color = *src++;
				if (xCur <= position.x && position.x < xCur + val)
					return color != 0; // ignore shadows
				xCur += val;
			} else {
				val = GetClxOpaquePixelsWidth(val);
				for (uint8_t pixel = 0; pixel < val; pixel++) {
					uint8_t color = *src++;
					if (xCur == position.x)
						return color != 0; // ignore shadows
					xCur++;
				}
			}
		}

		return false;
	}

	return false;
}

std::pair<int, int> ClxMeasureSolidHorizontalBounds(ClxSprite clx)
{
	const uint8_t *src = clx.pixelData();
	const uint8_t *end = src + clx.pixelDataSize();
	const uint16_t width = clx.width();

	int xBegin = width;
	int xEnd = 0;
	int xCur = 0;
	while (src < end) {
		while (xCur < width) {
			auto val = *src++;
			if (!IsClxOpaque(val)) {
				xCur += val;
				continue;
			}
			if (IsClxOpaqueFill(val)) {
				val = GetClxOpaqueFillWidth(val);
				++src;
			} else {
				val = GetClxOpaquePixelsWidth(val);
				src += val;
			}
			xBegin = std::min(xBegin, xCur);
			xCur += val;
			xEnd = std::max(xEnd, xCur);
		}
		while (xCur >= width)
			xCur -= width;
		if (xBegin == 0 && xEnd == width)
			break;
	}
	return { xBegin, xEnd };
}

#ifdef DEBUG_CLX
std::string ClxDescribe(ClxSprite clx)
{
	std::string out = StrCat(
	    "CLX sprite: ", clx.width(), "x", clx.height(), " pixelDataSize=", clx.pixelDataSize(),
	    "b\n\n"
	    "command | width | bytes | color(s)\n"
	    "--------|------:|------:|---------\n");
	const uint8_t *src = clx.pixelData();
	const uint8_t *end = src + clx.pixelDataSize();
	while (src < end) {
		const uint8_t control = *src++;
		if (IsClxOpaque(control)) {
			if (IsClxOpaqueFill(control)) {
				const uint8_t length = GetClxOpaqueFillWidth(control);
				out.append(fmt::format("Fill    | {:>5} | {:>5} | {}\n", length, 2, src[1]));
				++src;
			} else {
				const uint8_t length = GetClxOpaquePixelsWidth(control);
				out.append(fmt::format("Pixels  | {:>5} | {:>5} | {}\n", length, length + 1, fmt::join(src + 1, src + 1 + length, " ")));
				src += length;
			}
		} else {
			out.append(fmt::format("Transp. | {:>5} | {:>5} |\n", control, 1));
		}
	}
	return out;
}
#endif // DEBUG_CLX

void ClxDraw(const Surface &out, Point position, ClxSprite clx)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitDirect {});
}

void ClxDrawTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitWithMap { trn });
}

void ClxDrawWithLightmap(const Surface &out, Point position, ClxSprite clx, const Lightmap &lightmap)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitWithLightmap { lightmap });
}

void ClxDrawBlended(const Surface &out, Point position, ClxSprite clx)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitBlended {});
}

void ClxDrawBlendedTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitBlendedWithMap { trn });
}

void ClxDrawBlendedWithLightmap(const Surface &out, Point position, ClxSprite clx, const Lightmap &lightmap)
{
	DoRenderBackwards(out, position, clx.pixelData(), clx.pixelDataSize(), clx.width(), clx.height(), BlitBlendedWithLightmap { lightmap });
}

void ClxDrawOutline(const Surface &out, uint8_t col, Point position, ClxSprite clx)
{
	RenderClxOutline</*SkipColorIndexZero=*/false>(out, position, clx, col);
}

void ClxDrawOutlineSkipColorZero(const Surface &out, uint8_t col, Point position, ClxSprite clx)
{
	RenderClxOutline</*SkipColorIndexZero=*/true>(out, position, clx, col);
}

void ClearClxDrawCache()
{
	OutlinePixelsCache.spriteData = nullptr;
}

} // namespace devilution
