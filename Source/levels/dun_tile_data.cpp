#include "levels/dun_tile_data.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "engine/world_tile.hpp"
#include "levels/dun_tile.hpp"
#include "levels/gendung_defs.hpp"
#include "levels/reencode_dun_cels.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/bitset2d.hpp"
#include "utils/endian_swap.hpp"
#include "utils/status_macros.hpp"

namespace devilution {

uint8_t currlevel;
bool setlevel;

Bitset2d<DMAXX, DMAXY> DungeonMask;
uint8_t dungeon[DMAXX][DMAXY];
uint8_t pdungeon[DMAXX][DMAXY];
Bitset2d<DMAXX, DMAXY> Protected;
WorldTileRectangle SetPieceRoom;
WorldTileRectangle SetPiece;
OptionalOwnedClxSpriteList pSpecialCels;
std::unique_ptr<MegaTile[]> pMegaTiles;
std::unique_ptr<std::byte[]> pDungeonCels;
TileProperties SOLData[MAXTILES];
WorldTilePosition dminPosition;
WorldTilePosition dmaxPosition;
dungeon_type leveltype;
_setlevels setlvlnum;
dungeon_type setlvltype;
Point ViewPosition;
uint_fast8_t MicroTileLen;
int8_t TransVal;
std::array<bool, 256> TransList;
uint16_t dPiece[MAXDUNX][MAXDUNY];
MICROS DPieceMicros[MAXTILES];
int8_t dTransVal[MAXDUNX][MAXDUNY];
uint8_t dLight[MAXDUNX][MAXDUNY];
uint8_t dPreLight[MAXDUNX][MAXDUNY];
DungeonFlag dFlags[MAXDUNX][MAXDUNY];
int8_t dPlayer[MAXDUNX][MAXDUNY];
int16_t dMonster[MAXDUNX][MAXDUNY];
int8_t dCorpse[MAXDUNX][MAXDUNY];
int8_t dObject[MAXDUNX][MAXDUNY];
int8_t dSpecial[MAXDUNX][MAXDUNY];
int themeCount;
THEME_LOC themeLoc[MAXTHEMES];
uint32_t DungeonSeeds[NUMLEVELS];
std::optional<uint32_t> LevelSeeds[NUMLEVELS];

namespace {

std::unique_ptr<uint16_t[]> LoadMinData(size_t &tileCount)
{
	switch (leveltype) {
	case DTYPE_TOWN: {
		auto min = LoadFileInMemWithStatus<uint16_t>("nlevels\\towndata\\town.min", &tileCount);
		if (!min.has_value()) {
			return LoadFileInMem<uint16_t>("levels\\towndata\\town.min", &tileCount);
		} else {
			return std::move(*min);
		}
	}
	case DTYPE_CATHEDRAL:
		return LoadFileInMem<uint16_t>("levels\\l1data\\l1.min", &tileCount);
	case DTYPE_CATACOMBS:
		return LoadFileInMem<uint16_t>("levels\\l2data\\l2.min", &tileCount);
	case DTYPE_CAVES:
		return LoadFileInMem<uint16_t>("levels\\l3data\\l3.min", &tileCount);
	case DTYPE_HELL:
		return LoadFileInMem<uint16_t>("levels\\l4data\\l4.min", &tileCount);
	case DTYPE_NEST:
		return LoadFileInMem<uint16_t>("nlevels\\l6data\\l6.min", &tileCount);
	case DTYPE_CRYPT:
		return LoadFileInMem<uint16_t>("nlevels\\l5data\\l5.min", &tileCount);
	default:
		app_fatal("LoadMinData");
	}
}

} // namespace

tl::expected<void, std::string> LoadLevelSOLData()
{
	switch (leveltype) {
	case DTYPE_TOWN:
		if (!LoadIntegralFileInMemWithStatus("nlevels\\towndata\\town.sol", SOLData).has_value()) {
			RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("levels\\towndata\\town.sol", SOLData));
		}
		break;
	case DTYPE_CATHEDRAL:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("levels\\l1data\\l1.sol", SOLData));
		// Fix incorrectly marked arched tiles
		SOLData[9] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[15] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[16] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[20] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[21] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[27] |= TileProperties::BlockMissile;
		SOLData[28] |= TileProperties::BlockMissile;
		SOLData[51] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[56] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[58] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[61] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[63] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[65] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[72] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[208] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[247] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[253] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[257] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[323] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		SOLData[403] |= TileProperties::BlockLight;
		// Fix incorrectly marked pillar tile
		SOLData[24] |= TileProperties::BlockLight;
		// Fix incorrectly marked wall tile
		SOLData[450] |= TileProperties::BlockLight | TileProperties::BlockMissile;
		break;
	case DTYPE_CATACOMBS:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("levels\\l2data\\l2.sol", SOLData));
		break;
	case DTYPE_CAVES:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("levels\\l3data\\l3.sol", SOLData));
		// The graphics for tile 48 sub-tile 171 frame 461 are partly incorrect, as they
		// have a few pixels that should belong to the solid tile 49 instead.
		// Marks the sub-tile as "BlockMissile" to avoid treating it as a floor during rendering.
		SOLData[170] |= TileProperties::BlockMissile;
		// Fence sub-tiles 481 and 487 are substitutes for solid sub-tiles 473 and 479
		// but are not marked as solid.
		SOLData[481] |= TileProperties::Solid;
		SOLData[487] |= TileProperties::Solid;
		break;
	case DTYPE_HELL:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("levels\\l4data\\l4.sol", SOLData));
		SOLData[210] = TileProperties::None; // Tile is incorrectly marked as being solid
		break;
	case DTYPE_NEST:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("nlevels\\l6data\\l6.sol", SOLData));
		break;
	case DTYPE_CRYPT:
		RETURN_IF_ERROR(LoadIntegralFileInMemWithStatus("nlevels\\l5data\\l5.sol", SOLData));
		SOLData[142] = TileProperties::None; // Tile is incorrectly marked as being solid
		break;
	default:
		return tl::make_unexpected("LoadLevelSOLData");
	}
	return {};
}

