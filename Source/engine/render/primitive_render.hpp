#pragma once

#include <cstdint>
#include <cstdlib>

#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Fill a rectangle with the given color.
 */
void FillRect(const Surface &out, int x, int y, int width, int height, uint8_t colorIndex);

/**
 * @brief Draw a horizontal line segment in the target buffer (left to right)
 * @param out Target buffer
 * @param from Start of the line segment
 * @param width
 * @param colorIndex Color index from current palette
 */
void DrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex);

/** Same as DrawHorizontalLine but without bounds clipping. */
void UnsafeDrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex);

/**
 * @brief Draw a vertical line segment in the target buffer (top to bottom)
 * @param out Target buffer
 * @param from Start of the line segment
 * @param height
 * @param colorIndex Color index from current palette
 */
void DrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex);

/** Same as DrawVerticalLine but without bounds clipping. */
void UnsafeDrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex);

void DrawHalfTransparentHorizontalLine(const Surface &out, Point from, int width, uint8_t colorIndex);
void DrawHalfTransparentVerticalLine(const Surface &out, Point from, int width, uint8_t colorIndex);

/**
 * Draws a half-transparent rectangle by palette blending with black.
 *
 * @brief Render a transparent black rectangle
 * @param out Target buffer
 * @param sx Screen coordinate
 * @param sy Screen coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 */
void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height);

void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height, uint8_t color);

/**
 * Draws a half-transparent pixel
 *
 * @brief Render a transparent pixel
 * @param out Target buffer
 * @param position Screen coordinates
 * @param col Pixel color
 */
void SetHalfTransparentPixel(const Surface &out, Point position, uint8_t color);

/**
 * Draws a 2px inset border.
 *
 * @param out Target buffer
 * @param rect The rectangle that border pixels are rendered inside of.
 * @param color Border color.
 */
void UnsafeDrawBorder2px(const Surface &out, Rectangle rect, uint8_t color);

} // namespace devilution
