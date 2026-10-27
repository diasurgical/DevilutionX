#include "utils/pcx_to_clx.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <array>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#else
#include <SDL.h>
#endif

#include "appfat.h"
#include "utils/clx_encode.hpp"
#include "utils/endian_read.hpp"
#include "utils/endian_swap.hpp"
#include "utils/endian_write.hpp"
#include "utils/pcx.hpp"

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

/**
 * @param bytesPerLine The size of each decoded scanline, including padding.
 */
bool LoadPcxMeta(AssetHandle &handle, int &width, int &height, uint8_t &bpp, uint8_t &numPlanes, unsigned &bytesPerLine)
{
	PCXHeader pcxhdr;
	if (!handle.read(&pcxhdr, PcxHeaderSize)) {
		return false;
	}
	width = Swap16LE(pcxhdr.Xmax) - Swap16LE(pcxhdr.Xmin) + 1;
	height = Swap16LE(pcxhdr.Ymax) - Swap16LE(pcxhdr.Ymin) + 1;
	bpp = pcxhdr.BitsPerPixel;
	numPlanes = pcxhdr.NPlanes;
	bytesPerLine = Swap16LE(pcxhdr.BytesPerLine);
	return true;
}

} // namespace

std::expected<OwnedClxSpriteList, std::string> PcxToClx(AssetHandle &handle, size_t fileSize, int numFramesOrFrameHeight, std::optional<uint8_t> transparentColor, SDL_Color *outPalette)
{
	if (fileSize <= PcxHeaderSize) {
		return std::unexpected("PCX file is too small");
	}

	int width;
	int height;
	uint8_t bpp;
	uint8_t numPlanes;
	unsigned bytesPerLine;
	if (!LoadPcxMeta(handle, width, height, bpp, numPlanes, bytesPerLine)) {
		return std::unexpected(std::string("Failed to read the PCX header: ").append(handle.error()));
	}
	if (bpp != 8 || numPlanes != 1) {
		return std::unexpected("Only 8-bit single-plane PCX images are supported");
	}
	if (width <= 0 || height <= 0) {
		return std::unexpected("Invalid PCX image dimensions");
	}
	// The PCX specification requires the bytes per line to be even, so this can be larger than the width.
	if (bytesPerLine < static_cast<unsigned>(width)) {
		return std::unexpected("Invalid PCX header: bytes per line is less than the image width");
	}

	unsigned numFrames;
	unsigned frameHeight;
	if (numFramesOrFrameHeight > 0) {
		numFrames = numFramesOrFrameHeight;
		frameHeight = height / numFrames;
	} else if (numFramesOrFrameHeight < 0) {
		frameHeight = -numFramesOrFrameHeight;
		numFrames = height / frameHeight;
	} else {
		return std::unexpected("The number of frames must not be 0");
	}
	if (numFrames == 0 || frameHeight == 0) {
		return std::unexpected("PCX image is too small for the requested frame size or count");
	}

	const size_t pixelDataSize = fileSize - PcxHeaderSize;
	std::unique_ptr<uint8_t[]> fileBuffer { new uint8_t[pixelDataSize] };
	if (!handle.read(fileBuffer.get(), pixelDataSize)) {
		return std::unexpected(std::string("Failed to read the PCX data: ").append(handle.error()));
	}
	const uint8_t *const dataEnd = fileBuffer.get() + pixelDataSize;

	// CLX header: frame count, frame offset for each frame, file size
	std::vector<uint8_t> cl2Data;
	cl2Data.reserve(GetReservationSize(pixelDataSize));
	cl2Data.resize(4 * (2 + static_cast<size_t>(numFrames)));
	WriteLE32(cl2Data.data(), numFrames);

	// We process the PCX a whole frame at a time because the lines are reversed in CEL.
	auto frameBuffer = std::unique_ptr<uint8_t[]>(new uint8_t[static_cast<size_t>(frameHeight) * width]);

	auto lineBuffer = std::unique_ptr<uint8_t[]>(new uint8_t[bytesPerLine]);

	const uint8_t *dataPtr = fileBuffer.get();
	for (unsigned frame = 1; frame <= numFrames; ++frame) {
		WriteLE32(&cl2Data[4 * static_cast<size_t>(frame)], static_cast<uint32_t>(cl2Data.size()));

		const size_t frameHeaderPos = cl2Data.size();
		cl2Data.resize(cl2Data.size() + ClxFrameHeaderSize);

		// Frame header:
		WriteLE16(&cl2Data[frameHeaderPos], ClxFrameHeaderSize);
		WriteLE16(&cl2Data[frameHeaderPos + 2], static_cast<uint16_t>(width));
		WriteLE16(&cl2Data[frameHeaderPos + 4], static_cast<uint16_t>(frameHeight));

		for (unsigned j = 0; j < frameHeight; ++j) {
			for (unsigned x = 0; x < bytesPerLine;) {
				if (dataPtr == dataEnd) {
					return std::unexpected("PCX pixel data is truncated");
				}
				constexpr uint8_t PcxMaxSinglePixel = 0xBF;
				const uint8_t byte = *dataPtr++;
				if (byte <= PcxMaxSinglePixel) {
					lineBuffer[x++] = byte;
					continue;
				}
				if (dataPtr == dataEnd) {
					return std::unexpected("PCX pixel data is truncated");
				}
				constexpr uint8_t PcxRunLengthMask = 0x3F;
				const uint8_t runLength = (byte & PcxRunLengthMask);
				if (runLength > bytesPerLine - x) {
					return std::unexpected("PCX run crosses a scanline boundary");
				}
				std::memset(&lineBuffer[x], *dataPtr++, runLength);
				x += runLength;
			}
			std::memcpy(&frameBuffer[static_cast<size_t>(j) * width], lineBuffer.get(), width);
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
		// The 256-color palette is stored at the end of the file:
		// a separator byte followed by 256 RGB triplets.
		constexpr unsigned PcxPaletteSeparator = 0x0C;
		constexpr size_t PcxPaletteSize = 1 + 256 * 3;
		if (static_cast<size_t>(dataEnd - dataPtr) < PcxPaletteSize) {
			return std::unexpected("PCX has no palette");
		}
		dataPtr = dataEnd - PcxPaletteSize;
		if (*dataPtr++ != PcxPaletteSeparator) {
			return std::unexpected("PCX has no palette");
		}

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
	lineBuffer = nullptr;
	fileBuffer = nullptr;

	auto out = std::unique_ptr<uint8_t[]>(new uint8_t[cl2Data.size()]);
	memcpy(&out[0], cl2Data.data(), cl2Data.size());
#ifdef DEBUG_PCX_TO_CL2_SIZE
	std::cout << "\t" << pixelDataSize << "\t" << cl2Data.size() << "\t" << std::setprecision(1) << std::fixed << (static_cast<int>(cl2Data.size()) - static_cast<int>(pixelDataSize)) / ((float)pixelDataSize) * 100 << "%" << std::endl;
#endif
	return OwnedClxSpriteList { std::move(out) };
}

} // namespace devilution
