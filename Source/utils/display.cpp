#include "utils/display.h"

#include <algorithm>
#include <cstdint>

#ifdef __vita__
#include <psp2/power.h>
#endif

#ifdef __3DS__
#include "platform/ctr/display.hpp"
#endif

#ifdef NXDK
#include <hal/video.h>
#endif

#include "DiabloUI/diabloui.h"
#include "control.h"
#include "controls/controller.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/devices/joystick.h"
#include "controls/devices/kbcontroller.h"
#include "controls/game_controls.h"
#include "controls/touch/gamepad.h"
#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "options.h"
#include "utils/log.hpp"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"
#include "utils/str_cat.hpp"

#ifdef USE_SDL1
#ifndef SDL1_VIDEO_MODE_BPP
#define SDL1_VIDEO_MODE_BPP 0
#endif
#ifndef SDL1_VIDEO_MODE_FLAGS
#define SDL1_VIDEO_MODE_FLAGS SDL_SWSURFACE
#endif
#endif

namespace devilution {

extern SDLSurfaceUniquePtr RendererTextureSurface; /** defined in dx.cpp */
SDL_Window *ghMainWnd;

Size forceResolution;

Uint16 gnScreenWidth;
Uint16 gnScreenHeight;
Uint16 gnViewportHeight;

Uint16 GetScreenWidth()
{
	return gnScreenWidth;
}

Uint16 GetScreenHeight()
{
	return gnScreenHeight;
}

Uint16 GetViewportHeight()
{
	return gnViewportHeight;
}

Rectangle UIRectangle;
const Rectangle &GetUIRectangle()
{
	return UIRectangle;
}

namespace {

#ifndef USE_SDL1
void CalculatePreferredWindowSize(int &width, int &height)
{
	SDL_DisplayMode mode;
	if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
		ErrSdl();
	}

	if (mode.w < mode.h) {
		std::swap(mode.w, mode.h);
	}

	if (*sgOptions.Graphics.integerScaling) {
		int factor = std::min(mode.w / width, mode.h / height);
		width = mode.w / factor;
		height = mode.h / factor;
		return;
	}

	float wFactor = (float)mode.w / width;
	float hFactor = (float)mode.h / height;

	if (wFactor > hFactor) {
		width = mode.w * height / mode.h;
	} else {
		height = mode.h * width / mode.w;
	}
}

void FreeRenderer()
{
#if defined(_WIN32) && !defined(NXDK)
	bool wasD3D9 = false;
	if (renderer != nullptr) {
		SDL_RendererInfo previousRendererInfo;
		SDL_GetRendererInfo(renderer, &previousRendererInfo);
		wasD3D9 = (string_view(previousRendererInfo.name) == "direct3d");
	}
#endif

	if (renderer != nullptr) {
		SDL_DestroyRenderer(renderer);
		renderer = nullptr;
	}

#if defined(_WIN32) && !defined(NXDK) && !defined(USE_SDL1)
	// On Windows 11 the directx9 VSYNC timer doesn't get recreated properly, see https://github.com/libsdl-org/SDL/issues/5099
	if (wasD3D9 && *sgOptions.Graphics.upscale && *sgOptions.Graphics.frameRateControl != FrameRateControl::VerticalSync) {
		std::string title = SDL_GetWindowTitle(ghMainWnd);
		Uint32 flags = SDL_GetWindowFlags(ghMainWnd);
		Rectangle dimensions;

		SDL_GetWindowPosition(ghMainWnd, &dimensions.position.x, &dimensions.position.y);
		SDL_GetWindowSize(ghMainWnd, &dimensions.size.width, &dimensions.size.height);
		SDL_DestroyWindow(ghMainWnd);

		ghMainWnd = SDL_CreateWindow(
		    title.c_str(),
		    dimensions.position.x,
		    dimensions.position.y,
		    dimensions.size.width,
		    dimensions.size.height,
		    flags);
	}
#endif
}

SDL_DisplayMode GetNearestDisplayMode(Size preferredSize)
{
	SDL_DisplayMode nearestDisplayMode;
	if (SDL_GetWindowDisplayMode(ghMainWnd, &nearestDisplayMode) != 0)
		ErrSdl();

	int displayIndex = SDL_GetWindowDisplayIndex(ghMainWnd);
	int modeCount = SDL_GetNumDisplayModes(displayIndex);
	for (int modeIndex = 0; modeIndex < modeCount; modeIndex++) {
		SDL_DisplayMode displayMode;
		if (SDL_GetDisplayMode(displayIndex, modeIndex, &displayMode) != 0)
			continue;

		int diffHeight = std::abs(nearestDisplayMode.h - preferredSize.height) - std::abs(displayMode.h - preferredSize.height);
		int diffWidth = std::abs(nearestDisplayMode.w - preferredSize.width) - std::abs(displayMode.w - preferredSize.width);
		if (diffHeight < 0)
			continue;
		if (diffHeight == 0 && diffWidth < 0)
			continue;
		nearestDisplayMode = displayMode;
	}

	return nearestDisplayMode;
}
#endif

void CalculateUIRectangle()
{
	constexpr Size UISize { 640, 480 };
	UIRectangle = {
		{ (gnScreenWidth - UISize.width) / 2, (gnScreenHeight - UISize.height) / 2 },
		UISize
	};
}

Size GetPreferredWindowSize()
{
	Size windowSize = forceResolution.width != 0 ? forceResolution : *sgOptions.Graphics.resolution;

#ifndef USE_SDL1
	if (*sgOptions.Graphics.upscale && *sgOptions.Graphics.fitToScreen) {
		CalculatePreferredWindowSize(windowSize.width, windowSize.height);
	}
#endif
	AdjustToScreenGeometry(windowSize);
	return windowSize;
}

} // namespace

