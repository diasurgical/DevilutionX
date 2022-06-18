#pragma once

#include "engine/point.hpp"
#include "engine/size.hpp"

namespace devilution {

struct Rectangle {
	Point position;
	Size size;

	Rectangle() = default;

	constexpr Rectangle(Point position, Size size)
	    : position(position)
	    , size(size)
	{
	}

	/**
	 * @brief Constructs a rectangle centered on the given point and including all tiles within the given radius.
	 *
	 * The resulting rectangle will be square with an odd size equal to 2*radius + 1.
	 *
	 * @param center center point of the target rectangle
	 * @param radius a non-negative value indicating how many tiles to include around the center
	 */
	explicit constexpr Rectangle(Point center, int radius)
	    : position(center - Displacement { radius })
	    , size(2 * radius + 1)
	{
	}

	constexpr bool Contains(Point point) const
	{
		return point.x >= this->position.x
		    && point.x < (this->position.x + this->size.width)
		    && point.y >= this->position.y
		    && point.y < (this->position.y + this->size.height);
	}

	/**
	 * @brief Computes the center of this rectangle in integer coordinates. Values are truncated towards zero.
	 */
	constexpr Point center() const
	{
		return position + Displacement(size / 2);
	}
};

} // namespace devilution
