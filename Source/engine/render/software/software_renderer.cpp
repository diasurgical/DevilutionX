/**
 * @file software_renderer.cpp
 *
 * Implementation of the SoftwareRenderer class — the CPU-based software
 * rendering backend. Lifecycle, frame-present, palette, resize, and clear
 * methods are implemented here. Draw methods are left as empty stubs to be
 * filled in a later phase.
 */
#include "engine/render/software/software_renderer.h"

#include <array>
#include <cstdlib>
#include <cstring>

#ifdef USE_SDL3
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#else
#include <SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "appfat.h"
#include "control/control.hpp"
#include "controls/control_mode.hpp"
#include "cursor.h"
#include "diablo.h"

#include "engine/frame_limiter.h"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/software/dun_render_internal.hpp"
#include "engine/render/software/light_render_internal.hpp"
#include "engine/surface.hpp"
#include "headless_mode.hpp"
#include "init.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "options.h"
#include "utils/display.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"
#include "utils/str_cat.hpp"

#ifndef USE_SDL1
#include "controls/touch/renderers.h"
#endif

namespace devilution {

SDL_Surface *SoftwareRenderer::GetOutputSurface() const
{
#ifdef USE_SDL1
	SDL_Surface *ret = SDL_GetVideoSurface();
	if (ret == nullptr)
		ErrSdl();
	return ret;
#else
	if (sdlRenderer_ != nullptr)
		return rendererTextureSurface_.get();
	SDL_Surface *ret = SDL_GetWindowSurface(ghMainWnd);
	if (ret == nullptr)
		ErrSdl();
	return ret;
#endif
}

bool SoftwareRenderer::OutputRequiresScaling() const
{
#ifdef USE_SDL1
	if (HeadlessMode)
		return false;
	return gnScreenWidth != GetOutputSurface()->w || gnScreenHeight != GetOutputSurface()->h;
#else
	return false;
#endif
}

void SoftwareRenderer::ScaleOutputRect(SDL_Rect *rect) const
{
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	rect->x = rect->x * surface->w / gnScreenWidth;
	rect->y = rect->y * surface->h / gnScreenHeight;
	rect->w = rect->w * surface->w / gnScreenWidth;
	rect->h = rect->h * surface->h / gnScreenHeight;
}

void SoftwareRenderer::BlitToOutput(SDL_Surface *src, SDL_Rect *srcRect, SDL_Rect *dstRect)
{
	if (HeadlessMode)
		return;

	SDL_Surface *dst = GetOutputSurface();
#if defined(USE_SDL3)
	if (!SDL_BlitSurface(src, srcRect, dst, dstRect)) ErrSdl();
#elif !defined(USE_SDL1)
	if (SDL_BlitSurface(src, srcRect, dst, dstRect) < 0)
		ErrSdl();
#else
	if (!OutputRequiresScaling()) {
		if (SDL_BlitSurface(src, srcRect, dst, dstRect) < 0)
			ErrSdl();
		return;
	}

	SDL_Rect scaledDstRect;
	if (dstRect != NULL) {
		scaledDstRect = *dstRect;
		ScaleOutputRect(&scaledDstRect);
		dstRect = &scaledDstRect;
	}

	// Same pixel format: We can call BlitScaled directly.
	if (SDLBackport_PixelFormatFormatEq(src->format, dst->format)) {
		if (SDL_BlitScaled(src, srcRect, dst, dstRect) < 0)
			ErrSdl();
		return;
	}

	// If the surface has a color key, we must stretch first and can then call BlitSurface.
	if (SDL_HasColorKey(src)) {
		SDLSurfaceUniquePtr stretched = SDLWrap::CreateRGBSurface(SDL_SWSURFACE, dstRect->w, dstRect->h, src->format->BitsPerPixel,
		    src->format->Rmask, src->format->Gmask, src->format->BitsPerPixel, src->format->Amask);
		SDL_SetColorKey(stretched.get(), SDL_SRCCOLORKEY, src->format->colorkey);
		if (src->format->palette != NULL)
			SDL_SetPalette(stretched.get(), SDL_LOGPAL, src->format->palette->colors, 0, src->format->palette->ncolors);
		SDL_Rect stretched_rect = { 0, 0, dstRect->w, dstRect->h };
		if (SDL_SoftStretch(src, srcRect, stretched.get(), &stretched_rect) < 0
		    || SDL_BlitSurface(stretched.get(), &stretched_rect, dst, dstRect) < 0) {
			ErrSdl();
		}
		return;
	}

	// A surface with a non-output pixel format but without a color key needs scaling.
	// We can convert the format and then call BlitScaled.
	SDLSurfaceUniquePtr converted = SDLWrap::ConvertSurface(src, dst->format, 0);
	if (SDL_BlitScaled(converted.get(), srcRect, dst, dstRect) < 0)
		ErrSdl();
#endif
}

void SoftwareRenderer::Init(SDL_Window * /*window*/)
{
	CreateBackBuffer();
}

void SoftwareRenderer::InitPalette()
{
	if (palette_ == nullptr)
		palette_ = SDLWrap::AllocPalette();
}

void SoftwareRenderer::CreateBackBuffer()
{
	renderDirectlyToOutput_ = false;
#ifndef USE_SDL1
	// When upscale is enabled, HandleResize will create an SDL_Renderer.
	// We must not call SDL_GetWindowSurface before that (they're mutually exclusive).
	const bool willUseSDLRenderer = *GetOptions().Graphics.upscale;
#else
	const bool willUseSDLRenderer = false;
#endif
	if (!willUseSDLRenderer
#ifndef USE_SDL1
	    && GetSDLRenderer() == nullptr
#endif
	) {
#ifdef USE_SDL1
#ifdef SDL1_FORCE_DIRECT_RENDER
		renderDirectlyToOutput_ = true;
#else
		auto *outputSurface = GetOutputSurface();
		renderDirectlyToOutput_ = ((outputSurface->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF
		    && outputSurface->w == gnScreenWidth && outputSurface->h == gnScreenHeight
		    && outputSurface->format->BitsPerPixel == 8);
#endif
#else // !USE_SDL1
		SDL_Surface *outputSurface = GetOutputSurface();
		// Assumes double-buffering is available.
		renderDirectlyToOutput_ = outputSurface->w == static_cast<int>(gnScreenWidth)
		    && outputSurface->h == static_cast<int>(gnScreenHeight)
		    && SDLC_SURFACE_BITSPERPIXEL(outputSurface) == 8;
#endif
	}

	if (renderDirectlyToOutput_) {
		Log("{}", "Will render directly to the SDL output surface");
		palSurface_ = GetOutputSurface();
	} else {
		pinnedPalSurface_ = SDLWrap::CreateRGBSurfaceWithFormat(
		    /*flags=*/0,
		    /*width=*/gnScreenWidth,
		    /*height=*/gnScreenHeight,
		    /*depth=*/8,
		    SDL_PIXELFORMAT_INDEX8);
		palSurface_ = pinnedPalSurface_.get();
	}

#if defined(USE_SDL3)
	if (!SDL_SetSurfacePalette(palSurface_, palette_.get()))
		ErrSdl();
#elif !defined(USE_SDL1)
	if (SDL_SetSurfacePalette(palSurface_, palette_.get()) < 0)
		ErrSdl();
#else
	// In SDL1, PalSurface owns its palette and we must update it every
	// time the global palette is changed. No need to do anything here as
	// the global palette doesn't have any colors set yet.
#endif
}

void SoftwareRenderer::Shutdown()
{
	palSurface_ = nullptr;
	pinnedPalSurface_ = nullptr;
	rendererTextureSurface_ = nullptr;
#ifndef USE_SDL1
	texture_ = nullptr;
	FreeVirtualGamepadTextures();
	if (*GetOptions().Graphics.upscale && sdlRenderer_ != nullptr) {
		SDL_DestroyRenderer(sdlRenderer_);
		sdlRenderer_ = nullptr;
	}
#endif
}

bool SoftwareRenderer::NeedsFullRedraw() const
{
	return false;
}

bool SoftwareRenderer::SupportsLightingMode(LightingMode mode) const
{
	return mode == LightingMode::Tile || mode == LightingMode::TileSmooth;
}

void SoftwareRenderer::EndViewportZoom(int viewportHeight)
{
	if (!*GetOptions().Graphics.zoom)
		return;

	Surface fullOut(palSurface_, SDL_Rect { 0, 0, static_cast<int>(gnScreenWidth), static_cast<int>(gnScreenHeight) });
	Surface out = fullOut.subregionY(0, viewportHeight);

	int viewportWidth = out.w();
	int viewportOffsetX = 0;
	if (CanPanelsCoverView()) {
		if (IsLeftPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
			viewportOffsetX = SidePanelSize.width;
		} else if (IsRightPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
		}
	}

	const int srcWidth = (viewportWidth + 1) / 2;
	const int doubleableWidth = viewportWidth / 2;
	const int srcHeight = (out.h() + 1) / 2;
	const int doubleableHeight = out.h() / 2;

	uint8_t *src = out.at(srcWidth - 1, srcHeight - 1);
	uint8_t *dst = out.at(viewportOffsetX + viewportWidth - 1, out.h() - 1);
	const bool oddViewportWidth = (viewportWidth % 2) == 1;

	for (int hgt = 0; hgt < doubleableHeight; hgt++) {
		for (int i = 0; i < doubleableWidth; i++) {
			*dst-- = *src;
			*dst-- = *src;
			--src;
		}
		if (oddViewportWidth) {
			*dst-- = *src;
			--src;
		}
		src -= (out.pitch() - srcWidth);
		memcpy(dst - out.pitch() + 1, dst + 1, viewportWidth);
		dst -= 2 * out.pitch() - viewportWidth;
	}
	if ((out.h() % 2) == 1) {
		memcpy(dst - out.pitch() + 1, dst + 1, viewportWidth);
	}
}

void SoftwareRenderer::SetCursor(SDL_Surface *surface, Point hotpoint)
{
	cursorSurface_ = surface;
	cursorHotpoint_ = hotpoint;
	if (surface != nullptr) {
#ifdef USE_SDL3
		Uint32 key = 0;
		SDL_GetSurfaceColorKey(surface, &key);
		cursorTransparentColor_ = static_cast<uint8_t>(key);
#elif defined(USE_SDL1)
		cursorTransparentColor_ = static_cast<uint8_t>(surface->format->colorkey);
#else
		Uint32 key = 0;
		if (SDL_GetColorKey(surface, &key) == 0)
			cursorTransparentColor_ = static_cast<uint8_t>(key);
		else
			cursorTransparentColor_ = 0;
#endif
	}
}

void SoftwareRenderer::SetCursorVisible(bool visible)
{
	cursorVisible_ = visible;
}

void SoftwareRenderer::UndrawCursorFromBuffer()
{
	if (!cursorDrawn_ || palSurface_ == nullptr)
		return;

	const Rectangle &rect = cursorRect_;
	if (rect.size.width == 0 || rect.size.height == 0)
		return;

	uint8_t *dst = static_cast<uint8_t *>(palSurface_->pixels)
	    + rect.position.y * palSurface_->pitch + rect.position.x;
	const uint8_t *src = cursorBehindBuffer_;
	for (int row = 0; row < rect.size.height; ++row) {
		memcpy(dst, src, rect.size.width);
		dst += palSurface_->pitch;
		src += rect.size.width;
	}

	prevCursorRect_ = cursorRect_;
	cursorDrawn_ = false;
}

void SoftwareRenderer::DrawCursorToBuffer()
{
	if (!cursorVisible_ || cursorSurface_ == nullptr || palSurface_ == nullptr) {
		cursorRect_.size = { 0, 0 };
		return;
	}

	const int cursorW = cursorSurface_->w;
	const int cursorH = cursorSurface_->h;
	if (cursorW == 0 || cursorH == 0) {
		cursorRect_.size = { 0, 0 };
		return;
	}

	// Compute the top-left position of the cursor on palSurface_.
	int x = MousePosition.x - cursorHotpoint_.x;
	int y = MousePosition.y - cursorHotpoint_.y;

	// Clip to palSurface_ bounds.
	int srcX = 0, srcY = 0;
	int w = cursorW, h = cursorH;

	if (x < 0) {
		srcX = -x;
		w += x;
		x = 0;
	}
	if (y < 0) {
		srcY = -y;
		h += y;
		y = 0;
	}
	if (x + w > palSurface_->w)
		w = palSurface_->w - x;
	if (y + h > palSurface_->h)
		h = palSurface_->h - y;

	if (w <= 0 || h <= 0) {
		cursorRect_.size = { 0, 0 };
		return;
	}

	// Check that the behind buffer is large enough.
	if (static_cast<size_t>(w) * static_cast<size_t>(h) > CursorBehindBufferSize) {
		cursorRect_.size = { 0, 0 };
		return;
	}

	cursorRect_ = { { x, y }, { w, h } };

	// Save pixels behind cursor.
	const uint8_t *palPixels = static_cast<const uint8_t *>(palSurface_->pixels)
	    + y * palSurface_->pitch + x;
	uint8_t *behind = cursorBehindBuffer_;
	for (int row = 0; row < h; ++row) {
		memcpy(behind, palPixels, w);
		palPixels += palSurface_->pitch;
		behind += w;
	}

	// Blit cursor surface onto palSurface_ preserving palette indices.
	// We cannot use SDL_BlitSurface because blitting between two 8-bit
	// indexed surfaces performs palette color matching, remapping indices.
	// Instead, copy indices directly, skipping the transparent color.
	const uint8_t *cursorPixels = static_cast<const uint8_t *>(cursorSurface_->pixels)
	    + srcY * cursorSurface_->pitch + srcX;
	uint8_t *dst = static_cast<uint8_t *>(palSurface_->pixels)
	    + y * palSurface_->pitch + x;
	for (int row = 0; row < h; ++row) {
		for (int col = 0; col < w; ++col) {
			const uint8_t pixel = cursorPixels[col];
			if (pixel != cursorTransparentColor_)
				dst[col] = pixel;
		}
		cursorPixels += cursorSurface_->pitch;
		dst += palSurface_->pitch;
	}

	cursorDrawn_ = true;
}

void SoftwareRenderer::BeginFrame()
{
	UndrawCursorFromBuffer();
	numDirtyRects_ = 0;
	fullScreenDirty_ = false;
}

void SoftwareRenderer::MarkDirtyRect(Rectangle area)
{
	if (fullScreenDirty_)
		return;
	if (numDirtyRects_ >= MaxDirtyRects) {
		fullScreenDirty_ = true;
		return;
	}
	dirtyRects_[numDirtyRects_++] = MakeSdlRect(area);
}

void SoftwareRenderer::MarkScreenDirty()
{
	fullScreenDirty_ = true;
}

void SoftwareRenderer::EndFrame()
{
	if (HeadlessMode)
		return;

	// Composite the software cursor before presenting.
	DrawCursorToBuffer();

	// Mark cursor dirty rects.
	if (prevCursorRect_.size.width != 0 && prevCursorRect_.size.height != 0) {
		MarkDirtyRect(prevCursorRect_);
	}
	if (cursorRect_.size.width != 0 && cursorRect_.size.height != 0) {
		MarkDirtyRect(cursorRect_);
	}

	const int fadeLevel = fade_.Update();
	if (fadeLevel >= 0)
		ApplyFade(fadeLevel);

	if (!renderDirectlyToOutput_ && !videoPlayback_) {
		if (fullScreenDirty_ || numDirtyRects_ == 0) {
			BlitToOutput(palSurface_, nullptr, nullptr);
		} else {
			for (size_t i = 0; i < numDirtyRects_; ++i) {
				BlitToOutput(palSurface_, &dirtyRects_[i], &dirtyRects_[i]);
			}
		}
	}

	SDL_Surface *surface = GetOutputSurface();

	if (!gbActive) {
#ifdef __EMSCRIPTEN__
		// Just yield to browser when inactive instead of blocking
		emscripten_sleep(1);
#else
		LimitFrameRate();
#endif
		return;
	}

#ifndef USE_SDL1
	if (sdlRenderer_ != nullptr) {
#ifdef USE_SDL3
		if (!SDL_SetRenderDrawColor(sdlRenderer_, 0, 0, 0, 255)) ErrSdl();
		if (!SDL_RenderClear(sdlRenderer_)) ErrSdl();
		if (!SDL_UpdateTexture(texture_.get(), nullptr, surface->pixels, surface->pitch)) ErrSdl();
		if (!SDL_RenderTexture(sdlRenderer_, texture_.get(), nullptr, nullptr)) ErrSdl();
#else
		if (SDL_SetRenderDrawColor(sdlRenderer_, 0, 0, 0, 255) <= -1) ErrSdl();
		if (SDL_RenderClear(sdlRenderer_) <= -1) ErrSdl();
		if (SDL_UpdateTexture(texture_.get(), nullptr, surface->pixels, surface->pitch) <= -1) ErrSdl();
		if (SDL_RenderCopy(sdlRenderer_, texture_.get(), nullptr, nullptr) <= -1) ErrSdl();
#endif

		if (ControlMode == ControlTypes::VirtualGamepad) {
			RenderVirtualGamepad(sdlRenderer_);
		}
		SDL_RenderPresent(sdlRenderer_);

#ifdef __EMSCRIPTEN__
		// TODO: Refactor to use emscripten_set_main_loop or requestAnimationFrame instead.
		// For now, yield to browser to allow rendering via ASYNCIFY sleep.
		emscripten_sleep(1);
#endif

		if (*GetOptions().Graphics.frameRateControl != FrameRateControl::VerticalSync) {
			LimitFrameRate();
		}
	} else {
		if (ControlMode == ControlTypes::VirtualGamepad) {
			RenderVirtualGamepad(surface);
		}

#ifdef USE_SDL3
		if (!SDL_UpdateWindowSurface(ghMainWnd)) ErrSdl();
#else
		if (SDL_UpdateWindowSurface(ghMainWnd) <= -1) ErrSdl();
#endif

		if (renderDirectlyToOutput_) {
			palSurface_ = GetOutputSurface();
		}
		LimitFrameRate();
	}
#else
	if (SDL_Flip(surface) <= -1) {
		ErrSdl();
	}
	if (renderDirectlyToOutput_) {
		palSurface_ = GetOutputSurface();
	}
	LimitFrameRate();
#endif
}

void SoftwareRenderer::ClearScreen()
{
	assert(palSurface_ != nullptr);
	SDL_FillSurfaceRect(palSurface_, nullptr, 0);
}

void SoftwareRenderer::HandleResize(int windowWidth, int windowHeight)
{
#ifdef USE_SDL1
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface == nullptr) {
		ErrSdl();
	}
	AdjustToScreenGeometry(Size(surface->w, surface->h));
#else
	if (*GetOptions().Graphics.upscale) {
		// We don't recreate the renderer, because this can result in a freezing (not refreshing) rendering
		if (sdlRenderer_ == nullptr) {
#ifdef USE_SDL3
			sdlRenderer_ = SDL_CreateRenderer(ghMainWnd, nullptr);
#else
			sdlRenderer_ = SDL_CreateRenderer(ghMainWnd, -1, 0);
#endif
			if (sdlRenderer_ == nullptr) {
				ErrSdl();
			}
		}

#ifdef USE_SDL3
		SDL_SetRenderVSync(sdlRenderer_, *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync ? 1 : 0);
#elif SDL_VERSION_ATLEAST(2, 0, 18)
		SDL_RenderSetVSync(sdlRenderer_, *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync ? 1 : 0);
#endif

#ifdef USE_SDL3
		if (!SDL_SetRenderLogicalPresentation(sdlRenderer_, gnScreenWidth, gnScreenHeight,
		        *GetOptions().Graphics.integerScaling
		            ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
		            : SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
			ErrSdl();
		}
#else
		if (SDL_RenderSetIntegerScale(sdlRenderer_, *GetOptions().Graphics.integerScaling ? SDL_TRUE : SDL_FALSE) < 0) {
			ErrSdl();
		}

		if (SDL_RenderSetLogicalSize(sdlRenderer_, gnScreenWidth, gnScreenHeight) <= -1) {
			ErrSdl();
		}
#endif

		ReinitializeTexture();

#ifdef USE_SDL3
		rendererTextureSurface_ = SDLSurfaceUniquePtr { SDL_CreateSurface(gnScreenWidth, gnScreenHeight, texture_.get()->format) };
		if (rendererTextureSurface_ == nullptr) ErrSdl();
#else
		Uint32 format;
		if (SDL_QueryTexture(texture_.get(), &format, nullptr, nullptr, nullptr) < 0) ErrSdl();
		rendererTextureSurface_ = SDLWrap::CreateRGBSurfaceWithFormat(0, gnScreenWidth, gnScreenHeight, SDL_BITSPERPIXEL(format), format);
#endif
	} else {
		AdjustToScreenGeometry({ windowWidth, windowHeight });
	}
#endif
}

SDLSurfaceUniquePtr SoftwareRenderer::CaptureScreen()
{
	if (palSurface_ == nullptr)
		return nullptr;
#ifdef USE_SDL3
	return SDLWrap::ConvertSurfaceFormat(palSurface_, palSurface_->format, 0);
#else
	return SDLWrap::ConvertSurface(palSurface_, palSurface_->format, 0);
#endif
}

void SoftwareRenderer::RenderToAllBackBuffers(std::function<void()> drawFn)
{
	if (!renderDirectlyToOutput_ || palSurface_ == nullptr) {
		// Not using direct rendering or no surface — one call suffices.
		drawFn();
		return;
	}
	// When rendering directly to a multi-buffered output surface,
	// we must draw into each buffer. The pointer changes on each present.
	const void *initialPixels = palSurface_->pixels;
	drawFn();
	while (palSurface_->pixels != initialPixels) {
		drawFn();
	}
}

void SoftwareRenderer::ReinitVirtualGamepad()
{
#if !defined(USE_SDL1) && !defined(__vita__)
	if (sdlRenderer_ != nullptr)
		InitVirtualGamepadTextures(*sdlRenderer_);
#endif
}

void SoftwareRenderer::ApplyFade(int fadeLevel)
{
	if (HeadlessMode)
		return;

	// fadeLevel: 0 = fully visible, 256 = fully black.
	// Compute faded palette from system_palette and apply.
	const unsigned visibility = 256 - static_cast<unsigned>(fadeLevel);

	std::array<SDL_Color, 256> fadedColors;
	ApplyFadeLevel(visibility, fadedColors.data(), system_palette.data());

	// Update palSurface_ palette directly for rendering.
	assert(palette_);
	if (!SDLC_SetSurfaceAndPaletteColors(palSurface_, palette_.get(), fadedColors.data(), 0, 256)) {
		ErrSdl();
	}
}

void SoftwareRenderer::MapWindowToLogical(int *x, int *y) const
{
#ifdef USE_SDL3
	if (sdlRenderer_ == nullptr) return;
	float outX, outY;
	if (!SDL_RenderCoordinatesFromWindow(sdlRenderer_, static_cast<float>(*x), static_cast<float>(*y), &outX, &outY)) {
		LogError("SDL_RenderCoordinatesFromWindow: {}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	*x = static_cast<int>(outX);
	*y = static_cast<int>(outY);
#elif !defined(USE_SDL1)
	if (sdlRenderer_ == nullptr) return;
	float scaleX;
	SDL_RenderGetScale(sdlRenderer_, &scaleX, nullptr);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<int>(*x / scale);
	*y = static_cast<int>(*y / scale);
	SDL_Rect view;
	SDL_RenderGetViewport(sdlRenderer_, &view);
	*x -= view.x;
	*y -= view.y;
#else
	if (!OutputRequiresScaling()) return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * gnScreenWidth / surface->w;
	*y = *y * gnScreenHeight / surface->h;
#endif
}

void SoftwareRenderer::MapLogicalToWindow(int *x, int *y) const
{
#ifdef USE_SDL3
	if (sdlRenderer_ == nullptr) return;
	float outX, outY;
	if (!SDL_RenderCoordinatesToWindow(sdlRenderer_, static_cast<float>(*x), static_cast<float>(*y), &outX, &outY)) {
		LogError("SDL_RenderCoordinatesToWindow: {}", SDL_GetError());
		SDL_ClearError();
		return;
	}
	*x = static_cast<int>(outX);
	*y = static_cast<int>(outY);
#elif !defined(USE_SDL1)
	if (sdlRenderer_ == nullptr) return;
	SDL_Rect view;
	SDL_RenderGetViewport(sdlRenderer_, &view);
	*x += view.x;
	*y += view.y;
	float scaleX;
	SDL_RenderGetScale(sdlRenderer_, &scaleX, nullptr);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<int>(*x * scale);
	*y = static_cast<int>(*y * scale);
#else
	if (!OutputRequiresScaling()) return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * surface->w / gnScreenWidth;
	*y = *y * surface->h / gnScreenHeight;
#endif
}

#ifndef USE_SDL1
SDL_Renderer *SoftwareRenderer::GetSDLRenderer() const
{
	return sdlRenderer_;
}
#endif

Size SoftwareRenderer::GetOutputSize() const
{
#ifndef USE_SDL1
	if (sdlRenderer_ != nullptr) {
		int w, h;
#ifdef USE_SDL3
		if (!SDL_GetRenderOutputSize(sdlRenderer_, &w, &h)) {
			LogError("SDL_GetRenderOutputSize: {}", SDL_GetError());
			SDL_ClearError();
			return { gnScreenWidth, gnScreenHeight };
		}
#else
		if (SDL_GetRendererOutputSize(sdlRenderer_, &w, &h) < 0) {
			LogError("SDL_GetRendererOutputSize: {}", SDL_GetError());
			SDL_ClearError();
			return { gnScreenWidth, gnScreenHeight };
		}
#endif
		return { w, h };
	}
#endif
	const SDL_Surface *surface = GetOutputSurface();
	return { surface->w, surface->h };
}

Size SoftwareRenderer::GetScaledCursorSize(Size logicalSize) const
{
#ifdef USE_SDL3
	if (sdlRenderer_ != nullptr) {
		SDL_FRect logicalDstRect;
		int logicalWidth;
		int logicalHeight;
		if (!SDL_GetRenderLogicalPresentation(sdlRenderer_, &logicalWidth, &logicalHeight, /*mode=*/nullptr)
		    || !SDL_GetRenderLogicalPresentationRect(sdlRenderer_, &logicalDstRect)) {
			return logicalSize;
		}
		const float dispScale = SDL_GetWindowDisplayScale(ghMainWnd);
		if (dispScale == 0.0F)
			return logicalSize;
		const float scaleX = logicalDstRect.w / static_cast<float>(logicalWidth) / dispScale;
		const float scaleY = logicalDstRect.h / static_cast<float>(logicalHeight) / dispScale;
		return {
			static_cast<int>(static_cast<float>(logicalSize.width) * scaleX),
			static_cast<int>(static_cast<float>(logicalSize.height) * scaleY)
		};
	}
#elif !defined(USE_SDL1)
	if (sdlRenderer_ != nullptr) {
		float scaleX = 1.0F;
		float scaleY = 1.0F;
		SDL_RenderGetScale(sdlRenderer_, &scaleX, &scaleY);
		return {
			static_cast<int>(logicalSize.width * scaleX),
			static_cast<int>(logicalSize.height * scaleY)
		};
	}
#endif
	return logicalSize;
}

#if !defined(USE_SDL1) && !defined(USE_SDL3)
void SoftwareRenderer::ConvertEventCoordinates(SDL_Event &event) const
{
	// SDL2 with SDL_RenderSetLogicalSize auto-converts mouse events.
	if (sdlRenderer_ != nullptr)
		return;
	if (event.type == SDL_MOUSEMOTION) {
		MapWindowToLogical(&event.motion.x, &event.motion.y);
	} else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
		MapWindowToLogical(&event.button.x, &event.button.y);
	}
}
#elif defined(USE_SDL3)
void SoftwareRenderer::ConvertEventCoordinates(SDL_Event &event) const
{
	if (sdlRenderer_ != nullptr) {
		SDL_ConvertEventToRenderCoordinates(sdlRenderer_, &event);
		return;
	}
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
}
#else // USE_SDL1
void SoftwareRenderer::ConvertEventCoordinates(SDL_Event &event) const
{
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
}
#endif

#ifndef USE_SDL1
SDL_Texture *SoftwareRenderer::GetSDLTexture() const
{
	return texture_.get();
}

void SoftwareRenderer::SetSDLTexture(SDLTextureUniquePtr newTexture)
{
	texture_ = std::move(newTexture);
}

void SoftwareRenderer::ReinitializeTexture()
{
	texture_.reset();

	if (sdlRenderer_ == nullptr)
		return;

#ifdef USE_SDL3
	if (!SDL_SetDefaultTextureScaleMode(sdlRenderer_,
	        *GetOptions().Graphics.scaleQuality == ScalingQuality::NearestPixel
	            ? SDL_SCALEMODE_PIXELART
	            : SDL_SCALEMODE_LINEAR)) {
		Log("SDL_SetDefaultTextureScaleMode: {}", SDL_GetError());
		SDL_ClearError();
	}
#else
	auto quality = StrCat(static_cast<int>(*GetOptions().Graphics.scaleQuality));
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality.c_str());
#endif
	texture_ = SDLWrap::CreateTexture(sdlRenderer_, DEVILUTIONX_DISPLAY_TEXTURE_FORMAT, SDL_TEXTUREACCESS_STREAMING, gnScreenWidth, gnScreenHeight);
}

void SoftwareRenderer::SetIntegerScale(bool enable)
{
	if (sdlRenderer_ == nullptr) return;

#ifdef USE_SDL3
	int w, h;
	SDL_RendererLogicalPresentation mode;
	if (!SDL_GetRenderLogicalPresentation(sdlRenderer_, &w, &h, &mode)) ErrSdl();
	const SDL_RendererLogicalPresentation newMode = enable
	    ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
	    : SDL_LOGICAL_PRESENTATION_LETTERBOX;
	if (mode != newMode) SDL_SetRenderLogicalPresentation(sdlRenderer_, w, h, newMode);
#else
	if (SDL_RenderSetIntegerScale(sdlRenderer_, enable ? SDL_TRUE : SDL_FALSE) < 0) {
		ErrSdl();
	}
#endif
}
#else
void SoftwareRenderer::ReinitializeTexture() { }
void SoftwareRenderer::SetIntegerScale(bool /*enable*/) { }
#endif

#ifndef USE_SDL1
SDL_Surface *SoftwareRenderer::GetRendererTextureSurface() const
{
	return rendererTextureSurface_.get();
}
#endif

void SoftwareRenderer::NotifyPaletteChanged(int first, int ncolor)
{
	assert(palette_);
	if (!SDLC_SetSurfaceAndPaletteColors(palSurface_, palette_.get(), system_palette.data() + first, first, ncolor)) {
		ErrSdl();
	}
}

void SoftwareRenderer::InvalidateTextureCache()
{
	// No-op: software renderer has no texture cache.
}

void SoftwareRenderer::ClearTextureCache()
{
	// No-op: software renderer has no texture cache.
}

// ---------------------------------------------------------------------------
// Draw methods — render to palSurface_ using the existing software pixel code.
//
// These methods receive absolute screen coordinates. When a clip region is
// active, we construct a Surface with that region and convert coordinates
// to be relative to the clip rect. When no clip is set, we use the full
// palSurface_.
//
// Because NeedsFullRedraw() returns false for SoftwareRenderer, the public
// draw functions (ClxDraw, FillRect, etc.) will NOT recurse back into these
// methods — they go straight to their software pixel code path.
// ---------------------------------------------------------------------------

namespace {

Surface MakeTargetSurface(SDL_Surface *palSurface, bool hasClip, const SDL_Rect &clipRect)
{
	if (hasClip)
		return Surface(palSurface, clipRect);
	return Surface(palSurface);
}

Point ToRelative(Point position, bool hasClip, const SDL_Rect &clipRect)
{
	if (hasClip)
		return { position.x - clipRect.x, position.y - clipRect.y };
	return position;
}

} // namespace

void SoftwareRenderer::SetClipRegion(int x, int y, int w, int h)
{
	clipRect_ = MakeSdlRect(x, y, w, h);
	hasClip_ = true;
}

void SoftwareRenderer::ClearClipRegion()
{
	hasClip_ = false;
}

void SoftwareRenderer::DrawClx(Point position, ClxSprite clx)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	devilution::ClxDraw(out, ToRelative(position, hasClip_, clipRect_), clx);
}

void SoftwareRenderer::DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	devilution::ClxDrawTRN(out, ToRelative(position, hasClip_, clipRect_), clx, trn);
}

