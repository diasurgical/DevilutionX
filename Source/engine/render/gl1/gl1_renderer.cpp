/**
 * @file gl1_renderer.cpp
 *
 * OpenGL 1.x rendering backend implementation for DevilutionX.
 */

#ifdef DEVILUTIONX_GL1_RENDERER

#include "engine/render/gl1/gl1_renderer.h"
#include "engine/palette.h"
#include "engine/render/gl1/gl1_texture_cache.h"
#include "engine/render/renderer_backend.h"
#include "lighting.h"
#include "options.h"
#include "utils/clx_decode.hpp"
#include "utils/is_of.hpp"
#include "utils/log.hpp"
#include "utils/ui_fwd.h"

#include <algorithm>
#include <vector>

#include <GL/gl.h>

#ifdef USE_SDL3
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#else
#include <SDL.h>
#endif

namespace devilution {

namespace {

#ifndef USE_SDL1
SDL_GLContext glContext = nullptr;
SDL_Window *gl1Window = nullptr;
#endif
bool gl1Active = false;
bool frameBegun = false;
bool zoomActive = false;
Gl1TextureCache textureCache;
uint16_t logicalWidth = 640;
uint16_t logicalHeight = 480;
int drawCallCount = 0;
bool scissorEnabled = false;
int viewportX = 0, viewportY = 0, viewportW = 0, viewportH = 0;
int drawableW = 0, drawableH = 0;

void EnsureFrameBegun()
{
	if (frameBegun)
		return;
	frameBegun = true;
	drawCallCount = 0;
	logicalWidth = gnScreenWidth;
	logicalHeight = gnScreenHeight;

	int drawW = 0, drawH = 0;
	int vpX = 0, vpY = 0, vpW = 0, vpH = 0;
#ifdef USE_SDL1
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface != nullptr) {
		drawW = surface->w;
		drawH = surface->h;
	}
#else
	if (gl1Window != nullptr) {
#ifdef USE_SDL3
		SDL_GetWindowSizeInPixels(gl1Window, &drawW, &drawH);
#else
		SDL_GL_GetDrawableSize(gl1Window, &drawW, &drawH);
#endif
	}
#endif
	if (drawW > 0 && drawH > 0) {
		const float scaleX = static_cast<float>(drawW) / logicalWidth;
		const float scaleY = static_cast<float>(drawH) / logicalHeight;
		const float scale = (scaleX < scaleY) ? scaleX : scaleY;
		vpW = static_cast<int>(logicalWidth * scale);
		vpH = static_cast<int>(logicalHeight * scale);
		vpX = (drawW - vpW) / 2;
		vpY = (drawH - vpH) / 2;
	} else {
		vpW = logicalWidth;
		vpH = logicalHeight;
	}

	// Store for coordinate mapping.
	viewportX = vpX;
	viewportY = vpY;
	viewportW = vpW;
	viewportH = vpH;
	drawableW = drawW;
	drawableH = drawH;

