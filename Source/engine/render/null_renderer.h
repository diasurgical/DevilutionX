/**
 * @file null_renderer.h
 *
 * NullRenderer: complete no-op renderer for headless mode and tests.
 * All methods are empty. ApplyFade is a no-op since there is no
 * visual output to darken.
 */
#pragma once

#include <functional>

#include "engine/render/renderer.h"

namespace devilution {

class NullRenderer : public Renderer {
public:
	void Init(SDL_Window * /*window*/) override { }
	void CreateBackBuffer() override { }
	void Shutdown() override { }
	void OnVideoModeChanged() override { }

	void InitPalette() override { }
	void BeginScene(Point /*position*/, Point /*targetBufferPosition*/, int /*rows*/, int /*columns*/) override { }
	void EndScene() override { }
	void SetLightingMode(LightingMode /*mode*/) override { }
	Size GetBackBufferSize() const override { return { 640, 480 }; }
	void *GetBackBufferKey() const override { return nullptr; }

	Size GetOutputSize() const override { return { 640, 480 }; }
	Size GetScaledCursorSize(Size logicalSize) const override { return logicalSize; }

	void BlackOutScreen() override { }
	void StartFadeIn(int /*fr*/) override { }
	void StartFadeOut(int /*fr*/) override { }
	bool IsFading() const override { return false; }
	int GetFadeLevel() const override { return 0; }
	bool IsBlackedOut() const override { return false; }

	void BeginFrame() override { }
	void EndFrame() override { }
	void ClearScreen() override { }

	void SetCursor(SDL_Surface * /*surface*/, Point /*hotpoint*/) override { }
	void SetCursorVisible(bool /*visible*/) override { }
	void MarkDirtyRect(Rectangle /*area*/) override { }
	void MarkScreenDirty() override { }

	bool NeedsFullRedraw() const override { return false; }
	bool SupportsLightingMode(LightingMode /*mode*/) const override { return true; }
	bool HasPresentationLayer() const override { return false; }

	void DrawClx(Point /*position*/, ClxSprite /*clx*/) override { }
	void DrawClxTRN(Point /*position*/, ClxSprite /*clx*/, const uint8_t * /*trn*/) override { }
	void DrawClxBlended(Point /*position*/, ClxSprite /*clx*/) override { }
	void DrawClxBlendedTRN(Point /*position*/, ClxSprite /*clx*/, const uint8_t * /*trn*/) override { }
	void DrawClxLit(Point /*position*/, ClxSprite /*clx*/, int /*lightTableIndex*/) override { }
	void DrawClxLitBlended(Point /*position*/, ClxSprite /*clx*/, int /*lightTableIndex*/) override { }
	void DrawClxOutline(uint8_t /*col*/, Point /*position*/, ClxSprite /*clx*/, bool /*skipColorIndexZero*/) override { }

	void DrawLevelTile(Point /*position*/,
	    uint16_t /*levelPieceId*/, int /*lightTableIndex*/,
	    bool /*transparency*/, TileProperties /*tileProperties*/, bool /*isFloor*/) override { }
	void DrawFloorLevelTile(Point /*position*/,
	    uint16_t /*levelPieceId*/, int /*lightTableIndex*/) override { }
	void DrawBlackTile(int /*sx*/, int /*sy*/) override { }

	void FillRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, uint8_t /*colorIndex*/) override { }
	void DrawBlendedRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/) override { }
	void DrawPixel(Point /*position*/, uint8_t /*colorIndex*/) override { }
	void DrawBlendedPixel(Point /*position*/, uint8_t /*colorIndex*/) override { }
	void DrawLine(Point /*from*/, Point /*to*/, uint8_t /*colorIndex*/) override { }
	void DrawHorizontalLine(Point /*from*/, int /*width*/, uint8_t /*colorIndex*/) override { }
	void DrawVerticalLine(Point /*from*/, int /*height*/, uint8_t /*colorIndex*/) override { }
	void DrawBlendedHorizontalLine(Point /*from*/, int /*width*/, uint8_t /*colorIndex*/) override { }
	void DrawBlendedVerticalLine(Point /*from*/, int /*height*/, uint8_t /*colorIndex*/) override { }
	void TintRect(int /*x*/, int /*y*/, int /*w*/, int /*h*/, uint8_t /*colorIndex*/) override { }
	void ApplyTRN(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const uint8_t * /*trn*/, uint8_t /*skipColorIndicesBelow*/) override { }

	void BlitSurface(const Surface & /*src*/, SDL_Rect /*srcRect*/, Point /*targetPosition*/) override { }
	void BlitSurfaceSkipColorIndexZero(const Surface & /*src*/, SDL_Rect /*srcRect*/, Point /*targetPosition*/) override { }

	void SetClipRegion(int /*x*/, int /*y*/, int /*w*/, int /*h*/) override { }
	void ClearClipRegion() override { }

	void BeginViewportZoom() override { }
	void EndViewportZoom(int /*viewportHeight*/) override { }

	void NotifyPaletteChanged(int /*first*/, int /*ncolor*/) override { }

	void InvalidateTextureCache() override { }
	void ClearTextureCache() override { }

	void HandleResize(int /*windowWidth*/, int /*windowHeight*/) override { }
	void ReinitializeTexture() override { }
	void SetIntegerScale(bool /*enable*/) override { }
	void PrepareVideoPlayback(unsigned /*width*/, unsigned /*height*/) override { }
	bool DrawVideoFrame(SDL_Surface * /*frame*/, unsigned /*width*/, unsigned /*height*/) override { return true; }
	void FinishVideoPlayback() override { }
	void NotifyVideoPaletteChanged(SDL_Palette * /*palette*/) override { }
	void RenderToAllBackBuffers(std::function<void()> drawFn) override { drawFn(); }
	void ReinitVirtualGamepad() override { }
	SDLSurfaceUniquePtr CaptureScreen() override { return nullptr; }
	void MapWindowToLogical(int * /*x*/, int * /*y*/) const override { }
	void MapLogicalToWindow(int * /*x*/, int * /*y*/) const override { }
	void ConvertEventCoordinates(SDL_Event & /*event*/) const override { }
};

} // namespace devilution