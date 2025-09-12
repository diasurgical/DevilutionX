// bannedwords.cpp

#include "bannedwords.hpp"

#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"

#include <algorithm>
#include <cctype>

namespace devilution {

std::vector<std::string> BannedWords;

void LoadBannedWords()
{
	BannedWords.clear();
	const std::string path = paths::ConfigPath() + "bannedwords.txt";

	// Create file if it doesn't exist
	if (!FileExists(path.c_str())) {
		if (FILE *file = OpenFile(path.c_str(), "wb")) {
			const char *comment = "# Enter each word to be censored on its own line\n";
			std::fwrite(comment, std::strlen(comment), 1, file);
			std::fclose(file);
		} else {
			LogError("Failed to create bannedwords.txt at {}", path);
		}
		return;
	}

	// Read file contents
	std::vector<char> buffer;
	uintmax_t size = 0;
	if (GetFileSize(path.c_str(), &size) && size > 0) {
		buffer.resize(size);
		if (FILE *file = OpenFile(path.c_str(), "rb")) {
			if (std::fread(buffer.data(), size, 1, file) == 1) {
				std::string_view content(buffer.data(), size);
				std::string line;
				for (size_t i = 0, start = 0; i <= content.size(); ++i) {
					if (i == content.size() || content[i] == '\n' || content[i] == '\r') {
						size_t end = i;
						if (start < end) {
							line.assign(content.substr(start, end - start));
							if (!line.empty() && line[0] != '#') {
								std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) { return std::tolower(c); });
								BannedWords.push_back(line);
							}
						}
						if (i + 1 < content.size() && content[i] == '\r' && content[i + 1] == '\n')
							i++;
						start = i + 1;
					}
				}
			} else {
				LogError("Failed to read bannedwords.txt at {}", path);
			}
			std::fclose(file);
		}
	}
}

std::string CensorMessage(const std::string &original)
{
	std::string result = original;
	std::string lowered = original;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return std::tolower(c); });

	for (const std::string &word : BannedWords) {
		size_t pos = 0;
		while ((pos = lowered.find(word, pos)) != std::string::npos) {
			result.replace(pos, word.size(), word.size(), '*');
			lowered.replace(pos, word.size(), word.size(), '*'); // keep lowered in sync
			pos += word.size();
		}
	}

	return result;
}

} // namespace devilution