	// Clear the full framebuffer to black (letterbox bars).
	glDisable(GL_SCISSOR_TEST);
	glViewport(0, 0, drawW > 0 ? drawW : vpW, drawH > 0 ? drawH : vpH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Set the letterboxed viewport for actual drawing.
	glViewport(vpX, vpY, vpW, vpH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_SCISSOR_TEST);
	scissorEnabled = false;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, logicalWidth, logicalHeight, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor4f(1, 1, 1, 1);
}

void DrawTexturedQuad(uint32_t texId, int x, int y, int w, int h)
{
	glBindTexture(GL_TEXTURE_2D, texId);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2i(x, y);
	glTexCoord2f(1, 0);
	glVertex2i(x + w, y);
	glTexCoord2f(1, 1);
	glVertex2i(x + w, y + h);
	glTexCoord2f(0, 1);
	glVertex2i(x, y + h);
	glEnd();
}

// Measured brightness ratios from Diablo's cathedral palette light tables.
// The original lighting shifts palette indices within color ramps rather than
// applying a linear multiplier. Sprites and tiles use different palette regions
// with different ramp structures, so they need separate falloff curves.

// Sprite lighting: global palette colors (indices 128-254).
// These ramps span full luminance (100%->0%) and include both 8-entry and
// 16-entry ramps. Averaged across all pixel positions in the ramps.
constexpr float SpriteLightBrightness[16] = {
	1.000f, // Level  0: fully lit
	0.884f, // Level  1
	0.776f, // Level  2
	0.674f, // Level  3
	0.580f, // Level  4
	0.493f, // Level  5
	0.413f, // Level  6
	0.340f, // Level  7
	0.275f, // Level  8
	0.217f, // Level  9
	0.165f, // Level 10
	0.120f, // Level 11
	0.082f, // Level 12
	0.050f, // Level 13
	0.026f, // Level 14
	0.000f, // Level 15: fully dark
};

// Tile lighting: least-squares best-fit vertex color across all dungeon palette entries
// (indices 1-127). Minimizes per-pixel RMS error for standard tex * vertexColor blending.
constexpr float TileLightBrightness[16] = {
	1.000f, // Level  0: fully lit
	0.910f, // Level  1
	0.822f, // Level  2
	0.734f, // Level  3
	0.649f, // Level  4
	0.566f, // Level  5
	0.486f, // Level  6
	0.411f, // Level  7
	0.341f, // Level  8
	0.275f, // Level  9
	0.215f, // Level 10
	0.160f, // Level 11
	0.112f, // Level 12
	0.070f, // Level 13
	0.038f, // Level 14
	0.000f, // Level 15: fully dark
};

void SetVertexLighting(uint8_t lightLevel)
{
	float brightness = SpriteLightBrightness[std::min<uint8_t>(lightLevel, 15)];
	glColor4f(brightness, brightness, brightness, 1.0f);
}

void ApplyScissor(int regionX, int regionY, int regionW, int regionH)
{
	// When zoom is active, the game renders at half coordinates but
	// the scissor operates in window space. Scale the region by 2x
	// so the scissor matches the zoomed output.
	if (zoomActive) {
		regionX *= 2;
		regionY *= 2;
		regionW *= 2;
		regionH *= 2;
	}

	if (regionX == 0 && regionY == 0
	    && regionW == static_cast<int>(logicalWidth)
	    && regionH == static_cast<int>(logicalHeight)) {
		if (scissorEnabled) {
			glDisable(GL_SCISSOR_TEST);
			scissorEnabled = false;
		}
		return;
	}

	// Convert from top-left-origin logical coords to GL bottom-left-origin viewport coords.
	// We need the current viewport to do the conversion.
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	const float scaleX = static_cast<float>(vp[2]) / logicalWidth;
	const float scaleY = static_cast<float>(vp[3]) / logicalHeight;
	const int sx = vp[0] + static_cast<int>(regionX * scaleX);
	const int sy = vp[1] + static_cast<int>((logicalHeight - regionY - regionH) * scaleY);
	const int sw = static_cast<int>(regionW * scaleX);
	const int sh = static_cast<int>(regionH * scaleY);

	glEnable(GL_SCISSOR_TEST);
	glScissor(sx, sy, sw, sh);
	scissorEnabled = true;
}

void ClearScissor()
{
	if (scissorEnabled) {
		glDisable(GL_SCISSOR_TEST);
		scissorEnabled = false;
	}
}

} // namespace

bool IsGl1RendererActive()
{
	return gl1Active;
}

void SetGl1RendererActive(bool active)
{
	gl1Active = active;
}

bool Gl1Init(SDL_Window *window)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	// Don't request a specific GL version — let the driver give us a
	// compatibility context.  Requesting 1.1 explicitly can fail on
	// modern Mesa/EGL drivers that only expose core profiles.

#ifdef USE_SDL1
	// SDL1: GL context is created implicitly by SDL_SetVideoMode with SDL_OPENGL.
	// By the time Gl1Init is called, the video mode is already set.
	(void)window;
#else
	glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr) {
		LogError("Failed to create GL context: {}", SDL_GetError());
		return false;
	}

#ifdef USE_SDL3
	if (!SDL_GL_MakeCurrent(window, glContext)) {
		LogError("SDL_GL_MakeCurrent failed: {}", SDL_GetError());
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
		return false;
	}
#else
	if (SDL_GL_MakeCurrent(window, glContext) < 0) {
		LogError("SDL_GL_MakeCurrent failed: {}", SDL_GetError());
		SDL_GL_DeleteContext(glContext);
		glContext = nullptr;
		return false;
	}
#endif
#endif // !USE_SDL1

	const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
	const char *rendererStr = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
	const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
	SDL_Log("GL_VENDOR:   %s", vendor ? vendor : "(null)");
	SDL_Log("GL_RENDERER: %s", rendererStr ? rendererStr : "(null)");
	SDL_Log("GL_VERSION:  %s", version ? version : "(null)");

#ifndef USE_SDL1
	gl1Window = window;
#endif
	gl1Active = true;

	glEnable(GL_TEXTURE_2D);
	GLenum err = glGetError();
	SDL_Log("GL1: glEnable(GL_TEXTURE_2D) err=%d", err);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glClearColor(0, 0, 0, 1);

	int w = 0, h = 0;