void SoftwareRenderer::DrawClxBlended(Point position, ClxSprite clx)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	devilution::ClxDrawBlended(out, ToRelative(position, hasClip_, clipRect_), clx);
}

void SoftwareRenderer::DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	devilution::ClxDrawBlendedTRN(out, ToRelative(position, hasClip_, clipRect_), clx, trn);
}

void SoftwareRenderer::DrawClxLit(Point position, ClxSprite clx, int lightTableIndex)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	Point rel = ToRelative(position, hasClip_, clipRect_);
	if (frameLightmap_ != nullptr && frameLightmap_->isPerPixel()) {
		devilution::ClxDrawWithLightmap(out, rel, clx, *frameLightmap_);
	} else if (lightTableIndex != 0) {
		devilution::ClxDrawTRN(out, rel, clx, LightTables[lightTableIndex].data());
	} else {
		devilution::ClxDraw(out, rel, clx);
	}
}

void SoftwareRenderer::DrawClxLitBlended(Point position, ClxSprite clx, int lightTableIndex)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	Point rel = ToRelative(position, hasClip_, clipRect_);
	if (frameLightmap_ != nullptr && frameLightmap_->isPerPixel()) {
		devilution::ClxDrawBlendedWithLightmap(out, rel, clx, *frameLightmap_);
	} else if (lightTableIndex != 0) {
		devilution::ClxDrawBlendedTRN(out, rel, clx, LightTables[lightTableIndex].data());
	} else {
		devilution::ClxDrawBlended(out, rel, clx);
	}
}

