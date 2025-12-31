#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace devilution {

struct SubtitleEntry {
	uint64_t startTimeMs; // Start time in milliseconds
	uint64_t endTimeMs;   // End time in milliseconds
	std::string text;     // Subtitle text (may contain multiple lines)
};

/**
 * @brief Parse SRT timestamp (HH:MM:SS,mmm or HH:MM:SS.mmm) to milliseconds
 * @param timestamp Timestamp string in SRT format
 * @return Time in milliseconds, or 0 if parsing fails
 */
uint64_t ParseSrtTimestamp(std::string_view timestamp);

/**
 * @brief Load and parse SRT subtitle file
 * @param subtitlePath Path to the SRT file
 * @return Vector of subtitle entries, empty if file not found or parsing fails
 */
std::vector<SubtitleEntry> LoadSrtFile(std::string_view subtitlePath);

/**
 * @brief Get subtitle text for a given time from a list of subtitle entries
 * @param subtitles Vector of subtitle entries
 * @param videoTimeMs Current video time in milliseconds
 * @return Subtitle text if a subtitle is active at this time, empty string otherwise
 */
std::string GetSubtitleAtTime(const std::vector<SubtitleEntry> &subtitles, uint64_t videoTimeMs);

} // namespace devilution

