#include "utils/cl2_to_clx.hpp"

#include <cstdint>
#include <cstring>

#include <vector>

#include "utils/clx_decode.hpp"
#include "utils/clx_encode.hpp"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"

namespace devilution {

uint16_t Cl2ToClx(const uint8_t *data, size_t size,
    PointerOrValue<uint16_t> widthOrWidths, std::vector<uint8_t> &clxData)
{
	uint32_t numGroups = 1;
	const uint32_t maybeNumFrames = LoadLE32(data);
	const uint8_t *groupBegin = data;

	// If it is a number of frames, then the last frame offset will be equal to the size of the file.
	if (LoadLE32(&data[maybeNumFrames * 4 + 4]) != size) {
		// maybeNumFrames is the address of the first group, right after
		// the list of group offsets.
		numGroups = maybeNumFrames / 4;
		clxData.resize(maybeNumFrames);
	}

	// Transient buffer for a contiguous run of non-transparent pixels.
	std::vector<uint8_t> pixels;
	pixels.reserve(4096);

	for (size_t group = 0; group < numGroups; ++group) {
		uint32_t numFrames;
		if (numGroups == 1) {
			numFrames = maybeNumFrames;
		} else {
			groupBegin = &data[LoadLE32(&data[group * 4])];
			numFrames = LoadLE32(groupBegin);
			WriteLE32(&clxData[4 * group], static_cast<uint32_t>(clxData.size()));
		}

		// CLX header: frame count, frame offset for each frame, file size
		const size_t clxDataOffset = clxData.size();
		clxData.resize(clxData.size() + 4 * (2 + static_cast<size_t>(numFrames)));
		WriteLE32(&clxData[clxDataOffset], numFrames);

		const uint8_t *frameEnd = &groupBegin[LoadLE32(&groupBegin[4])];
		for (size_t frame = 1; frame <= numFrames; ++frame) {
			WriteLE32(&clxData[clxDataOffset + 4 * frame],
			    static_cast<uint32_t>(clxData.size() - clxDataOffset));

			const uint8_t *frameBegin = frameEnd;
			frameEnd = &groupBegin[LoadLE32(&groupBegin[4 * (frame + 1)])];

			const uint16_t frameWidth = widthOrWidths.HoldsPointer() ? widthOrWidths.AsPointer()[frame - 1] : widthOrWidths.AsValue();

			const size_t frameHeaderPos = clxData.size();
			clxData.resize(clxData.size() + ClxFrameHeaderSize);
			WriteLE16(&clxData[frameHeaderPos], ClxFrameHeaderSize);
			WriteLE16(&clxData[frameHeaderPos + 2], frameWidth);

			unsigned transparentRunWidth = 0;
			int_fast16_t xOffset = 0;
			size_t frameHeight = 0;
			const uint8_t *src = frameBegin + LoadLE16(frameBegin);
			while (src != frameEnd) {
				auto remainingWidth = static_cast<int_fast16_t>(frameWidth) - xOffset;
				while (remainingWidth > 0) {
					const uint8_t control = *src++;
					if (!IsClxOpaque(control)) {
						if (!pixels.empty()) {
							AppendClxPixelsOrFillRun(pixels.data(), pixels.size(), clxData);
							pixels.clear();
						}
						transparentRunWidth += control;
						remainingWidth -= control;
					} else if (IsClxOpaqueFill(control)) {
						AppendClxTransparentRun(transparentRunWidth, clxData);
						transparentRunWidth = 0;
						const uint8_t width = GetClxOpaqueFillWidth(control);
						const uint8_t color = *src++;
						pixels.insert(pixels.end(), width, color);
						remainingWidth -= width;
					} else {
						AppendClxTransparentRun(transparentRunWidth, clxData);
						transparentRunWidth = 0;
						const uint8_t width = GetClxOpaquePixelsWidth(control);
						pixels.insert(pixels.end(), src, src + width);
						src += width;
						remainingWidth -= width;
					}
				}

				const auto skipSize = GetSkipSize(remainingWidth, static_cast<int_fast16_t>(frameWidth));
				xOffset = skipSize.xOffset;
				frameHeight += skipSize.wholeLines;
			}
			if (!pixels.empty()) {
				AppendClxPixelsOrFillRun(pixels.data(), pixels.size(), clxData);
				pixels.clear();
			}
			AppendClxTransparentRun(transparentRunWidth, clxData);

			WriteLE16(&clxData[frameHeaderPos + 4], static_cast<uint16_t>(frameHeight));
		}

		WriteLE32(&clxData[clxDataOffset + 4 * (1 + static_cast<size_t>(numFrames))], static_cast<uint32_t>(clxData.size() - clxDataOffset));
	}
	return numGroups == 1 ? 0 : numGroups;
}

} // namespace devilution