void SoftwareRenderer::DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero)
{
	Surface out = MakeTargetSurface(palSurface_, hasClip_, clipRect_);
	Point rel = ToRelative(position, hasClip_, clipRect_);
	if (skipColorIndexZero) {
		devilution::ClxDrawOutlineSkipColorZero(out, col, rel, clx);
	} else {
		devilution::ClxDrawOutline(out, col, rel, clx);
	}
}

void SoftwareRenderer::BeginScene(Point position, Point targetBufferPosition, int rows, int columns)
{
	const LightingMode lightingMode = *GetOptions().Graphics.lightingMode;
	frameLightmap_ = std::make_unique<Lightmap>(LightmapBuild(lightingMode, position, targetBufferPosition,
	    gnScreenWidth, gnViewportHeight, rows, columns,
	    static_cast<const uint8_t *>(palSurface_->pixels),
	    static_cast<uint16_t>(palSurface_->pitch),
	    LightTables, FullyLitLightTable, FullyDarkLightTable,
	    dLight, MicroTileLen));
}

void SoftwareRenderer::EndScene()
{
	frameLightmap_.reset();
}

void SoftwareRenderer::DrawLevelTile(Point position,
    uint16_t levelPieceId, int lightTableIndex,
    bool transparency, TileProperties tileProperties, bool isFloor)
{
	const MICROS *pMap = &DPieceMicros[levelPieceId];
	const uint8_t *tbl = LightTables[lightTableIndex].data();

	// Create a special lightmap buffer to bleed light up walls
	uint8_t lightmapBuffer[TILE_WIDTH * TILE_HEIGHT];
	const Lightmap bleedLightmap = LightmapBleedUp(*frameLightmap_, position, lightmapBuffer);

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
				RenderTileFoliage(palSurface_, bleedLightmap, position, pDungeonCels.get(), levelCelBlock, tbl);
			} else {
				RenderTile(palSurface_, bleedLightmap, position, pDungeonCels.get(), levelCelBlock, getMaskLeft(tileType), tbl);
			}
		}
	}
	if (const LevelCelBlock levelCelBlock { pMap->mt[1] }; levelCelBlock.hasValue()) {
		const TileType tileType = levelCelBlock.type();
		if (!isFloor || tileType == TileType::TransparentSquare) {
			if (isFloor && tileType == TileType::TransparentSquare) {
				RenderTileFoliage(palSurface_, bleedLightmap, position + Displacement { DunFrameWidth, 0 },
				    pDungeonCels.get(), levelCelBlock, tbl);
			} else {
				RenderTile(palSurface_, bleedLightmap, position + Displacement { DunFrameWidth, 0 },
				    pDungeonCels.get(), levelCelBlock, getMaskRight(tileType), tbl);
			}
		}
	}
	position.y -= TILE_HEIGHT;

	// Upper rows
	for (uint_fast8_t i = 2, n = MicroTileLen; i < n; i += 2) {
		if (const LevelCelBlock levelCelBlock { pMap->mt[i] }; levelCelBlock.hasValue()) {
			RenderTile(palSurface_, bleedLightmap, position, pDungeonCels.get(), levelCelBlock,
			    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
		}
		if (const LevelCelBlock levelCelBlock { pMap->mt[i + 1] }; levelCelBlock.hasValue()) {
			RenderTile(palSurface_, bleedLightmap, position + Displacement { DunFrameWidth, 0 },
			    pDungeonCels.get(), levelCelBlock,
			    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
		}
		position.y -= TILE_HEIGHT;
	}
}

void SoftwareRenderer::DrawFloorLevelTile(Point position,
    uint16_t levelPieceId, int lightTableIndex)
{
	const uint8_t *tbl = LightTables[lightTableIndex].data();
	const MICROS &micros = DPieceMicros[levelPieceId];

	if (const LevelCelBlock levelCelBlock { micros.mt[0] }; levelCelBlock.hasValue()) {
		RenderTileFrame(palSurface_, *frameLightmap_, position, TileType::LeftTriangle,
		    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()), DunFrameTriangleHeight, MaskType::Solid, tbl);
	}
	if (const LevelCelBlock levelCelBlock { micros.mt[1] }; levelCelBlock.hasValue()) {
		RenderTileFrame(palSurface_, *frameLightmap_, position + Displacement { DunFrameWidth, 0 }, TileType::RightTriangle,
		    GetDunFrame(pDungeonCels.get(), levelCelBlock.frame()), DunFrameTriangleHeight, MaskType::Solid, tbl);
	}
}

