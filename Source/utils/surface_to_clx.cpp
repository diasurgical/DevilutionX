#include "utils/surface_to_clx.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#include "utils/clx_encode.hpp"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"

#ifdef DEBUG_SURFACE_TO_CLX_SIZE
#include <iomanip>
#include <iostream>
#endif

namespace devilution {

OwnedClxSpriteList SurfaceToClx(const Surface &surface, unsigned numFrames,
    std::optional<uint8_t> transparentColor)
{
	// CLX header: frame count, frame offset for each frame, file size
	std::vector<uint8_t> clxData(4 * (2 + static_cast<size_t>(numFrames)));
	WriteLE32(clxData.data(), numFrames);

	const auto height = static_cast<unsigned>(surface.h());
	const auto width = static_cast<unsigned>(surface.w());
	const auto pitch = static_cast<unsigned>(surface.pitch());
	const unsigned frameHeight = height / numFrames;

	// We process the surface a whole frame at a time because the lines are reversed in CEL.
	const uint8_t *dataPtr = surface.begin();
	for (unsigned frame = 1; frame <= numFrames; ++frame) {
		WriteLE32(&clxData[4 * static_cast<size_t>(frame)], static_cast<uint32_t>(clxData.size()));

		const size_t frameHeaderPos = clxData.size();
		clxData.resize(clxData.size() + ClxFrameHeaderSize);

		// Frame header:
		WriteLE16(&clxData[frameHeaderPos], ClxFrameHeaderSize);
		WriteLE16(&clxData[frameHeaderPos + 2], static_cast<uint16_t>(width));
		WriteLE16(&clxData[frameHeaderPos + 4], static_cast<uint16_t>(frameHeight));

		unsigned transparentRunWidth = 0;
		size_t line = 0;
		while (line != frameHeight) {
			// Process line:
			const uint8_t *src = &dataPtr[(frameHeight - (line + 1)) * pitch];
			if (transparentColor) {
				unsigned solidRunWidth = 0;
				for (const uint8_t *srcEnd = src + width; src != srcEnd; ++src) {
					if (*src == *transparentColor) {
						if (solidRunWidth != 0) {
							AppendClxPixelsOrFillRun(src - transparentRunWidth - solidRunWidth, solidRunWidth, clxData);
							solidRunWidth = 0;
						}
						++transparentRunWidth;
					} else {
						AppendClxTransparentRun(transparentRunWidth, clxData);
						transparentRunWidth = 0;
						++solidRunWidth;
					}
				}
				if (solidRunWidth != 0) {
					AppendClxPixelsOrFillRun(src - solidRunWidth, solidRunWidth, clxData);
				}
			} else {
				AppendClxPixelsOrFillRun(src, width, clxData);
			}
			++line;
		}
		AppendClxTransparentRun(transparentRunWidth, clxData);

		dataPtr += static_cast<unsigned>(pitch * frameHeight);
	}

	WriteLE32(&clxData[4 * (1 + static_cast<size_t>(numFrames))], static_cast<uint32_t>(clxData.size()));

	auto out = std::unique_ptr<uint8_t[]>(new uint8_t[clxData.size()]);
	memcpy(&out[0], clxData.data(), clxData.size());

#ifdef DEBUG_SURFACE_TO_CLX_SIZE
	const int surfaceSize = surface.h() * surface.pitch();
	std::cout << "Surface(" << surface.w() << ", " << surface.h() << ") -> CLX\t"
	          << surfaceSize << " -> " << clxData.size() << "\t"
	          << std::setprecision(1) << std::fixed << (static_cast<int>(clxData.size()) - surfaceSize) / ((float)surfaceSize) * 100 << "%" << std::endl;
#endif

	return OwnedClxSpriteList { std::move(out) };
}

} // namespace devilution
