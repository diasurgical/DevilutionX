/**
 * @file dun_render.hpp
 *
 * Interface of functionality for rendering the level tiles.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "levels/dun_tile.hpp"
#include "utils/attributes.h"
#include "utils/endian_swap.hpp"

// #define DUN_RENDER_STATS
#ifdef DUN_RENDER_STATS
#include <ankerl/unordered_dense.h>
#endif

namespace devilution {

/**
 * @brief Specifies the mask to use for rendering.
 */
enum class MaskType : uint8_t {
	/** @brief The entire tile is opaque. */
	Solid,

	/** @brief The entire tile is blended with transparency. */
	Transparent,

	/**
	 * @brief Upper-right triangle is blended with transparency.
	 *
	 * Can only be used with `TileType::LeftTrapezoid` and
	 * `TileType::TransparentSquare`.
	 *
	 * The lower 16 rows are opaque.
	 * The upper 16 rows are arranged like this (🮆 = opaque, 🮐 = blended):
	 *
	 * 🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮐🮐
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 */
	Right,

	/**
	 * @brief Upper-left triangle is blended with transparency.
	 *
	 * Can only be used with `TileType::RightTrapezoid` and
	 * `TileType::TransparentSquare`.
	 *
	 * The lower 16 rows are opaque.
	 * The upper 16 rows are arranged like this (🮆 = opaque, 🮐 = blended):
	 *
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮐🮐🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 * 🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆🮆
	 */
	Left,
};

#ifdef DUN_RENDER_STATS
struct DunRenderType {
	TileType tileType;
	MaskType maskType;
	bool operator==(const DunRenderType &other) const
	{
		return tileType == other.tileType && maskType == other.maskType;
	}
};
struct DunRenderTypeHash {
	size_t operator()(DunRenderType t) const noexcept
	{
		return std::hash<uint32_t> {}((1 < static_cast<uint8_t>(t.tileType)) | static_cast<uint8_t>(t.maskType));
	}
};
extern ankerl::unordered_dense::map<DunRenderType, size_t, DunRenderTypeHash> DunRenderStats;

std::string_view TileTypeToString(TileType tileType);

std::string_view MaskTypeToString(MaskType maskType);
#endif

/**
 * @brief Returns the raw data for the given dungeon frame.
 */
DVL_ALWAYS_INLINE const uint8_t *GetDunFrame(const std::byte *dungeonCelData, uint32_t frame)
{
	const auto *frameTable = reinterpret_cast<const uint32_t *>(dungeonCelData);
	return reinterpret_cast<const uint8_t *>(&dungeonCelData[Swap32LE(frameTable[frame])]);
}

/**
 * @brief Returns the raw data for the given dungeon frame's foliage.
 */
DVL_ALWAYS_INLINE const uint8_t *GetDunFrameFoliage(const std::byte *dungeonCelData, uint32_t frame)
{
	return GetDunFrame(dungeonCelData, frame) + ReencodedTriangleFrameSize;
}

} // namespace devilution
