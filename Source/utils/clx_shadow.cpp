#include "utils/clx_shadow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "appfat.h"
#include "engine/shadow_fix.hpp"
#include "utils/clx_decode.hpp"
#include "utils/clx_encode.hpp"
#include "utils/endian_write.hpp"

namespace devilution {

namespace {

enum PixelFlags : uint8_t {
	Opaque = 1 << 0,
	/** Part of the sprite itself. */
	Body = 1 << 1,
	/** Covered by the shadow of a body pixel. */
	Shadow = 1 << 2,
	/** A black pixel that is neither covered by a shadow nor plausibly a body pixel. */
	Orphan = 1 << 3,
};

/** The columns `[begin, end)` of a row. Empty if `begin >= end`. */
struct Extent {
	uint16_t begin;
	uint16_t end;

	void include(int from, int to)
	{
		if (from < begin) begin = static_cast<uint16_t>(from);
		if (to > end) end = static_cast<uint16_t>(to);
	}
};

/**
 * A decoded sprite. Rows are stored top-down.
 *
 * Most of a sprite is empty, so the rows that have pixels and the extent of the pixels
 * of each row are kept, and everything works on those instead of on whole rows.
 */
struct Frame {
	int width = 0;
	int height = 0;
	/** Only valid for opaque pixels. */
	std::vector<uint8_t> colors;
	std::vector<uint8_t> flags;
	/** The extent of the opaque pixels of each row. */
	std::vector<Extent> opaque;
	/** An extent that contains the pixels of each row that have the `Shadow` flag. */
	std::vector<Extent> shadow;
	/** The indices of the black pixels, in order. Filled by `SeparateShadow` and `KeepAll`. */
	std::vector<uint32_t> blackPixels;
	/** Scratch space of `SeparateShadow`: columns of a row. */
	std::vector<int> uncovered;
	/** The first and the last row that have an opaque pixel. `bottom` is -1 if there are none. */
	int top = 0;
	int bottom = -1;

	[[nodiscard]] size_t index(int x, int y) const
	{
		return static_cast<size_t>(y) * width + x;
	}

	[[nodiscard]] bool isBlack(size_t i) const
	{
		return (flags[i] & Opaque) != 0 && colors[i] == 0;
	}

