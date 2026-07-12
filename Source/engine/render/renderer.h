/**
 * @file renderer.h
 *
 * Abstract renderer interface. Both the software renderer and GPU backends
 * implement this interface. Game code calls through GetRenderer() instead
 * of branching on #ifdef or IsGl1RendererActive().
 *
 * Fade system:
 *   The renderer maintains a black overlay with a variable opacity.
 *   Once enabled (via BlackOutScreen or StartFadeOut), the overlay stays
 *   on until a StartFadeIn completes — there is no way for a single frame
 *   to accidentally flash through without the overlay.
 *
 *   BlackOutScreen() — Immediately sets the overlay to fully opaque.
 *   StartFadeIn(fr)  — Records a timestamp. On each EndFrame(), the overlay
 *                       opacity decreases based on elapsed time. When it
 *                       reaches 0 the overlay is disabled.
 *   StartFadeOut(fr)  — Records a timestamp. On each EndFrame(), the overlay
 *                       opacity increases. When it reaches 100% it stays
 *                       there (equivalent to BlackOutScreen).
 *   IsFading()        — True while a fade is in progress (not yet reached
 *                       its target).
 *
 *   How the overlay is rendered depends on the backend:
 *     - Software: adjusts system_palette before the blit.
 *     - GL: draws a fullscreen black quad with the appropriate alpha.
 *
 *   Typical usage — blocking fade-out then load:
 *     GetRenderer().StartFadeOut(8);
 *     while (GetRenderer().IsFading()) { RedrawScene(); GetRenderer().EndFrame(); }
 *     // screen is now black, safe to load
 *
 *   Typical usage — non-blocking fade-in (menus, game start):
 *     GetRenderer().BlackOutScreen();
 *     // ... set up scene ...
 *     GetRenderer().StartFadeIn(8);
 *     // normal render loop — overlay fades out automatically
 *
 *   Interrupting a fade-in with a fade-out (or vice-versa) starts from
 *   whatever the current overlay opacity is — no flash.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/lighting_mode.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "utils/sdl_ptrs.h"

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

namespace devilution {

/**
 * Abstract renderer interface.
 *
 * Operations (the core draw/blit/state primitives) are pure virtual: every
 * backend must implement them explicitly, so no backend silently inherits
 * wrong behaviour. A method may carry a default implementation only when the
 * default is correct for every backend AND a backend can meaningfully
 * specialize it (e.g. RenderClx, where a GPU backend can skip the bottom-left
 * coordinate round-trip). Behaviour shared across all backends with no such
 * specialization opportunity belongs in a free function, not here.
 */
class Renderer {
public:
	virtual ~Renderer() = default;

	// --- Lifecycle ---
	virtual void Init(SDL_Window *window) = 0;
	virtual void Shutdown() = 0;

	/**
	 * @brief Called after the video mode changes (e.g. fullscreen toggle on SDL1).
	 * Renderers that need to rebuild their context (e.g. GL after SDL_SetVideoMode
	 * destroys the GL context) do so here. Software renderers typically no-op.
	 */
	virtual void OnVideoModeChanged() = 0;

	// --- Back buffer and palette ---
	virtual void CreateBackBuffer() = 0;
	virtual void InitPalette() = 0;

	/**
	 * @brief Prepare the renderer for a scene (tile grid) draw.
	 *
	 * Called once per frame before DrawFloor / DrawTileContent / DrawOOB.
	 * Renderers that need per-frame state (e.g. the software renderer's
	 * TileSmooth lightmap) build it here and store it as a member.
	 * GPU renderers with no such state implement this as a no-op.
	 *
	 * @param position          Top-left tile position in the dungeon grid.
	 * @param targetBufferPosition Screen-space offset of that tile.
	 * @param rows              Number of tile rows in the viewport.
	 * @param columns           Number of tile columns in the viewport.
	 */
	virtual void BeginScene(Point position, Point targetBufferPosition, int rows, int columns) = 0;

	/**
	 * @brief Called after the last tile draw call for the frame.
	 * Renderers may use this to release any per-frame scene state.
	 * No-op for renderers with no per-scene state.
	 */
	virtual void EndScene() = 0;

	/**
	 * @brief Notify the renderer that the active lighting mode has changed.
	 *
	 * Called at renderer init and whenever the user changes the lighting
	 * setting. Renderers that cache the mode internally (e.g. to avoid
	 * reading options on every tile draw) update their cached value here.
	 */
	virtual void SetLightingMode(LightingMode mode) = 0;

	/** @brief Returns the logical back buffer dimensions (game resolution). */
	virtual Size GetBackBufferSize() const = 0;

	/**
	 * @brief Returns an opaque key identifying the current back buffer.
	 * Used by backbuffer_state to track per-buffer redraw state in multi-buffered
	 * rendering. Returns nullptr for single-buffered or GPU renderers.
	 */
	virtual void *GetBackBufferKey() const = 0;

	// --- Frame lifecycle ---
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
	virtual void ClearScreen() = 0;

