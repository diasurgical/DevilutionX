#include "utils/display.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

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
#include "headless_mode.hpp"
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

	if (*GetOptions().Graphics.integerScaling) {
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
	Size windowSize = forceResolution.width != 0 ? forceResolution : *GetOptions().Graphics.resolution;

#ifndef USE_SDL1
	if (*GetOptions().Graphics.upscale && *GetOptions().Graphics.fitToScreen) {
		CalculatePreferredWindowSize(windowSize.width, windowSize.height);
	}
#endif
	AdjustToScreenGeometry(windowSize);
	return windowSize;
}

const auto OptionChangeHandlerResolution = (GetOptions().Graphics.resolution.SetValueChangedCallback(ResizeWindow), true);
const auto OptionChangeHandlerFullscreen = (GetOptions().Graphics.fullscreen.SetValueChangedCallback(SetFullscreenMode), true);

void OptionGrabInputChanged()
{
#ifdef USE_SDL1
	SDL_WM_GrabInput(*GetOptions().Gameplay.grabInput ? SDL_GRAB_ON : SDL_GRAB_OFF);
#else
	if (ghMainWnd != nullptr)
		SDL_SetWindowGrab(ghMainWnd, *GetOptions().Gameplay.grabInput ? SDL_TRUE : SDL_FALSE);
#endif
}
const auto OptionChangeHandlerGrabInput = (GetOptions().Gameplay.grabInput.SetValueChangedCallback(OptionGrabInputChanged), true);

void UpdateAvailableResolutions()
{
	GraphicsOptions &graphicsOptions = GetOptions().Graphics;

	std::vector<Size> sizes;
	float scaleFactor = GetDpiScalingFactor();

	// Add resolutions
	bool supportsAnyResolution = false;
#ifdef USE_SDL1
	auto *modes = SDL_ListModes(nullptr, SDL_FULLSCREEN | SDL_HWPALETTE);
	// SDL_ListModes returns -1 if any resolution is allowed (for example returned on 3DS)
	if (modes == (SDL_Rect **)-1) {
		supportsAnyResolution = true;
	} else if (modes != nullptr) {
		for (size_t i = 0; modes[i] != nullptr; i++) {
			if (modes[i]->w < modes[i]->h) {
				std::swap(modes[i]->w, modes[i]->h);
			}
			sizes.emplace_back(Size {
			    static_cast<int>(modes[i]->w * scaleFactor),
			    static_cast<int>(modes[i]->h * scaleFactor) });
		}
	}
#else
	int displayModeCount = SDL_GetNumDisplayModes(0);
	for (int i = 0; i < displayModeCount; i++) {
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(0, i, &mode) != 0) {
			ErrSdl();
		}
		if (mode.w < mode.h) {
			std::swap(mode.w, mode.h);
		}
		sizes.emplace_back(Size {
		    static_cast<int>(mode.w * scaleFactor),
		    static_cast<int>(mode.h * scaleFactor) });
	}
	supportsAnyResolution = *GetOptions().Graphics.upscale;
#endif

	if (supportsAnyResolution && sizes.size() == 1) {
		// Attempt to provide sensible options for 4:3 and the native aspect ratio
		const int width = sizes[0].width;
		const int height = sizes[0].height;
		const int commonHeights[] = { 480, 540, 720, 960, 1080, 1440, 2160 };
		for (int commonHeight : commonHeights) {
			if (commonHeight > height)
				break;
			sizes.emplace_back(Size { commonHeight * 4 / 3, commonHeight });
			if (commonHeight * width % height == 0)
				sizes.emplace_back(Size { commonHeight * width / height, commonHeight });
		}
	}

	const Size configuredSize = *graphicsOptions.resolution;

	// Ensures that the ini specified resolution is present in resolution list even if it doesn't match a monitor resolution (for example if played in window mode)
	sizes.push_back(configuredSize);
	// Ensures that the platform's preferred default resolution is always present
	sizes.emplace_back(Size { DEFAULT_WIDTH, DEFAULT_HEIGHT });
	// Ensures that the vanilla Diablo resolution is present on systems that would support it
	if (supportsAnyResolution)
		sizes.emplace_back(Size { 640, 480 });

