/**
 * @file light_render_internal.hpp
 *
 * Internal Lightmap factory functions — TileSmooth lightmap construction
 * and wall-bleed. Only used by the software renderer and GL1 renderer.
 * Not part of the public Lightmap API.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/lighting_defs.hpp"
#include "engine/render/light_render.hpp"
#include "engine/render/lighting_mode.hpp"
#include "levels/gendung_defs.hpp"

namespace devilution {

/**
 * @brief Build the per-frame lightmap for the viewport.
 *
 * For TileSmooth mode this interpolates tile light levels into a
 * per-pixel buffer sized to the viewport. For other modes the returned
 * Lightmap carries only the light-table metadata (perPixel = false).
 */
Lightmap LightmapBuild(LightingMode lightingMode, Point tilePosition, Point targetBufferPosition,
    int viewportWidth, int viewportHeight, int rows, int columns,
    const uint8_t *outBuffer, uint16_t outPitch,
    std::span<const std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables,
    const uint8_t *fullyLitLightTable, const uint8_t *fullyDarkLightTable,
    const uint8_t tileLights[MAXDUNX][MAXDUNY],
    uint_fast8_t microTileLen);

/**
 * @brief Derive a wall-bleed lightmap for a special cel (arch tile).
 *
 * Copies and bleeds light values upward from the base lightmap so that
 * wall arches appear to be lit from below. If the source lightmap is not
 * per-pixel (i.e. TileSmooth is not active) the source is returned
 * unchanged.
 */
Lightmap LightmapBleedUp(const Lightmap &source, Point targetBufferPosition, std::span<uint8_t> lightmapBuffer);

} // namespace devilution
