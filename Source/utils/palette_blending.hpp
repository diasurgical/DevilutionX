#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#else
#include <SDL.h>
#endif

namespace devilution {

/**
 * @brief Lookup table for the average of two colors in `logical_palette`.
 */
extern uint8_t paletteTransparencyLookup[256][256];

/**
 * @brief Generates `paletteTransparencyLookup` table.
 *
 * This is based of the same technique found in Quake2.
 *
 * To mimic 50% transparency we figure out what colors in the existing palette are the best match for the combination of any 2 colors.
 * We save this into a lookup table for use during rendering.
 *
 * @param skipFrom Do not use colors between this index and skipTo
 * @param skipTo Do not use colors between skipFrom and this index
 */
void GenerateBlendedLookupTable(const SDL_Color *palette, int skipFrom = -1, int skipTo = -1);

/**
 * @brief Updates the transparency lookup table for a single color.
 */
void UpdateBlendedLookupTableSingleColor(const SDL_Color *palette, unsigned i);

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
/**
 * A lookup table from black for a pair of colors in `logical_palette`.
 *
 * For a pair of colors i and j, the index `i | (j << 8)` contains
 * `paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8)`.
 *
 * On big-endian platforms, the indices are encoded as `j | (i << 8)`, while the
 * value order remains the same.
 */
extern uint16_t paletteTransparencyLookupBlack16[65536];

void UpdateTransparencyLookupBlack16(unsigned from, unsigned to);
#endif

} // namespace devilution
