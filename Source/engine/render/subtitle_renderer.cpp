#include "engine/render/subtitle_renderer.hpp"

#include <algorithm>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "DiabloUI/ui_flags.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"
#include "utils/srt_parser.hpp"

namespace devilution {

void SubtitleRenderer::LoadSubtitles(const char *videoFilename)
{
	Clear();

	// Generate subtitle filename by replacing .smk with .srt
	std::string subtitlePath(videoFilename);
	std::replace(subtitlePath.begin(), subtitlePath.end(), '\\', '/');
	const size_t extPos = subtitlePath.rfind('.');
	subtitlePath = (extPos != std::string::npos ? subtitlePath.substr(0, extPos) : subtitlePath) + ".srt";

	Log("Loading subtitles from: {}", subtitlePath);
	subtitles_ = LoadSrtFile(subtitlePath);
	Log("Loaded {} subtitle entries", subtitles_.size());
	if (!subtitles_.empty()) {
		Log("First subtitle: {}ms-{}ms: \"{}\"",
		    subtitles_[0].startTimeMs,
		    subtitles_[0].endTimeMs,
		    subtitles_[0].text);
	}
}

void SubtitleRenderer::RenderSubtitles(SDL_Surface *videoSurface, uint32_t videoWidth, uint32_t videoHeight, uint64_t currentTimeMs)
{
	if (subtitles_.empty() || videoSurface == nullptr)
		return;

	const std::string subtitleText = GetSubtitleAtTime(subtitles_, currentTimeMs);
	if (subtitleText.empty())
		return;

	LogVerbose(LogCategory::Video, "Rendering subtitle at {}ms: \"{}\"", currentTimeMs, subtitleText);

	if (SDLC_SURFACE_BITSPERPIXEL(videoSurface) != 8)
		return;

	const int videoWidthInt = static_cast<int>(videoWidth);
	const int videoHeightInt = static_cast<int>(videoHeight);

	// Create subtitle overlay surface if not already created
	if (subtitleSurface_ == nullptr) {
		constexpr int SubtitleMaxHeight = 100;
		subtitleSurface_ = SDLWrap::CreateRGBSurface(
		    0, videoWidthInt, SubtitleMaxHeight, 8, 0, 0, 0, 0);

		// Create and set up palette for subtitle surface - copy from video surface
		subtitlePalette_ = SDLWrap::AllocPalette();
#ifdef USE_SDL3
		const SDL_Palette *videoPalette = SDL_GetSurfacePalette(videoSurface);
#else
		const SDL_Palette *videoPalette = videoSurface->format->palette;
#endif
		if (videoPalette != nullptr) {
			// Copy the video surface's palette so text colors map correctly
			SDL_Color *colors = subtitlePalette_->colors;
			constexpr int MaxColors = 256;
			for (int i = 0; i < MaxColors && i < videoPalette->ncolors; i++) {
				colors[i] = videoPalette->colors[i];
			}
			// Ensure index 0 is black/transparent for color key
			colors[0].r = 0;
			colors[0].g = 0;
			colors[0].b = 0;
		} else {
			// Fallback: initialize palette manually
			SDL_Color *colors = subtitlePalette_->colors;
			colors[0].r = 0;
			colors[0].g = 0;
			colors[0].b = 0;
			constexpr int MaxColors = 256;
			for (int i = 1; i < MaxColors; i++) {
				colors[i].r = 255;
				colors[i].g = 255;
				colors[i].b = 255;
			}
		}
#ifndef USE_SDL1
		constexpr int MaxColors = 256;
		for (int i = 0; i < MaxColors; i++) {
			subtitlePalette_->colors[i].a = SDL_ALPHA_OPAQUE;
		}
#endif

		if (!SDLC_SetSurfacePalette(subtitleSurface_.get(), subtitlePalette_.get())) {
			Log("Failed to set subtitle overlay palette");
		}

		// Set color key for transparency (index 0 = transparent)
#ifdef USE_SDL1
		SDL_SetColorKey(subtitleSurface_.get(), SDL_SRCCOLORKEY, 0);
#else
		if (!SDL_SetSurfaceColorKey(subtitleSurface_.get(), true, 0)) {
			Log("Failed to set color key: {}", SDL_GetError());
		}
#endif
	}

	// Clear the overlay surface (fill with transparent color)
	SDL_FillSurfaceRect(subtitleSurface_.get(), nullptr, 0);

	// Render text to the overlay surface
	Surface overlaySurface(subtitleSurface_.get());
	constexpr int SubtitleMaxHeight = 100;
	constexpr int SubtitleBottomPadding = 12;
	// Position text rectangle at bottom of overlay (FontSize12 has line height ~12)
	// Position y so text appears near bottom of overlay
	constexpr int TextLineHeight = 12;
	const int textY = SubtitleMaxHeight - TextLineHeight - SubtitleBottomPadding;
	constexpr int TextHorizontalPadding = 10;
	Rectangle subtitleRect { { TextHorizontalPadding, textY }, { videoWidthInt - TextHorizontalPadding * 2, TextLineHeight + SubtitleBottomPadding } };

	TextRenderOptions opts;
	opts.flags = UiFlags::AlignCenter | UiFlags::ColorWhite | UiFlags::FontSize12;
	opts.spacing = 1;
	DrawString(overlaySurface, subtitleText, subtitleRect, opts);

	// Blit the overlay onto the video surface at the bottom
	SDL_Rect dstRect;
	dstRect.x = 0;
	// Position overlay at the very bottom of the video, with small padding
	dstRect.y = videoHeightInt - SubtitleMaxHeight - SubtitleBottomPadding;
	dstRect.w = videoWidthInt;
	dstRect.h = SubtitleMaxHeight;

#ifdef USE_SDL3
	if (!SDL_BlitSurface(subtitleSurface_.get(), nullptr, videoSurface, &dstRect)) {
		Log("Failed to blit subtitle overlay: {}", SDL_GetError());
	}
#else
	if (SDL_BlitSurface(subtitleSurface_.get(), nullptr, videoSurface, &dstRect) < 0) {
		Log("Failed to blit subtitle overlay: {}", SDL_GetError());
	}
#endif
}

void SubtitleRenderer::Clear()
{
	subtitles_.clear();
	subtitleSurface_ = nullptr;
	subtitlePalette_ = nullptr;
}

} // namespace devilution