	// --- Cursor ---
	/** @brief Set the cursor surface and hotpoint. The renderer composites it at the mouse position during EndFrame(). */
	virtual void SetCursor(SDL_Surface *surface, Point hotpoint) = 0;

	/** @brief Show or hide the software cursor. */
	virtual void SetCursorVisible(bool visible) = 0;

	/**
	 * @brief Mark a screen region as dirty (needs presenting).
	 * The software renderer accumulates these and only blits the dirty
	 * regions in EndFrame(). GPU renderers should implement this as a no-op.
	 */
	virtual void MarkDirtyRect(Rectangle area) = 0;

	/**
	 * @brief Mark the entire screen as dirty.
	 */
	virtual void MarkScreenDirty() = 0;

	// --- Frame properties ---
	virtual bool NeedsFullRedraw() const = 0;

	/**
	 * @brief Returns true if the renderer supports the given lighting mode.
	 *
	 * Used to validate the active lighting mode at renderer init and to
	 * filter the options UI so only supported modes are shown.
	 */
	virtual bool SupportsLightingMode(LightingMode mode) const = 0;

	/**
	 * @brief Returns true if the renderer owns presentation (scaling logical→physical).
	 *
	 * True for GPU renderers (GL viewport) and the SDL_Renderer-based software path.
	 * False only for the raw window-surface software path (no SDL_Renderer).
	 *
	 * Callers use this to decide whether the window should be resizable, whether
	 * display mode changes are needed for video playback, etc.
	 */
	virtual bool HasPresentationLayer() const = 0;

	// --- Fade / black overlay control ---
	virtual void BlackOutScreen() = 0;
	virtual void StartFadeIn(int fr) = 0;
	virtual void StartFadeOut(int fr) = 0;
	virtual bool IsFading() const = 0;
	virtual int GetFadeLevel() const = 0;
	virtual bool IsBlackedOut() const = 0;

	// --- Sprite rendering ---
	virtual void DrawClx(Point position, ClxSprite clx) = 0;