void SoftwareRenderer::DrawBlackTile(int sx, int sy)
{
	const int viewportH = !*GetOptions().Graphics.zoom ? static_cast<int>(gnViewportHeight) : (static_cast<int>(gnViewportHeight) + 1) / 2;
	Surface out(palSurface_, SDL_Rect { 0, 0, static_cast<int>(gnScreenWidth), viewportH });
	constexpr int Width = 32;          // DunFrameWidth
	constexpr int TriangleHeight = 31; // DunFrameTriangleHeight

	// Left triangle: starts at (sx, sy) with width 2, expands by 2 each row going up.
	for (int row = 0; row < TriangleHeight; ++row) {
		const int rowWidth = 2 + row * 2;
		const int startX = sx + (Width - 2) - row * 2;
		const int y = sy - row;
		if (y < 0 || y >= out.h())
			continue;
		const int clippedStartX = std::max(startX, 0);
		const int clippedEndX = std::min(startX + rowWidth, out.w());
		if (clippedStartX >= clippedEndX)
			continue;
		memset(&out[Point { clippedStartX, y }], 0, clippedEndX - clippedStartX);
	}

	// Right triangle: starts at (sx + Width, sy) with width 2, expands by 2 each row going up.
	for (int row = 0; row < TriangleHeight; ++row) {
		const int rowWidth = 2 + row * 2;
		const int startX = sx + Width;
		const int y = sy - row;
		if (y < 0 || y >= out.h())
			continue;
		const int clippedStartX = std::max(startX, 0);
		const int clippedEndX = std::min(startX + rowWidth, out.w());
		if (clippedStartX >= clippedEndX)
			continue;
		memset(&out[Point { clippedStartX, y }], 0, clippedEndX - clippedStartX);
	}
}

