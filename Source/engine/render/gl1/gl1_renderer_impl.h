/**
 * @file gl1_renderer_impl.h
 *
 * Gl1Renderer: implements the Renderer interface by forwarding to the
 * existing Gl1* free functions.
 */
#pragma once

#ifdef DEVILUTIONX_GL1_RENDERER

#include "engine/backbuffer_state.hpp"

#include <cstring>
#include <vector>

#include <GL/gl.h>

#include "appfat.h"
#include "diablo.h"
#include "engine/frame_limiter.h"
#include "engine/palette.h"
#include "engine/render/fade_controller.h"
#include "engine/render/gl1/gl1_renderer.h"
#include "engine/render/renderer.h"
#include "engine/render/software/light_render_internal.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "options.h"
#include "utils/display.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"
#include "utils/ui_fwd.h"

namespace devilution {

class Gl1Renderer final : public Renderer {
public:
	void Init(SDL_Window *window) override
	{
		Gl1Init(window);
		CreateBackBuffer();
		SetLightingMode(*GetOptions().Graphics.lightingMode);
	}

	void CreateBackBuffer() override
	{
		pinnedPalSurface_ = SDLWrap::CreateRGBSurfaceWithFormat(
		    /*flags=*/0,
		    /*width=*/gnScreenWidth,
		    /*height=*/gnScreenHeight,
		    /*depth=*/8,
		    SDL_PIXELFORMAT_INDEX8);
		palSurface_ = pinnedPalSurface_.get();
#if defined(USE_SDL3)
		if (!SDL_SetSurfacePalette(palSurface_, palette_.get()))
			ErrSdl();
#elif !defined(USE_SDL1)
		if (SDL_SetSurfacePalette(palSurface_, palette_.get()) < 0)
			ErrSdl();
#endif
	}

	void InitPalette() override
	{
		if (palette_ == nullptr)
			palette_ = SDLWrap::AllocPalette();
	}

	SDL_Palette *GetPalette() const { return palette_.get(); }

	void BeginScene(Point /*position*/, Point /*targetBufferPosition*/, int /*rows*/, int /*columns*/) override { }
	void EndScene() override { }

	void SetLightingMode(LightingMode mode) override { lightingMode_ = mode; }
	Size GetBackBufferSize() const override { return { palSurface_->w, palSurface_->h }; }
	void *GetBackBufferKey() const override { return nullptr; }

	void Shutdown() override
	{
		Gl1Shutdown();
	}

	void OnVideoModeChanged() override
	{
		// On SDL1, SDL_SetVideoMode destroys the GL context.
		// Re-initialize GL state and clear the texture cache.
		Gl1Shutdown();
		Gl1Init(ghMainWnd);
		CreateBackBuffer();
		NotifyPaletteChanged(0, 256);
	}

	void BeginFrame() override
	{
		RedrawEverything();
		Gl1BeginFrame();
	}

	void EndFrame() override
	{
		DrawCursorGL();
		const int fadeLevel = fade_.Update();
		if (fadeLevel >= 0)
			Gl1ApplyFade(fadeLevel);
		Gl1EndFrame();
#ifndef USE_SDL1
		if (*GetOptions().Graphics.frameRateControl != FrameRateControl::VerticalSync) {
			LimitFrameRate();
		}
#else
		LimitFrameRate();
#endif
	}

	void ClearScreen() override
	{
		Gl1FillRect(0, 0, gnScreenWidth, gnScreenHeight, 0);
	}

	void SetCursor(SDL_Surface *surface, Point hotpoint) override
	{
		cursorSurface_ = surface;
		cursorHotpoint_ = hotpoint;
		UploadCursorTexture();
	}

	void SetCursorVisible(bool visible) override
	{
		cursorVisible_ = visible;
	}

	void MarkDirtyRect(Rectangle /*area*/) override { }
	void MarkScreenDirty() override { }

	bool NeedsFullRedraw() const override
	{
		return true;
	}

	bool SupportsLightingMode(LightingMode mode) const override
	{
		return mode == LightingMode::Vertex || mode == LightingMode::Tile;
	}

	bool HasPresentationLayer() const override { return true; }

