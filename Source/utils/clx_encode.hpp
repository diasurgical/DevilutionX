#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace devilution {

/**
 * CLX frame header is 6 bytes:
 *
 *  Bytes |   Type   | Value
 * :-----:|:--------:|-------------
 *  0..2  | uint16_t | header size
 *  2..4  | uint16_t | width
 *  4..6  | uint16_t | height
 */
constexpr size_t ClxFrameHeaderSize = 6;

inline void AppendClxTransparentRun(unsigned width, std::vector<uint8_t> &out)
{
	while (width >= 0x7F) {
		out.push_back(0x7F);
		width -= 0x7F;
	}
	if (width == 0)
		return;
	out.push_back(width);
}

inline void AppendClxFillRun(uint8_t color, unsigned width, std::vector<uint8_t> &out)
{
	while (width >= 0x3F) {
		out.push_back(0x80);
		out.push_back(color);
		width -= 0x3F;
	}
	if (width == 0)
		return;
	out.push_back(0xBF - width);
	out.push_back(color);
}

inline void AppendClxPixelsRun(const uint8_t *src, unsigned width, std::vector<uint8_t> &out)
{
	while (width >= 0x41) {
		out.push_back(0xBF);
		for (size_t i = 0; i < 0x41; ++i)
			out.push_back(src[i]);
		width -= 0x41;
		src += 0x41;
	}
	if (width == 0)
		return;
	out.push_back(256 - width);
	for (size_t i = 0; i < width; ++i)
		out.push_back(src[i]);
}

inline void AppendClxPixelsOrFillRun(const uint8_t *src, size_t length, std::vector<uint8_t> &out)
{
	const uint8_t *begin = src;
	const uint8_t *prevColorBegin = src;
	unsigned prevColorRunLength = 1;
	uint8_t prevColor = *src++;
	while (--length > 0) {
		const uint8_t color = *src;
		if (prevColor == color) {
			++prevColorRunLength;
		} else {
			// A tunable parameter that decides at which minimum length we encode a fill run.
			// 3 appears to be optimal for most of our data (much better than 2, rarely very slightly worse than 4).
			constexpr unsigned MinFillRunLength = 3;
			if (prevColorRunLength >= MinFillRunLength) {
				AppendClxPixelsRun(begin, static_cast<unsigned>(prevColorBegin - begin), out);
				AppendClxFillRun(prevColor, prevColorRunLength, out);
				begin = src;
			}
			prevColorBegin = src;
			prevColorRunLength = 1;
			prevColor = color;
		}
		++src;
	}

	// Here we use 2 instead of `MinFillRunLength` because we know that this run
	// is followed by transparent pixels.
	// Width=2 Fill command takes 2 bytes, while the Pixels command is 3 bytes.
	if (prevColorRunLength >= 2) {
		AppendClxPixelsRun(begin, static_cast<unsigned>(prevColorBegin - begin), out);
		AppendClxFillRun(prevColor, prevColorRunLength, out);
	} else {
		AppendClxPixelsRun(begin, static_cast<unsigned>(prevColorBegin - begin + prevColorRunLength), out);
	}
}

} // namespace devilution