void SoftwareRenderer::FillRect(int x, int y, int w, int h, uint8_t colorIndex)
{
	Surface out(palSurface_);
	devilution::FillRect(out, x, y, w, h, colorIndex);
}

void SoftwareRenderer::DrawBlendedRect(int x, int y, int w, int h)
{
	Surface out(palSurface_);
	devilution::DrawBlendedRectTo(out, x, y, w, h);
}

void SoftwareRenderer::DrawPixel(Point position, uint8_t colorIndex)
{
	Surface out(palSurface_);
	out.SetPixel(position, colorIndex);
}

void SoftwareRenderer::DrawBlendedPixel(Point position, uint8_t colorIndex)
{
	Surface out(palSurface_);
	SetHalfTransparentPixel(out, position, colorIndex);
}

void SoftwareRenderer::DrawLine(Point from, Point to, uint8_t colorIndex)
{
	Surface out(palSurface_);
	// Bresenham's line algorithm
	int dx = std::abs(to.x - from.x);
	int dy = std::abs(to.y - from.y);
	int sx = from.x < to.x ? 1 : -1;
	int sy = from.y < to.y ? 1 : -1;
	int err = dx - dy;
	while (true) {
		out.SetPixel(from, colorIndex);
		if (from.x == to.x && from.y == to.y)
			break;
		int e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			from.x += sx;
		}
		if (e2 < dx) {
			err += dx;
			from.y += sy;
		}
	}
}