void SetDungeonMicros(std::unique_ptr<std::byte[]> &dungeonCels, uint_fast8_t &microTileLen)
{
	microTileLen = 10;
	size_t blocks = 10;

	if (leveltype == DTYPE_TOWN) {
		microTileLen = 16;
		blocks = 16;
	} else if (leveltype == DTYPE_HELL) {
		microTileLen = 12;
		blocks = 16;
	}

	size_t tileCount;
	const std::unique_ptr<uint16_t[]> levelPieces = LoadMinData(tileCount);

	ankerl::unordered_dense::map<uint16_t, DunFrameInfo> frameToTypeMap;
	frameToTypeMap.reserve(4096);
	for (size_t levelPieceId = 0; levelPieceId < tileCount / blocks; levelPieceId++) {
		uint16_t *pieces = &levelPieces[blocks * levelPieceId];
		for (uint32_t block = 0; block < blocks; block++) {
			const LevelCelBlock levelCelBlock { Swap16LE(pieces[blocks - 2 + (block & 1) - (block & 0xE)]) };
			DPieceMicros[levelPieceId].mt[block] = levelCelBlock;
			if (levelCelBlock.hasValue()) {
				if (const auto it = frameToTypeMap.find(levelCelBlock.frame()); it == frameToTypeMap.end()) {
					frameToTypeMap.emplace_hint(it, levelCelBlock.frame(),
					    DunFrameInfo { static_cast<uint8_t>(block), levelCelBlock.type(), SOLData[levelPieceId] });
				}
			}
		}
	}
	std::vector<std::pair<uint16_t, DunFrameInfo>> frameToTypeList = std::move(frameToTypeMap).extract();
	c_sort(frameToTypeList, [](const std::pair<uint16_t, DunFrameInfo> &a, const std::pair<uint16_t, DunFrameInfo> &b) {
		return a.first < b.first;
	});
	ReencodeDungeonCels(dungeonCels, frameToTypeList);

	std::vector<std::pair<uint16_t, uint16_t>> celBlockAdjustments = ComputeCelBlockAdjustments(frameToTypeList);
	if (celBlockAdjustments.size() == 0) return;
	for (size_t levelPieceId = 0; levelPieceId < tileCount / blocks; levelPieceId++) {
		for (uint32_t block = 0; block < blocks; block++) {
			LevelCelBlock &levelCelBlock = DPieceMicros[levelPieceId].mt[block];
			const uint16_t frame = levelCelBlock.frame();
			const auto pair = std::make_pair(frame, frame);
			const auto it = std::upper_bound(celBlockAdjustments.begin(), celBlockAdjustments.end(), pair,
			    [](std::pair<uint16_t, uint16_t> p1, std::pair<uint16_t, uint16_t> p2) { return p1.first < p2.first; });
			if (it != celBlockAdjustments.end()) {
				levelCelBlock.data -= it->second;
			}
		}
	}
}

} // namespace devilution
