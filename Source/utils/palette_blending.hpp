#pragma once

#include <cstdint>

#include <SDL.h>

namespace devilution {

/** Lookup table for transparency */
extern uint8_t paletteTransparencyLookup[256][256];

/**
 * @brief Generate lookup table for transparency
 *
 * This is based of the same technique found in Quake2.
 *
 * To mimic 50% transparency we figure out what colors in the existing palette are the best match for the combination of any 2 colors.
 * We save this into a lookup table for use during rendering.
 *
 * @param palette The colors to operate on
 * @param skipFrom Do not use colors between this index and skipTo
 * @param skipTo Do not use colors between skipFrom and this index
 */
void GenerateBlendedLookupTable(SDL_Color palette[256], int skipFrom, int skipTo);

void UpdateBlendedLookupTableSingleColor(unsigned i, SDL_Color palette[256], int skipFrom, int skipTo);

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

void UpdateTransparencyLookupBlack16(unsigned from, unsigned to);
#endif

} // namespace devilution