#ifdef USE_SDL1
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface != nullptr) {
		w = surface->w;
		h = surface->h;
	}
#else
	SDL_GetWindowSize(window, &w, &h);
#endif
	SDL_Log("GL1: Window size: %dx%d", w, h);
	Gl1HandleResize(w, h);

#ifndef USE_SDL1
	SDL_GL_SetSwapInterval(0);
#endif

	Log("GL1 renderer initialized successfully");
	return true;
}

void Gl1Shutdown()
{
	textureCache.Clear();

#ifndef USE_SDL1
	if (glContext != nullptr) {
#ifdef USE_SDL3
		SDL_GL_DestroyContext(glContext);
#else
		SDL_GL_DeleteContext(glContext);
#endif
		glContext = nullptr;
	}
	gl1Window = nullptr;
#endif

	gl1Active = false;
}

void Gl1BeginFrame()
{
	EnsureFrameBegun();
}

void Gl1EndFrame()
{
	EnsureFrameBegun();

#ifndef USE_SDL1
	// Respect the current vsync setting each frame so runtime changes
	// (e.g. timedemo overriding to FrameRateControl::None) take effect.
	int desiredInterval = (*GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync) ? 1 : 0;
	int currentInterval = 0;
#ifdef USE_SDL3
	SDL_GL_GetSwapInterval(&currentInterval);
#else
	currentInterval = SDL_GL_GetSwapInterval();
#endif
	if (currentInterval != desiredInterval)
		SDL_GL_SetSwapInterval(desiredInterval);
#endif

	glFlush();
#ifdef USE_SDL1
	SDL_GL_SwapBuffers();
#else
	SDL_GL_SwapWindow(gl1Window);
#endif
	frameBegun = false;
}

void Gl1HandleResize(int windowWidth, int windowHeight)
{
	// Viewport is now computed per-frame in EnsureFrameBegun to handle
	// high-DPI and aspect ratio correctly. This function is kept for
	// future use (e.g. responding to resize events).
	(void)windowWidth;
	(void)windowHeight;
}

Size Gl1GetOutputSize()
{
	int drawW = 0, drawH = 0;
#ifdef USE_SDL1
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface != nullptr) {
		drawW = surface->w;
		drawH = surface->h;
	}
#else
	if (gl1Window != nullptr) {
#ifdef USE_SDL3
		SDL_GetWindowSizeInPixels(gl1Window, &drawW, &drawH);
#else
		SDL_GL_GetDrawableSize(gl1Window, &drawW, &drawH);
#endif
	}
#endif
	if (drawW <= 0 || drawH <= 0) {
		drawW = gnScreenWidth;
		drawH = gnScreenHeight;
	}
	return { drawW, drawH };
}

Size Gl1GetScaledCursorSize(Size logicalSize)
{
#ifdef USE_SDL1
	// SDL1 has no window scaling; cursor is 1:1 with game pixels on screen.
	return logicalSize;
#else
	if (gnScreenWidth <= 0 || gnScreenHeight <= 0)
		return logicalSize;
	int winW = 0, winH = 0;
	if (gl1Window != nullptr)
		SDL_GetWindowSize(gl1Window, &winW, &winH);
	if (winW <= 0 || winH <= 0)
		return logicalSize;
	const float scaleX = static_cast<float>(winW) / gnScreenWidth;
	const float scaleY = static_cast<float>(winH) / gnScreenHeight;
	const float scale = (scaleX < scaleY) ? scaleX : scaleY;
#ifdef USE_SDL3
	const float dispScale = SDL_GetWindowDisplayScale(gl1Window);
	const float adjustedScale = dispScale > 0.0F ? scale / dispScale : scale;
#else
	const float adjustedScale = scale;
#endif
	return {
		static_cast<int>(static_cast<float>(logicalSize.width) * adjustedScale),
		static_cast<int>(static_cast<float>(logicalSize.height) * adjustedScale)
	};
#endif
}