void SoftwareRenderer::DrawHorizontalLine(Point from, int width, uint8_t colorIndex)
{
	Surface out(palSurface_);
	devilution::DrawHorizontalLine(out, from, width, colorIndex);
}

void SoftwareRenderer::DrawVerticalLine(Point from, int height, uint8_t colorIndex)
{
	Surface out(palSurface_);
	devilution::DrawVerticalLine(out, from, height, colorIndex);
}

void SoftwareRenderer::DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex)
{
	Surface out(palSurface_);
	DrawHalfTransparentHorizontalLine(out, from, width, colorIndex);
}

void SoftwareRenderer::DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex)
{
	Surface out(palSurface_);
	DrawHalfTransparentVerticalLine(out, from, height, colorIndex);
}

void SoftwareRenderer::BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition)
{
	Surface out(palSurface_);
	out.BlitFrom(src, srcRect, targetPosition);
}

void SoftwareRenderer::ApplyTRN(int x, int y, int w, int h, const uint8_t *trn, uint8_t skipColorIndicesBelow)
{
	Surface out(palSurface_);
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > out.w()) w = out.w() - x;
	if (y + h > out.h()) h = out.h() - y;
	if (w <= 0 || h <= 0)
		return;

	uint8_t *dst = out.at(x, y);
	const auto pitch = out.pitch();
	for (int row = 0; row < h; ++row, dst += pitch) {
		for (int col = 0; col < w; ++col) {
			if (dst[col] >= skipColorIndicesBelow)
				dst[col] = trn[dst[col]];
		}
	}
}

