#include "engine/render/light_render.hpp"

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>

#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "levels/dun_tile.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "options.h"

namespace devilution {

namespace {

std::vector<uint8_t> LightmapBuffer;

// Half-space method for drawing triangles
// Points must be provided using counter-clockwise rotation
// https://web.archive.org/web/20050408192410/http://sw-shader.sourceforge.net/rasterizer.html
void RenderTriangle(Point p1, Point p2, Point p3, uint8_t lightLevel, uint8_t *lightmap, uint16_t pitch, uint16_t scanLines)
{
	// Deltas (points are already 28.4 fixed-point)
	int dx12 = p1.x - p2.x;
	int dx23 = p2.x - p3.x;
	int dx31 = p3.x - p1.x;

	int dy12 = p1.y - p2.y;
	int dy23 = p2.y - p3.y;
	int dy31 = p3.y - p1.y;

	// 24.8 fixed-point deltas
	int fdx12 = dx12 << 4;
	int fdx23 = dx23 << 4;
	int fdx31 = dx31 << 4;

	int fdy12 = dy12 << 4;
	int fdy23 = dy23 << 4;
	int fdy31 = dy31 << 4;

	// Bounding rectangle
	int minx = (std::min({ p1.x, p2.x, p3.x }) + 0xF) >> 4;
	int maxx = (std::max({ p1.x, p2.x, p3.x }) + 0xF) >> 4;
	int miny = (std::min({ p1.y, p2.y, p3.y }) + 0xF) >> 4;
	int maxy = (std::max({ p1.y, p2.y, p3.y }) + 0xF) >> 4;
	minx = std::max<int>(minx, 0);
	maxx = std::min<int>(maxx, pitch);
	miny = std::max<int>(miny, 0);
	maxy = std::min<int>(maxy, scanLines);

	uint8_t *dst = lightmap + miny * pitch;

	// Half-edge constants
	int c1 = dy12 * p1.x - dx12 * p1.y;
	int c2 = dy23 * p2.x - dx23 * p2.y;
	int c3 = dy31 * p3.x - dx31 * p3.y;

	// Correct for fill convention
	if (dy12 < 0 || (dy12 == 0 && dx12 > 0)) c1++;
	if (dy23 < 0 || (dy23 == 0 && dx23 > 0)) c2++;
	if (dy31 < 0 || (dy31 == 0 && dx31 > 0)) c3++;

	int cy1 = c1 + dx12 * (miny << 4) - dy12 * (minx << 4);
	int cy2 = c2 + dx23 * (miny << 4) - dy23 * (minx << 4);
	int cy3 = c3 + dx31 * (miny << 4) - dy31 * (minx << 4);

	for (int y = miny; y < maxy; y++) {
		int cx1 = cy1;
		int cx2 = cy2;
		int cx3 = cy3;

		for (int x = minx; x < maxx; x++) {
			if (cx1 > 0 && cx2 > 0 && cx3 > 0)
				dst[x] = lightLevel;

			cx1 -= fdy12;
			cx2 -= fdy23;
			cx3 -= fdy31;
		}

		cy1 += fdx12;
		cy2 += fdx23;
		cy3 += fdx31;

		dst += pitch;
	}
}

uint8_t GetLightLevel(Point tile)
{
	int x = std::clamp(tile.x, 0, MAXDUNX - 1);
	int y = std::clamp(tile.y, 0, MAXDUNY - 1);
	return dLight[x][y];
}

uint8_t Interpolate(int q1, int q2, int lightLevel)
{
	// Result will be 28.4 fixed-point
	int numerator = (lightLevel - q1) << 4;
	int result = (numerator + 0x8) / (q2 - q1);
	assert(result >= 0);
	return static_cast<uint8_t>(result);
}

void RenderCell(uint8_t quad[4], Point position, uint8_t lightLevel, uint8_t *lightmap, uint16_t pitch, uint16_t scanLines)
{
	Point center0 = position;
	Point center1 = position + Displacement { TILE_WIDTH / 2, TILE_HEIGHT / 2 };
	Point center2 = position + Displacement { 0, TILE_HEIGHT };
	Point center3 = position + Displacement { -TILE_WIDTH / 2, TILE_HEIGHT / 2 };

	// 28.4 fixed-point coordinates
	Point fpCenter0 = center0 * (1 << 4);
	Point fpCenter1 = center1 * (1 << 4);
	Point fpCenter2 = center2 * (1 << 4);
	Point fpCenter3 = center3 * (1 << 4);

	// Marching squares
	// https://en.wikipedia.org/wiki/Marching_squares
	uint8_t shape = 0;
	shape |= quad[0] <= lightLevel ? 8 : 0;
	shape |= quad[1] <= lightLevel ? 4 : 0;
	shape |= quad[2] <= lightLevel ? 2 : 0;
	shape |= quad[3] <= lightLevel ? 1 : 0;

	switch (shape) {
	// The whole cell is darker than lightLevel
	case 0: break;

	// Fill in the bottom-left corner of the cell
	// In isometric view, only the west tile of the quad is lit
	case 1: {
		uint8_t bottomFactor = Interpolate(quad[3], quad[2], lightLevel);
		uint8_t leftFactor = Interpolate(quad[3], quad[0], lightLevel);
		Point p1 = fpCenter3 + (center2 - center3) * bottomFactor;
		Point p2 = fpCenter3;
		Point p3 = fpCenter3 + (center0 - center3) * leftFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the bottom-right corner of the cell
	// In isometric view, only the south tile of the quad is lit
	case 2: {
		uint8_t rightFactor = Interpolate(quad[2], quad[1], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[2], quad[3], lightLevel);
		Point p1 = fpCenter2 + (center1 - center2) * rightFactor;
		Point p2 = fpCenter2;
		Point p3 = fpCenter2 + (center3 - center2) * bottomFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the bottom half of the cell
	// In isometric view, the south and west tiles of the quad are lit
	case 3: {
		uint8_t rightFactor = Interpolate(quad[2], quad[1], lightLevel);
		uint8_t leftFactor = Interpolate(quad[3], quad[0], lightLevel);
		Point p1 = fpCenter2 + (center1 - center2) * rightFactor;
		Point p2 = fpCenter2;
		Point p3 = fpCenter3;
		Point p4 = fpCenter3 + (center1 - center2) * leftFactor;
		RenderTriangle(p1, p4, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p2, p4, p3, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the top-right corner of the cell
	// In isometric view, only the east tile of the quad is lit
	case 4: {
		uint8_t topFactor = Interpolate(quad[1], quad[0], lightLevel);
		uint8_t rightFactor = Interpolate(quad[1], quad[2], lightLevel);
		Point p1 = fpCenter1 + (center0 - center1) * topFactor;
		Point p2 = fpCenter1;
		Point p3 = fpCenter1 + (center2 - center1) * rightFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the top-right and bottom-left corners of the cell
	// Use the average of all values in the quad to determine whether to fill in the center
	// In isometric view, the east and west tiles of the quad are lit
	case 5: {
		uint8_t cell = (quad[0] + quad[1] + quad[2] + quad[3] + 2) / 4;
		uint8_t topFactor = Interpolate(quad[1], quad[0], lightLevel);
		uint8_t rightFactor = Interpolate(quad[1], quad[2], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[3], quad[2], lightLevel);
		uint8_t leftFactor = Interpolate(quad[3], quad[0], lightLevel);
		Point p1 = fpCenter1 + (center0 - center1) * topFactor;
		Point p2 = fpCenter1;
		Point p3 = fpCenter1 + (center2 - center1) * rightFactor;
		Point p4 = fpCenter3 + (center2 - center3) * bottomFactor;
		Point p5 = fpCenter3;
		Point p6 = fpCenter3 + (center0 - center3) * leftFactor;

		if (cell <= lightLevel) {
			uint8_t midFactor0 = Interpolate(quad[0], cell, lightLevel);
			uint8_t midFactor2 = Interpolate(quad[2], cell, lightLevel);
			Point p7 = fpCenter0 + (center2 - center0) / 2 * midFactor0;
			Point p8 = fpCenter2 + (center0 - center2) / 2 * midFactor2;
			RenderTriangle(p1, p7, p2, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p2, p7, p8, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p2, p8, p3, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p4, p8, p5, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p5, p8, p7, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p5, p7, p6, lightLevel, lightmap, pitch, scanLines);
		} else {
			uint8_t midFactor1 = Interpolate(quad[1], cell, lightLevel);
			uint8_t midFactor3 = Interpolate(quad[3], cell, lightLevel);
			Point p7 = fpCenter1 + (center3 - center1) / 2 * midFactor1;
			Point p8 = fpCenter3 + (center1 - center3) / 2 * midFactor3;
			RenderTriangle(p1, p7, p2, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p2, p7, p3, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p4, p8, p5, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p5, p8, p6, lightLevel, lightmap, pitch, scanLines);
		}
	} break;

	// Fill in the right half of the cell
	// In isometric view, the south and east tiles of the quad are lit
	case 6: {
		uint8_t topFactor = Interpolate(quad[1], quad[0], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[2], quad[3], lightLevel);
		Point p1 = fpCenter1 + (center0 - center1) * topFactor;
		Point p2 = fpCenter1;
		Point p3 = fpCenter2;
		Point p4 = fpCenter2 + (center3 - center2) * bottomFactor;
		RenderTriangle(p1, p4, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p2, p4, p3, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in everything except the top-left corner of the cell
	// In isometric view, the south, east, and west tiles of the quad are lit
	case 7: {
		uint8_t topFactor = Interpolate(quad[1], quad[0], lightLevel);
		uint8_t leftFactor = Interpolate(quad[3], quad[0], lightLevel);
		Point p1 = fpCenter1 + (center0 - center1) * topFactor;
		Point p2 = fpCenter1;
		Point p3 = fpCenter2;
		Point p4 = fpCenter3;
		Point p5 = fpCenter3 + (center0 - center3) * leftFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p1, p5, p3, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p3, p5, p4, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the top-left corner of the cell
	// In isometric view, only the north tile of the quad is lit
	case 8: {
		uint8_t topFactor = Interpolate(quad[0], quad[1], lightLevel);
		uint8_t leftFactor = Interpolate(quad[0], quad[3], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter0 + (center1 - center0) * topFactor;
		Point p3 = fpCenter0 + (center3 - center0) * leftFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the left half of the cell
	// In isometric view, the north and west tiles of the quad are lit
	case 9: {
		uint8_t topFactor = Interpolate(quad[0], quad[1], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[3], quad[2], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter0 + (center1 - center0) * topFactor;
		Point p3 = fpCenter3 + (center2 - center3) * bottomFactor;
		Point p4 = fpCenter3;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p1, p4, p3, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the top-left and bottom-right corners of the cell
	// Use the average of all values in the quad to determine whether to fill in the center
	// In isometric view, the north and south tiles of the quad are lit
	case 10: {
		uint8_t cell = (quad[0] + quad[1] + quad[2] + quad[3] + 2) / 4;
		uint8_t topFactor = Interpolate(quad[0], quad[1], lightLevel);
		uint8_t rightFactor = Interpolate(quad[2], quad[1], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[2], quad[3], lightLevel);
		uint8_t leftFactor = Interpolate(quad[0], quad[3], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter0 + (center1 - center0) * topFactor;
		Point p3 = fpCenter2 + (center1 - center2) * rightFactor;
		Point p4 = fpCenter2;
		Point p5 = fpCenter2 + (center3 - center2) * bottomFactor;
		Point p6 = fpCenter0 + (center3 - center0) * leftFactor;

		if (cell <= lightLevel) {
			uint8_t midFactor1 = Interpolate(quad[1], cell, lightLevel);
			uint8_t midFactor3 = Interpolate(quad[3], cell, lightLevel);
			Point p7 = fpCenter1 + (center3 - center1) / 2 * midFactor1;
			Point p8 = fpCenter3 + (center1 - center3) / 2 * midFactor3;
			RenderTriangle(p1, p7, p2, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p1, p6, p8, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p1, p8, p7, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p3, p7, p4, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p4, p8, p5, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p4, p7, p8, lightLevel, lightmap, pitch, scanLines);
		} else {
			uint8_t midFactor0 = Interpolate(quad[0], cell, lightLevel);
			uint8_t midFactor2 = Interpolate(quad[2], cell, lightLevel);
			Point p7 = fpCenter0 + (center2 - center0) / 2 * midFactor0;
			Point p8 = fpCenter2 + (center0 - center2) / 2 * midFactor2;
			RenderTriangle(p1, p7, p2, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p1, p6, p7, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p3, p8, p4, lightLevel, lightmap, pitch, scanLines);
			RenderTriangle(p4, p8, p5, lightLevel, lightmap, pitch, scanLines);
		}
	} break;

	// Fill in everything except the top-right corner of the cell
	// In isometric view, the north, south, and west tiles of the quad are lit
	case 11: {
		uint8_t topFactor = Interpolate(quad[0], quad[1], lightLevel);
		uint8_t rightFactor = Interpolate(quad[2], quad[1], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter0 + (center1 - center0) * topFactor;
		Point p3 = fpCenter2 + (center1 - center2) * rightFactor;
		Point p4 = fpCenter2;
		Point p5 = fpCenter3;
		RenderTriangle(p1, p5, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p2, p5, p3, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p3, p5, p4, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the top half of the cell
	// In isometric view, the north and east tiles of the quad are lit
	case 12: {
		uint8_t rightFactor = Interpolate(quad[1], quad[2], lightLevel);
		uint8_t leftFactor = Interpolate(quad[0], quad[3], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter1;
		Point p3 = fpCenter1 + (center2 - center1) * rightFactor;
		Point p4 = fpCenter0 + (center3 - center0) * leftFactor;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p1, p4, p3, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in everything except the bottom-right corner of the cell
	// In isometric view, the north, east, and west tiles of the quad are lit
	case 13: {
		uint8_t rightFactor = Interpolate(quad[1], quad[2], lightLevel);
		uint8_t bottomFactor = Interpolate(quad[3], quad[2], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter1;
		Point p3 = fpCenter1 + (center2 - center1) * rightFactor;
		Point p4 = fpCenter3 + (center2 - center3) * bottomFactor;
		Point p5 = fpCenter3;
		RenderTriangle(p1, p3, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p1, p4, p3, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p1, p5, p4, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in everything except the bottom-left corner of the cell
	// In isometric view, the north, south, and east tiles of the quad are lit
	case 14: {
		uint8_t bottomFactor = Interpolate(quad[2], quad[3], lightLevel);
		uint8_t leftFactor = Interpolate(quad[0], quad[3], lightLevel);
		Point p1 = fpCenter0;
		Point p2 = fpCenter1;
		Point p3 = fpCenter2;
		Point p4 = fpCenter2 + (center3 - center2) * bottomFactor;
		Point p5 = fpCenter0 + (center3 - center0) * leftFactor;
		RenderTriangle(p1, p5, p2, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p2, p5, p4, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(p2, p4, p3, lightLevel, lightmap, pitch, scanLines);
	} break;

	// Fill in the whole cell
	// All four tiles in the quad are lit
	case 15: {
		RenderTriangle(fpCenter0, fpCenter2, fpCenter1, lightLevel, lightmap, pitch, scanLines);
		RenderTriangle(fpCenter0, fpCenter3, fpCenter2, lightLevel, lightmap, pitch, scanLines);
	} break;
	}
}

void BuildLightmap(Point tilePosition, Point targetBufferPosition, uint16_t viewportWidth, uint16_t viewportHeight, int rows, int columns)
{
	if (!*GetOptions().Graphics.perPixelLighting)
		return;

	// Since light may need to bleed up to the top of wall tiles,
	// expand the buffer space to include the full base diamond of the tallest tile graphics
	const uint16_t bufferHeight = viewportHeight + TILE_HEIGHT * (MicroTileLen / 2 + 1);
	rows += MicroTileLen + 2;

	const size_t totalPixels = static_cast<size_t>(viewportWidth) * bufferHeight;
	LightmapBuffer.resize(totalPixels);

	// Since rendering occurs in cells between quads,
	// expand the rendering space to include tiles outside the viewport
	tilePosition += Displacement(Direction::NorthWest) * 2;
	targetBufferPosition -= Displacement { TILE_WIDTH, TILE_HEIGHT };
	rows += 3;
	columns++;

	uint8_t *lightmap = LightmapBuffer.data();
	memset(lightmap, LightsMax, totalPixels);
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++, tilePosition += Direction::East, targetBufferPosition.x += TILE_WIDTH) {
			Point center0 = targetBufferPosition + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };

			Point tile0 = tilePosition;
			Point tile1 = tilePosition + Displacement { 1, 0 };
			Point tile2 = tilePosition + Displacement { 1, 1 };
			Point tile3 = tilePosition + Displacement { 0, 1 };

			uint8_t quad[] = {
				GetLightLevel(tile0),
				GetLightLevel(tile1),
				GetLightLevel(tile2),
				GetLightLevel(tile3)
			};

			uint8_t maxLight = std::max({ quad[0], quad[1], quad[2], quad[3] });
			uint8_t minLight = std::min({ quad[0], quad[1], quad[2], quad[3] });

			for (uint8_t i = 0; i < LightsMax; i++) {
				uint8_t lightLevel = LightsMax - i - 1;
				if (lightLevel > maxLight)
					continue;
				if (lightLevel < minLight)
					break;
				RenderCell(quad, center0, lightLevel, lightmap, viewportWidth, bufferHeight);
			}
		}

		// Return to start of row
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;

		// Jump to next row
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

} // namespace

Lightmap::Lightmap(const uint8_t *outBuffer, uint16_t outPitch,
    std::span<const uint8_t> lightmapBuffer, uint16_t lightmapPitch,
    const uint8_t *lightTables, size_t lightTableSize)
    : outBuffer(outBuffer)
    , outPitch(outPitch)
    , lightmapBuffer(lightmapBuffer)
    , lightmapPitch(lightmapPitch)
    , lightTables(lightTables)
    , lightTableSize(lightTableSize)
{
}

Lightmap Lightmap::build(Point tilePosition, Point targetBufferPosition,
    int viewportWidth, int viewportHeight, int rows, int columns,
    const uint8_t *outBuffer, uint16_t outPitch,
    const uint8_t *lightTables, size_t lightTableSize)
{
	BuildLightmap(tilePosition, targetBufferPosition, viewportWidth, viewportHeight, rows, columns);
	return Lightmap(outBuffer, outPitch, LightmapBuffer, gnScreenWidth, lightTables, lightTableSize);
}

Lightmap Lightmap::bleedUp(const Lightmap &source, Point targetBufferPosition, std::span<uint8_t> lightmapBuffer)
{
	assert(lightmapBuffer.size() >= TILE_WIDTH * TILE_HEIGHT);

	if (!*GetOptions().Graphics.perPixelLighting)
		return source;

	const int sourceHeight = static_cast<int>(source.lightmapBuffer.size() / source.lightmapPitch);
	const int clipLeft = std::max(0, -targetBufferPosition.x);
	const int clipTop = std::max(0, -(targetBufferPosition.y - TILE_HEIGHT + 1));
	const int clipRight = std::max(0, targetBufferPosition.x + TILE_WIDTH - source.lightmapPitch);
	const int clipBottom = std::max(0, targetBufferPosition.y - sourceHeight + 1);

	// Nothing we can do if the tile is completely outside the bounds of the lightmap
	if (clipLeft + clipRight >= TILE_WIDTH)
		return source;
	if (clipTop + clipBottom >= TILE_HEIGHT)
		return source;

	const uint16_t lightmapPitch = std::max(0, TILE_WIDTH - clipLeft - clipRight);
	const uint16_t lightmapHeight = TILE_HEIGHT - clipTop - clipBottom;

	// Find the left edge of the last row in the tile
	const int outOffset = std::max(0, (targetBufferPosition.y - clipBottom) * source.outPitch + targetBufferPosition.x + clipLeft);
	const uint8_t *outLoc = source.outBuffer + outOffset;
	const uint8_t *outBuffer = outLoc - (lightmapHeight - 1) * source.outPitch;

	// Start copying bytes from the bottom row of the tile
	const uint8_t *src = source.getLightingAt(outLoc);
	uint8_t *dst = lightmapBuffer.data() + (lightmapHeight - 1) * lightmapPitch;

	int rowCount = clipBottom;
	while (src >= source.lightmapBuffer.data() && dst >= lightmapBuffer.data()) {
		const int bleed = std::max(0, (rowCount - TILE_HEIGHT / 2) * 2);
		const int lightOffset = std::max(bleed, clipLeft) - clipLeft;
		const int lightLength = std::max(0, TILE_WIDTH - clipLeft - std::max(bleed, clipRight) - lightOffset);

		// Bleed pixels up by copying data from the row below this one
		if (rowCount > clipBottom && lightLength < lightmapPitch)
			memcpy(dst, dst + lightmapPitch, lightmapPitch);

		// Copy data from the source lightmap between the top edge of the base diamond
		assert(dst + lightOffset + lightLength <= lightmapBuffer.data() + TILE_WIDTH * TILE_HEIGHT);
		assert(src + lightOffset + lightLength <= source.lightmapBuffer.data() + source.lightmapBuffer.size());
		memcpy(dst + lightOffset, src + lightOffset, lightLength);

		src -= source.lightmapPitch;
		dst -= lightmapPitch;
		rowCount++;
	}

	return Lightmap(outBuffer, source.outPitch,
	    lightmapBuffer, lightmapPitch,
	    source.lightTables, source.lightTableSize);
}

} // namespace devilution
