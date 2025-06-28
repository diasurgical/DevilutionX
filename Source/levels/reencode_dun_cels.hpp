#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "levels/dun_tile.hpp"

namespace devilution {

struct DunFrameInfo {
	// Only floor tiles have this.
	uint8_t microTileIndex;

	TileType type;
	TileProperties properties;

	[[nodiscard]] bool isFloor() const
	{
		// The BlockMissile check is for stairs in L3 and L4, e.g. tile 46 sub-tile 141 frame 386 in L4.
		return !HasAnyOf(properties, TileProperties::Solid | TileProperties::BlockMissile)
		    && (microTileIndex == 0 || microTileIndex == 1);
	}
	[[nodiscard]] bool isFloorLeft() const { return microTileIndex == 0; }
};

/**
 * @brief Re-encodes dungeon cels.
 *
 * 1. Removing redundant padding bytes from triangles and trapezoids.
 * 2. Extracts floor tile foliage into a triangle with the floor frame and a separate 16-px tall `TransparentSquare`.
 *
 * This reduces memory usage and simplifies the rendering.
 */
void ReencodeDungeonCels(std::unique_ptr<std::byte[]> &dungeonCels, std::span<std::pair<uint16_t, DunFrameInfo>> frames);

/**
 * @brief Computes adjustments to apply to frame indexes in cel block data.
 *
 * Re-encoding the dungeon cels removes frames that are not referenced.
 * Indexes must also be adjusted to avoid errors when doing lookups.
 */
std::vector<std::pair<uint16_t, uint16_t>> ComputeCelBlockAdjustments(std::span<std::pair<uint16_t, DunFrameInfo>> frames);

} // namespace devilution
