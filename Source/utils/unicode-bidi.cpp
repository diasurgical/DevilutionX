#include "utils/utf8.hpp"
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <SheenBidi/SheenBidi.h>
}

namespace devilution {

constexpr char32_t ZWSP = U'\u200B'; // Zero-width space

// Convert logical text order to visual text order using SheenBidi
std::string ConvertLogicalToVisual(std::string_view input)
{
	if (input.empty())
		return {};

	// Create a copy of the input (needed for SheenBidi)
	std::string inputCopy(input);

	// Filter out ZWSP characters as they shouldn't affect text direction
	std::string filteredInput;
	std::string_view remaining = inputCopy;
	size_t cpLen;

	while (!remaining.empty()) {
		char32_t c = DecodeFirstUtf8CodePoint(remaining, &cpLen);
		if (c == Utf8DecodeError) break;

		if (c != ZWSP) {
			filteredInput.append(remaining.data(), cpLen);
		}
		remaining.remove_prefix(cpLen);
	}

	if (filteredInput.empty())
		return {};

	// Set up SheenBidi codepoint sequence
	SBCodepointSequence codepointSequence = {
		SBStringEncodingUTF8,
		(void *)filteredInput.data(),
		filteredInput.length()
	};

	// Create algorithm and paragraph objects
	SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&codepointSequence);
	if (!bidiAlgorithm)
		return inputCopy;

	SBParagraphRef firstParagraph = SBAlgorithmCreateParagraph(
	    bidiAlgorithm,
	    0,
	    filteredInput.length(), // Use the actual length
	    SBLevelDefaultRTL);

	if (!firstParagraph) {
		SBAlgorithmRelease(bidiAlgorithm);
		return inputCopy;
	}

	SBUInteger paragraphLength = SBParagraphGetLength(firstParagraph);

	// Get the visual run information
	SBLineRef paragraphLine = SBParagraphCreateLine(firstParagraph, 0, paragraphLength);
	if (!paragraphLine) {
		SBParagraphRelease(firstParagraph);
		SBAlgorithmRelease(bidiAlgorithm);
		return inputCopy;
	}

	// Get the visual runs
	SBUInteger runCount = SBLineGetRunCount(paragraphLine);
	const SBRun *runArray = SBLineGetRunsPtr(paragraphLine);

	std::string result;
	result.reserve(filteredInput.length());

	// Process each visual run
	for (SBUInteger i = 0; i < runCount; i++) {
		const SBRun *run = &runArray[i];

		// For RTL runs, we reverse the character order within the run
		if ((run->level & 1) == 1) { // Odd levels are RTL
			std::string runText(
			    filteredInput.data() + run->offset,
			    run->length);

			// We need to reverse by characters, not bytes
			std::vector<std::string> chars;
			std::string_view runView = runText;

			while (!runView.empty()) {
				char32_t c = DecodeFirstUtf8CodePoint(runView, &cpLen);
				if (c == Utf8DecodeError) break;

				chars.push_back(std::string(runView.data(), cpLen));
				runView.remove_prefix(cpLen);
			}

			std::reverse(chars.begin(), chars.end());

			for (const auto &ch : chars) {
				result += ch;
			}
		} else {
			// LTR runs can be appended as-is
			result.append(
			    filteredInput.data() + run->offset,
			    run->length);
		}
	}

	// Clean up
	SBLineRelease(paragraphLine);
	SBParagraphRelease(firstParagraph);
	SBAlgorithmRelease(bidiAlgorithm);

	return result;
}

