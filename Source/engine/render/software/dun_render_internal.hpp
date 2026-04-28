/**
 * @file dun_render_internal.hpp
 *
 * Internal tile rendering functions used only by the software renderer
 * and the dun_render benchmark. Not part of the public API.
 */
#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "engine/point.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/light_render.hpp"
#include "levels/dun_tile.hpp"
#include "utils/endian_swap.hpp"

namespace devilution {

/**
 * @brief Low-level tile rendering function.
 */
void RenderTileFrame(SDL_Surface *palSurface, const Lightmap &lightmap, const Point &position, TileType tile, const uint8_t *src, int_fast16_t height,
    MaskType maskType, const uint8_t *tbl);

/**
 * @brief Blit current world CEL to the given buffer
 * @param lightmap Per-pixel light buffer
 * @param position Target buffer coordinates
 * @param dungeonCelData Dungeon CEL data.
 * @param levelCelBlock The MIN block of the level CEL file.
 * @param maskType The mask to use,
 * @param tbl LightTable or TRN for a tile.
 */
DVL_ALWAYS_INLINE void RenderTile(SDL_Surface *palSurface, const Lightmap &lightmap, const Point &position,
    const std::byte *dungeonCelData, LevelCelBlock levelCelBlock, MaskType maskType, const uint8_t *tbl)
{
	const TileType tileType = levelCelBlock.type();
	RenderTileFrame(palSurface, lightmap, position, tileType,
	    GetDunFrame(dungeonCelData, levelCelBlock.frame()),
	    (tileType == TileType::LeftTriangle || tileType == TileType::RightTriangle)
	        ? DunFrameTriangleHeight
	        : DunFrameHeight,
	    maskType, tbl);
}

/**
 * @brief Renders a floor foliage tile.
 */
DVL_ALWAYS_INLINE void RenderTileFoliage(SDL_Surface *palSurface, const Lightmap &lightmap, const Point &position,
    const std::byte *dungeonCelData, LevelCelBlock levelCelBlock, const uint8_t *tbl)
{
	RenderTileFrame(palSurface, lightmap, Point { position.x, position.y - 16 }, TileType::TransparentSquare,
	    GetDunFrameFoliage(dungeonCelData, levelCelBlock.frame()), /*height=*/16, MaskType::Solid, tbl);
}

} // namespace devilution
