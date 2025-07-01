#include <cstddef>

#include <benchmark/benchmark.h>

#include "engine/render/light_render.hpp"
#include "engine/surface.hpp"
#include "lighting.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

void BM_BuildLightmap(benchmark::State &state)
{
	std::string benchmarkDataPath = paths::BasePath() + "test/fixtures/light_render_benchmark/dLight.dmp";
	FILE *lightFile = std::fopen(benchmarkDataPath.c_str(), "rb");
	if (lightFile != nullptr) {
		std::fread(&dLight[0][0], sizeof(uint8_t), MAXDUNX * MAXDUNY, lightFile);
		std::fclose(lightFile);
	}

	SDLSurfaceUniquePtr sdl_surface = SDLWrap::CreateRGBSurfaceWithFormat(
	    /*flags=*/0, /*width=*/640, /*height=*/480, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	if (sdl_surface == nullptr) {
		LogError("Failed to create SDL Surface: {}", SDL_GetError());
		exit(1);
	}
	Surface out = Surface(sdl_surface.get());

	Point tilePosition { 48, 44 };
	Point targetBufferPosition { 0, -17 };
	int viewportWidth = 640;
	int viewportHeight = 352;
	int rows = 25;
	int columns = 10;
	const uint8_t *outBuffer = out.at(0, 0);
	uint16_t outPitch = out.pitch();
	const uint8_t *lightTables = LightTables[0].data();
	size_t lightTableSize = LightTables[0].size();

	size_t numViewportTiles = rows * columns;
	size_t numPixels = viewportWidth * viewportHeight;
	size_t numBytesProcessed = 0;
	size_t numItemsProcessed = 0;
	for (auto _ : state) {
		Lightmap lightmap = Lightmap::build(tilePosition, targetBufferPosition,
		    viewportWidth, viewportHeight, rows, columns,
		    outBuffer, outPitch, lightTables, lightTableSize);

		uint8_t lightLevel = *lightmap.getLightingAt(outBuffer + outPitch * 120 + 120);
		benchmark::DoNotOptimize(lightLevel);
		numItemsProcessed += numViewportTiles;
		numBytesProcessed += numPixels;
	}
	state.SetBytesProcessed(numBytesProcessed);
	state.SetItemsProcessed(numItemsProcessed);
}

BENCHMARK(BM_BuildLightmap);

} // namespace
} // namespace devilution
