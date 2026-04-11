/**
 * @file subtitle_renderer.hpp
 *
 * Subtitle rendering for video playback.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "utils/sdl_wrap.h"
#include "utils/srt_parser.hpp"

namespace devilution {

/**
 * @brief Manages subtitle rendering state for video playback
 */
class SubtitleRenderer {
public:
	SubtitleRenderer() = default;
	~SubtitleRenderer() = default;

	SubtitleRenderer(const SubtitleRenderer &) = delete;
	SubtitleRenderer &operator=(const SubtitleRenderer &) = delete;
	SubtitleRenderer(SubtitleRenderer &&) = default;
	SubtitleRenderer &operator=(SubtitleRenderer &&) = default;

	/**
	 * @brief Load subtitles for the given video
	 * @param videoFilename Path to the video file (will look for matching .srt file)
	 */
	void LoadSubtitles(const char *videoFilename);

	/**
	 * @brief Render subtitles onto a video surface
	 * @param videoSurface The video surface to render subtitles onto
	 * @param videoWidth Width of the video
	 * @param videoHeight Height of the video
	 * @param currentTimeMs Current playback time in milliseconds
	 */
	void RenderSubtitles(SDL_Surface *videoSurface, uint32_t videoWidth, uint32_t videoHeight, uint64_t currentTimeMs);

	/**
	 * @brief Clear subtitle data and free resources
	 */
	void Clear();

	/**
	 * @brief Check if subtitles are loaded
	 * @return True if subtitles are available
	 */
	bool HasSubtitles() const { return !subtitles_.empty(); }

private:
	std::vector<SubtitleEntry> subtitles_;
	SDLSurfaceUniquePtr subtitleSurface_;
	SDLPaletteUniquePtr subtitlePalette_;
};

} // namespace devilution