void AdjustToScreenGeometry(Size windowSize)
{
	gnScreenWidth = windowSize.width;
	gnScreenHeight = windowSize.height;
	CalculateUIRectangle();
	CalculatePanelAreas();
}

float GetDpiScalingFactor()
{
#ifdef USE_SDL1
	return 1.0F;
#else
	if (renderer == nullptr)
		return 1.0F;

	int renderWidth;
	int renderHeight;
	SDL_GetRendererOutputSize(renderer, &renderWidth, &renderHeight);

	int windowWidth;
	int windowHeight;
	SDL_GetWindowSize(ghMainWnd, &windowWidth, &windowHeight);

	float hfactor = static_cast<float>(renderWidth) / windowWidth;
	float vhfactor = static_cast<float>(renderHeight) / windowHeight;

	return std::min(hfactor, vhfactor);
#endif
}

#ifdef USE_SDL1
void SetVideoMode(int width, int height, int bpp, uint32_t flags)
{
	Log("Setting video mode {}x{} bpp={} flags=0x{:08X}", width, height, bpp, flags);
	ghMainWnd = SDL_SetVideoMode(width, height, bpp, flags);
	if (ghMainWnd == nullptr) {
		ErrSdl();
	}
	const SDL_VideoInfo &current = *SDL_GetVideoInfo();
	Log("Video mode is now {}x{} bpp={} flags=0x{:08X}",
	    current.current_w, current.current_h, current.vfmt->BitsPerPixel, SDL_GetVideoSurface()->flags);
}

void SetVideoModeToPrimary(bool fullscreen, int width, int height)
{
	int flags = SDL1_VIDEO_MODE_FLAGS | SDL_HWPALETTE;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
#ifdef __3DS__
	flags &= ~SDL_FULLSCREEN;
	flags |= Get3DSScalingFlag(*sgOptions.Graphics.fitToScreen, width, height);
#endif
	SetVideoMode(width, height, SDL1_VIDEO_MODE_BPP, flags);
	if (OutputRequiresScaling())
		Log("Using software scaling");
}
#endif

