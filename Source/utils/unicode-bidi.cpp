#include "utils/unicode-bidi.hpp"
#include "utils/utf8.hpp"
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <SheenBidi/SheenBidi.h>
}

namespace devilution {

constexpr char32_t ZWSP = U'\u200B'; // Zero-width space

BidiText::BidiText(std::string_view text)
    : text_(text)
{
	std::string input(text);
	AppendUtf8(ZWSP, input);

	SBCodepointSequence seq = { SBStringEncodingUTF8, (void *)input.data(), input.length() };
	SBAlgorithmRef algo = SBAlgorithmCreate(&seq);
	if (!algo) {
		visual_ = input;
		return;
	}

	SBParagraphRef para = SBAlgorithmCreateParagraph(algo, 0, input.length(), SBLevelDefaultRTL);
	if (!para) {
		SBAlgorithmRelease(algo);
		visual_ = input;
		return;
	}

	SBLineRef line = SBParagraphCreateLine(para, 0, SBParagraphGetLength(para));
	if (!line) {
		SBParagraphRelease(para);
		SBAlgorithmRelease(algo);
		visual_ = input;
		return;
	}

	const SBUInteger runCount = SBLineGetRunCount(line);
	const SBRun *runs = SBLineGetRunsPtr(line);

	logicalToVisualMap_.assign(input.size(), -1);
	visualToLogicalMap_.assign(input.size(), -1);
	visual_.reserve(input.size());
	int visualPos = 0;

	for (SBUInteger i = 0; i < runCount; i++) {
		const SBRun *run = &runs[i];
		const int runOff = run->offset;
		const int runLen = run->length;
		const bool isRTLRun = (run->level & 1) == 1;

		const auto mapChar = [&](int charStart, int charLen) {
			for (int b = 0; b < charLen; b++) {
				logicalToVisualMap_[charStart + b] = visualPos + b;
				visualToLogicalMap_[visualPos + b] = charStart + b;
			}
			visual_.append(input.data() + charStart, static_cast<size_t>(charLen));
			for (int b = 0; b < charLen; b++)
				isRTLVisualMap_.push_back(isRTLRun);
			visualPos += charLen;
		};

		if (isRTLRun) {
			int pos = runOff + runLen;
			while (pos > runOff) {
				int charEnd = pos;
				int charStart = pos - 1;
				while (charStart > runOff && IsTrailUtf8CodeUnit(input[charStart]))
					charStart--;
				mapChar(charStart, charEnd - charStart);
				pos = charStart;
			}
		} else {
			int pos = runOff;
			while (pos < runOff + runLen) {
				int charLen = static_cast<int>(Utf8CodePointLen(input.data() + pos));
				mapChar(pos, charLen);
				pos += charLen;
			}
		}
	}

	SBLineRelease(line);
	SBParagraphRelease(para);
	SBAlgorithmRelease(algo);
}

size_t BidiText::logicalToVisual(size_t logicalPos) const
{
	if (logicalPos >= logicalToVisualMap_.size())
		return logicalPos;
	return static_cast<size_t>(logicalToVisualMap_[logicalPos]);
}

size_t BidiText::visualToLogical(size_t visualPos) const
{
	if (visualPos >= visualToLogicalMap_.size())
		return visualPos;
	return static_cast<size_t>(visualToLogicalMap_[visualPos]);
}

bool BidiText::isVisualPositionRTL(size_t visualPos) const
{
	if (visualPos >= isRTLVisualMap_.size())
		return false;
	return isRTLVisualMap_[visualPos];
}

} // namespace devilution