void Gl1MapWindowToLogical(int *x, int *y)
{
	// If the viewport hasn't been computed yet (no frame rendered), compute it now.
	if (viewportW <= 0) {
		int dw = 0, dh = 0;
#ifdef USE_SDL1
		const SDL_Surface *surface = SDL_GetVideoSurface();
		if (surface != nullptr) {
			dw = surface->w;
			dh = surface->h;
		}
#elif defined(USE_SDL3)
		if (gl1Window != nullptr)
			SDL_GetWindowSizeInPixels(gl1Window, &dw, &dh);
#else
		if (gl1Window != nullptr)
			SDL_GL_GetDrawableSize(gl1Window, &dw, &dh);
#endif
		if (dw > 0 && dh > 0) {
			const float scaleX = static_cast<float>(dw) / gnScreenWidth;
			const float scaleY = static_cast<float>(dh) / gnScreenHeight;
			const float scale = (scaleX < scaleY) ? scaleX : scaleY;
			viewportW = static_cast<int>(gnScreenWidth * scale);
			viewportH = static_cast<int>(gnScreenHeight * scale);
			viewportX = (dw - viewportW) / 2;
			viewportY = (dh - viewportH) / 2;
			drawableW = dw;
			drawableH = dh;
		}
	}

	if (viewportW <= 0 || viewportH <= 0)
		return;

	// Mouse events are in window coordinates. Convert to drawable (pixel) coordinates
	// in case of high-DPI scaling.
	float pixelX = static_cast<float>(*x);
	float pixelY = static_cast<float>(*y);
#ifndef USE_SDL1
	if (gl1Window != nullptr && drawableW > 0) {
		int winW = 0, winH = 0;
#ifdef USE_SDL3
		SDL_GetWindowSize(gl1Window, &winW, &winH);
#else
		SDL_GetWindowSize(gl1Window, &winW, &winH);
#endif
		if (winW > 0 && winH > 0) {
			pixelX = pixelX * static_cast<float>(drawableW) / static_cast<float>(winW);
			pixelY = pixelY * static_cast<float>(drawableH) / static_cast<float>(winH);
		}
	}
#endif

	// Map from drawable pixel coordinates to logical coordinates.
	*x = static_cast<int>((pixelX - viewportX) * logicalWidth / viewportW);
	*y = static_cast<int>((pixelY - viewportY) * logicalHeight / viewportH);
}

void Gl1MapLogicalToWindow(int *x, int *y)
{
	// If the viewport hasn't been computed yet (no frame rendered), compute it now.
	if (viewportW <= 0) {
		int dw = 0, dh = 0;
#ifdef USE_SDL1
		const SDL_Surface *surface = SDL_GetVideoSurface();
		if (surface != nullptr) {
			dw = surface->w;
			dh = surface->h;
		}
#elif defined(USE_SDL3)
		if (gl1Window != nullptr)
			SDL_GetWindowSizeInPixels(gl1Window, &dw, &dh);
#else
		if (gl1Window != nullptr)
			SDL_GL_GetDrawableSize(gl1Window, &dw, &dh);
#endif
		if (dw > 0 && dh > 0) {
			const float scaleX = static_cast<float>(dw) / gnScreenWidth;
			const float scaleY = static_cast<float>(dh) / gnScreenHeight;
			const float scale = (scaleX < scaleY) ? scaleX : scaleY;
			viewportW = static_cast<int>(gnScreenWidth * scale);
			viewportH = static_cast<int>(gnScreenHeight * scale);
			viewportX = (dw - viewportW) / 2;
			viewportY = (dh - viewportH) / 2;
			drawableW = dw;
			drawableH = dh;
		}
	}

	if (viewportW <= 0 || viewportH <= 0)
		return;

	// Map from logical coordinates to drawable pixel coordinates.
	float pixelX = static_cast<float>(*x) * viewportW / logicalWidth + viewportX;
	float pixelY = static_cast<float>(*y) * viewportH / logicalHeight + viewportY;

	// Convert from drawable (pixel) coordinates to window coordinates.
#ifndef USE_SDL1
	if (gl1Window != nullptr && drawableW > 0) {
		int winW = 0, winH = 0;
#ifdef USE_SDL3
		SDL_GetWindowSize(gl1Window, &winW, &winH);
#else
		SDL_GetWindowSize(gl1Window, &winW, &winH);
#endif
		if (winW > 0 && winH > 0) {
			pixelX = pixelX * static_cast<float>(winW) / static_cast<float>(drawableW);
			pixelY = pixelY * static_cast<float>(winH) / static_cast<float>(drawableH);
		}
	}
#endif

	*x = static_cast<int>(pixelX);
	*y = static_cast<int>(pixelY);
}

void Gl1SetClipRegion(int regionX, int regionY, int regionW, int regionH)
{
	EnsureFrameBegun();
	ApplyScissor(regionX, regionY, regionW, regionH);
}

void Gl1ClearClipRegion()
{
	ClearScissor();
}

void Gl1DrawClx(Point position, ClxSprite clx)
{
	auto &tex = textureCache.GetSpriteTexture(clx, nullptr);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	glColor4f(1, 1, 1, 1);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
}