bool IsFullScreen()
{
#ifdef USE_SDL1
	return (SDL_GetVideoSurface()->flags & SDL_FULLSCREEN) != 0;
#else
	return (SDL_GetWindowFlags(ghMainWnd) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
#endif
}

bool SpawnWindow(const char *lpWindowName)
{
#ifdef __vita__
	scePowerSetArmClockFrequency(444);
#endif
#ifdef NXDK
	{
		Size windowSize = forceResolution.width != 0 ? forceResolution : *sgOptions.Graphics.resolution;
		VIDEO_MODE xmode;
		void *p = nullptr;
		while (XVideoListModes(&xmode, 0, 0, &p)) {
			if (windowSize.width >= xmode.width && windowSize.height == xmode.height)
				break;
		}
		XVideoSetMode(xmode.width, xmode.height, xmode.bpp, xmode.refresh);
	}
#endif

#if SDL_VERSION_ATLEAST(2, 0, 4)
	SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 6) && defined(__vita__)
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 10)
	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 2)
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 12)
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#endif

	int initFlags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
#ifndef NOSOUND
	initFlags |= SDL_INIT_AUDIO;
#endif
#ifndef USE_SDL1
	initFlags |= SDL_INIT_GAMECONTROLLER;

	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
	if (SDL_Init(initFlags) <= -1) {
		ErrSdl();
	}
	RegisterCustomEvents();

#ifndef USE_SDL1
	if (sgOptions.Controller.szMapping[0] != '\0') {
		SDL_GameControllerAddMapping(sgOptions.Controller.szMapping);
	}
#endif

#ifdef USE_SDL1
	// On SDL 1, there are no ADDED/REMOVED events.
	// Always try to initialize the first joystick.
	Joystick::Add(0);
#ifdef __SWITCH__
	// TODO: There is a bug in SDL2 on Switch where it does not report controllers on startup (Jan 1, 2020)
	GameController::Add(0);
#endif
#endif

	Size windowSize = GetPreferredWindowSize();

#ifdef USE_SDL1
	SDL_WM_SetCaption(lpWindowName, WINDOW_ICON_NAME);
	SetVideoModeToPrimary(*sgOptions.Graphics.fullscreen, windowSize.width, windowSize.height);
	if (*sgOptions.Gameplay.grabInput)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	atexit(SDL_VideoQuit); // Without this video mode is not restored after fullscreen.
#else
	int flags = SDL_WINDOW_ALLOW_HIGHDPI;
	if (*sgOptions.Graphics.upscale) {
		if (*sgOptions.Graphics.fullscreen) {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		flags |= SDL_WINDOW_RESIZABLE;
	} else if (*sgOptions.Graphics.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	ghMainWnd = SDL_CreateWindow(lpWindowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowSize.width, windowSize.height, flags);

	// Note: https://github.com/libsdl-org/SDL/issues/962
	// This is a solution to a problem related to SDL mouse grab.
	// See https://github.com/diasurgical/devilutionX/issues/4251
	if (ghMainWnd != nullptr)
		SDL_SetWindowGrab(ghMainWnd, *sgOptions.Gameplay.grabInput ? SDL_TRUE : SDL_FALSE);

#endif
	if (ghMainWnd == nullptr) {
		ErrSdl();
	}

	int refreshRate = 60;
#ifndef USE_SDL1
	SDL_DisplayMode mode;
	SDL_GetDisplayMode(0, 0, &mode);
	if (mode.refresh_rate != 0) {
		refreshRate = mode.refresh_rate;
	}
#endif
	refreshDelay = 1000000 / refreshRate;

	ReinitializeRenderer();

	return ghMainWnd != nullptr;
}

#ifndef USE_SDL1
void ReinitializeTexture()
{
	if (texture)
		texture.reset();

	if (renderer == nullptr)
		return;

	auto quality = StrCat(static_cast<int>(*sgOptions.Graphics.scaleQuality));
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality.c_str());

	texture = SDLWrap::CreateTexture(renderer, DEVILUTIONX_DISPLAY_TEXTURE_FORMAT, SDL_TEXTUREACCESS_STREAMING, gnScreenWidth, gnScreenHeight);
}

