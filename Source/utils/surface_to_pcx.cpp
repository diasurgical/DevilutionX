#include "utils/surface_to_pcx.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <memory>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include "engine/surface.hpp"
#include "utils/endian_swap.hpp"
#include "utils/pcx.hpp"
#include "utils/sdl_compat.h"

namespace devilution {
namespace {

std::expected<void, std::string> CheckedFWrite(const void *ptr, size_t size, SDL_IOStream *out)
{
	if (SDL_WriteIO(out, ptr, size) != size) {
		const char *errorMessage = SDL_GetError();
		if (errorMessage == nullptr)
			errorMessage = "";
		std::expected<void, std::string> result = std::unexpected(std::string("write failed with: ").append(errorMessage));
		SDL_ClearError();
		return result;
	}
	return {};
}

/**
 * @brief The PCX specification requires the number of bytes per scanline to be even.
 */
uint16_t PcxBytesPerLine(int16_t width)
{
	return static_cast<uint16_t>(width + (width % 2));
}

/**
 * @brief Write the PCX-file header
 * @param width Image width
 * @param height Image height
 * @param out File stream to write to
 * @return True on success
 */
std::expected<void, std::string> WritePcxHeader(int16_t width, int16_t height, SDL_IOStream *out)
{
	PCXHeader buffer;

	memset(&buffer, 0, sizeof(buffer));
	buffer.Manufacturer = 10;
	buffer.Version = 5;
	buffer.Encoding = 1;
	buffer.BitsPerPixel = 8;
	buffer.Xmax = Swap16LE(width - 1);
	buffer.Ymax = Swap16LE(height - 1);
	buffer.HDpi = Swap16LE(width);
	buffer.VDpi = Swap16LE(height);
	buffer.NPlanes = 1;
	buffer.BytesPerLine = Swap16LE(PcxBytesPerLine(width));

	return CheckedFWrite(&buffer, sizeof(buffer), out);
}

/**
 * @brief Write the current in-game palette to the PCX file
 * @param palette Current palette. Missing colors (if it has fewer than 256) are written as black.
 * @param out File stream for the PCX file.
 * @return True if successful, else false
 */
std::expected<void, std::string> WritePcxPalette(const SDL_Palette &palette, SDL_IOStream *out)
{
	uint8_t pcxPalette[1 + 256 * 3] = {};

	pcxPalette[0] = 12;
	for (int i = 0; i < 256 && i < palette.ncolors; i++) {
		pcxPalette[1 + 3 * i + 0] = palette.colors[i].r;
		pcxPalette[1 + 3 * i + 1] = palette.colors[i].g;
		pcxPalette[1 + 3 * i + 2] = palette.colors[i].b;
	}

	return CheckedFWrite(pcxPalette, sizeof(pcxPalette), out);
}

/**
 * @brief RLE compress the pixel data
 * @param src Raw pixel buffer
 * @param dst Output buffer
 * @param width Width of pixel buffer

 * @return Output buffer
 */
uint8_t *WritePcxLine(const uint8_t *src, uint8_t *dst, int width)
{
	int rleLength;

	do {
		const uint8_t rlePixel = *src;
		src++;
		rleLength = 1;

		width--;

		while (width > 0 && rleLength < 63 && rlePixel == *src) {
			rleLength++;

			width--;
			src++;
		}

		if (rleLength > 1 || rlePixel > 0xBF) {
			*dst = rleLength | 0xC0;
			dst++;
		}

		*dst = rlePixel;
		dst++;
	} while (width > 0);

	return dst;
}

/**
 * @brief Write the pixel data to the PCX file
 *
 * @param buf Pixel data
 * @param out File stream for the PCX file.
 * @return True if successful, else false
 */
std::expected<void, std::string> WritePcxPixels(const Surface &buf, SDL_IOStream *out)
{
	const int width = buf.w();
	const int bytesPerLine = PcxBytesPerLine(width);
	const std::unique_ptr<uint8_t[]> lineBuffer { new uint8_t[static_cast<size_t>(bytesPerLine)] };
	std::memset(lineBuffer.get(), 0, bytesPerLine);
	const std::unique_ptr<uint8_t[]> pBuffer { new uint8_t[static_cast<size_t>(2 * bytesPerLine)] };
	uint8_t *pixels = buf.begin();
	for (int height = buf.h(); height > 0; height--) {
		std::memcpy(lineBuffer.get(), pixels, width);
		const uint8_t *pBufferEnd = WritePcxLine(lineBuffer.get(), pBuffer.get(), bytesPerLine);
		pixels += buf.pitch();
		std::expected<void, std::string> result = CheckedFWrite(pBuffer.get(), pBufferEnd - pBuffer.get(), out);
		if (!result.has_value()) return result;
	}
	return {};
}

} // namespace

std::expected<void, std::string>
WriteSurfaceToFilePcx(const Surface &buf, SDL_IOStream *outStream)
{
#ifdef USE_SDL3
	const SDL_Palette *palette = SDL_GetSurfacePalette(buf.surface);
#else
	const SDL_Palette *palette = buf.surface->format->palette;
#endif
	if (palette == nullptr) {
		return std::unexpected("surface has no palette");
	}

	std::expected<void, std::string> result = WritePcxHeader(buf.w(), buf.h(), outStream);
	if (!result.has_value()) return result;
	result = WritePcxPixels(buf, outStream);
	if (!result.has_value()) return result;
	result = WritePcxPalette(*palette, outStream);
	if (!result.has_value()) return result;
	SDL_CloseIO(outStream);
	return {};
}

} // namespace devilution