// Convert a logical character index to its visual index
int ConvertLogicalToVisualPosition(std::string_view text, int logicalPos)
{
	if (text.empty() || logicalPos < 0 || logicalPos > static_cast<int>(text.size()))
		return logicalPos; // Invalid input, return unchanged

	// Create a copy of the input (needed for SheenBidi)
	std::string inputCopy(text);

	// Filter out ZWSP characters and track character position mappings
	std::string filteredInput;
	std::vector<int> originalToFiltered; // Maps from original text indices to filtered text indices
	std::vector<int> filteredToOriginal; // Maps from filtered text indices to original text indices

	originalToFiltered.resize(text.size() + 1, -1); // +1 for position after the last character

	std::string_view remaining = inputCopy;
	size_t cpLen;
	int originalIndex = 0;

	while (!remaining.empty()) {
		char32_t c = DecodeFirstUtf8CodePoint(remaining, &cpLen);
		if (c == Utf8DecodeError) break;

		if (c != ZWSP) {
			originalToFiltered[originalIndex] = filteredInput.size();
			filteredToOriginal.push_back(originalIndex);
			filteredInput.append(remaining.data(), cpLen);
		}

		originalIndex += cpLen;
		remaining.remove_prefix(cpLen);
	}

	// Map the position after the last character
	originalToFiltered[text.size()] = filteredInput.size();
	filteredToOriginal.push_back(originalIndex);

	if (filteredInput.empty())
		return logicalPos;

	// Get the filtered logical position
	int filteredLogicalPos = originalToFiltered[logicalPos];
	if (filteredLogicalPos == -1) {
		// Find the next valid position if this one was a ZWSP
		for (int i = logicalPos + 1; i <= static_cast<int>(text.size()); i++) {
			if (originalToFiltered[i] != -1) {
				filteredLogicalPos = originalToFiltered[i];
				break;
			}
		}

		// If still not found, try previous positions
		if (filteredLogicalPos == -1) {
			for (int i = logicalPos - 1; i >= 0; i--) {
				if (originalToFiltered[i] != -1) {
					filteredLogicalPos = originalToFiltered[i];
					break;
				}
			}
		}

		if (filteredLogicalPos == -1)
			return logicalPos; // Fallback
	}

	// Set up SheenBidi
	SBCodepointSequence codepointSequence = {
		SBStringEncodingUTF8,
		(void *)filteredInput.data(),
		filteredInput.length()
	};

	SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&codepointSequence);
	if (!bidiAlgorithm)
		return logicalPos;

	SBParagraphRef paragraph = SBAlgorithmCreateParagraph(
	    bidiAlgorithm,
	    0,
	    filteredInput.length(),
	    SBLevelDefaultRTL);

	if (!paragraph) {
		SBAlgorithmRelease(bidiAlgorithm);
		return logicalPos;
	}

	// We need to create a mapping from logical to visual positions
	SBLineRef line = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
	if (!line) {
		SBParagraphRelease(paragraph);
		SBAlgorithmRelease(bidiAlgorithm);
		return logicalPos;
	}

	// Build a logical-to-visual mapping using the runs
	std::vector<int> logicalToVisual(filteredInput.length() + 1, -1);

	SBUInteger runCount = SBLineGetRunCount(line);
	const SBRun *runs = SBLineGetRunsPtr(line);

	// Process each run and build the mapping
	int visualPos = 0;
	for (SBUInteger i = 0; i < runCount; i++) {
		const SBRun *run = &runs[i];

		if ((run->level & 1) == 1) { // RTL run
			// For RTL runs, characters are visually reversed
			for (int j = run->offset + run->length - 1; j >= static_cast<int>(run->offset); j--) {
				logicalToVisual[j] = visualPos++;
			}
		} else { // LTR run
			// For LTR runs, characters are in the same order
			for (SBUInteger j = run->offset; j < run->offset + run->length; j++) {
				logicalToVisual[j] = visualPos++;
			}
		}
	}

	// Handle position after the last character
	logicalToVisual[filteredInput.length()] = filteredInput.length();

	// Get the visual position for our logical position
	int filteredVisualPos = logicalToVisual[filteredLogicalPos];

	// Convert back to original text position
	visualPos = -1;
	if (filteredVisualPos >= 0 && filteredVisualPos < static_cast<int>(filteredToOriginal.size())) {
		visualPos = filteredToOriginal[filteredVisualPos];
	} else {
		visualPos = text.size(); // Default to end of text
	}

	// Clean up
	SBLineRelease(line);
	SBParagraphRelease(paragraph);
	SBAlgorithmRelease(bidiAlgorithm);

	return visualPos;
}