void Gl1DrawClxTRN(Point position, ClxSprite clx, const uint8_t *trn)
{
	auto &tex = textureCache.GetSpriteTexture(clx, trn);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	glColor4f(1, 1, 1, 1);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
}

void Gl1DrawClxBlended(Point position, ClxSprite clx)
{
	auto &tex = textureCache.GetSpriteTexture(clx, nullptr);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	glColor4f(1, 1, 1, 0.5f);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawClxBlendedTRN(Point position, ClxSprite clx, const uint8_t *trn)
{
	auto &tex = textureCache.GetSpriteTexture(clx, trn);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	glColor4f(1, 1, 1, 0.5f);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawClxWithLight(Point position, ClxSprite clx, uint8_t lightLevel)
{
	auto &tex = textureCache.GetSpriteTexture(clx, nullptr);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	SetVertexLighting(lightLevel);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawClxWithLightBlended(Point position, ClxSprite clx, uint8_t lightLevel)
{
	auto &tex = textureCache.GetSpriteTexture(clx, nullptr);
	int x = position.x;
	int y = position.y - clx.height() + 1;
	EnsureFrameBegun();
	float brightness = SpriteLightBrightness[std::min<uint8_t>(lightLevel, 15)];
	glColor4f(brightness, brightness, brightness, 0.5f);
	DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawTile(Point position, TileType tileType, const uint8_t *src, int_fast16_t height,
    MaskType maskType, const uint8_t *tbl, LightingMode lightingMode)
{
	EnsureFrameBegun();

	const bool useVertexLighting = lightingMode == LightingMode::Vertex;

	if (useVertexLighting) {
		// Vertex lighting: texture is unlit, vertex color darkens it.
		auto &tex = textureCache.GetTileTexture(src, tileType, maskType, height);

		// Derive light level from tbl pointer by comparing against LightTables.
		uint8_t lightLevel = 0;
		if (tbl != nullptr) {
			for (size_t i = 0; i < NumLightingLevels; i++) {
				if (tbl == LightTables[i].data()) {
					lightLevel = static_cast<uint8_t>(i);
					break;
				}
			}
		}
		float brightness = TileLightBrightness[std::min<uint8_t>(lightLevel, 15)];
		if (maskType == MaskType::Transparent) {
			glColor4f(brightness, brightness, brightness, 0.5f);
		} else {
			glColor4f(brightness, brightness, brightness, 1.0f);
		}
		int x = position.x;
		int y = position.y - static_cast<int>(tex.height) + 1;
		DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
		glColor4f(1, 1, 1, 1);
	} else {
		// Pixel lighting: light table baked into the texture at upload time.
		auto &tex = textureCache.GetTileTexture(src, tileType, maskType, height, tbl);

		if (maskType == MaskType::Transparent) {
			glColor4f(1, 1, 1, 0.5f);
		}
		int x = position.x;
		int y = position.y - static_cast<int>(tex.height) + 1;
		DrawTexturedQuad(tex.textureId, x, y, tex.width, tex.height);
		if (maskType == MaskType::Transparent) {
			glColor4f(1, 1, 1, 1);
		}
	}
}

void Gl1DrawBlackTile(int sx, int sy)
{
	EnsureFrameBegun();
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, 1);
	glBegin(GL_QUADS);
	glVertex2i(sx, sy - 15);
	glVertex2i(sx + 64, sy - 15);
	glVertex2i(sx + 64, sy + 16);
	glVertex2i(sx, sy + 16);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1FillRect(int x, int y, int width, int height, uint8_t colorIndex)
{
	EnsureFrameBegun();
	const auto &c = system_palette[colorIndex];
	glDisable(GL_TEXTURE_2D);
	glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + width, y);
	glVertex2i(x + width, y + height);
	glVertex2i(x, y + height);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawBlendedRect(int x, int y, int width, int height)
{
	EnsureFrameBegun();
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, 0.5f);
	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + width, y);
	glVertex2i(x + width, y + height);
	glVertex2i(x, y + height);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

// Draw an axis-aligned span as a 1px-thick quad. GL_LINES rasterization of
// axis-aligned spans is implementation-defined (diamond-exit rule), so a quad
// is used to guarantee the same pixel coverage as the software renderer.
namespace {
void Gl1DrawLineSpan(int x, int y, int w, int h, float r, float g, float b, float a)
{
	EnsureFrameBegun();
	glDisable(GL_TEXTURE_2D);
	glColor4f(r, g, b, a);
	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + w, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x, y + h);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}
} // namespace

void Gl1DrawHorizontalLine(Point from, int width, uint8_t colorIndex)
{
	const auto &c = system_palette[colorIndex];
	Gl1DrawLineSpan(from.x, from.y, width, 1, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
}

void Gl1DrawVerticalLine(Point from, int height, uint8_t colorIndex)
{
	const auto &c = system_palette[colorIndex];
	Gl1DrawLineSpan(from.x, from.y, 1, height, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
}

void Gl1DrawBlendedHorizontalLine(Point from, int width, uint8_t colorIndex)
{
	const auto &c = system_palette[colorIndex];
	Gl1DrawLineSpan(from.x, from.y, width, 1, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 0.5f);
}

void Gl1DrawBlendedVerticalLine(Point from, int height, uint8_t colorIndex)
{
	const auto &c = system_palette[colorIndex];
	Gl1DrawLineSpan(from.x, from.y, 1, height, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 0.5f);
}

void Gl1DrawClxOutline(uint8_t col, Point position, ClxSprite clx, bool skipColorIndexZero)
{
	EnsureFrameBegun();

	const unsigned width = clx.width();
	const unsigned height = clx.height();

	// Decode the sprite silhouette to find outline pixels.
	// We track which pixels are "solid" (opaque) and emit border pixels.
	std::vector<bool> solid(static_cast<size_t>(width + 2) * (height + 2), false);
	auto solidAt = [&](int x, int y) -> bool {
		if (x < 0 || x >= static_cast<int>(width + 2) || y < 0 || y >= static_cast<int>(height + 2))
			return false;
		return solid[static_cast<size_t>(y) * (width + 2) + x];
	};
	auto setSolid = [&](int x, int y) {
		if (x >= 0 && x < static_cast<int>(width + 2) && y >= 0 && y < static_cast<int>(height + 2))
			solid[static_cast<size_t>(y) * (width + 2) + x] = true;
	};

	// Decode CLX to find solid pixels. Row 0 in CLX = bottom of sprite.
	// We store with +1 offset so border checks don't go out of bounds.
	const uint8_t *src = clx.pixelData();
	const uint8_t *srcEnd = src + clx.pixelDataSize();
	int currentRow = static_cast<int>(height) - 1;
	int xPos = 0;

	while (src < srcEnd && currentRow >= 0) {
		const uint8_t v = *src++;
		if (!IsClxOpaque(v)) {
			xPos += v;
		} else if (IsClxOpaqueFill(v)) {
			const uint8_t fillWidth = GetClxOpaqueFillWidth(v);
			if (src >= srcEnd) break;
			const uint8_t paletteIndex = *src++;
			if (!skipColorIndexZero || paletteIndex != 0) {
				for (int i = 0; i < fillWidth; i++) {
					setSolid(xPos + 1, currentRow + 1);
					xPos++;
				}
			} else {
				xPos += fillWidth;
			}
		} else {
			const uint8_t runLength = GetClxOpaquePixelsWidth(v);
			for (int i = 0; i < runLength && src < srcEnd; i++) {
				const uint8_t paletteIndex = *src++;
				if (!skipColorIndexZero || paletteIndex != 0) {
					setSolid(xPos + 1, currentRow + 1);
				}
				xPos++;
			}
		}
		while (xPos >= static_cast<int>(width) && currentRow >= 0) {
			xPos -= static_cast<int>(width);
			currentRow--;
		}
	}

	// Now find outline pixels: non-solid pixels adjacent to solid pixels.
	const auto &c = system_palette[col];
	glDisable(GL_TEXTURE_2D);
	glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);

	// position is the bottom-left of the sprite in game coords.
	// The outline extends 1 pixel in each direction.
	const int baseX = position.x - 1;
	const int baseY = position.y - static_cast<int>(height);

	glBegin(GL_QUADS);
	for (int y = 0; y < static_cast<int>(height + 2); y++) {
		for (int x = 0; x < static_cast<int>(width + 2); x++) {
			if (solid[static_cast<size_t>(y) * (width + 2) + x])
				continue;
			// Check 4-connected neighbors.
			if (solidAt(x - 1, y) || solidAt(x + 1, y) || solidAt(x, y - 1) || solidAt(x, y + 1)) {
				const int px = baseX + x;
				const int py = baseY + y;
				glVertex2i(px, py);
				glVertex2i(px + 1, py);
				glVertex2i(px + 1, py + 1);
				glVertex2i(px, py + 1);
			}
		}
	}
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawPixel(Point position, uint8_t colorIndex)
{
	EnsureFrameBegun();
	const auto &c = system_palette[colorIndex];
	glDisable(GL_TEXTURE_2D);
	glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2i(position.x, position.y);
	glVertex2i(position.x + 1, position.y);
	glVertex2i(position.x + 1, position.y + 1);
	glVertex2i(position.x, position.y + 1);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawBlendedPixel(Point position, uint8_t colorIndex)
{
	EnsureFrameBegun();
	const auto &c = system_palette[colorIndex];
	glDisable(GL_TEXTURE_2D);
	glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 0.5f);
	glBegin(GL_QUADS);
	glVertex2i(position.x, position.y);
	glVertex2i(position.x + 1, position.y);
	glVertex2i(position.x + 1, position.y + 1);
	glVertex2i(position.x, position.y + 1);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1DrawLine(Point from, Point to, uint8_t colorIndex)
{
	EnsureFrameBegun();
	const auto &c = system_palette[colorIndex];
	glDisable(GL_TEXTURE_2D);
	glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
	glBegin(GL_LINES);
	glVertex2i(from.x, from.y);
	glVertex2i(to.x, to.y);
	glEnd();
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1TintRect(int x, int y, int w, int h, uint8_t colorIndex)
{
	EnsureFrameBegun();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	if (scissorEnabled) {
		glDisable(GL_SCISSOR_TEST);
		scissorEnabled = false;
	}
	// Multiply blend tints the gray slot texture to a colored tone.
	// Colors determined empirically to match the software palette remap.
	float r, g, b;
	if (colorIndex >= PAL16_BLUE && colorIndex < PAL16_BLUE + 16) {
		// Blue (magic items)
		r = 0x99 / 255.0f;
		g = 0xB8 / 255.0f;
		b = 0xFC / 255.0f;
	} else if (colorIndex >= PAL16_YELLOW && colorIndex < PAL16_YELLOW + 16) {
		// Gold (unique items)
		r = 0xFA / 255.0f;
		g = 0xD6 / 255.0f;
		b = 0x91 / 255.0f;
	} else if (colorIndex >= PAL16_BEIGE && colorIndex < PAL16_BEIGE + 16) {
		// Beige/red (normal items)
		r = 0xF3 / 255.0f;
		g = 0x8E / 255.0f;
		b = 0x8E / 255.0f;
	} else {
		// Orange (inspecting other player) or fallback
		r = 0xFC / 255.0f;
		g = 0xA0 / 255.0f;
		b = 0x60 / 255.0f;
	}
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glColor4f(r, g, b, 1.0f);
	glBegin(GL_QUADS);
	glVertex2i(x, y);
	glVertex2i(x + w, y);
	glVertex2i(x + w, y + h);
	glVertex2i(x, y + h);
	glEnd();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1BlitSurface(const Surface &src, SDL_Rect srcRect, Point targetPosition)
{
	EnsureFrameBegun();

	const int w = srcRect.w;
	const int h = srcRect.h;
	if (w <= 0 || h <= 0)
		return;

	// Decode the 8-bit paletted region to RGBA.
	std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
	for (int row = 0; row < h; row++) {
		const uint8_t *srcRow = src.at(srcRect.x, srcRect.y + row);
		uint8_t *dstRow = rgba.data() + static_cast<size_t>(row) * w * 4;
		for (int col = 0; col < w; col++) {
			const uint8_t index = srcRow[col];
			const SDL_Color &c = system_palette[index];
			dstRow[col * 4 + 0] = c.r;
			dstRow[col * 4 + 1] = c.g;
			dstRow[col * 4 + 2] = c.b;
			dstRow[col * 4 + 3] = 255;
		}
	}

	// Upload to a transient texture and draw.
	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glColor4f(1, 1, 1, 1);
	DrawTexturedQuad(texId, targetPosition.x, targetPosition.y, w, h);

	glDeleteTextures(1, &texId);
}

void Gl1BlitSurfaceSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition)
{
	EnsureFrameBegun();

	const int w = srcRect.w;
	const int h = srcRect.h;
	if (w <= 0 || h <= 0)
		return;

	std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
	for (int row = 0; row < h; row++) {
		const uint8_t *srcRow = src.at(srcRect.x, srcRect.y + row);
		uint8_t *dstRow = rgba.data() + static_cast<size_t>(row) * w * 4;
		for (int col = 0; col < w; col++) {
			const uint8_t index = srcRow[col];
			if (index == 0) {
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

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glColor4f(1, 1, 1, 1);
	DrawTexturedQuad(texId, targetPosition.x, targetPosition.y, w, h);

	glDeleteTextures(1, &texId);
}

void Gl1InvalidateTextureCache()
{
	textureCache.NotifyFullPaletteChanged();
}

void Gl1ClearTextureCache()
{
	textureCache.NotifyFullPaletteChanged();
}

void Gl1NotifyPaletteChanged()
{
	textureCache.NotifyPaletteChanged();
}

void Gl1NotifyFullPaletteChanged()
{
	textureCache.NotifyFullPaletteChanged();
}

void Gl1DrawVideoFrame(SDL_Surface *surface, uint32_t videoWidth, uint32_t videoHeight)
{
	EnsureFrameBegun();

	const int w = surface->w;
	const int h = surface->h;
	if (w <= 0 || h <= 0)
		return;

	// Convert the surface to RGBA.
	std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
#ifdef USE_SDL3
	const SDL_Palette *pal = SDL_GetSurfacePalette(surface);
#else
	const SDL_Palette *pal = surface->format->palette;
#endif
	const bool isPaletted = (pal != nullptr);

	if (isPaletted) {
		const uint8_t *pixels = static_cast<const uint8_t *>(surface->pixels);
		for (int row = 0; row < h; row++) {
			const uint8_t *srcRow = pixels + row * surface->pitch;
			uint8_t *dstRow = rgba.data() + static_cast<size_t>(row) * w * 4;
			for (int col = 0; col < w; col++) {
				const SDL_Color &c = pal->colors[srcRow[col]];
				dstRow[col * 4 + 0] = c.r;
				dstRow[col * 4 + 1] = c.g;
				dstRow[col * 4 + 2] = c.b;
				dstRow[col * 4 + 3] = 255;
			}
		}
	} else {
		// Assume 32-bit RGBA surface.
		const uint8_t *pixels = static_cast<const uint8_t *>(surface->pixels);
		for (int row = 0; row < h; row++) {
			std::memcpy(rgba.data() + static_cast<size_t>(row) * w * 4,
			    pixels + row * surface->pitch, static_cast<size_t>(w) * 4);
		}
	}

	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Compute letterboxed output rect maintaining aspect ratio.
	int outW, outH;
	if (static_cast<uint64_t>(videoWidth) * logicalHeight > static_cast<uint64_t>(videoHeight) * logicalWidth) {
		// Landscape fit: width-limited.
		outW = logicalWidth;
		outH = static_cast<int>(static_cast<uint64_t>(videoHeight) * logicalWidth / videoWidth);
	} else {
		// Portrait fit: height-limited.
		outW = static_cast<int>(static_cast<uint64_t>(videoWidth) * logicalHeight / videoHeight);
		outH = logicalHeight;
	}
	const int outX = (logicalWidth - outW) / 2;
	const int outY = (logicalHeight - outH) / 2;

	// Clear to black for letterbox bars.
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glColor4f(1, 1, 1, 1);
	DrawTexturedQuad(texId, outX, outY, outW, outH);

	glDeleteTextures(1, &texId);
}

void Gl1SetZoom()
{
	EnsureFrameBegun();
	zoomActive = true;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glScalef(2.0f, 2.0f, 1.0f);
}

void Gl1ClearZoom()
{
	zoomActive = false;
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void Gl1DrawRedBack()
{
	EnsureFrameBegun();

	// The software renderer applies pause.trn which remaps palette indices
	// to red-tinted equivalents. Examining the cathedral palette, the target
	// ramp entries (e.g. orange ramp 96-111) have roughly R*1.0, G*0.35, B*0.35
	// relative to the source luminance. We approximate this with two passes:
	//
	// 1. Multiply blend to shift hue toward red (keep full red, reduce G/B).
	// 2. Darken slightly to match the overall dimming of the remap.
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);

	// Pass 1: Multiply — tint toward red.
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glColor4f(1.0f, 0.2f, 0.2f, 1.0f);
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(logicalWidth, 0);
	glVertex2i(logicalWidth, logicalHeight);
	glVertex2i(0, logicalHeight);
	glEnd();

	// Pass 2: Darken — the TRN remap also lowers overall brightness.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.0f, 0.0f, 0.0f, 0.15f);
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(logicalWidth, 0);
	glVertex2i(logicalWidth, logicalHeight);
	glVertex2i(0, logicalHeight);
	glEnd();

	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

void Gl1ApplyFade(int fadeLevel)
{
	if (fadeLevel <= 0)
		return;

	EnsureFrameBegun();

	const float alpha = (fadeLevel >= 256) ? 1.0f : (static_cast<float>(fadeLevel) / 256.0f);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(logicalWidth, 0);
	glVertex2i(logicalWidth, logicalHeight);
	glVertex2i(0, logicalHeight);
	glEnd();
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);
}

} // namespace devilution

#endif // DEVILUTIONX_GL1_RENDERER
