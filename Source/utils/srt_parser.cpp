#include "utils/srt_parser.hpp"

#include <algorithm>
#include <cstdio>
#include <istream>
#include <sstream>
#include <string>

#include "engine/assets.hpp"

namespace devilution {

uint64_t ParseSrtTimestamp(std::string_view timestamp)
{
	unsigned long long h = 0, m = 0, s = 0, ms = 0;
	if (sscanf(timestamp.data(), "%llu:%llu:%llu,%llu", &h, &m, &s, &ms) == 4
	    || sscanf(timestamp.data(), "%llu:%llu:%llu.%llu", &h, &m, &s, &ms) == 4) {
		return static_cast<uint64_t>(h * 3600000ULL + m * 60000ULL + s * 1000ULL + ms);
	}
	return 0;
}

std::vector<SubtitleEntry> LoadSrtFile(std::string_view subtitlePath)
{
	std::vector<SubtitleEntry> subtitles;

	std::string pathStr(subtitlePath);
	auto assetData = LoadAsset(pathStr);
	if (!assetData.has_value())
		return subtitles;

	std::string content(assetData->data.get(), assetData->size);
	std::istringstream stream(content);
	std::string line, text;
	uint64_t startTime = 0, endTime = 0;

	while (std::getline(stream, line)) {
		// Remove \r if present
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		// Skip empty lines (end of subtitle block)
		if (line.empty()) {
			if (!text.empty() && startTime < endTime) {
				// Remove trailing newline from text
				if (!text.empty() && text.back() == '\n')
					text.pop_back();
				subtitles.push_back({ startTime, endTime, text });
				text.clear();
			}
			continue;
		}

		// Check if line is a number (subtitle index) - skip it
		if (std::all_of(line.begin(), line.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
			continue;

		// Check if line contains --> (timestamp line)
		const size_t arrowPos = line.find("-->");
		if (arrowPos != std::string::npos) {
			const std::string startStr = line.substr(0, arrowPos);
			const std::string endStr = line.substr(arrowPos + 3);
			startTime = ParseSrtTimestamp(startStr);
			endTime = ParseSrtTimestamp(endStr);
			continue;
		}

		// Otherwise it's subtitle text
		if (!text.empty())
			text += "\n";
		text += line;
	}

	// Handle last subtitle if file doesn't end with blank line
	if (!text.empty() && startTime < endTime) {
		if (!text.empty() && text.back() == '\n')
			text.pop_back();
		subtitles.push_back({ startTime, endTime, text });
	}

	return subtitles;
}

std::string GetSubtitleAtTime(const std::vector<SubtitleEntry> &subtitles, uint64_t videoTimeMs)
{
	for (const auto &entry : subtitles) {
		if (videoTimeMs >= entry.startTimeMs && videoTimeMs < entry.endTimeMs) {
			return entry.text;
		}
	}
	return "";
}

} // namespace devilution