void SoftwareRenderer::TintRect(int x, int y, int w, int h, uint8_t colorIndex)
{
	// Palette remap: pixels in the slot area that are in the gray ramp
	// get shifted to a colored ramp (blue/yellow/beige/orange).
	// colorIndex is the target ramp base + 4 (e.g. PAL16_BLUE + 4).
	// The shift moves pixels from PAL16_GRAY range into the target range.
	Surface out(palSurface_);

	// Clip to surface bounds.
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > out.w()) w = out.w() - x;
	if (y + h > out.h()) h = out.h() - y;
	if (w <= 0 || h <= 0)
		return;

	// The color shift maps gray ramp pixels to the target color ramp.
	// colorIndex is targetColor + 4. PAL16_GRAY is the gray ramp start.
	// colorShift = PAL16_GRAY - (colorIndex - 4) - 1 = PAL16_GRAY - colorIndex + 3
	const uint8_t colorShift = PAL16_GRAY - colorIndex + 3;

	uint8_t *dst = &out[Point { x, y }];
	const auto dstPitch = out.pitch();
	for (int row = 0; row < h; ++row, dst += dstPitch) {
		for (int col = 0; col < w; ++col) {
			uint8_t &pix = dst[col];
			if (pix >= PAL16_GRAY) {
				pix -= colorShift;
			}
		}
	}
}