#ifndef USE_SDL1
	if (*graphicsOptions.fitToScreen) {
		SDL_DisplayMode mode;
		if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
			ErrSdl();
		}
		for (auto &size : sizes) {
			// Ensure that the ini specified resolution remains present in the resolution list
			if (size.height == configuredSize.height)
				size.width = configuredSize.width;
			else
				size.width = size.height * mode.w / mode.h;
		}
	}
#endif

	// Sort by width then by height
	c_sort(sizes, [](const Size &x, const Size &y) -> bool {
		if (x.width == y.width)
			return x.height > y.height;
		return x.width > y.width;
	});
	// Remove duplicate entries
	sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

	std::vector<std::pair<Size, std::string>> resolutions;
	for (auto &size : sizes) {
#ifndef USE_SDL1
		if (*graphicsOptions.fitToScreen) {
			resolutions.emplace_back(size, StrCat(size.height, "p"));
			continue;
		}
#endif
		resolutions.emplace_back(size, StrCat(size.width, "x", size.height));
	}
	graphicsOptions.resolution.setAvailableResolutions(std::move(resolutions));
}

#if !defined(USE_SDL1) || defined(__3DS__)
void ResizeWindowAndUpdateResolutionOptions()
{
	ResizeWindow();
#ifndef __3DS__
	UpdateAvailableResolutions();
#endif
}
const auto OptionChangeHandlerFitToScreen = (GetOptions().Graphics.fitToScreen.SetValueChangedCallback(ResizeWindowAndUpdateResolutionOptions), true);
#endif

#ifndef USE_SDL1
const auto OptionChangeHandlerScaleQuality = (GetOptions().Graphics.scaleQuality.SetValueChangedCallback(ReinitializeTexture), true);
const auto OptionChangeHandlerIntegerScaling = (GetOptions().Graphics.integerScaling.SetValueChangedCallback(ReinitializeIntegerScale), true);
const auto OptionChangeHandlerVSync = (GetOptions().Graphics.frameRateControl.SetValueChangedCallback(ReinitializeRenderer), true);
#endif

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
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface == nullptr) {
		ErrSdl();
	}
	Log("Video surface is now {}x{} bpp={} flags=0x{:08X}",
	    surface->w, surface->h, surface->format->BitsPerPixel, surface->flags);
}

