/**
 * @file gl1_renderer.h
 *
 * Public API for the OpenGL 1.x rendering backend.
 */
#pragma once

#ifdef DEVILUTIONX_GL1_RENDERER

#include <cstdint>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/lighting_mode.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

struct SDL_Window;

namespace devilution {

/**
 * @brief Initialize the GL1 renderer. Creates the GL context on the given window.
 * @return true on success
 */
bool Gl1Init(SDL_Window *window);

/**
 * @brief Shut down the GL1 renderer and release all resources.
 */
void Gl1Shutdown();

/**
 * @brief Begin a new frame. Clears the framebuffer and sets up the projection.
 */
void Gl1BeginFrame();

/**
 * @brief End the current frame. Calls SDL_GL_SwapWindow.
 */
void Gl1EndFrame();

/**
 * @brief Called when the window is resized to update glViewport.
 */
void Gl1HandleResize(int windowWidth, int windowHeight);

/**
 * @brief Map window pixel coordinates to logical game coordinates.
 */
void Gl1MapWindowToLogical(int *x, int *y);

/**
 * @brief Map logical game coordinates to window pixel coordinates.
 */
void Gl1MapLogicalToWindow(int *x, int *y);

/**
 * @brief Returns the physical drawable size of the GL window.
 */
Size Gl1GetOutputSize();

/**
 * @brief Scale a cursor size from logical to window coordinates.
 */
Size Gl1GetScaledCursorSize(Size logicalSize);

// ---- Sprite rendering (replaces ClxDraw* in the GL path) ----

/**
 * @brief Draw a CLX sprite as a textured quad.
 * @param position Bottom-left position in logical coordinates (same as ClxDraw).
 * @param clx The sprite to draw.
 */
void Gl1DrawClx(Point position, ClxSprite clx);

/**
 * @brief Draw a CLX sprite with a TRN color translation applied.
 */
void Gl1DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Draw a CLX sprite with 50% alpha blending.
 */
void Gl1DrawClxBlended(Point position, ClxSprite clx);

/**
 * @brief Draw a CLX sprite with TRN and 50% alpha blending.
 */
void Gl1DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Draw a CLX sprite with vertex lighting.
 * @param lightLevel 0 = fully lit, 15 = fully dark.
 */
void Gl1DrawClxWithLight(Point position, ClxSprite clx, uint8_t lightLevel);

/**
 * @brief Draw a CLX sprite with vertex lighting and transparency blending.
 * @param lightLevel 0 = fully lit, 15 = fully dark.
 */
void Gl1DrawClxWithLightBlended(Point position, ClxSprite clx, uint8_t lightLevel);

/**
 * @brief Draw a CLX sprite outline (solid-color border pixels).
 * @param col The palette index for the outline color.
 * @param position Bottom-left position in logical coordinates (same as ClxDrawOutline).
 * @param clx The sprite whose outline to draw.
 * @param skipColorIndexZero If true, treat color-index-0 pixels as transparent when computing the outline.
 */
void Gl1DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero);

// ---- Dungeon tile rendering ----

/**
 * @brief Draw a dungeon tile as a textured quad.
 * @param position Screen position of the tile.
 * @param tileType The type of tile (square, triangle, trapezoid).
 * @param src Raw frame data pointer.
 * @param height Frame height in pixels.
 * @param maskType Transparency mask type.
 * @param tbl Light table pointer (from LightTables[lightLevel].data()), used to derive vertex lighting.
 */
void Gl1DrawTile(Point position, TileType tileType, const uint8_t *src, int_fast16_t height,
    MaskType maskType, const uint8_t *tbl, LightingMode lightingMode);

/**
 * @brief Draw the black diamond tile used for unexplored areas.
 */
void Gl1DrawBlackTile(int sx, int sy);

// ---- Primitive rendering (for automap, UI rectangles, etc.) ----

/**
 * @brief Draw a filled rectangle.
 */
void Gl1FillRect(int x, int y, int width, int height, uint8_t colorIndex);

/**
 * @brief Draw a half-transparent rectangle (blended with black).
 */
void Gl1DrawBlendedRect(int x, int y, int width, int height);

/**
 * @brief Draw a horizontal line.
 */
void Gl1DrawHorizontalLine(Point from, int width, uint8_t colorIndex);

