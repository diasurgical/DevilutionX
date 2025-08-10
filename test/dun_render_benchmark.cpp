#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>

#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/displacement.hpp"
#include "engine/lighting_defs.hpp"
#include "engine/load_file.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/surface.hpp"
#include "levels/dun_tile.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "options.h"
#include "utils/log.hpp"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

SDLSurfaceUniquePtr SdlSurface;
ankerl::unordered_dense::map<TileType, std::vector<LevelCelBlock>> Tiles;
std::unique_ptr<std::byte[]> BmDunCelData;
uint_fast8_t BmMicroTileLen;

void InitOnce()
{
	[[maybe_unused]] static const bool GlobalInitDone = []() {
		LoadCoreArchives();
		LoadGameArchives();
		if (!HaveMainData()) {
			LogError("This benchmark needs spawn.mpq or diabdat.mpq");
			exit(1);
		}

		leveltype = DTYPE_CATHEDRAL;
		BmDunCelData = LoadFileInMem("levels\\l1data\\l1.cel");
		SetDungeonMicros(BmDunCelData, BmMicroTileLen);
		MakeLightTable();

		SdlSurface = SDLWrap::CreateRGBSurfaceWithFormat(
		    /*flags=*/0, /*width=*/640, /*height=*/480, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
		if (SdlSurface == nullptr) {
			LogError("Failed to create SDL Surface: {}", SDL_GetError());
			exit(1);
		}

		for (size_t i = 0; i < 700; ++i) {
			for (size_t j = 0; j < 10; ++j) {
				if (const LevelCelBlock levelCelBlock = DPieceMicros[i].mt[j]; levelCelBlock.hasValue()) {
					if ((j == 0 || j == 1) && levelCelBlock.type() == TileType::TransparentSquare) {
						// This could actually be re-encoded foliage, which is a triangle followed by TransparentSquare.
						// Simply skip it.
						continue;
					}
					Tiles[levelCelBlock.type()].push_back(levelCelBlock);
				}
			}
		}
		GetOptions().Graphics.perPixelLighting.SetValue(false);
		return true;
	}();
}

void RunForTileMaskLight(benchmark::State &state, TileType tileType, MaskType maskType, const uint8_t *lightTable)
{
	Surface out = Surface(SdlSurface.get());
	std::array<std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables;
	Lightmap lightmap(/*outBuffer=*/nullptr, /*lightmapBuffer=*/ {}, /*pitch=*/1, lightTables, FullyLitLightTable, FullyDarkLightTable);
	const std::span<const LevelCelBlock> tiles = Tiles[tileType];
	for (auto _ : state) {
		for (const LevelCelBlock &levelCelBlock : tiles) {
			RenderTile(out, lightmap, Point { 320, 240 }, BmDunCelData.get(), levelCelBlock, maskType, lightTable);
			uint8_t color = out[Point { 310, 200 }];
			benchmark::DoNotOptimize(color);
		}
	}
	state.SetItemsProcessed(state.iterations() * tiles.size());
}

using GetLightTableFn = const uint8_t *();

const uint8_t *FullyLit() { return LightTables[0].data(); }
const uint8_t *FullyDark() { return LightTables.back().data(); }
const uint8_t *PartiallyLit() { return LightTables[5].data(); }

template <TileType TileT, MaskType MaskT, GetLightTableFn GetLightTableFnT>
void Render(benchmark::State &state)
{
	InitOnce();
	RunForTileMaskLight(state, TileT, MaskT, GetLightTableFnT());
}

// Define aliases in order to have shorter benchmark names.
constexpr auto LeftTriangle = TileType::LeftTriangle;
constexpr auto RightTriangle = TileType::RightTriangle;
constexpr auto TransparentSquare = TileType::TransparentSquare;
constexpr auto Square = TileType::Square;
constexpr auto LeftTrapezoid = TileType::LeftTrapezoid;
constexpr auto RightTrapezoid = TileType::RightTrapezoid;
constexpr auto Transparent = MaskType::Transparent;
constexpr auto Solid = MaskType::Solid;

#define DEFINE_FOR_TILE_AND_MASK_TYPE(TILE_TYPE, MASK_TYPE)      \
	BENCHMARK_TEMPLATE(Render, TILE_TYPE, MASK_TYPE, FullyLit);  \
	BENCHMARK_TEMPLATE(Render, TILE_TYPE, MASK_TYPE, FullyDark); \
	BENCHMARK_TEMPLATE(Render, TILE_TYPE, MASK_TYPE, PartiallyLit);

#define DEFINE_FOR_TILE_TYPE(TILE_TYPE)             \
	DEFINE_FOR_TILE_AND_MASK_TYPE(TILE_TYPE, Solid) \
	DEFINE_FOR_TILE_AND_MASK_TYPE(TILE_TYPE, Transparent)

DEFINE_FOR_TILE_TYPE(LeftTriangle)
DEFINE_FOR_TILE_TYPE(RightTriangle)
DEFINE_FOR_TILE_TYPE(TransparentSquare)
DEFINE_FOR_TILE_TYPE(Square)
DEFINE_FOR_TILE_TYPE(LeftTrapezoid)
DEFINE_FOR_TILE_TYPE(RightTrapezoid)

void BM_RenderBlackTile(benchmark::State &state)
{
	InitOnce();
	Surface out = Surface(SdlSurface.get());
	for (auto _ : state) {
		world_draw_black_tile(out, 320, 240);
		uint8_t color = out[Point { 310, 200 }];
		benchmark::DoNotOptimize(color);
	}
	state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RenderBlackTile);

} // namespace
} // namespace devilution