void ReinitializeIntegerScale()
{
	if (*sgOptions.Graphics.fitToScreen) {
		ResizeWindow();
		return;
	}

	if (renderer != nullptr && SDL_RenderSetIntegerScale(renderer, *sgOptions.Graphics.integerScaling ? SDL_TRUE : SDL_FALSE) < 0) {
		ErrSdl();
	}
}
#endif

void ReinitializeRenderer()
{
	if (ghMainWnd == nullptr)
		return;

#ifdef USE_SDL1
	const SDL_VideoInfo &current = *SDL_GetVideoInfo();
	Size windowSize = { current.current_w, current.current_h };
	AdjustToScreenGeometry(windowSize);
#else
	if (texture)
		texture.reset();

	FreeRenderer();

	if (*sgOptions.Graphics.upscale) {
		Uint32 rendererFlags = 0;

		if (*sgOptions.Graphics.frameRateControl == FrameRateControl::VerticalSync) {
			rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
		}

		renderer = SDL_CreateRenderer(ghMainWnd, -1, rendererFlags);
		if (renderer == nullptr) {
			ErrSdl();
		}

		ReinitializeTexture();

		if (SDL_RenderSetIntegerScale(renderer, *sgOptions.Graphics.integerScaling ? SDL_TRUE : SDL_FALSE) < 0) {
			ErrSdl();
		}

		if (SDL_RenderSetLogicalSize(renderer, gnScreenWidth, gnScreenHeight) <= -1) {
			ErrSdl();
		}

		Uint32 format;
		if (SDL_QueryTexture(texture.get(), &format, nullptr, nullptr, nullptr) < 0)
			ErrSdl();
		RendererTextureSurface = SDLWrap::CreateRGBSurfaceWithFormat(0, gnScreenWidth, gnScreenHeight, SDL_BITSPERPIXEL(format), format);
	} else {
		Size windowSize = {};
		SDL_GetWindowSize(ghMainWnd, &windowSize.width, &windowSize.height);
		AdjustToScreenGeometry(windowSize);
	}
#endif
}

void SetFullscreenMode()
{
#ifdef USE_SDL1
	Uint32 flags = ghMainWnd->flags ^ SDL_FULLSCREEN;
	if (*sgOptions.Graphics.fullscreen) {
		flags |= SDL_FULLSCREEN;
	}
	ghMainWnd = SDL_SetVideoMode(0, 0, 0, flags);
	if (ghMainWnd == NULL) {
		ErrSdl();
	}
#else
	// When switching from windowed to "true fullscreen",
	// update the display mode of the window before changing the
	// fullscreen mode so that the display mode only has to change once
	if (*sgOptions.Graphics.fullscreen && !*sgOptions.Graphics.upscale) {
		Size windowSize = GetPreferredWindowSize();
		SDL_DisplayMode displayMode = GetNearestDisplayMode(windowSize);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &displayMode) != 0) {
			ErrSdl();
		}
	}

	Uint32 flags = 0;
	if (*sgOptions.Graphics.fullscreen) {
		flags = *sgOptions.Graphics.upscale ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	}
	if (SDL_SetWindowFullscreen(ghMainWnd, flags) != 0) {
		ErrSdl();
	}

	if (!*sgOptions.Graphics.fullscreen) {
		SDL_RestoreWindow(ghMainWnd); // Avoid window being maximized before resizing
		Size windowSize = GetPreferredWindowSize();
		SDL_SetWindowSize(ghMainWnd, windowSize.width, windowSize.height);
	}
	if (!*sgOptions.Graphics.upscale) {
		// Because "true fullscreen" is locked into specific resolutions based on the modes
		// supported by the display, the resolution may have changed when fullscreen was toggled
		ReinitializeRenderer();
		CreateBackBuffer();
	}
	InitializeVirtualGamepad();
#endif
	RedrawEverything();
}

