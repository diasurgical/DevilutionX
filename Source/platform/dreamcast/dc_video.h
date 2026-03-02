/**
 * @file dc_video.h
 * @brief Dreamcast-specific video conversion layer
 *
 * DevilutionX renders to an 8bpp paletted surface. Dreamcast SDL only
 * supports 16bpp (RGB565). This module provides:
 *
 * 1. Palette management: Converts 8-bit RGB palette entries to RGB565 LUT
 * 2. Frame conversion: Expands 8bpp pixels to 16bpp using the LUT
 *
 * Performance target: <5ms per frame at 640x480
 */

#ifndef DEVILUTIONX_DC_VIDEO_H
#define DEVILUTIONX_DC_VIDEO_H

#ifdef __DREAMCAST__

#include <SDL.h>
#include <cstdint>

namespace devilution {
namespace dc {

/**
 * @brief Initialize the Dreamcast video conversion layer
 * @param width Screen width (typically 640)
 * @param height Screen height (typically 480)
 * @return true on success
 */
bool VideoInit(int width, int height);

/**
 * @brief Shutdown and free video resources
 */
void VideoShutdown();

/**
 * @brief Update the RGB565 palette lookup table
 *
 * Called when the game changes its palette. Converts each entry
 * from RGB888 to RGB565 format.
 *
 * @param palette SDL palette with 256 RGB entries
 */
void UpdatePalette(const SDL_Palette *palette);

/**
 * @brief Update a range of palette entries
 * @param colors Pointer to SDL_Color array
 * @param firstColor Starting index (0-255)
 * @param nColors Number of colors to update
 */
void UpdatePaletteRange(const SDL_Color *colors, int firstColor, int nColors);

/**
 * @brief Convert an 8bpp frame to 16bpp
 *
 * This is the hot path - called every frame.
 *
 * @param src 8bpp source surface (PalSurface)
 * @param dst 16bpp destination surface (video framebuffer)
 */
void ConvertAndUpload(const SDL_Surface *src, SDL_Surface *dst);

/**
 * @brief Convert RGB888 to RGB565
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return Packed RGB565 value
 */
inline uint16_t RGB888toRGB565(uint8_t r, uint8_t g, uint8_t b)
{
	// RGB565: RRRRRGGGGGGBBBBB
	return static_cast<uint16_t>(
	    ((r & 0xF8) << 8) | // 5 bits red
	    ((g & 0xFC) << 3) | // 6 bits green
	    ((b & 0xF8) >> 3)); // 5 bits blue
}

/**
 * @brief Check if DC video layer is active
 */
bool IsInitialized();

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
#endif // DEVILUTIONX_DC_VIDEO_H