	void BlackOutScreen() override { fade_.BlackOutScreen(); }
	void StartFadeIn(int fr) override { fade_.StartFadeIn(fr); }
	void StartFadeOut(int fr) override { fade_.StartFadeOut(fr); }
	bool IsFading() const override { return fade_.IsFading(); }
	int GetFadeLevel() const override { return fade_.GetFadeLevel(); }
	bool IsBlackedOut() const override { return fade_.IsBlackedOut(); }

protected:
	void DrawClx(Point position, ClxSprite clx) override
	{
		Gl1DrawClx(position, clx);
	}

	void DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn) override
	{
		Gl1DrawClxTRN(position, clx, trn);
	}

	void DrawClxBlended(Point position, ClxSprite clx) override
	{
		Gl1DrawClxBlended(position, clx);
	}

	void DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn) override
	{
		Gl1DrawClxBlendedTRN(position, clx, trn);
	}

	void DrawClxLit(Point position, ClxSprite clx, int lightTableIndex) override
	{
		if (lightingMode_ == LightingMode::Vertex) {
			Gl1DrawClxWithLight(position, clx, static_cast<uint8_t>(lightTableIndex));
		} else if (lightTableIndex != 0) {
			Gl1DrawClxTRN(position, clx, LightTables[lightTableIndex].data());
		} else {
			Gl1DrawClx(position, clx);
		}
	}

	void DrawClxLitBlended(Point position, ClxSprite clx, int lightTableIndex) override
	{
		if (lightingMode_ == LightingMode::Vertex) {
			Gl1DrawClxWithLightBlended(position, clx, static_cast<uint8_t>(lightTableIndex));
		} else if (lightTableIndex != 0) {
			Gl1DrawClxBlendedTRN(position, clx, LightTables[lightTableIndex].data());
		} else {
			Gl1DrawClxBlended(position, clx);
		}
	}

	void DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero) override
	{
		Gl1DrawClxOutline(col, position, clx, skipColorIndexZero);
	}

	void DrawLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex,
	    bool transparency, TileProperties tileProperties, bool isFloor) override
	{
		const MICROS *pMap = &DPieceMicros[levelPieceId];
		const uint8_t *tbl = LightTables[lightTableIndex].data();

		const auto getMaskLeft = [&](TileType tile) -> MaskType {
			if (transparency) {
				switch (tile) {
				case TileType::LeftTrapezoid:
				case TileType::TransparentSquare:
					return HasAnyOf(tileProperties, TileProperties::TransparentLeft)
					    ? MaskType::Left
					    : MaskType::Solid;
				case TileType::LeftTriangle:
					return MaskType::Solid;
				default:
					return MaskType::Transparent;
				}
			}
			return MaskType::Solid;
		};

		const auto getMaskRight = [&](TileType tile) -> MaskType {
			if (transparency) {
				switch (tile) {
				case TileType::RightTrapezoid:
				case TileType::TransparentSquare:
					return HasAnyOf(tileProperties, TileProperties::TransparentRight)
					    ? MaskType::Right
					    : MaskType::Solid;
				case TileType::RightTriangle:
					return MaskType::Solid;
				default:
					return MaskType::Transparent;
				}
			}
			return MaskType::Solid;
		};

		// First row (bottom two micro-tiles)
		if (const LevelCelBlock levelCelBlock { pMap->mt[0] }; levelCelBlock.hasValue()) {
			const TileType tileType = levelCelBlock.type();
			if (!isFloor || tileType == TileType::TransparentSquare) {
				if (isFloor && tileType == TileType::TransparentSquare) {
					// Foliage: render the foliage portion of the cel
					Gl1DrawTile(Point { position.x, position.y - 16 }, TileType::TransparentSquare,
					    GetDunFrameFoliage(pDungeonCels.get(), levelCelBlock.frame()),
					    16, MaskType::Solid, tbl, lightingMode_);
				} else {
					Gl1DrawTile(position, tileType,
					    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
					    (tileType == TileType::LeftTriangle || tileType == TileType::RightTriangle)
					        ? DunFrameTriangleHeight
					        : DunFrameHeight,
					    getMaskLeft(tileType), tbl, lightingMode_);
				}
			}
		}
		if (const LevelCelBlock levelCelBlock { pMap->mt[1] }; levelCelBlock.hasValue()) {
			const TileType tileType = levelCelBlock.type();
			if (!isFloor || tileType == TileType::TransparentSquare) {
				if (isFloor && tileType == TileType::TransparentSquare) {
					Gl1DrawTile(Point { position.x + DunFrameWidth, position.y - 16 }, TileType::TransparentSquare,
					    GetDunFrameFoliage(pDungeonCels.get(), levelCelBlock.frame()),
					    16, MaskType::Solid, tbl, lightingMode_);
				} else {
					Gl1DrawTile(position + Displacement { DunFrameWidth, 0 }, tileType,
					    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
					    (tileType == TileType::LeftTriangle || tileType == TileType::RightTriangle)
					        ? DunFrameTriangleHeight
					        : DunFrameHeight,
					    getMaskRight(tileType), tbl, lightingMode_);
				}
			}
		}
		position.y -= TILE_HEIGHT;

		// Upper rows
		for (uint_fast8_t i = 2, n = MicroTileLen; i < n; i += 2) {
			if (const LevelCelBlock levelCelBlock { pMap->mt[i] }; levelCelBlock.hasValue()) {
				const TileType tileType = levelCelBlock.type();
				Gl1DrawTile(position, tileType,
				    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
				    (tileType == TileType::LeftTriangle || tileType == TileType::RightTriangle)
				        ? DunFrameTriangleHeight
				        : DunFrameHeight,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl, lightingMode_);
			}
			if (const LevelCelBlock levelCelBlock { pMap->mt[i + 1] }; levelCelBlock.hasValue()) {
				const TileType tileType = levelCelBlock.type();
				Gl1DrawTile(position + Displacement { DunFrameWidth, 0 }, tileType,
				    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
				    (tileType == TileType::LeftTriangle || tileType == TileType::RightTriangle)
				        ? DunFrameTriangleHeight
				        : DunFrameHeight,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl, lightingMode_);
			}
			position.y -= TILE_HEIGHT;
		}
	}

	void DrawFloorLevelTile(Point position,
	    uint16_t levelPieceId, int lightTableIndex) override
	{
		const uint8_t *tbl = LightTables[lightTableIndex].data();
		const MICROS &micros = DPieceMicros[levelPieceId];

		if (const LevelCelBlock levelCelBlock { micros.mt[0] }; levelCelBlock.hasValue()) {
			Gl1DrawTile(position, TileType::LeftTriangle,
			    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
			    DunFrameTriangleHeight, MaskType::Solid, tbl, lightingMode_);
		}
		if (const LevelCelBlock levelCelBlock { micros.mt[1] }; levelCelBlock.hasValue()) {
			Gl1DrawTile(position + Displacement { DunFrameWidth, 0 }, TileType::RightTriangle,
			    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()),
			    DunFrameTriangleHeight, MaskType::Solid, tbl, lightingMode_);
		}
	}

	void DrawBlackTile(int sx, int sy) override
	{
		Gl1DrawBlackTile(sx, sy);
	}

	void FillRect(int x, int y, int w, int h, uint8_t colorIndex) override
	{
		Gl1FillRect(x, y, w, h, colorIndex);
	}

	void DrawBlendedRect(int x, int y, int w, int h) override
	{
		Gl1DrawBlendedRect(x, y, w, h);
	}

	void DrawPixel(Point position, uint8_t colorIndex) override
	{
		Gl1DrawPixel(position, colorIndex);
	}

	void DrawBlendedPixel(Point position, uint8_t colorIndex) override
	{
		Gl1DrawBlendedPixel(position, colorIndex);
	}

	void DrawLine(Point from, Point to, uint8_t colorIndex) override
	{
		Gl1DrawLine(from, to, colorIndex);
	}

	void DrawHorizontalLine(Point from, int width, uint8_t colorIndex) override
	{
		Gl1DrawHorizontalLine(from, width, colorIndex);
	}

	void DrawVerticalLine(Point from, int height, uint8_t colorIndex) override
	{
		Gl1DrawVerticalLine(from, height, colorIndex);
	}

	void DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex) override
	{
		Gl1DrawBlendedHorizontalLine(from, width, colorIndex);
	}

	void DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex) override
	{
		Gl1DrawBlendedVerticalLine(from, height, colorIndex);
	}

	void TintRect(int x, int y, int w, int h, uint8_t colorIndex) override
	{
		Gl1TintRect(x, y, w, h, colorIndex);
	}

	void ApplyTRN(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const uint8_t * /*trn*/, uint8_t /*skipColorIndicesBelow*/) override
	{
		// TODO: Generalize — currently only supports the pause red-tint effect.
		Gl1DrawRedBack();
	}

	void BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition) override
	{
		Gl1BlitSurface(src, srcRect, targetPosition);
	}

	void BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) override
	{
		Gl1BlitSurfaceSkipColorIndexZero(src, srcRect, targetPosition);
	}

	void SetClipRegion(int x, int y, int w, int h) override
	{
		Gl1SetClipRegion(x, y, w, h);
	}

	void ClearClipRegion() override
	{
		Gl1ClearClipRegion();
	}

	void BeginViewportZoom() override
	{
		if (*GetOptions().Graphics.zoom)
			Gl1SetZoom();
	}

	void EndViewportZoom(int /*viewportHeight*/) override
	{
		if (*GetOptions().Graphics.zoom)
			Gl1ClearZoom();
	}

	void NotifyPaletteChanged(int first, int ncolor) override
	{
		// Update the SDL Palette object — needed for hardware cursor surfaces.
		if (palette_) {
			SDLC_SetSurfaceAndPaletteColors(palSurface_, palette_.get(), system_palette.data() + first, first, ncolor);
		}
		if (first == 0 && ncolor == 256) {
			Gl1NotifyFullPaletteChanged();
		} else {
			Gl1NotifyPaletteChanged();
		}
	}

	void InvalidateTextureCache() override
	{
		Gl1InvalidateTextureCache();
	}

	void ClearTextureCache() override
	{
		Gl1ClearTextureCache();
	}

	void HandleResize(int windowWidth, int windowHeight) override
	{
		Gl1HandleResize(windowWidth, windowHeight);
		AdjustToScreenGeometry({ windowWidth, windowHeight });
	}

	void ReinitializeTexture() override { }
	void SetIntegerScale(bool /*enable*/) override { }

	void MapWindowToLogical(int *x, int *y) const override
	{
		Gl1MapWindowToLogical(x, y);
	}

	void MapLogicalToWindow(int *x, int *y) const override
	{
		Gl1MapLogicalToWindow(x, y);
	}

	Size GetOutputSize() const override
	{
		return Gl1GetOutputSize();
	}

	Size GetScaledCursorSize(Size logicalSize) const override
	{
		return Gl1GetScaledCursorSize(logicalSize);
	}

	SDLSurfaceUniquePtr CaptureScreen() override
	{
		const Size size = Gl1GetOutputSize();
		SDLSurfaceUniquePtr rgba = SDLWrap::CreateRGBSurfaceWithFormat(
#ifdef USE_SDL1
		    0, size.width, size.height, 32, SDL_PIXELFORMAT_RGBA8888);
#else
		    0, size.width, size.height, 32, SDL_PIXELFORMAT_RGBA32);