	/**
	 * @brief Draw a CLX sprite positioned by its top-left corner.
	 *
	 * UI code works in top-left coordinates, whereas DrawClx takes the
	 * bottom-left corner. The default converts and forwards to DrawClx;
	 * a backend may override to render top-left natively and skip the
	 * coordinate round-trip.
	 */
	virtual void RenderClx(Point position, ClxSprite clx)
	{
		DrawClx({ position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx);
	}

	virtual void DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn) = 0;
	virtual void DrawClxBlended(Point position, ClxSprite clx) = 0;
	virtual void DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn) = 0;

	/**
	 * @brief Draw a CLX sprite with lighting applied.
	 *
	 * The renderer dispatches based on the current lighting mode:
	 * - Vertex: per-vertex lighting using lightTableIndex as a light level.
	 * - Tile/TileSmooth: palette-based lighting via LightTables[lightTableIndex].
	 * - TileSmooth: per-pixel lightmap built during BeginScene is used automatically.
	 */
	virtual void DrawClxLit(Point position, ClxSprite clx, int lightTableIndex) = 0;
	virtual void DrawClxLitBlended(Point position, ClxSprite clx, int lightTableIndex) = 0;

	virtual void DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero) = 0;

	// --- Tile rendering (whole-tile API) ---

	/**
	 * @brief Render an entire level tile (two columns of up to 5 rows = 64×160).
	 *
	 * The engine submits the whole tile as a single call. Simple renderers
	 * (software, GL1) iterate the micro-tiles internally via DrawTile().
	 * Advanced renderers may cache the composed 64×160 tile as a texture.
	 *
	 * @param position       Screen position (bottom-left of the tile's first row).
	 * @param levelPieceId   Index into DPieceMicros[] for the MICROS data.
	 * @param lightTableIndex Index into LightTables[] for the light/TRN table.
	 * @param transparency   Whether transparency masking is active for this tile.
	 * @param tileProperties The TileProperties flags for this tile position.
	 * @param isFloor        Whether the tile position is a floor (skip floor micro-tiles,
	 *                       render foliage for TransparentSquare types).
	 */
	virtual void DrawLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex,
	    bool transparency, TileProperties tileProperties, bool isFloor)
	    = 0;

	/**
	 * @brief Render a floor-only level tile (the bottom two micro-tiles as triangles).
	 *
	 * @param position       Screen position (bottom-left of the floor tile).
	 * @param levelPieceId   Index into DPieceMicros[] for the MICROS data.
	 * @param lightTableIndex Index into LightTables[] for the light/TRN table.
	 */
	virtual void DrawFloorLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex)
	    = 0;

	virtual void DrawBlackTile(int sx, int sy) = 0;

	// --- Primitives ---
	virtual void FillRect(int x, int y, int w, int h, uint8_t colorIndex) = 0;
	virtual void DrawBlendedRect(int x, int y, int w, int h) = 0;
	virtual void DrawPixel(Point position, uint8_t colorIndex) = 0;
	virtual void DrawBlendedPixel(Point position, uint8_t colorIndex) = 0;
	virtual void DrawLine(Point from, Point to, uint8_t colorIndex) = 0;

	/**
	 * @brief Draw a solid axis-aligned line of `width`/`height` pixels starting at `from`.
	 *
	 * Axis-aligned lines are common for UI borders, bars and the minimap frame.
	 * Backends emit a single primitive (one GL span, or a direct surface run)
	 * instead of N pixel calls. Use DrawLine for diagonals.
	 */
	virtual void DrawHorizontalLine(Point from, int width, uint8_t colorIndex) = 0;
	virtual void DrawVerticalLine(Point from, int height, uint8_t colorIndex) = 0;

	/** @brief Half-transparent counterparts of DrawHorizontalLine / DrawVerticalLine. */
	virtual void DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex) = 0;
	virtual void DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex) = 0;

	/**
	 * @brief Draw a tinted overlay on a rectangular region.
	 * @param x Absolute screen X position.
	 * @param y Absolute screen Y position.
	 * @param w Width in pixels.
	 * @param h Height in pixels.
	 * @param colorIndex The base palette index for the slot color (e.g. PAL16_BLUE+1).
	 */
	virtual void TintRect(int x, int y, int w, int h, uint8_t colorIndex) = 0;

	/**
	 * @brief Apply a palette translation table (TRN) to a rectangular screen region.
	 * Each pixel's palette index is remapped through the table.
	 * @param x Screen X position.
	 * @param y Screen Y position.
	 * @param w Width in pixels.
	 * @param h Height in pixels.
	 * @param trn 256-byte translation table mapping old palette index → new palette index.
	 * @param skipColorIndicesBelow Skip remapping for palette indices below this value (0 = remap all).
	 */
	virtual void ApplyTRN(int x, int y, int w, int h, const uint8_t *trn, uint8_t skipColorIndicesBelow = 0) = 0;

	// --- Surface blits (pre-rendered panels, UI) ---
	virtual void BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition) = 0;
	virtual void BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) = 0;

	// --- Clipping ---
	virtual void SetClipRegion(int x, int y, int w, int h) = 0;
	virtual void ClearClipRegion() = 0;

	// --- Viewport zoom ---
	virtual void BeginViewportZoom() = 0;
	virtual void EndViewportZoom(int viewportHeight) = 0;

	// --- Palette notification ---
	virtual void NotifyPaletteChanged(int first, int ncolor) = 0;

	// --- Texture cache ---
	virtual void InvalidateTextureCache() = 0;
	virtual void ClearTextureCache() = 0;

	// --- Window events ---
	virtual void HandleResize(int windowWidth, int windowHeight) = 0;

	/** @brief Recreate the presentation texture (e.g. after scale quality change). No-op for GPU renderers. */
	virtual void ReinitializeTexture() = 0;

	/** @brief Enable or disable integer scaling for the presentation layer. No-op for GPU renderers. */
	virtual void SetIntegerScale(bool enable) = 0;

	// --- Video playback ---
	virtual void PrepareVideoPlayback(unsigned width, unsigned height) = 0;
	virtual bool DrawVideoFrame(SDL_Surface *frame, unsigned width, unsigned height) = 0;
	virtual void FinishVideoPlayback() = 0;
	virtual void NotifyVideoPaletteChanged(SDL_Palette *palette) = 0;

	// --- Multi-buffer support ---
	virtual void RenderToAllBackBuffers(std::function<void()> drawFn) = 0;

	// --- Virtual gamepad ---
	/** @brief Reinitialize virtual gamepad textures after they've been invalidated. No-op for renderers that don't use them. */
	virtual void ReinitVirtualGamepad() = 0;

	// --- Screen capture ---
	/** @brief Capture the current screen contents as an owned surface, or nullptr if headless. */
	virtual SDLSurfaceUniquePtr CaptureScreen() = 0;

	// --- Coordinate mapping ---
	virtual void MapWindowToLogical(int *x, int *y) const = 0;
	virtual void MapLogicalToWindow(int *x, int *y) const = 0;
	/**
	 * @brief Returns the physical pixel dimensions of the renderer's output.
	 *
	 * This is the display resolution in physical pixels, accounting for
	 * HiDPI where applicable. Use this for anything that maps directly
	 * to screen pixels (e.g. cursor sizing, screenshot buffers).
	 */
	virtual Size GetOutputSize() const = 0;

	/**
	 * @brief Scale a cursor size from logical (game) pixels to window coordinates.
	 *
	 * Hardware cursors are specified in window coordinates, so a logical cursor
	 * sprite must be scaled to match how the renderer presents game content.
	 * On HiDPI displays (SDL3), the result is divided by the display scale
	 * because SDL handles the final window→physical scaling for cursors.
	 */
	virtual Size GetScaledCursorSize(Size logicalSize) const = 0;

	/**
	 * @brief Convert mouse event coordinates from window space to logical (game) space.
	 */
	virtual void ConvertEventCoordinates(SDL_Event &event) const = 0;
};

/**
 * @brief Returns the active renderer instance.
 */
Renderer &GetRenderer();

/**
 * @brief Sets the active renderer. Called once at startup.
 */
void SetRenderer(std::unique_ptr<Renderer> renderer);

} // namespace devilution