void ResizeWindow()
{
	if (ghMainWnd == nullptr)
		return;

	Size windowSize = GetPreferredWindowSize();

#ifdef USE_SDL1
	SetVideoModeToPrimary(*sgOptions.Graphics.fullscreen, windowSize.width, windowSize.height);
#else
	// For "true fullscreen" windows, the window resizes automatically based on the display mode
	bool trueFullscreen = *sgOptions.Graphics.fullscreen && !*sgOptions.Graphics.upscale;
	if (trueFullscreen) {
		SDL_DisplayMode displayMode = GetNearestDisplayMode(windowSize);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &displayMode) != 0)
			ErrSdl();
	}

	// Handle switching between "fake fullscreen" and "true fullscreen" when upscale is toggled
	bool upscaleChanged = *sgOptions.Graphics.upscale != (renderer != nullptr);
	if (upscaleChanged && *sgOptions.Graphics.fullscreen) {
		Uint32 flags = *sgOptions.Graphics.upscale ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		if (SDL_SetWindowFullscreen(ghMainWnd, flags) != 0)
			ErrSdl();
		if (!*sgOptions.Graphics.fullscreen)
			SDL_RestoreWindow(ghMainWnd); // Avoid window being maximized before resizing
	}

	if (!trueFullscreen)
		SDL_SetWindowSize(ghMainWnd, windowSize.width, windowSize.height);
#endif

	ReinitializeRenderer();

#ifndef USE_SDL1
	SDL_SetWindowResizable(ghMainWnd, renderer != nullptr ? SDL_TRUE : SDL_FALSE);
	InitializeVirtualGamepad();
#endif

	CreateBackBuffer();
	RedrawEverything();
}

SDL_Surface *GetOutputSurface()
{
#ifdef USE_SDL1
	SDL_Surface *ret = SDL_GetVideoSurface();
	if (ret == nullptr)
		ErrSdl();
	return ret;
#else
	if (renderer != nullptr)
		return RendererTextureSurface.get();
	SDL_Surface *ret = SDL_GetWindowSurface(ghMainWnd);
	if (ret == nullptr)
		ErrSdl();
	return ret;
#endif
}

bool OutputRequiresScaling()
{
#ifdef USE_SDL1
	if (HeadlessMode)
		return false;
	return gnScreenWidth != GetOutputSurface()->w || gnScreenHeight != GetOutputSurface()->h;
#else // SDL2, scaling handled by renderer.
	return false;
#endif
}

void ScaleOutputRect(SDL_Rect *rect)
{
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	rect->x = rect->x * surface->w / gnScreenWidth;
	rect->y = rect->y * surface->h / gnScreenHeight;
	rect->w = rect->w * surface->w / gnScreenWidth;
	rect->h = rect->h * surface->h / gnScreenHeight;
}

#ifdef USE_SDL1
namespace {

SDLSurfaceUniquePtr CreateScaledSurface(SDL_Surface *src)
{
	SDL_Rect stretched_rect = MakeSdlRect(0, 0, src->w, src->h);
	ScaleOutputRect(&stretched_rect);
	SDLSurfaceUniquePtr stretched = SDLWrap::CreateRGBSurface(
	    SDL_SWSURFACE, stretched_rect.w, stretched_rect.h, src->format->BitsPerPixel,
	    src->format->Rmask, src->format->Gmask, src->format->Bmask, src->format->Amask);
	if (SDL_HasColorKey(src)) {
		SDL_SetColorKey(stretched.get(), SDL_SRCCOLORKEY, src->format->colorkey);
		if (src->format->palette != NULL)
			SDL_SetPalette(stretched.get(), SDL_LOGPAL, src->format->palette->colors, 0, src->format->palette->ncolors);
	}
	if (SDL_SoftStretch((src), NULL, stretched.get(), &stretched_rect) < 0)
		ErrSdl();
	return stretched;
}

} // namespace
#endif // USE_SDL1

SDLSurfaceUniquePtr ScaleSurfaceToOutput(SDLSurfaceUniquePtr surface)
{
#ifdef USE_SDL1
	if (OutputRequiresScaling())
		return CreateScaledSurface(surface.get());
#endif
	return surface;
}

} // namespace devilution
