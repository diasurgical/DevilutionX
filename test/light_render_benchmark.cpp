#include <array>
#include <cstddef>
#include <cstdio>

#include <benchmark/benchmark.h>

#include "engine/lighting_defs.hpp"
#include "engine/render/light_render.hpp"
#include "engine/surface.hpp"
#include "levels/gendung_defs.hpp"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

void BM_BuildLightmap(benchmark::State &state)
{
	const std::string benchmarkDataPath = paths::BasePath() + "test/fixtures/light_render_benchmark/dLight.dmp";
	FILE *lightFile = std::fopen(benchmarkDataPath.c_str(), "rb");
	uint8_t dLight[MAXDUNX][MAXDUNY];
	std::array<std::array<uint8_t, LightTableSize>, NumLightingLevels> lightTables;
	if (lightFile != nullptr) {
		if (std::fread(&dLight[0][0], sizeof(uint8_t) * MAXDUNX * MAXDUNY, 1, lightFile) != 1) {
			std::perror("Failed to read dLight.dmp");
			exit(1);
		}
		std::fclose(lightFile);
	}

	SDLSurfaceUniquePtr sdl_surface = SDLWrap::CreateRGBSurfaceWithFormat(
	    /*flags=*/0, /*width=*/640, /*height=*/480, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	if (sdl_surface == nullptr) {
		std::fprintf(stderr, "Failed to create SDL Surface: %s\n", SDL_GetError());
		exit(1);
	}
	Surface out = Surface(sdl_surface.get());

	const Point tilePosition { 48, 44 };
	const Point targetBufferPosition { 0, -17 };
	const int viewportWidth = 640;
	const int viewportHeight = 352;
	const int rows = 25;
	const int columns = 10;
	const uint8_t *outBuffer = out.at(0, 0);
	const uint16_t outPitch = out.pitch();

	for (auto _ : state) {
		Lightmap lightmap = Lightmap::build(/*perPixelLighting=*/true,
		    tilePosition, targetBufferPosition,
		    viewportWidth, viewportHeight, rows, columns,
		    outBuffer, outPitch, lightTables, lightTables[0].data(), lightTables.back().data(),
		    dLight, /*microTileLen=*/10);

		uint8_t lightLevel = *lightmap.getLightingAt(outBuffer + outPitch * 120 + 120);
		benchmark::DoNotOptimize(lightLevel);
	}
	state.SetBytesProcessed(state.iterations() * viewportWidth * viewportHeight);
	state.SetItemsProcessed(state.iterations() * rows * columns);
}

BENCHMARK(BM_BuildLightmap);

} // namespace
} // namespace devilution
