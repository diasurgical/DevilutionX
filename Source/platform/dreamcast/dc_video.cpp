/**
 * @file dc_video.cpp
 * @brief Dreamcast video conversion implementation
 *
 * The "Inner Loop" - this code runs 307,200 times per frame (640x480).
 * Every cycle counts!
 *
 * Optimization techniques used:
 * 1. Palette LUT fits in L1 cache (512 bytes for 256 RGB565 entries)
 * 2. Process 16 pixels at a time for throughput
 */

#ifdef __DREAMCAST__

#include "dc_video.h"

#include <kos.h>

namespace devilution {
namespace dc {

namespace {

// RGB565 palette lookup table (256 entries x 2 bytes = 512 bytes)
// Aligned to 32 bytes for cache efficiency
alignas(32) uint16_t palette565[256];
// 32-bit word lookup tables used by the packed conversion path.
alignas(32) uint32_t palette565FirstWord[256];
alignas(32) uint32_t palette565SecondWord[256];

bool initialized = false;

inline void UpdatePaletteEntry(int index, uint16_t rgb565)
{
	palette565[index] = rgb565;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	palette565FirstWord[index] = rgb565;
	palette565SecondWord[index] = static_cast<uint32_t>(rgb565) << 16;
#else
	palette565FirstWord[index] = static_cast<uint32_t>(rgb565) << 16;
	palette565SecondWord[index] = rgb565;
#endif
}

/**
 * @brief Convert 16 pixels from 8bpp to 16bpp
 *
 * This is the innermost loop - fully unrolled for speed.
 */
inline void Convert16PixelsScalar(const uint8_t *src, uint16_t *dst)
{
	dst[0] = palette565[src[0]];
	dst[1] = palette565[src[1]];
	dst[2] = palette565[src[2]];
	dst[3] = palette565[src[3]];
	dst[4] = palette565[src[4]];
	dst[5] = palette565[src[5]];
	dst[6] = palette565[src[6]];
	dst[7] = palette565[src[7]];
	dst[8] = palette565[src[8]];
	dst[9] = palette565[src[9]];
	dst[10] = palette565[src[10]];
	dst[11] = palette565[src[11]];
	dst[12] = palette565[src[12]];
	dst[13] = palette565[src[13]];
	dst[14] = palette565[src[14]];
	dst[15] = palette565[src[15]];
}

/**
 * @brief Convert 16 pixels using packed 32-bit writes (2 pixels per store)
 *
 * SH4 is efficient at aligned 32-bit loads/stores, so this path halves
 * the number of destination stores compared to scalar 16-bit writes.
 */
inline void Convert16PixelsPacked(const uint8_t *src, uint16_t *dst)
{
#if defined(__SH4__) || defined(__sh__)
	// Pull upcoming source bytes into cache early on SH4.
	__builtin_prefetch(src + 32, 0, 3);
	__builtin_prefetch(src + 64, 0, 3);
#endif
	uint32_t *dst32 = reinterpret_cast<uint32_t *>(dst);
	dst32[0] = palette565FirstWord[src[0]] | palette565SecondWord[src[1]];
	dst32[1] = palette565FirstWord[src[2]] | palette565SecondWord[src[3]];
	dst32[2] = palette565FirstWord[src[4]] | palette565SecondWord[src[5]];
	dst32[3] = palette565FirstWord[src[6]] | palette565SecondWord[src[7]];
	dst32[4] = palette565FirstWord[src[8]] | palette565SecondWord[src[9]];
	dst32[5] = palette565FirstWord[src[10]] | palette565SecondWord[src[11]];
	dst32[6] = palette565FirstWord[src[12]] | palette565SecondWord[src[13]];
	dst32[7] = palette565FirstWord[src[14]] | palette565SecondWord[src[15]];
}

void ConvertFrame(const uint8_t *src, uint16_t *dst, int width, int height, int srcPitch, int dstPitch)
{
	for (int y = 0; y < height; y++) {
		const uint8_t *srcRow = src + y * srcPitch;
		uint16_t *dstRow = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstPitch);
		const bool canUsePackedPath = (reinterpret_cast<uintptr_t>(dstRow) & (alignof(uint32_t) - 1)) == 0;

		int x = 0;
		if (canUsePackedPath) {
			for (; x + 16 <= width; x += 16) {
				Convert16PixelsPacked(srcRow + x, dstRow + x);
			}
		} else {
			for (; x + 16 <= width; x += 16) {
				Convert16PixelsScalar(srcRow + x, dstRow + x);
			}
		}

		for (; x < width; x++) {
			dstRow[x] = palette565[srcRow[x]];
		}
	}
}

} // anonymous namespace

bool VideoInit([[maybe_unused]] int width, [[maybe_unused]] int height)
{
	for (int i = 0; i < 256; i++) {
		UpdatePaletteEntry(i, RGB888toRGB565(i, i, i));
	}

	initialized = true;
	return true;
}

void VideoShutdown()
{
	initialized = false;
}

void UpdatePalette(const SDL_Palette *palette)
{
	if (!palette || !palette->colors)
		return;

	UpdatePaletteRange(palette->colors, 0, palette->ncolors);
}

void UpdatePaletteRange(const SDL_Color *colors, int firstColor, int nColors)
{
	if (!colors)
		return;

	if (firstColor + nColors > 256)
		nColors = 256 - firstColor;

	for (int i = 0; i < nColors; i++) {
		const SDL_Color &c = colors[i];
		UpdatePaletteEntry(firstColor + i, RGB888toRGB565(c.r, c.g, c.b));
	}
}

void ConvertAndUpload(const SDL_Surface *src, SDL_Surface *dst)
{
	if (!initialized || !src || !dst)
		return;

	const uint8_t *srcPixels = static_cast<const uint8_t *>(src->pixels);
	uint16_t *dstPixels = static_cast<uint16_t *>(dst->pixels);

	if (!srcPixels || !dstPixels)
		return;

	const int width = src->w < dst->w ? src->w : dst->w;
	const int height = src->h < dst->h ? src->h : dst->h;

	ConvertFrame(srcPixels, dstPixels, width, height, src->pitch, dst->pitch);
}

bool IsInitialized()
{
	return initialized;
}

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
