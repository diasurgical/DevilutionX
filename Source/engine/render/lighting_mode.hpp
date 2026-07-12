#pragma once

#include <cstdint>

namespace devilution {

enum class LightingMode : uint8_t {
	Vertex,     // Per-tile flat vertex color (cheapest on hardware).
	Tile,       // Light-table-baked per-pixel lighting (palette-accurate).
	TileSmooth, // Per-pixel interpolated light table (enhanced lighting).
};

} // namespace devilution
