/**
 * @file software_renderer.h
 *
 * SoftwareRenderer: implements the Renderer interface for the existing
 * CPU-based software rendering path.
 *
 * Fade handling: ApplyFade reads system_palette, applies a fade dimming
 * factor, and writes the result to the SDL surface palette. It does NOT
 * modify system_palette — palette management (loading, cycling, brightness)
 * is owned by palette.cpp. The renderer only owns how the palette becomes
 * visible pixels (the "effective palette" is computed transiently here).
 */
#pragma once

#include "engine/palette.h"
#include "engine/render/fade_controller.h"
#include "engine/render/light_render.hpp"
#include "engine/render/renderer.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_ptrs.h"

namespace devilution {

class SoftwareRenderer final : public Renderer {
public:
	void Init(SDL_Window *window) override;
	void CreateBackBuffer() override;
	void Shutdown() override;
	void OnVideoModeChanged() override { }

	void InitPalette() override;
	void BeginScene(Point position, Point targetBufferPosition, int rows, int columns) override;
	void EndScene() override;
	void SetLightingMode(LightingMode /*mode*/) override { }
	Size GetBackBufferSize() const override { return { palSurface_->w, palSurface_->h }; }
	void *GetBackBufferKey() const override { return palSurface_ != nullptr ? palSurface_->pixels : nullptr; }

	void BlackOutScreen() override { fade_.BlackOutScreen(); }
	void StartFadeIn(int fr) override { fade_.StartFadeIn(fr); }
	void StartFadeOut(int fr) override { fade_.StartFadeOut(fr); }
	bool IsFading() const override { return fade_.IsFading(); }
	int GetFadeLevel() const override { return fade_.GetFadeLevel(); }
	bool IsBlackedOut() const override { return fade_.IsBlackedOut(); }

	void BeginFrame() override;
	void EndFrame() override;
	void ClearScreen() override;

	void SetCursor(SDL_Surface *surface, Point hotpoint) override;
	void SetCursorVisible(bool visible) override;
	void MarkDirtyRect(Rectangle area) override;
	void MarkScreenDirty() override;

	void BeginViewportZoom() override { }
	void EndViewportZoom(int viewportHeight) override;

	bool NeedsFullRedraw() const override;
	bool SupportsLightingMode(LightingMode mode) const override;

	bool HasPresentationLayer() const override
	{
#ifndef USE_SDL1
		return sdlRenderer_ != nullptr;
#else
		return false;
#endif
	}

	void DrawClx(Point position, ClxSprite clx) override;
	void DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn) override;
	void DrawClxBlended(Point position, ClxSprite clx) override;
	void DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn) override;
	void DrawClxLit(Point position, ClxSprite clx, int lightTableIndex) override;
	void DrawClxLitBlended(Point position, ClxSprite clx, int lightTableIndex) override;
	void DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero) override;

	void DrawLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex,
	    bool transparency, TileProperties tileProperties, bool isFloor) override;
	void DrawFloorLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex) override;
	void DrawBlackTile(int sx, int sy) override;

	void FillRect(int x, int y, int w, int h, uint8_t colorIndex) override;
	void DrawBlendedRect(int x, int y, int w, int h) override;
	void DrawPixel(Point position, uint8_t colorIndex) override;
	void DrawBlendedPixel(Point position, uint8_t colorIndex) override;
	void DrawLine(Point from, Point to, uint8_t colorIndex) override;
	void DrawHorizontalLine(Point from, int width, uint8_t colorIndex) override;
	void DrawVerticalLine(Point from, int height, uint8_t colorIndex) override;
	void DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex) override;
	void DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex) override;
	void TintRect(int x, int y, int w, int h, uint8_t colorIndex) override;
	void ApplyTRN(int x, int y, int w, int h, const uint8_t *trn, uint8_t skipColorIndicesBelow) override;

	void BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition) override;
	void BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) override;

	void SetClipRegion(int x, int y, int w, int h) override;
	void ClearClipRegion() override;

	void NotifyPaletteChanged(int first, int ncolor) override;

	void InvalidateTextureCache() override;
	void ClearTextureCache() override;

	void HandleResize(int windowWidth, int windowHeight) override;

	void PrepareVideoPlayback(unsigned width, unsigned height) override;
	bool DrawVideoFrame(SDL_Surface *frame, unsigned width, unsigned height) override;
	void FinishVideoPlayback() override;
	void NotifyVideoPaletteChanged(SDL_Palette *palette) override;

	SDLSurfaceUniquePtr CaptureScreen() override;
	void RenderToAllBackBuffers(std::function<void()> drawFn) override;
	void ReinitVirtualGamepad() override;
	void MapWindowToLogical(int *x, int *y) const override;
	void MapLogicalToWindow(int *x, int *y) const override;
	Size GetOutputSize() const override;
	Size GetScaledCursorSize(Size logicalSize) const override;
	void ConvertEventCoordinates(SDL_Event &event) const override;
	void ReinitializeTexture() override;
	void SetIntegerScale(bool enable) override;
#ifndef USE_SDL1
	SDL_Renderer *GetSDLRenderer() const;
	SDL_Texture *GetSDLTexture() const;
	void SetSDLTexture(SDLTextureUniquePtr newTexture);
	SDL_Surface *GetRendererTextureSurface() const;
#endif

protected:
	void ApplyFade(int fadeLevel);

private:
	SDL_Surface *GetOutputSurface() const;
	bool OutputRequiresScaling() const;
	void ScaleOutputRect(SDL_Rect *rect) const;
	void BlitToOutput(SDL_Surface *src, SDL_Rect *srcRect, SDL_Rect *dstRect);

	FadeController fade_;
	SDLPaletteUniquePtr palette_;
	SDLSurfaceUniquePtr pinnedPalSurface_;
	SDL_Surface *palSurface_ = nullptr;

	std::unique_ptr<Lightmap> frameLightmap_;

	bool renderDirectlyToOutput_ = false;
	bool videoPlayback_ = false;
	SDL_Rect clipRect_ {};
	bool hasClip_ = false;
#ifndef USE_SDL1
	SDLTextureUniquePtr texture_;
	SDL_Renderer *sdlRenderer_ = nullptr;
#endif
	/** 24-bit renderer texture surface */
	SDLSurfaceUniquePtr rendererTextureSurface_;

	static constexpr size_t MaxDirtyRects = 32;
	SDL_Rect dirtyRects_[MaxDirtyRects] {};
	size_t numDirtyRects_ = 0;
	bool fullScreenDirty_ = false;

	// --- Cursor compositing state ---
	SDL_Surface *cursorSurface_ = nullptr;
	Point cursorHotpoint_;
	bool cursorVisible_ = false;
	Point cursorPosition_;
	uint8_t cursorTransparentColor_ = 0;

	/** Rectangle on palSurface_ where the cursor was last drawn. */
	Rectangle cursorRect_;
	/** Previous cursor rect for dirty-rect marking. */
	Rectangle prevCursorRect_;
	/** Saved pixels behind the cursor. */
	static constexpr size_t CursorBehindBufferSize = 8192;
	uint8_t cursorBehindBuffer_[CursorBehindBufferSize] {};
	bool cursorDrawn_ = false;

	void UndrawCursorFromBuffer();
	void DrawCursorToBuffer();
};

} // namespace devilution