#endif
		glFinish();
		glReadPixels(0, 0, size.width, size.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
		// OpenGL origin is bottom-left; flip vertically for SDL top-left convention.
		const int pitch = rgba->pitch;
		std::vector<uint8_t> rowBuffer(pitch);
		uint8_t *pixels = static_cast<uint8_t *>(rgba->pixels);
		for (int y = 0; y < size.height / 2; ++y) {
			uint8_t *top = pixels + y * pitch;
			uint8_t *bot = pixels + (size.height - 1 - y) * pitch;
			std::memcpy(rowBuffer.data(), top, pitch);
			std::memcpy(top, bot, pitch);
			std::memcpy(bot, rowBuffer.data(), pitch);
		}
		return rgba;
	}

	void ConvertEventCoordinates(SDL_Event &event) const override
	{
#ifdef USE_SDL3
		if (event.type == SDL_EVENT_MOUSE_MOTION) {
			int x = static_cast<int>(event.motion.x);
			int y = static_cast<int>(event.motion.y);
			MapWindowToLogical(&x, &y);
			event.motion.x = static_cast<float>(x);
			event.motion.y = static_cast<float>(y);
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			int x = static_cast<int>(event.button.x);
			int y = static_cast<int>(event.button.y);
			MapWindowToLogical(&x, &y);
			event.button.x = static_cast<float>(x);
			event.button.y = static_cast<float>(y);
		}
#elif !defined(USE_SDL1)
		if (event.type == SDL_MOUSEMOTION) {
			MapWindowToLogical(&event.motion.x, &event.motion.y);
		} else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
			MapWindowToLogical(&event.button.x, &event.button.y);
		}
