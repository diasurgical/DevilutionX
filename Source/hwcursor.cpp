#include "hwcursor.hpp"

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL_version.h>

#ifndef USE_SDL1
#include <SDL_mouse.h>
#include <SDL_surface.h>
#endif
#endif

#include "DiabloUI/diabloui.h"
#include "engine/render/clx_render.hpp"
#include "utils/sdl_bilinear_scale.hpp"

namespace devilution {
namespace {
CursorInfo CurrentCursorInfo;

#if SDL_VERSION_ATLEAST(2, 0, 0)
SDLCursorUniquePtr CurrentCursor;

enum class HotpointPosition : uint8_t {
	TopLeft,
	Center,
};

Size ScaledSize(Size size)
{
	return GetRenderer().GetScaledCursorSize(size);
}

bool IsCursorSizeAllowed(Size size)
{
	if (*GetOptions().Graphics.hardwareCursorMaxSize <= 0)
		return true;
	size = ScaledSize(size);
	return size.width <= *GetOptions().Graphics.hardwareCursorMaxSize
	    && size.height <= *GetOptions().Graphics.hardwareCursorMaxSize;
}

Point GetHotpointPosition(const SDL_Surface &surface, HotpointPosition position)
{
	switch (position) {
	case HotpointPosition::TopLeft:
		return { 0, 0 };
	case HotpointPosition::Center:
		return { surface.w / 2, surface.h / 2 };
	}
	app_fatal("Unhandled enum value");
}

bool ShouldUseBilinearScaling()
{
	return *GetOptions().Graphics.scaleQuality != ScalingQuality::NearestPixel;
}

bool SetHardwareCursorFromSurface(SDL_Surface *surface, HotpointPosition hotpointPosition)
{
	SDLCursorUniquePtr newCursor;
	const Size size { surface->w, surface->h };
	const Size scaledSize = ScaledSize(size);

	if (size == scaledSize) {
#if LOG_HWCURSOR
		Log("hwcursor: SetHardwareCursorFromSurface {}x{}", size.width, size.height);
#endif
		const Point hotpoint = GetHotpointPosition(*surface, hotpointPosition);
		newCursor = SDLCursorUniquePtr { SDL_CreateColorCursor(surface, hotpoint.x, hotpoint.y) };
	} else {
		// SDL does not support BlitScaled from 8-bit to RGBA.
		const SDLSurfaceUniquePtr converted {
#ifdef USE_SDL3
			SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888)
#else
			SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0)
#endif
		};

		const SDLSurfaceUniquePtr scaledSurface = SDLWrap::CreateRGBSurfaceWithFormat(0, scaledSize.width, scaledSize.height, 32, SDL_PIXELFORMAT_ARGB8888);
		if (ShouldUseBilinearScaling()) {
#if LOG_HWCURSOR
			Log("hwcursor: SetHardwareCursorFromSurface {}x{} scaled to {}x{} using bilinear scaling",
			    size.width, size.height, scaledSize.width, scaledSize.height);
#endif
			BilinearScale32(converted.get(), scaledSurface.get());
		} else {
#if LOG_HWCURSOR
			Log("hwcursor: SetHardwareCursorFromSurface {}x{} scaled to {}x{} using nearest neighbour scaling",
			    size.width, size.height, scaledSize.width, scaledSize.height);
#endif
#ifdef USE_SDL3
			SDL_BlitSurfaceScaled(converted.get(), nullptr, scaledSurface.get(), nullptr, SDL_SCALEMODE_PIXELART);
#else
			SDL_BlitScaled(converted.get(), nullptr, scaledSurface.get(), nullptr);
#endif
		}
		const Point hotpoint = GetHotpointPosition(*scaledSurface, hotpointPosition);
		newCursor = SDLCursorUniquePtr { SDL_CreateColorCursor(scaledSurface.get(), hotpoint.x, hotpoint.y) };
	}
	if (newCursor == nullptr) {
		LogError("SDL_CreateColorCursor: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
#ifdef USE_SDL3
	if (!SDL_SetCursor(newCursor.get())) {
		LogError("SDL_SetCursor: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
#else
	SDL_SetCursor(newCursor.get());
#endif
	CurrentCursor = std::move(newCursor);
	return true;
}

bool SetHardwareCursorFromClxSprite(ClxSprite sprite, HotpointPosition hotpointPosition)
{
	const OwnedSurface surface { sprite.width(), sprite.height() };
	// Use system_palette directly (not Palette which may have faded colors during transitions).
	SDLPaletteUniquePtr cursorPalette = SDLWrap::AllocPalette();
	SDL_SetPaletteColors(cursorPalette.get(), system_palette.data(), 0, 256);
	if (!SDLC_SetSurfacePalette(surface.surface, cursorPalette.get())) {
		LogError("SDL_SetSurfacePalette: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
	if (!SDL_SetSurfaceColorKey(surface.surface, true, 0)) {
		LogError("SDL_SetSurfaceColorKey: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
	RenderClxSprite(surface, sprite, { 0, 0 });
	return SetHardwareCursorFromSurface(surface.surface, hotpointPosition);
}

bool SetHardwareCursorFromSprite(int pcurs)
{
	const bool isItem = pcurs >= CURSOR_FIRSTITEM && MyPlayer != nullptr && !MyPlayer->HoldItem.isEmpty();
	if (isItem && !*GetOptions().Graphics.hardwareCursorForItems)
		return false;

	auto size = GetInvItemSize(pcurs);
	const int outlineWidth = isItem ? 1 : 0;
	size.width += 2 * outlineWidth;
	size.height += 2 * outlineWidth;

	if (!IsCursorSizeAllowed(size))
		return false;

	Point hotpoint;
	auto surface = CreateSoftwareCursorSurface(pcurs, hotpoint);
	if (!surface)
		return false;

	return SetHardwareCursorFromSurface(
	    surface->surface, isItem ? HotpointPosition::Center : HotpointPosition::TopLeft);
}
#endif

} // namespace

CursorInfo &GetCurrentCursorInfo()
{
	return CurrentCursorInfo;
}

void SetHardwareCursor(CursorInfo cursorInfo)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	CurrentCursorInfo = cursorInfo;
	CurrentCursorInfo.setNeedsReinitialization(false);
	switch (cursorInfo.type()) {
	case CursorType::Game:
#if LOG_HWCURSOR
		Log("hwcursor: SetHardwareCursor Game");
#endif
		CurrentCursorInfo.SetEnabled(SetHardwareCursorFromSprite(cursorInfo.id()));
		break;
	case CursorType::UserInterface:
#if LOG_HWCURSOR
		Log("hwcursor: SetHardwareCursor UserInterface");
#endif
		// ArtCursor is null while loading the game on the progress screen,
		// called via palette fade from ShowProgress.
		CurrentCursorInfo.SetEnabled(
		    ArtCursor && IsCursorSizeAllowed(Size { (*ArtCursor)[0].width(), (*ArtCursor)[0].height() })
		    && SetHardwareCursorFromClxSprite((*ArtCursor)[0], HotpointPosition::TopLeft));
		break;
	case CursorType::Unknown:
#if LOG_HWCURSOR
		Log("hwcursor: SetHardwareCursor Unknown");
#endif
		CurrentCursorInfo.SetEnabled(false);
		break;
	}
	if (!CurrentCursorInfo.Enabled())
		SetHardwareCursorVisible(false);
#endif
}

} // namespace devilution
