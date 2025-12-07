#include "hwcursor.hpp"

#include <cstdint>
#include <tuple>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL_version.h>

#ifndef USE_SDL1
#include <SDL_mouse.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#endif
#endif

#include "DiabloUI/diabloui.h"

#include "appfat.h"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/display.h"
#include "utils/sdl_bilinear_scale.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"

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
	if (renderer != nullptr) {
#ifdef USE_SDL3
		SDL_FRect logicalDstRect;
		int logicalWidth;
		int logicalHeight;
		if (!SDL_GetRenderLogicalPresentation(renderer, &logicalWidth, &logicalHeight, /*mode=*/nullptr)) {
			LogError("SDL_GetRenderOutputSize: {}", SDL_GetError());
			SDL_ClearError();
			return size;
		}
		if (!SDL_GetRenderLogicalPresentationRect(renderer, &logicalDstRect)) {
			LogError("SDL_GetRenderLogicalPresentationRect: {}", SDL_GetError());
			SDL_ClearError();
			return size;
		}
		const float dispScale = SDL_GetWindowDisplayScale(ghMainWnd);
		if (dispScale == 0.0F) {
			LogError("SDL_GetWindowDisplayScale: {}", SDL_GetError());
			SDL_ClearError();
			return size;
		}
		const float scaleX = logicalDstRect.w / static_cast<float>(logicalWidth);
		const float scaleY = logicalDstRect.h / static_cast<float>(logicalHeight);
		size.width = static_cast<int>(static_cast<float>(size.width) * scaleX / dispScale);
		size.height = static_cast<int>(static_cast<float>(size.height) * scaleY / dispScale);
#else
		float scaleX = 1.0F;
		float scaleY = 1.0F;
		if (!SDL_GetRenderScale(renderer, &scaleX, &scaleY)) {
			LogError("SDL_GetRenderScale: {}", SDL_GetError());
			SDL_ClearError();
		}
		size.width = static_cast<int>(size.width * scaleX);
		size.height = static_cast<int>(size.height * scaleY);
#endif
	}
	return size;
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
	if (!SDLC_SetSurfacePalette(surface.surface, Palette.get())) {
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
	const bool isItem = !MyPlayer->HoldItem.isEmpty();
	if (isItem && !*GetOptions().Graphics.hardwareCursorForItems)
		return false;

	const int outlineWidth = isItem ? 1 : 0;

	auto size = GetInvItemSize(pcurs);
	size.width += 2 * outlineWidth;
	size.height += 2 * outlineWidth;

	if (!IsCursorSizeAllowed(size))
		return false;

	const OwnedSurface out { size };
	SDL_SetSurfacePalette(out.surface, Palette.get());

	// Transparent color must not be used in the sprite itself.
	// Colors 1-127 are outside of the UI palette so are safe to use.
	constexpr std::uint8_t TransparentColor = 1;
	if (!SDL_FillSurfaceRect(out.surface, nullptr, TransparentColor)) {
		LogError("SDL_FillSurfaceRect: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
	if (!SDL_SetSurfaceColorKey(out.surface, true, TransparentColor)) {
		LogError("SDL_SetSurfaceColorKey: {}", SDL_GetError());
		SDL_ClearError();
		return false;
	}
	DrawSoftwareCursor(out, { outlineWidth, size.height - outlineWidth - 1 }, pcurs);

	const bool result = SetHardwareCursorFromSurface(
	    out.surface, isItem ? HotpointPosition::Center : HotpointPosition::TopLeft);
	return result;
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