/**
 * @brief Draw a vertical line.
 */
void Gl1DrawVerticalLine(Point from, int height, uint8_t colorIndex);

/**
 * @brief Draw a half-transparent horizontal line.
 */
void Gl1DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex);

/**
 * @brief Draw a half-transparent vertical line.
 */
void Gl1DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex);

/**
 * @brief Draw a single pixel.
 */
void Gl1DrawPixel(Point position, uint8_t colorIndex);

/**
 * @brief Draw a half-transparent single pixel (blended with the palette transparency lookup).
 */
void Gl1DrawBlendedPixel(Point position, uint8_t colorIndex);

/**
 * @brief Draw a line between two points (Bresenham).
 */
void Gl1DrawLine(Point from, Point to, uint8_t colorIndex);

/**
 * @brief Draw a colored overlay on an inventory slot background.
 */
void Gl1TintRect(int x, int y, int w, int h, uint8_t colorIndex);

// ---- Clipping ----

/**
 * @brief Set the GL scissor rectangle to clip rendering to a subregion.
 * Coordinates are in logical (game) space.
 */
void Gl1SetClipRegion(int regionX, int regionY, int regionW, int regionH);

/**
 * @brief Clear the scissor rectangle, restoring full-screen rendering.
 */
void Gl1ClearClipRegion();

// ---- Texture cache management ----

/**
 * @brief Blit an 8-bit paletted surface region to the GL framebuffer as a textured quad.
 * Used for pre-rendered surfaces like the main panel (BottomBuffer).
 * @param src The source 8-bit surface.
 * @param srcRect The region of the source to blit.
 * @param targetPosition Where to draw in logical coordinates.
 */
void Gl1BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition);

/**
 * @brief Same as Gl1BlitSurface but skips pixels with color index 0.
 */
void Gl1BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition);

/**
 * @brief Invalidate all cached textures. Called when the palette changes.
 */
void Gl1InvalidateTextureCache();

/**
 * @brief Clear the entire texture cache. Called when CLX sprites are freed.
 */
void Gl1ClearTextureCache();

/**
 * @brief Notify the GL1 renderer that animated palette indices (1–31) have cycled.
 * Bumps the palette generation counter so stale animated textures get re-uploaded.
 */
void Gl1NotifyPaletteChanged();

/**
 * @brief Notify the GL1 renderer that the entire palette has changed (level load, fade, brightness).
 * Clears the entire texture cache since all cached RGBA data is stale.
 */
void Gl1NotifyFullPaletteChanged();

// ---- Video playback ----

/**
 * @brief Draw an SDL surface as a fullscreen quad, letterboxed to maintain aspect ratio.
 * Used by Smacker video playback to present decoded frames through the GL renderer.
 * The surface can be 8-bit paletted (decoded via its own palette) or 32-bit RGBA.
 * @param surface The video frame surface.
 * @param videoWidth The native width of the video.
 * @param videoHeight The native height of the video.
 */
void Gl1DrawVideoFrame(SDL_Surface *surface, uint32_t videoWidth, uint32_t videoHeight);

// ---- Zoom ----

/**
 * @brief Apply a 2x scale to the modelview matrix for zoom mode.
 * Call before rendering the game world when zoom is enabled.
 * All subsequent GL draws will be scaled 2x from the top-left corner.
 */
void Gl1SetZoom();

/**
 * @brief Remove the 2x zoom scale, restoring the identity modelview matrix.
 * Call after the zoomed game world rendering is complete.
 */
void Gl1ClearZoom();

// ---- Post-processing overlays ----

/**
 * @brief Draw a fullscreen red-tint overlay for the death screen.
 * Uses multiply blending (GL_DST_COLOR, GL_ZERO) with a red quad to strip
 * green and blue channels, matching the software renderer's pause.trn effect.
 */
void Gl1DrawRedBack();

// ---- Fade overlay ----

/**
 * @brief Draw a fullscreen black overlay for fade effects.
 * Called by the Gl1Renderer::ApplyFade() implementation.
 * @param fadeLevel 0 = fully black, 256 = fully visible (no overlay drawn).
 */
void Gl1ApplyFade(int fadeLevel);

} // namespace devilution

#endif // DEVILUTIONX_GL1_RENDERER