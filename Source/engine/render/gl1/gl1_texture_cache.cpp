/**
 * @file gl1_texture_cache.cpp
 *
 * OpenGL 1.x texture cache implementation.
 */

#ifdef DEVILUTIONX_GL1_RENDERER

#include "engine/render/gl1/gl1_texture_cache.h"

#include <GL/gl.h>
#include <cstring>

#include "engine/palette.h"
#include "engine/render/dun_render.hpp"
#include "levels/dun_tile.hpp"
#include "utils/clx_decode.hpp"

namespace devilution {

namespace {

/**
 * @brief Compute the alpha value for a tile pixel based on the mask type.
 *
 * MaskType::Solid — all pixels fully opaque (255).
 * MaskType::Transparent — all pixels half-transparent (128).
 * MaskType::Right — diagonal split in the upper half: the left portion of each
 *   row is opaque and the right portion is blended. Lower half is fully opaque.
 * MaskType::Left — diagonal split in the upper half: the left portion of each
 *   row is blended and the right portion is opaque. Lower half is fully opaque.
 *
 * The upper half is defined as buffer rows 0–15 for trapezoids (which have a
 * 16-row triangle lower half followed by a 16-row rectangular upper half) and
 * for transparent squares (which are 32 rows with the upper 16 having the
 * diagonal). Triangles are only 31 rows tall and never receive Left/Right masks.
 *
 * @param maskType The transparency mask for this tile.
 * @param tileType The geometric type of the tile.
 * @param x Column within the tile (0-based, left to right).
 * @param bufferRow Row within the output texture (0 = top of texture).
 * @param height Total texture height.
 */
uint8_t GetTilePixelAlpha(MaskType maskType, TileType tileType, int x, int bufferRow, int height)
{
	constexpr uint8_t Opaque = 255;
	constexpr uint8_t Blended = 128;
	constexpr int UpperHalfRows = 16;
	constexpr int TileWidth = 32; // DunFrameWidth

	switch (maskType) {
	case MaskType::Solid:
		return Opaque;

	case MaskType::Transparent:
		return Opaque;

	case MaskType::Right: {
		// Lower half of the tile is always opaque.
		// Upper half (buffer rows 0 to UpperHalfRows-1) has a diagonal:
		//   Row 0 (top):  2 opaque pixels on the left, 30 blended on the right
		//   Row 14:       30 opaque, 2 blended
		//   Row 15:       all opaque (this is the bottom of the upper half)
		// For transparent squares the upper half starts at buffer row 0;
		// for trapezoids the upper half is also rows 0–15 because the lower
		// triangle is rows 16–31.
		//
		// Opaque prefix length = 2 * (bufferRow + 1), clamped to TileWidth.
		if (bufferRow >= UpperHalfRows)
			return Opaque;
		int opaquePrefix = 2 * (bufferRow + 1);
		if (opaquePrefix >= TileWidth)
			return Opaque;
		return x < opaquePrefix ? Opaque : Blended;
	}

	case MaskType::Left: {
		// Mirror of Right: blended pixels are on the left side.
		//   Row 0 (top):  30 blended on the left, 2 opaque on the right
		//   Row 14:       2 blended, 30 opaque
		//   Row 15:       all opaque
		if (bufferRow >= UpperHalfRows)
			return Opaque;
		int blendedPrefix = TileWidth - 2 * (bufferRow + 1);
		if (blendedPrefix <= 0)
			return Opaque;
		return x < blendedPrefix ? Blended : Opaque;
	}

	default:
		return Opaque;
	}
}

} // namespace

Gl1CachedTexture &Gl1TextureCache::GetSpriteTexture(ClxSprite clx, const uint8_t *trn)
{
	FlushIfNeeded();
	Gl1SpriteCacheKey key { clx.pixelData(), trn };
	auto it = spriteCache_.find(key);
	if (it != spriteCache_.end()) {
		Gl1CachedTexture &entry = it->second;
		if (!entry.needsPaletteUpdate || entry.paletteGeneration == paletteGeneration_) {
			return entry;
		}
		// Stale: re-decode and re-upload.
		std::vector<uint8_t> rgbaData;
		bool hasAnimatedPalette = false;
		DecodeClxToRgba(clx, trn, rgbaData, hasAnimatedPalette);
		UploadTexture(entry, clx.width(), clx.height(), rgbaData.data());
		entry.width = clx.width();
		entry.height = clx.height();
		entry.needsPaletteUpdate = hasAnimatedPalette;
		entry.paletteGeneration = paletteGeneration_;
		return entry;
	}

	Gl1CachedTexture &entry = spriteCache_[key];
	std::vector<uint8_t> rgbaData;
	bool hasAnimatedPalette = false;
	DecodeClxToRgba(clx, trn, rgbaData, hasAnimatedPalette);
	UploadTexture(entry, clx.width(), clx.height(), rgbaData.data());
	entry.width = clx.width();
	entry.height = clx.height();
	entry.needsPaletteUpdate = hasAnimatedPalette;
	entry.paletteGeneration = paletteGeneration_;
	return entry;
}

Gl1CachedTexture &Gl1TextureCache::GetTileTexture(const uint8_t *frameData,
    TileType tileType, MaskType maskType, int_fast16_t height,
    const uint8_t *tbl)
{
	FlushIfNeeded();
	Gl1TileCacheKey key { frameData, tileType, maskType, tbl };
	auto it = tileCache_.find(key);
	if (it != tileCache_.end()) {
		Gl1CachedTexture &entry = it->second;
		if (!entry.needsPaletteUpdate || entry.paletteGeneration == paletteGeneration_) {
			return entry;
		}
		// Stale: re-decode and re-upload.
		std::vector<uint8_t> rgbaData;
		uint16_t outWidth = 0;
		uint16_t outHeight = 0;
		bool hasAnimatedPalette = false;
		DecodeTileToRgba(frameData, tileType, maskType, height, tbl,
		    rgbaData, outWidth, outHeight, hasAnimatedPalette);
		UploadTexture(entry, outWidth, outHeight, rgbaData.data());
		entry.needsPaletteUpdate = hasAnimatedPalette;
		entry.paletteGeneration = paletteGeneration_;
		return entry;
	}

	Gl1CachedTexture &entry = tileCache_[key];
	std::vector<uint8_t> rgbaData;
	uint16_t outWidth = 0;
	uint16_t outHeight = 0;
	bool hasAnimatedPalette = false;
	DecodeTileToRgba(frameData, tileType, maskType, height, tbl,
	    rgbaData, outWidth, outHeight, hasAnimatedPalette);
	UploadTexture(entry, outWidth, outHeight, rgbaData.data());
	entry.needsPaletteUpdate = hasAnimatedPalette;
	entry.paletteGeneration = paletteGeneration_;
	return entry;
}

void Gl1TextureCache::WritePixel(uint8_t *rgbaOut, size_t bufferSize,
    int x, int y, uint16_t width, uint16_t height,
    uint8_t paletteIndex, const uint8_t *trn, bool &hasAnimatedPalette)
{
	if (x < 0 || x >= width || y < 0 || y >= height)
		return;

	uint8_t index = paletteIndex;
	if (trn != nullptr) {
		index = trn[index];
	}
	if (index >= 1 && index <= 31) {
		hasAnimatedPalette = true;
	}

	const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
	if (offset + 3 >= bufferSize)
		return;

	const SDL_Color &color = system_palette[index];
	rgbaOut[offset + 0] = color.r;
	rgbaOut[offset + 1] = color.g;
	rgbaOut[offset + 2] = color.b;
	rgbaOut[offset + 3] = 255;
}

void Gl1TextureCache::DecodeClxToRgba(ClxSprite clx, const uint8_t *trn,
    std::vector<uint8_t> &rgbaOut, bool &hasAnimatedPalette)
{
	const uint16_t width = clx.width();
	const uint16_t height = clx.height();
	const size_t bufferSize = static_cast<size_t>(width) * height * 4;

	rgbaOut.resize(bufferSize);
	std::memset(rgbaOut.data(), 0, rgbaOut.size());
	hasAnimatedPalette = false;

	const uint8_t *src = clx.pixelData();
	const uint8_t *srcEnd = src + clx.pixelDataSize();

	// CLX decodes bottom-to-top, left-to-right within each row.
	// We write into the output buffer in top-down order (row 0 = top of sprite).
	// So the first decoded row (bottom of sprite) maps to buffer row (height - 1).
	int currentRow = height - 1;
	int xPos = 0;

	while (src < srcEnd && currentRow >= 0) {
		const uint8_t v = *src++;

		if (!IsClxOpaque(v)) {
			// Transparent run of v pixels.
			// The buffer is already zeroed (RGBA 0,0,0,0), just advance xPos.
			xPos += v;
		} else if (IsClxOpaqueFill(v)) {
			// Opaque fill: repeat a single palette index for N pixels.
			const uint8_t fillWidth = GetClxOpaqueFillWidth(v);
			if (src >= srcEnd)
				break;
			const uint8_t paletteIndex = *src++;
			for (int i = 0; i < fillWidth; i++) {
				// bufferRow in top-down order
				const int bufferRow = currentRow;
				WritePixel(rgbaOut.data(), bufferSize,
				    xPos, bufferRow, width, height,
				    paletteIndex, trn, hasAnimatedPalette);
				xPos++;
			}
		} else {
			// Opaque pixel run: next N bytes are individual palette indices.
			const uint8_t runLength = GetClxOpaquePixelsWidth(v);
			for (int i = 0; i < runLength && src < srcEnd; i++) {
				const uint8_t paletteIndex = *src++;
				const int bufferRow = currentRow;
				WritePixel(rgbaOut.data(), bufferSize,
				    xPos, bufferRow, width, height,
				    paletteIndex, trn, hasAnimatedPalette);
				xPos++;
			}
		}

		// CLX commands can span across row boundaries.
		// When we've filled past the end of the current row, advance rows.
		while (xPos >= width && currentRow >= 0) {
			xPos -= width;
			currentRow--;
		}
	}
}

void Gl1TextureCache::WriteTilePixel(uint8_t *rgbaOut, int x, int y,
    uint16_t width, uint16_t height, uint8_t paletteIndex,
    uint8_t alpha, const uint8_t *tbl, bool &hasAnimatedPalette)
{
	if (x < 0 || x >= width || y < 0 || y >= height)
		return;
	if (tbl != nullptr)
		paletteIndex = tbl[paletteIndex];
	if (paletteIndex >= 1 && paletteIndex <= 31)
		hasAnimatedPalette = true;
	const SDL_Color &color = system_palette[paletteIndex];
	const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
	rgbaOut[offset + 0] = color.r;
	rgbaOut[offset + 1] = color.g;
	rgbaOut[offset + 2] = color.b;
	rgbaOut[offset + 3] = alpha;
}

void Gl1TextureCache::DecodeTileToRgba(const uint8_t *frameData,
    TileType tileType, MaskType maskType, int_fast16_t height,
    const uint8_t *tbl,
    std::vector<uint8_t> &rgbaOut, uint16_t &outWidth, uint16_t &outHeight,
    bool &hasAnimatedPalette)
{
	constexpr int Width = DunFrameWidth; // 32
	constexpr int XStep = 2;
	constexpr int LowerHeight = DunFrameHeight / 2;               // 16
	constexpr int TriangleUpperHeight = (DunFrameHeight / 2) - 1; // 15
	constexpr int TrapezoidUpperHeight = DunFrameHeight / 2;      // 16

	outWidth = static_cast<uint16_t>(Width);
	outHeight = static_cast<uint16_t>(height);
	hasAnimatedPalette = false;

	const size_t pixelCount = static_cast<size_t>(outWidth) * outHeight;
	rgbaOut.resize(pixelCount * 4);
	std::memset(rgbaOut.data(), 0, rgbaOut.size());

	const uint8_t *src = frameData;

	if (tileType == TileType::Square) {
		// Raw 32 bytes per row, stored bottom-to-top.
		for (int row = 0; row < outHeight; row++) {
			const int bufferRow = outHeight - 1 - row;
			for (int x = 0; x < Width; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
		}
	} else if (tileType == TileType::TransparentSquare) {
		// RLE-encoded transparent square: 32x32, bottom-to-top.
		for (int row = 0; row < outHeight; row++) {
			const int bufferRow = outHeight - 1 - row;
			int xPos = 0;
			while (xPos < Width) {
				int8_t runVal = static_cast<int8_t>(*src++);
				if (runVal < 0) {
					xPos += -runVal;
				} else {
					for (int i = 0; i < runVal; i++) {
						uint8_t index = *src++;
						uint8_t alpha = GetTilePixelAlpha(maskType, tileType, xPos, bufferRow, outHeight);
						WriteTilePixel(rgbaOut.data(), xPos, bufferRow, outWidth, outHeight,
						    index, alpha, tbl, hasAnimatedPalette);
						xPos++;
					}
				}
			}
		}
	} else if (tileType == TileType::LeftTriangle) {
		// 31 rows. Lower half (16 rows): widths 2,4,6,...,32, left-aligned starting
		// at x = Width - rowWidth. Upper half (15 rows): widths 30,28,...,2.
		// Data is bottom-to-top. Buffer row 0 = top.
		// Left/Right masks are not used with triangles; alpha is always determined
		// by MaskType::Solid or MaskType::Transparent only.
		int bufferRow = outHeight - 1;

		// Lower half: 16 rows, row i has width XStep*(i+1), pixels at x = Width - XStep*(i+1)
		for (int i = 0; i < LowerHeight; i++) {
			int rowWidth = XStep * (i + 1);
			int startX = Width - rowWidth;
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, startX + x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), startX + x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
		// Upper half: 15 rows, row i has width Width - XStep*(i+1), pixels at x = XStep*(i+1)
		for (int i = 0; i < TriangleUpperHeight; i++) {
			int rowWidth = Width - XStep * (i + 1);
			int startX = XStep * (i + 1);
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, startX + x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), startX + x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
	} else if (tileType == TileType::RightTriangle) {
		// 31 rows. Lower half (16 rows): widths 2,4,...,32, right-aligned at x=0.
		// Upper half (15 rows): widths 30,28,...,2, at x=0.
		int bufferRow = outHeight - 1;

		// Lower half: 16 rows, row i has width XStep*(i+1), pixels at x = 0
		for (int i = 0; i < LowerHeight; i++) {
			int rowWidth = XStep * (i + 1);
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
		// Upper half: 15 rows, row i has width Width - XStep*(i+1), pixels at x = 0
		for (int i = 0; i < TriangleUpperHeight; i++) {
			int rowWidth = Width - XStep * (i + 1);
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
	} else if (tileType == TileType::LeftTrapezoid) {
		// Lower triangle part (16 rows, same as LeftTriangle lower half),
		// then upper rectangle part (16 rows of full 32px width).
		int bufferRow = outHeight - 1;

		// Lower triangle: 16 rows — always opaque regardless of mask
		// (the diagonal only affects the upper half).
		for (int i = 0; i < LowerHeight; i++) {
			int rowWidth = XStep * (i + 1);
			int startX = Width - rowWidth;
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, startX + x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), startX + x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
		// Upper rectangle: 16 rows of full width — may have diagonal mask.
		for (int i = 0; i < TrapezoidUpperHeight; i++) {
			for (int x = 0; x < Width; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
	} else if (tileType == TileType::RightTrapezoid) {
		// Lower triangle part (16 rows, same as RightTriangle lower half),
		// then upper rectangle part (16 rows of full 32px width).
		int bufferRow = outHeight - 1;

		// Lower triangle: 16 rows
		for (int i = 0; i < LowerHeight; i++) {
			int rowWidth = XStep * (i + 1);
			for (int x = 0; x < rowWidth; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
		// Upper rectangle: 16 rows of full width
		for (int i = 0; i < TrapezoidUpperHeight; i++) {
			for (int x = 0; x < Width; x++) {
				uint8_t index = *src++;
				uint8_t alpha = GetTilePixelAlpha(maskType, tileType, x, bufferRow, outHeight);
				WriteTilePixel(rgbaOut.data(), x, bufferRow, outWidth, outHeight,
				    index, alpha, tbl, hasAnimatedPalette);
			}
			bufferRow--;
		}
	}
}

void Gl1TextureCache::UploadTexture(Gl1CachedTexture &entry, uint16_t width, uint16_t height, const uint8_t *rgbaData)
{
	if (entry.textureId == 0) {
		glGenTextures(1, &entry.textureId);
	}
	glBindTexture(GL_TEXTURE_2D, entry.textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	entry.width = width;
	entry.height = height;
}

void Gl1TextureCache::NotifyPaletteChanged()
{
	paletteGeneration_++;
}

void Gl1TextureCache::NotifyFullPaletteChanged()
{
	fullClearPending_ = true;
}

void Gl1TextureCache::FlushIfNeeded()
{
	if (!fullClearPending_)
		return;
	fullClearPending_ = false;
	Clear();
}

void Gl1TextureCache::Clear()
{
	for (auto &[key, entry] : spriteCache_) {
		if (entry.textureId != 0) {
			glDeleteTextures(1, &entry.textureId);
		}
	}
	spriteCache_.clear();

	for (auto &[key, entry] : tileCache_) {
		if (entry.textureId != 0) {
			glDeleteTextures(1, &entry.textureId);
		}
	}
	tileCache_.clear();

	paletteGeneration_ = 0;
}

} // namespace devilution

#endif // DEVILUTIONX_GL1_RENDERER