	void setShadow(int x, int y)
	{
		flags[index(x, y)] |= Shadow;
		shadow[y].include(x, x + 1);
	}
};

void DecodeFrame(ClxSprite sprite, Frame &frame)
{
	const int width = sprite.width();
	const int height = sprite.height();
	frame.width = width;
	frame.height = height;
	const size_t numPixels = static_cast<size_t>(width) * height;
	frame.colors.resize(numPixels);
	frame.flags.assign(numPixels, 0);
	const Extent empty { static_cast<uint16_t>(width), 0 };
	frame.opaque.assign(height, empty);
	frame.shadow.assign(height, empty);
	frame.top = height;
	frame.bottom = -1;
	frame.blackPixels.clear();

	// CLX pixel data is stored bottom-up. Transparent runs can continue on the next row.
	// Opaque runs must not, but a run that does is taken to continue there too, like the
	// run of the original image that it was made from.
	int x = 0;
	int y = height - 1;
	// Writes an opaque run, either the given pixels or `length` pixels of the fill color.
	const auto opaqueRun = [&](unsigned length, const uint8_t *pixels, uint8_t fillColor) {
		while (length != 0 && y >= 0) {
			const unsigned chunk = std::min<unsigned>(length, static_cast<unsigned>(width - x));
			const size_t i = frame.index(x, y);
			if (pixels != nullptr) {
				memcpy(&frame.colors[i], pixels, chunk);
				pixels += chunk;
			} else {
				memset(&frame.colors[i], fillColor, chunk);
			}
			memset(&frame.flags[i], Opaque, chunk);
			frame.opaque[y].include(x, x + static_cast<int>(chunk));
			if (y > frame.bottom) frame.bottom = y;
			if (y < frame.top) frame.top = y;
			x += static_cast<int>(chunk);
			length -= chunk;
			if (x == width) {
				x = 0;
				--y;
			}
		}
	};
	const uint8_t *src = sprite.pixelData();
	const uint8_t *end = src + sprite.pixelDataSize();
	while (src < end) {
		const uint8_t control = *src++;
		if (!IsClxOpaque(control)) {
			x += control;
			if (x >= width) {
				y -= x / width;
				x %= width;
			}
		} else if (IsClxOpaqueFill(control)) {
			opaqueRun(GetClxOpaqueFillWidth(control), nullptr, *src++);
		} else {
			const unsigned length = GetClxOpaquePixelsWidth(control);
			opaqueRun(length, src, 0);
			src += length;
		}
	}
}

/**
 * @brief The shadow function for a row: where the pixels of the row cast their shadow.
 */
struct RowShadow {
	/** Whether the pixels of the row cast a shadow inside the frame. */
	bool casts;
	/** The row that the shadow falls on. */
	int y;
	/** The number of pixels that the shadow is shifted to the left. */
	int shift;
};

RowShadow ShadowOfRow(const Frame &frame, int baseline, int y)
{
	if (y > baseline) return { false, 0, 0 };
	const int v = (baseline - y) / 3;
	const int shadowY = baseline - 2 - v;
	return { shadowY >= 0 && shadowY < frame.height, shadowY, (3 * v) / 4 };
}

/**
 * @brief Sets the `Body`, `Shadow` and `Orphan` flags of the frame.
 */
void SeparateShadow(Frame &frame, int baseline)
{
	// Everything that is not black is certainly not a shadow.
	// The black pixels are collected for the second pass.
	for (int y = frame.top; y <= frame.bottom; ++y) {
		const Extent extent = frame.opaque[y];
		if (extent.begin >= extent.end) continue;
		const RowShadow rowShadow = ShadowOfRow(frame, baseline, y);
		const size_t row = frame.index(0, y);
		// The shadow of the pixels to the left of `shift` falls outside of the frame.
		const size_t shadowRow = rowShadow.casts ? frame.index(0, rowShadow.y) : 0;
		const int shift = rowShadow.casts ? rowShadow.shift : frame.width;
		for (int x = extent.begin; x < extent.end; ++x) {
			if ((frame.flags[row + x] & Opaque) == 0) continue;
			if (frame.colors[row + x] == 0) {
				frame.blackPixels.push_back(static_cast<uint32_t>(row + x));
				continue;
			}
			frame.flags[row + x] |= Body;
			if (x >= shift) frame.flags[shadowRow + x - shift] |= Shadow;
		}
		if (rowShadow.casts && extent.end > shift) frame.shadow[rowShadow.y].include(std::max<int>(extent.begin, shift) - shift, extent.end - shift);
	}

	// A black pixel that is not in the shadow of the pixels above it must be a body pixel,
	// and so it casts a shadow too. Shadows fall downwards, so one top-down pass settles them.
	//
	// The lowest rows are the exception. The pixels of the rows 2 and 3 above the baseline shadow
	// themselves, which changes nothing. The pixels of the baseline row and of the row above it
	// cast their shadow upwards, onto the row 2 above the baseline. A black pixel of that row is
	// therefore judged before the black pixels underneath it are known to be body pixels:
	//
	//  - above a non-black pixel, it is in that pixel's shadow (set in the first pass)
	//    and is taken for shadow
	//  - above black pixels only, it is not in any shadow yet and is taken for a body pixel
	//
	// So the same shape gets two answers depending on the colour underneath. This is left as it
	// is: nothing in the sprite says which answer is right, and taking the pixel for shadow in the
	// second case would cut a row out of anything black that stands on the baseline, such as the
	// sole of a boot. Shadow corrections (see `shadow_fix.hpp`) are made against this behaviour.
	std::vector<int> &uncovered = frame.uncovered;
	const size_t numBlackPixels = frame.blackPixels.size();
	for (size_t next = 0; next < numBlackPixels;) {
		// The black pixels of one row.
		const int y = static_cast<int>(frame.blackPixels[next] / frame.width);
		const size_t row = frame.index(0, y);
		const size_t rowEnd = row + frame.width;
		uncovered.clear();
		for (; next < numBlackPixels && frame.blackPixels[next] < rowEnd; ++next) {
			const size_t i = frame.blackPixels[next];
			if ((frame.flags[i] & Shadow) == 0) uncovered.push_back(static_cast<int>(i - row));
		}
		if (uncovered.empty()) continue;

		const RowShadow rowShadow = ShadowOfRow(frame, baseline, y);
		for (const int x : uncovered) {
			const size_t i = row + x;
			// A pixel whose shadow would fall on a transparent pixel is not a body pixel.
			if (!rowShadow.casts || x < rowShadow.shift || (frame.flags[frame.index(x - rowShadow.shift, rowShadow.y)] & Opaque) != 0) {
				frame.flags[i] |= Body;
				continue;
			}

			// It may be the shadow of a black body pixel that we took for a shadow because it lies
			// inside the shadow of the pixels above it. Look for one in the 3 rows that shadow this row.
			const int v = baseline - 2 - y;
			const int sourceX = x + (3 * v) / 4;
			bool found = false;
			for (int sourceY = baseline - 3 * v - 2; v >= 2 && sourceY <= baseline - 3 * v && !found; ++sourceY) {
				if (sourceX >= frame.width || sourceY < 0) continue;
				const size_t source = frame.index(sourceX, sourceY);
				if (!frame.isBlack(source) || (frame.flags[source] & (Body | Orphan)) != 0) continue;
				frame.flags[source] |= Body;
				frame.setShadow(x, y);
				found = true;
			}
			if (!found) frame.flags[i] |= Orphan;
		}
		if (!rowShadow.casts) continue;
		for (const int x : uncovered) {
			if ((frame.flags[row + x] & Body) != 0 && x >= rowShadow.shift) frame.setShadow(x - rowShadow.shift, rowShadow.y);
		}
	}
}

/**
 * @brief Marks every pixel of the frame as part of the sprite, for a sprite whose shadow cannot be found by a rule.
 */
void KeepAll(Frame &frame)
{
	for (int y = frame.top; y <= frame.bottom; ++y) {
		const Extent extent = frame.opaque[y];
		const size_t row = frame.index(0, y);
		for (int x = extent.begin; x < extent.end; ++x) {
			if ((frame.flags[row + x] & Opaque) == 0) continue;
			frame.flags[row + x] |= Body;
			if (frame.colors[row + x] == 0) frame.blackPixels.push_back(static_cast<uint32_t>(row + x));
		}
	}
}

/**
 * @brief Applies the corrections of the frame (see `shadow_fix.hpp`), after its shadow was separated.
 *
 * Does nothing if the black pixels of the frame are not the ones that the corrections were made for.
 */
void ApplyShadowFix(Frame &frame, const ShadowFixFrame &fix)
{
	if (fix.guard != ShadowFixGuard(static_cast<uint16_t>(frame.width), static_cast<uint16_t>(frame.height), frame.blackPixels)) return;
	const auto forEachBlackPixel = [&frame](std::span<const uint8_t> mask, auto &&fn) {
		if (mask.empty()) return;
		ShadowMask { mask.data() }.forEachSpan([&](uint16_t rowFromBottom, ShadowSpan span) {
			const int y = frame.height - 1 - rowFromBottom;
			for (int x = span.x; x < span.x + span.width; ++x) {
				if (frame.isBlack(frame.index(x, y))) fn(x, y);
			}
		});
	};
	// A pixel that returns to the sprite stays in the mask: the mask includes the shadow underneath the sprite.
	forEachBlackPixel(fix.body, [&frame](int x, int y) {
		frame.flags[frame.index(x, y)] |= Body;
	});
	forEachBlackPixel(fix.shadow, [&frame](int x, int y) {
		frame.flags[frame.index(x, y)] &= ~(Body | Orphan);
		frame.setShadow(x, y);
	});
}

/** Whether the pixel stays in the sprite once its shadow is removed. */
bool IsKept(const Frame &frame, size_t i)
{
	const uint8_t flags = frame.flags[i];
	if ((flags & Opaque) == 0) return false;
	return frame.colors[i] != 0 || (flags & (Body | Orphan)) != 0;
}

/** Appends the shadow mask of the frame. Appends nothing if the frame has no shadow. */
void AppendShadowMask(const Frame &frame, std::vector<uint8_t> &out)
{
	constexpr uint8_t Mask = Opaque | Shadow;
	// The mask is limited to opaque pixels, so only look where a row has both.
	const auto maskExtent = [&](int y) {
		return Extent { std::max(frame.opaque[y].begin, frame.shadow[y].begin), std::min(frame.opaque[y].end, frame.shadow[y].end) };
	};
	const auto rowHasShadow = [&](int y) {
		const Extent extent = maskExtent(y);
		const size_t row = frame.index(0, y);
		for (int x = extent.begin; x < extent.end; ++x) {
			if ((frame.flags[row + x] & Mask) == Mask) return true;
		}
		return false;
	};
	int lowest = frame.bottom;
	while (lowest >= frame.top && !rowHasShadow(lowest))
		--lowest;
	if (lowest < frame.top) return;
	int highest = frame.top;
	while (highest < lowest && !rowHasShadow(highest))
		++highest;

	[[maybe_unused]] const size_t begin = out.size();
	out.push_back(static_cast<uint8_t>(frame.height - 1 - lowest));
	out.push_back(static_cast<uint8_t>(lowest - highest + 1));

	for (int y = lowest; y >= highest; --y) {
		const size_t rowBegin = out.size();
		const Extent extent = maskExtent(y);
		const size_t row = frame.index(0, y);
		int x = extent.begin;
		while (x < extent.end) {
			if ((frame.flags[row + x] & Mask) != Mask) {
				++x;
				continue;
			}
			int runEnd = x + 1;
			while (runEnd < extent.end && (frame.flags[row + runEnd] & Mask) == Mask)
				++runEnd;
			while (x < runEnd) {
				const int width = runEnd - x > MaxShadowSpanWidth ? MaxShadowSpanWidth : runEnd - x;
				out.push_back(static_cast<uint8_t>(x));
				out.push_back(static_cast<uint8_t>(width));
				x += width;
			}
		}
		// A row without shadow pixels is a span of width 0.
		if (out.size() == rowBegin) {
			out.push_back(0);
			out.push_back(0);
		}
		out.back() |= ShadowSpanEndOfRow;
	}
	// At most one span for every 2 pixels of a row.
	static_assert(ShadowMaskHeaderSize + MaxShadowMaskSpriteHeight * MaxShadowMaskSpriteWidth <= MaxShadowMaskSize);
	assert(out.size() - begin <= MaxShadowMaskSize);
}

/** Appends the frame without its shadow, with the shadow mask in the frame header. */
void AppendSpriteWithoutShadow(const Frame &frame, std::vector<uint8_t> &out)
{
	const size_t headerPos = out.size();
	out.resize(out.size() + ClxFrameHeaderSize);
	AppendShadowMask(frame, out);
	WriteLE16(&out[headerPos], static_cast<uint16_t>(out.size() - headerPos));
	WriteLE16(&out[headerPos + 2], static_cast<uint16_t>(frame.width));
	WriteLE16(&out[headerPos + 4], static_cast<uint16_t>(frame.height));

	// Opaque runs end with their row: only transparent runs can continue on the next one.
	unsigned transparentRunWidth = 0;
	for (int y = frame.height - 1; y >= 0; --y) {
		const Extent extent = frame.opaque[y];
		if (extent.begin >= extent.end) {
			transparentRunWidth += frame.width;
			continue;
		}
		const size_t rowBegin = frame.index(0, y);
		transparentRunWidth += extent.begin;
		int x = extent.begin;
		while (x < extent.end) {
			if (!IsKept(frame, rowBegin + x)) {
				++transparentRunWidth;
				++x;
				continue;
			}
			int runEnd = x + 1;
			while (runEnd < extent.end && IsKept(frame, rowBegin + runEnd))
				++runEnd;
			AppendClxTransparentRun(transparentRunWidth, out);
			transparentRunWidth = 0;
			AppendClxPixelsOrFillRun(&frame.colors[rowBegin + x], static_cast<size_t>(runEnd - x), out);
			x = runEnd;
		}
		transparentRunWidth += frame.width - extent.end;
	}
	AppendClxTransparentRun(transparentRunWidth, out);
}

/**
 * @brief Extracts the shadows of a list, appending the new list to the output.
 *
 * @param fixes the corrections of the sprite file, positioned at this list; can be null.
 * @return false if a sprite is too large.
 */
bool ExtractList(ClxSpriteList list, ShadowRule rule, ShadowFixReader *fixes, std::vector<uint8_t> &clxOut)
{
	const uint32_t numSprites = list.numSprites();
	if (fixes != nullptr && !fixes->beginList(numSprites)) fixes = nullptr;
	const size_t clxBegin = clxOut.size();
	clxOut.resize(clxBegin + 4 * (2 + static_cast<size_t>(numSprites)));
	WriteLE32(&clxOut[clxBegin], numSprites);

	Frame frame;
	int firstFrameBaseline = 0;
	for (uint32_t i = 0; i < numSprites; ++i) {
		const ClxSprite sprite = list[i];
		if (sprite.width() > MaxShadowMaskSpriteWidth || sprite.height() > MaxShadowMaskSpriteHeight) return false;
		DecodeFrame(sprite, frame);

		// The lowest row that has an opaque pixel, -1 if there is none.
		const int lowest = frame.bottom;
		if (i == 0) firstFrameBaseline = lowest;
		int baseline = rule.type == ShadowRuleType::EachFrame ? lowest : firstFrameBaseline;
		// Without a baseline (the sprite that it comes from has no pixels) there is no shadow to look for.
		if (rule.canExtractShadow() && baseline >= 0) {
			baseline += rule.baselineOffset;
			SeparateShadow(frame, baseline);
		} else {
			KeepAll(frame);
		}
		if (fixes != nullptr) {
			const ShadowFixFrame fix = fixes->nextFrame(static_cast<unsigned>(frame.width), static_cast<unsigned>(frame.height));
			if (!fix.empty()) ApplyShadowFix(frame, fix);
		}

		WriteLE32(&clxOut[clxBegin + 4 * (1 + static_cast<size_t>(i))], static_cast<uint32_t>(clxOut.size() - clxBegin));
		AppendSpriteWithoutShadow(frame, clxOut);
	}
	WriteLE32(&clxOut[clxBegin + 4 * (1 + static_cast<size_t>(numSprites))], static_cast<uint32_t>(clxOut.size() - clxBegin));
	return true;
}

std::unique_ptr<uint8_t[]> ToOwnedBuffer(const std::vector<uint8_t> &data)
{
	std::unique_ptr<uint8_t[]> result { new uint8_t[data.size()] };
	memcpy(result.get(), data.data(), data.size());
	return result;
}

} // namespace

std::optional<OwnedClxSpriteListOrSheet> ExtractClxShadows(ClxSpriteListOrSheet sprites, ShadowRule rule, std::span<const uint8_t> fixes)
{
	ShadowFixReader fixReader { fixes, static_cast<uint16_t>(sprites.isSheet() ? sprites.sheet().numLists() : 1) };
	ShadowFixReader *fixReaderPtr = fixReader.ok() ? &fixReader : nullptr;
	if (!rule.canExtractShadow() && fixReaderPtr == nullptr) return std::nullopt;

	std::vector<uint8_t> clxData;
	// The result is about as large as the sprites it is made from.
	clxData.reserve(sprites.dataSize() + sprites.dataSize() / 16);
	uint16_t numLists = 0;
	if (sprites.isSheet()) {
		const ClxSpriteSheet sheet = sprites.sheet();
		numLists = sheet.numLists();
		clxData.resize(static_cast<size_t>(numLists) * 4);
		for (uint16_t i = 0; i < numLists; ++i) {
			WriteLE32(&clxData[static_cast<size_t>(i) * 4], static_cast<uint32_t>(clxData.size()));
			if (!ExtractList(sheet[i], rule, fixReaderPtr, clxData)) return std::nullopt;
		}
	} else if (!ExtractList(sprites.list(), rule, fixReaderPtr, clxData)) {
		return std::nullopt;
	}

	return OwnedClxSpriteListOrSheet { ToOwnedBuffer(clxData), numLists };
}

} // namespace devilution
