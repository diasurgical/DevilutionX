/**
 * @file palette.h
 *
 * Interface of functions for handling the engines color palette.
 */
#pragma once

#include <array>
#include <cstdint>

#include "levels/gendung.h"

namespace devilution {

// Diablo uses a 256 color palette
// Entry 0-127 (0x00-0x7F) are level specific
// Entry 128-255 (0x80-0xFF) are global

// standard palette for all levels
// 8 or 16 shades per color
// example (dark blue): PAL16_BLUE+14, PAL8_BLUE+7
// example (light red): PAL16_RED+2, PAL8_RED
// example (orange): PAL16_ORANGE+8, PAL8_ORANGE+4
#define PAL8_BLUE 128
#define PAL8_RED 136
#define PAL8_YELLOW 144
#define PAL8_ORANGE 152
#define PAL16_BEIGE 160
#define PAL16_BLUE 176
#define PAL16_YELLOW 192
#define PAL16_ORANGE 208
#define PAL16_RED 224
#define PAL16_GRAY 240

extern std::array<SDL_Color, 256> logical_palette;
extern std::array<SDL_Color, 256> system_palette;
extern std::array<SDL_Color, 256> orig_palette;

/**
 * @brief Lookup table for the average of two colors in `logical_palette`.
 */
extern uint8_t paletteTransparencyLookup[256][256];

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
/**
 * A lookup table from black for a pair of colors.
 *
 * For a pair of colors i and j, the index `i | (j << 8)` contains
 * `paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8)`.
 *
 * On big-endian platforms, the indices are encoded as `j | (i << 8)`, while the
 * value order remains the same.
 */
extern uint16_t paletteTransparencyLookupBlack16[65536];
#endif

void palette_update(int first = 0, int ncolor = 256);
void palette_init();
void LoadPalette(const char *pszFileName, bool blend = true);
void LoadRndLvlPal(dungeon_type l);
void IncreaseGamma();
void ApplyGamma(std::array<SDL_Color, 256> &dst, const std::array<SDL_Color, 256> &src, int n);
void DecreaseGamma();
int UpdateGamma(int gamma);
void BlackPalette();
void SetFadeLevel(int fadeval, bool updateHardwareCursor = true);
/**
 * @brief Fade screen from black
 * @param fr Steps per 50ms
 */
void PaletteFadeIn(int fr);
/**
 * @brief Fade screen to black
 * @param fr Steps per 50ms
 */
void PaletteFadeOut(int fr);
void palette_update_caves();
void palette_update_crypt();
void palette_update_hive();
void palette_update_quest_palette(int n);

} // namespace devilution