// Convert a visual character index to its logical index
int ConvertVisualToLogicalPosition(std::string_view text, int visualPos)
{
	if (text.empty() || visualPos < 0 || visualPos > static_cast<int>(text.size()))
		return visualPos; // Invalid input, return unchanged

	// Similar to the above, but build a visual-to-logical mapping
	std::string inputCopy(text);

	// Filter out ZWSP characters and track character position mappings
	std::string filteredInput;
	std::vector<int> originalToFiltered;
	std::vector<int> filteredToOriginal;

	originalToFiltered.resize(text.size() + 1, -1);

	std::string_view remaining = inputCopy;
	size_t cpLen;
	int originalIndex = 0;

	while (!remaining.empty()) {
		char32_t c = DecodeFirstUtf8CodePoint(remaining, &cpLen);
		if (c == Utf8DecodeError) break;

		if (c != ZWSP) {
			originalToFiltered[originalIndex] = filteredInput.size();
			filteredToOriginal.push_back(originalIndex);
			filteredInput.append(remaining.data(), cpLen);
		}

		originalIndex += cpLen;
		remaining.remove_prefix(cpLen);
	}

	originalToFiltered[text.size()] = filteredInput.size();
	filteredToOriginal.push_back(originalIndex);

	if (filteredInput.empty())
		return visualPos;

	// Find the filtered visual position from the original visual position
	int filteredVisualPos = -1;
	for (int i = 0; i < static_cast<int>(filteredToOriginal.size()); i++) {
		if (filteredToOriginal[i] >= visualPos) {
			filteredVisualPos = i;
			break;
		}
	}

	if (filteredVisualPos == -1)
		filteredVisualPos = filteredInput.length();

	// Set up SheenBidi
	SBCodepointSequence codepointSequence = {
		SBStringEncodingUTF8,
		(void *)filteredInput.data(),
		filteredInput.length()
	};

	SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&codepointSequence);
	if (!bidiAlgorithm)
		return visualPos;

	SBParagraphRef paragraph = SBAlgorithmCreateParagraph(
	    bidiAlgorithm,
	    0,
	    filteredInput.length(),
	    SBLevelDefaultRTL);

	if (!paragraph) {
		SBAlgorithmRelease(bidiAlgorithm);
		return visualPos;
	}

	// Create a line and get the runs
	SBLineRef line = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
	if (!line) {
		SBParagraphRelease(paragraph);
		SBAlgorithmRelease(bidiAlgorithm);
		return visualPos;
	}

	// Build a visual-to-logical mapping using the runs
	std::vector<int> visualToLogical(filteredInput.length() + 1, -1);

	SBUInteger runCount = SBLineGetRunCount(line);
	const SBRun *runs = SBLineGetRunsPtr(line);

	// Process each run and build the mapping
	int currentVisualPos = 0;
	for (SBUInteger i = 0; i < runCount; i++) {
		const SBRun *run = &runs[i];

		if ((run->level & 1) == 1) { // RTL run
			// For RTL runs, characters are visually reversed
			for (int j = run->offset + run->length - 1; j >= static_cast<int>(run->offset); j--) {
				visualToLogical[currentVisualPos++] = j;
			}
		} else { // LTR run
			// For LTR runs, characters are in the same order
			for (SBUInteger j = run->offset; j < run->offset + run->length; j++) {
				visualToLogical[currentVisualPos++] = j;
			}
		}
	}

	// Handle position after the last character
	visualToLogical[filteredInput.length()] = filteredInput.length();

	// Get the logical position for our visual position
	int filteredLogicalPos = visualToLogical[filteredVisualPos];

	// Convert back to original text position
	int logicalPos = -1;
	for (int i = 0; i < static_cast<int>(text.size() + 1); i++) {
		if (originalToFiltered[i] == filteredLogicalPos) {
			logicalPos = i;
			break;
		}
	}

	if (logicalPos == -1) {
		// Find the closest position
		int minDistance = filteredInput.length();

		for (int i = 0; i < static_cast<int>(text.size() + 1); i++) {
			if (originalToFiltered[i] != -1) {
				int distance = std::abs(originalToFiltered[i] - filteredLogicalPos);
				if (distance < minDistance) {
					minDistance = distance;
					logicalPos = i;
				}
			}
		}
	}

	// Clean up
	SBLineRelease(line);
	SBParagraphRelease(paragraph);
	SBAlgorithmRelease(bidiAlgorithm);

	return logicalPos;
}

} // namespace devilution