#else
		if (event.type == SDL_MOUSEMOTION) {
			int x = event.motion.x;
			int y = event.motion.y;
			MapWindowToLogical(&x, &y);
			event.motion.x = static_cast<Uint16>(x);
			event.motion.y = static_cast<Uint16>(y);
		} else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
			int x = event.button.x;
			int y = event.button.y;
			MapWindowToLogical(&x, &y);
			event.button.x = static_cast<Uint16>(x);
			event.button.y = static_cast<Uint16>(y);
		}
#endif
	}

	bool DrawVideoFrame(SDL_Surface *frame, unsigned width, unsigned height) override
	{
		Gl1DrawVideoFrame(frame, width, height);
		GetRenderer().EndFrame();
		return true;
	}

	void PrepareVideoPlayback(unsigned /*width*/, unsigned /*height*/) override { }
	void FinishVideoPlayback() override { }
	void NotifyVideoPaletteChanged(SDL_Palette * /*palette*/) override { }
	void RenderToAllBackBuffers(std::function<void()> drawFn) override { drawFn(); }
	void ReinitVirtualGamepad() override { }

private:
	void UploadCursorTexture()
	{
		if (cursorTexId_ != 0) {
			glDeleteTextures(1, &cursorTexId_);
			cursorTexId_ = 0;
		}
		cursorTexW_ = 0;
		cursorTexH_ = 0;

		if (cursorSurface_ == nullptr)
			return;

		const int w = cursorSurface_->w;
		const int h = cursorSurface_->h;
		if (w <= 0 || h <= 0)
			return;

		// Determine the transparent color index.
		uint8_t transparentColor = 0;
#ifdef USE_SDL3
		{
			Uint32 key = 0;
			SDL_GetSurfaceColorKey(cursorSurface_, &key);
			transparentColor = static_cast<uint8_t>(key);
		}
#elif defined(USE_SDL1)
		transparentColor = static_cast<uint8_t>(cursorSurface_->format->colorkey);
#else
		{
			Uint32 key = 0;
			if (SDL_GetColorKey(cursorSurface_, &key) == 0)
				transparentColor = static_cast<uint8_t>(key);
		}
#endif

		// Convert 8-bit indexed surface to RGBA using system_palette.
		std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
		const uint8_t *pixels = static_cast<const uint8_t *>(cursorSurface_->pixels);
		for (int row = 0; row < h; row++) {
			const uint8_t *srcRow = pixels + row * cursorSurface_->pitch;
			uint8_t *dstRow = rgba.data() + static_cast<size_t>(row) * w * 4;
			for (int col = 0; col < w; col++) {
				const uint8_t index = srcRow[col];
				if (index == transparentColor) {
					dstRow[col * 4 + 0] = 0;
					dstRow[col * 4 + 1] = 0;
					dstRow[col * 4 + 2] = 0;
					dstRow[col * 4 + 3] = 0;
				} else {
					const SDL_Color &c = system_palette[index];
					dstRow[col * 4 + 0] = c.r;
					dstRow[col * 4 + 1] = c.g;
					dstRow[col * 4 + 2] = c.b;
					dstRow[col * 4 + 3] = 255;
				}
			}
		}

		glGenTextures(1, &cursorTexId_);
		glBindTexture(GL_TEXTURE_2D, cursorTexId_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		cursorTexW_ = w;
		cursorTexH_ = h;
	}

	void DrawCursorGL()
	{
		if (!cursorVisible_ || cursorTexId_ == 0)
			return;

		const int x = MousePosition.x - cursorHotpoint_.x;
		const int y = MousePosition.y - cursorHotpoint_.y;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(1, 1, 1, 1);

		glBindTexture(GL_TEXTURE_2D, cursorTexId_);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(x, y);
		glTexCoord2f(1, 0);
		glVertex2i(x + cursorTexW_, y);
		glTexCoord2f(1, 1);
		glVertex2i(x + cursorTexW_, y + cursorTexH_);
		glTexCoord2f(0, 1);
		glVertex2i(x, y + cursorTexH_);
		glEnd();

		glDisable(GL_BLEND);
	}

	FadeController fade_;
	LightingMode lightingMode_ = LightingMode::Vertex;
	SDL_Surface *cursorSurface_ = nullptr;
	Point cursorHotpoint_;
	bool cursorVisible_ = false;
	GLuint cursorTexId_ = 0;
	int cursorTexW_ = 0;
	int cursorTexH_ = 0;
	SDLPaletteUniquePtr palette_;
	SDLSurfaceUniquePtr pinnedPalSurface_;
	SDL_Surface *palSurface_ = nullptr;
};

} // namespace devilution

#endif // DEVILUTIONX_GL1_RENDERER