void SoftwareRenderer::BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition)
{
	Surface out(palSurface_);
	out.BlitFromSkipColorIndexZero(src, srcRect, targetPosition);
}

void SoftwareRenderer::PrepareVideoPlayback(unsigned width, unsigned height)
{
	videoPlayback_ = true;

	// Clear the output surface to black so video borders are black.
	SDL_FillSurfaceRect(GetOutputSurface(), nullptr, 0x000000);
#ifndef USE_SDL1
	if (sdlRenderer_ != nullptr) {
		const int renderWidth = static_cast<int>(width);
		const int renderHeight = static_cast<int>(height);
		texture_ = SDLWrap::CreateTexture(sdlRenderer_, DEVILUTIONX_DISPLAY_TEXTURE_FORMAT,
		    SDL_TEXTUREACCESS_STREAMING, renderWidth, renderHeight);
		if (
#ifdef USE_SDL3
		    !SDL_SetRenderLogicalPresentation(sdlRenderer_, renderWidth, renderHeight,
		        *GetOptions().Graphics.integerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_STRETCH)
#else
		    SDL_RenderSetLogicalSize(sdlRenderer_, renderWidth, renderHeight) <= -1
#endif
		) {
			ErrSdl();
		}
	}
#endif
}

bool SoftwareRenderer::DrawVideoFrame(SDL_Surface *frame, unsigned width, unsigned height)
{
#ifndef USE_SDL1
	if (sdlRenderer_ != nullptr) {
		SDL_Surface *outputSurface = GetOutputSurface();
		if (
#ifdef USE_SDL3
		    !SDL_BlitSurface(frame, nullptr, outputSurface, nullptr)
#else
		    SDL_BlitSurface(frame, nullptr, outputSurface, nullptr) <= -1
#endif
		) {
			Log("{}", SDL_GetError());
			return false;
		}
		GetRenderer().EndFrame();
		return true;
	}
#endif

	SDL_Surface *outputSurface = GetOutputSurface();
#ifdef USE_SDL1
	const bool isIndexedOutputFormat = SDLBackport_IsPixelFormatIndexed(outputSurface->format);
#else
#ifdef USE_SDL3
	const SDL_PixelFormat wndFormat = SDL_GetWindowPixelFormat(ghMainWnd);
#else
	const Uint32 wndFormat = SDL_GetWindowPixelFormat(ghMainWnd);
#endif
	const bool isIndexedOutputFormat = SDL_ISPIXELFORMAT_INDEXED(wndFormat);
#endif

	SDL_Rect outputRect;
	if (isIndexedOutputFormat) {
		outputRect.w = static_cast<int>(width);
		outputRect.h = static_cast<int>(height);
	} else if (width * static_cast<unsigned>(outputSurface->h) > static_cast<unsigned>(outputSurface->w) * height) {
		outputRect.w = outputSurface->w;
		outputRect.h = static_cast<int>(height * static_cast<unsigned>(outputSurface->w) / width);
	} else {
		outputRect.w = static_cast<int>(width * static_cast<unsigned>(outputSurface->h) / height);
		outputRect.h = outputSurface->h;
	}
	outputRect.x = (outputSurface->w - outputRect.w) / 2;
	outputRect.y = (outputSurface->h - outputRect.h) / 2;

	if (isIndexedOutputFormat
	    || outputSurface->w == static_cast<int>(width)
	    || outputSurface->h == static_cast<int>(height)) {
		if (
#ifdef USE_SDL3
		    SDL_BlitSurface(frame, nullptr, outputSurface, &outputRect)
#else
		    SDL_BlitSurface(frame, nullptr, outputSurface, &outputRect) <= -1
#endif
		) {
			ErrSdl();
		}
	} else {
#ifdef USE_SDL1
		SDLSurfaceUniquePtr converted = SDLWrap::ConvertSurface(frame, ghMainWnd->format, 0);
#else
		SDLSurfaceUniquePtr converted = SDLWrap::ConvertSurfaceFormat(frame, wndFormat, 0);
#endif
		if (
#ifdef USE_SDL3
		    SDL_BlitSurfaceScaled(converted.get(), nullptr, outputSurface, &outputRect, SDL_SCALEMODE_LINEAR)
#else
		    SDL_BlitScaled(converted.get(), nullptr, outputSurface, &outputRect) <= -1
#endif
		) {
			Log("{}", SDL_GetError());
			return false;
		}
	}

	GetRenderer().EndFrame();
	return true;
}

void SoftwareRenderer::FinishVideoPlayback()
{
	videoPlayback_ = false;
#ifndef USE_SDL1
	if (sdlRenderer_ != nullptr) {
		texture_ = SDLWrap::CreateTexture(sdlRenderer_, DEVILUTIONX_DISPLAY_TEXTURE_FORMAT,
		    SDL_TEXTUREACCESS_STREAMING, gnScreenWidth, gnScreenHeight);
		if (
#ifdef USE_SDL3
		    !SDL_SetRenderLogicalPresentation(sdlRenderer_, gnScreenWidth, gnScreenHeight,
		        *GetOptions().Graphics.integerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_STRETCH)
#else
		    SDL_RenderSetLogicalSize(sdlRenderer_, gnScreenWidth, gnScreenHeight) <= -1
#endif
		) {
			ErrSdl();
		}
	}
#endif
}

void SoftwareRenderer::NotifyVideoPaletteChanged(SDL_Palette *palette)
{
#ifndef USE_SDL1
	SDL_Surface *outputSurface = GetOutputSurface();
	if (SDLC_SURFACE_BITSPERPIXEL(outputSurface) == 8) {
		if (!SDLC_SetSurfacePalette(outputSurface, palette)) {
			ErrSdl();
		}
	}
#endif
}

} // namespace devilution
