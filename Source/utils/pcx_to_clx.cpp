#include "utils/pcx_to_clx.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <array>
#include <memory>
#include <vector>

#include <SDL_endian.h>

#include "appfat.h"
#include "utils/clx_encode.hpp"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"
#include "utils/pcx.hpp"
#include "utils/stdcompat/optional.hpp"

#ifdef DEBUG_PCX_TO_CL2_SIZE
#include <iomanip>
#include <iostream>
#endif

namespace devilution {

namespace {

size_t GetReservationSize(size_t pcxSize)
{
	// For the most part, CL2 is smaller than PCX, with a few exceptions.
	switch (pcxSize) {
	case 2352187: // ui_art\hf_logo1.pcx
		return 2464867;
	case 172347: // ui_art\creditsw.pcx
		return 172347;
	case 157275: // ui_art\credits.pcx
		return 173367;
	default:
		return pcxSize;
	}
}

bool LoadPcxMeta(AssetHandle &handle, int &width, int &height, uint8_t &bpp)
{
	PCXHeader pcxhdr;
	if (!handle.read(&pcxhdr, PcxHeaderSize)) {
		return false;
	}
	width = SDL_SwapLE16(pcxhdr.Xmax) - SDL_SwapLE16(pcxhdr.Xmin) + 1;
	height = SDL_SwapLE16(pcxhdr.Ymax) - SDL_SwapLE16(pcxhdr.Ymin) + 1;
	bpp = pcxhdr.BitsPerPixel;
	return true;
}

} // namespace

OptionalOwnedClxSpriteList PcxToClx(AssetHandle &handle, size_t fileSize, int numFramesOrFrameHeight, std::optional<uint8_t> transparentColor, SDL_Color *outPalette)
{
	int width;
	int height;
	uint8_t bpp;
	if (!LoadPcxMeta(handle, width, height, bpp)) {
		return std::nullopt;
	}
	assert(bpp == 8);

	unsigned numFrames;
	unsigned frameHeight;
	if (numFramesOrFrameHeight > 0) {
		numFrames = numFramesOrFrameHeight;
		frameHeight = height / numFrames;
	} else {
		frameHeight = -numFramesOrFrameHeight;
		numFrames = height / frameHeight;
	}

	size_t pixelDataSize = fileSize;
	if (pixelDataSize <= PcxHeaderSize) {
		return std::nullopt;
	}
	pixelDataSize -= PcxHeaderSize;

	std::unique_ptr<uint8_t[]> fileBuffer { new uint8_t[pixelDataSize] };
	if (handle.read(fileBuffer.get(), pixelDataSize) == 0) {
		return std::nullopt;
	}

	// CLX header: frame count, frame offset for each frame, file size
	std::vector<uint8_t> cl2Data;
	cl2Data.reserve(GetReservationSize(pixelDataSize));
	cl2Data.resize(4 * (2 + static_cast<size_t>(numFrames)));
	WriteLE32(cl2Data.data(), numFrames);

	// We process the PCX a whole frame at a time because the lines are reversed in CEL.
	auto frameBuffer = std::unique_ptr<uint8_t[]>(new uint8_t[static_cast<size_t>(frameHeight) * width]);

	const unsigned srcSkip = width % 2;
	uint8_t *dataPtr = fileBuffer.get();
	for (unsigned frame = 1; frame <= numFrames; ++frame) {
		WriteLE32(&cl2Data[4 * static_cast<size_t>(frame)], static_cast<uint32_t>(cl2Data.size()));

		const size_t frameHeaderPos = cl2Data.size();
		cl2Data.resize(cl2Data.size() + ClxFrameHeaderSize);

		// Frame header:
		WriteLE16(&cl2Data[frameHeaderPos], ClxFrameHeaderSize);
		WriteLE16(&cl2Data[frameHeaderPos + 2], static_cast<uint16_t>(width));
		WriteLE16(&cl2Data[frameHeaderPos + 4], static_cast<uint16_t>(frameHeight));

		for (unsigned j = 0; j < frameHeight; ++j) {
			uint8_t *buffer = &frameBuffer[static_cast<size_t>(j) * width];
			for (unsigned x = 0; x < static_cast<unsigned>(width);) {
				constexpr uint8_t PcxMaxSinglePixel = 0xBF;
				const uint8_t byte = *dataPtr++;
				if (byte <= PcxMaxSinglePixel) {
					*buffer++ = byte;
					++x;
					continue;
				}
				constexpr uint8_t PcxRunLengthMask = 0x3F;
				const uint8_t runLength = (byte & PcxRunLengthMask);
				std::memset(buffer, *dataPtr++, runLength);
				buffer += runLength;
				x += runLength;
			}
			dataPtr += srcSkip;
		}

		unsigned transparentRunWidth = 0;
		size_t line = 0;
		while (line != frameHeight) {
			// Process line:
			const uint8_t *src = &frameBuffer[(frameHeight - (line + 1)) * width];
			if (transparentColor) {
				unsigned solidRunWidth = 0;
				for (const uint8_t *srcEnd = src + width; src != srcEnd; ++src) {
					if (*src == *transparentColor) {
						if (solidRunWidth != 0) {
							AppendClxPixelsOrFillRun(src - transparentRunWidth - solidRunWidth, solidRunWidth, cl2Data);
							solidRunWidth = 0;
						}
						++transparentRunWidth;
					} else {
						AppendClxTransparentRun(transparentRunWidth, cl2Data);
						transparentRunWidth = 0;
						++solidRunWidth;
					}
				}
				if (solidRunWidth != 0) {
					AppendClxPixelsOrFillRun(src - solidRunWidth, solidRunWidth, cl2Data);
				}
			} else {
				AppendClxPixelsOrFillRun(src, width, cl2Data);
			}
			++line;
		}
		AppendClxTransparentRun(transparentRunWidth, cl2Data);
	}
	WriteLE32(&cl2Data[4 * (1 + static_cast<size_t>(numFrames))], static_cast<uint32_t>(cl2Data.size()));

	if (outPalette != nullptr) {
		[[maybe_unused]] constexpr unsigned PcxPaletteSeparator = 0x0C;
		assert(*dataPtr == PcxPaletteSeparator); // PCX may not have a palette
		++dataPtr;

		for (unsigned i = 0; i < 256; ++i) {
			outPalette->r = *dataPtr++;
			outPalette->g = *dataPtr++;
			outPalette->b = *dataPtr++;
#ifndef USE_SDL1
			outPalette->a = SDL_ALPHA_OPAQUE;
#endif
			++outPalette;
		}
	}

	// Release buffers before allocating the result array to reduce peak memory use.
	frameBuffer = nullptr;
	fileBuffer = nullptr;

	auto out = std::unique_ptr<uint8_t[]>(new uint8_t[cl2Data.size()]);
	memcpy(&out[0], cl2Data.data(), cl2Data.size());
#ifdef DEBUG_PCX_TO_CL2_SIZE
	std::cout << "\t" << pixelDataSize << "\t" << cl2Data.size() << "\t" << std::setprecision(1) << std::fixed << (static_cast<int>(cl2Data.size()) - static_cast<int>(pixelDataSize)) / ((float)pixelDataSize) * 100 << "%" << std::endl;
#endif
	return OwnedClxSpriteList { std::move(out) };
}

} // namespace devilution
