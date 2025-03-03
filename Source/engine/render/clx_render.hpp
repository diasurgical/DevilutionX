/**
 * @file clx_render.hpp
 *
 * CL2 rendering.
 */
#pragma once

#include <cstdint>

#include <array>
#include <utility>

#ifdef DEBUG_CLX
#include <string>
#endif

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/render/light_render.hpp"
#include "engine/surface.hpp"
#include "lighting.h"

namespace devilution {

/**
 * @brief Apply the color swaps to a CLX sprite list;
 */
void ClxApplyTrans(ClxSpriteList list, const uint8_t *trn);
void ClxApplyTrans(ClxSpriteSheet sheet, const uint8_t *trn);

/**
 * @brief Blit CL2 sprite, to the back buffer at the given coordianates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param frame CL2 frame number
 */
void ClxDraw(const Surface &out, Point position, ClxSprite clx);

/**
 * @brief Same as ClxDraw but position.y is the top of the sprite instead of the bottom.
 */
inline void RenderClxSprite(const Surface &out, ClxSprite clx, Point position)
{
	ClxDraw(out, { position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx);
}

/**
 * @brief Blit a solid colder shape one pixel larger than the given sprite shape, to the given buffer at the given coordinates
 * @param col Color index from current palette
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawOutline(const Surface &out, uint8_t col, Point position, ClxSprite clx);

/**
 * @brief Same as `ClxDrawOutline` but considers colors with index 0 (usually shadows) to be transparent.
 *
 * @param col Color index from current palette
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawOutlineSkipColorZero(const Surface &out, uint8_t col, Point position, ClxSprite clx);

/**
 * @brief Blit CL2 sprite, and apply given TRN to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param trn TRN to use
 */
void ClxDrawTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Same as ClxDrawTRN but position.y is the top of the sprite instead of the bottom.
 */
inline void RenderClxSpriteWithTRN(const Surface &out, ClxSprite clx, Point position, const uint8_t *trn)
{
	ClxDrawTRN(out, { position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx, trn);
}

void ClxDrawBlendedTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Blit CLX sprite with 50% transparency to the given buffer at the given coordinates.
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawBlended(const Surface &out, Point position, ClxSprite clx);

/**
 * @brief Blit CL2 sprite, and apply lighting, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
inline void ClxDrawLight(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDraw(out, position, clx);
	}
}

/**
 * @brief Blit CL2 sprite, and apply lighting, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param lightmap Per-pixel light buffer
 */
void ClxDrawWithLightmap(const Surface &out, Point position, ClxSprite clx, const Lightmap &lightmap);

/**
 * @brief Blit CL2 sprite, and apply lighting and transparency blending, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
inline void ClxDrawLightBlended(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawBlendedTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDrawBlended(out, position, clx);
	}
}

/**
 * @brief Blit CL2 sprite, and apply lighting and transparency blending, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param lightmap Per-pixel light buffer
 */
void ClxDrawBlendedWithLightmap(const Surface &out, Point position, ClxSprite clx, const Lightmap &lightmap);

/**
 * Returns if cursor is within the CLX sprite (ignores shadow)
 */
bool IsPointWithinClx(Point position, ClxSprite clx);

/**
 * Returns a pair of X coordinates containing the start (inclusive) and end (exclusive)
 * of fully transparent columns in the sprite.
 */
std::pair<int, int> ClxMeasureSolidHorizontalBounds(ClxSprite clx);

/**
 * @brief Clears the CLX draw cache.
 *
 * Must be called whenever CLX sprites are freed.
 */
void ClearClxDrawCache();

#ifdef DEBUG_CLX
std::string ClxDescribe(ClxSprite clx);
#endif

} // namespace devilution