void SetVideoModeToPrimary(bool fullscreen, int width, int height)
{
	int flags = SDL1_VIDEO_MODE_FLAGS | SDL_HWPALETTE;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
#ifdef __3DS__
	flags &= ~SDL_FULLSCREEN;
	flags |= Get3DSScalingFlag(*GetOptions().Graphics.fitToScreen, width, height);
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
		Size windowSize = forceResolution.width != 0 ? forceResolution : *GetOptions().Graphics.resolution;
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
	if (GetOptions().Controller.szMapping[0] != '\0') {
		SDL_GameControllerAddMapping(GetOptions().Controller.szMapping);
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
	SetVideoModeToPrimary(*GetOptions().Graphics.fullscreen, windowSize.width, windowSize.height);
	if (*GetOptions().Gameplay.grabInput)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	atexit(SDL_VideoQuit); // Without this video mode is not restored after fullscreen.
#else
	int flags = SDL_WINDOW_ALLOW_HIGHDPI;
	if (*GetOptions().Graphics.upscale) {
		if (*GetOptions().Graphics.fullscreen) {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		flags |= SDL_WINDOW_RESIZABLE;
	} else if (*GetOptions().Graphics.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	ghMainWnd = SDL_CreateWindow(lpWindowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowSize.width, windowSize.height, flags);

	// Note: https://github.com/libsdl-org/SDL/issues/962
	// This is a solution to a problem related to SDL mouse grab.
	// See https://github.com/diasurgical/devilutionX/issues/4251
	if (ghMainWnd != nullptr)
		SDL_SetWindowGrab(ghMainWnd, *GetOptions().Gameplay.grabInput ? SDL_TRUE : SDL_FALSE);

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

	if (ghMainWnd != nullptr) {
		UpdateAvailableResolutions();
		return true;
	}

	return false;
}

#ifndef USE_SDL1
void ReinitializeTexture()
{
	if (texture)
		texture.reset();

	if (renderer == nullptr)
		return;

	auto quality = StrCat(static_cast<int>(*GetOptions().Graphics.scaleQuality));
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality.c_str());

	texture = SDLWrap::CreateTexture(renderer, DEVILUTIONX_DISPLAY_TEXTURE_FORMAT, SDL_TEXTUREACCESS_STREAMING, gnScreenWidth, gnScreenHeight);
}

void ReinitializeIntegerScale()
{
	if (*GetOptions().Graphics.fitToScreen) {
		ResizeWindow();
		return;
	}

	if (renderer != nullptr && SDL_RenderSetIntegerScale(renderer, *GetOptions().Graphics.integerScaling ? SDL_TRUE : SDL_FALSE) < 0) {
		ErrSdl();
	}
}
#endif

void ReinitializeRenderer()
{
	if (ghMainWnd == nullptr)
		return;

#ifdef USE_SDL1
	const SDL_Surface *surface = SDL_GetVideoSurface();
	if (surface == nullptr) {
		ErrSdl();
	}
	AdjustToScreenGeometry(Size(surface->w, surface->h));
#else

	if (*GetOptions().Graphics.upscale) {
		// We don't recreate the renderer, because this can result in a freezing (not refreshing) rendering
		if (renderer == nullptr) {
			renderer = SDL_CreateRenderer(ghMainWnd, -1, 0);
			if (renderer == nullptr) {
				ErrSdl();
			}
		}

#if SDL_VERSION_ATLEAST(2, 0, 18)
		SDL_RenderSetVSync(renderer, *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync ? 1 : 0);
#endif

		ReinitializeTexture();

		if (SDL_RenderSetIntegerScale(renderer, *GetOptions().Graphics.integerScaling ? SDL_TRUE : SDL_FALSE) < 0) {
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
	if (*GetOptions().Graphics.fullscreen) {
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
	if (*GetOptions().Graphics.fullscreen && !*GetOptions().Graphics.upscale) {
		Size windowSize = GetPreferredWindowSize();
		SDL_DisplayMode displayMode = GetNearestDisplayMode(windowSize);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &displayMode) != 0) {
			ErrSdl();
		}
	}

	Uint32 flags = 0;
	if (*GetOptions().Graphics.fullscreen) {
		flags = *GetOptions().Graphics.upscale ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	}
	if (SDL_SetWindowFullscreen(ghMainWnd, flags) != 0) {
		ErrSdl();
	}

	if (!*GetOptions().Graphics.fullscreen) {
		SDL_RestoreWindow(ghMainWnd); // Avoid window being maximized before resizing
		Size windowSize = GetPreferredWindowSize();
		SDL_SetWindowSize(ghMainWnd, windowSize.width, windowSize.height);
	}
	if (!*GetOptions().Graphics.upscale) {
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
	SetVideoModeToPrimary(*GetOptions().Graphics.fullscreen, windowSize.width, windowSize.height);
#else
	// For "true fullscreen" windows, the window resizes automatically based on the display mode
	bool trueFullscreen = *GetOptions().Graphics.fullscreen && !*GetOptions().Graphics.upscale;
	if (trueFullscreen) {
		SDL_DisplayMode displayMode = GetNearestDisplayMode(windowSize);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &displayMode) != 0)
			ErrSdl();
	}

	// Handle switching between "fake fullscreen" and "true fullscreen" when upscale is toggled
	bool upscaleChanged = *GetOptions().Graphics.upscale != (renderer != nullptr);
	if (upscaleChanged && *GetOptions().Graphics.fullscreen) {
		Uint32 flags = *GetOptions().Graphics.upscale ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		if (SDL_SetWindowFullscreen(ghMainWnd, flags) != 0)
			ErrSdl();
		if (!*GetOptions().Graphics.fullscreen)
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
