#include <cstddef>

#include <benchmark/benchmark.h>

#include "engine/clx_sprite.hpp"
#include "engine/displacement.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/log.hpp"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

void BM_RenderSmallClx(benchmark::State &state)
{
	const SDLSurfaceUniquePtr sdl_surface = SDLWrap::CreateRGBSurfaceWithFormat(
	    /*flags=*/0, /*width=*/640, /*height=*/480, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	if (sdl_surface == nullptr) {
		LogError("Failed to create SDL Surface: {}", SDL_GetError());
		exit(1);
	}
	const Surface out = Surface(sdl_surface.get());
	const OwnedClxSpriteList sprites = LoadClx("data\\resistance.clx");

	const size_t numSprites = sprites.numSprites();
	for (auto _ : state) {
		for (size_t i = 0; i < numSprites; ++i) {
			RenderClxSprite(out, sprites[i], Point { static_cast<int>(i * 100), static_cast<int>(i * 60) });
		}
		uint8_t color = out[Point { 120, 120 }];
		benchmark::DoNotOptimize(color);
	}
	state.SetBytesProcessed(state.iterations() * sprites.dataSize());
	state.SetItemsProcessed(state.iterations() * numSprites);
}

void BM_RenderLargeClx(benchmark::State &state)
{
	const SDLSurfaceUniquePtr sdl_surface = SDLWrap::CreateRGBSurfaceWithFormat(
	    /*flags=*/0, /*width=*/640, /*height=*/480, /*depth=*/8, SDL_PIXELFORMAT_INDEX8);
	if (sdl_surface == nullptr) {
		LogError("Failed to create SDL Surface: {}", SDL_GetError());
		exit(1);
	}
	const Surface out = Surface(sdl_surface.get());
	const OwnedClxSpriteList sprites = LoadClx("ui_art\\dvl_lrpopup.clx");

	for (auto _ : state) {
		RenderClxSprite(out, sprites[0], Point { 100, 100 });
		uint8_t color = out[Point { 120, 120 }];
		benchmark::DoNotOptimize(color);
	}
	state.SetBytesProcessed(state.iterations() * sprites.dataSize());
	state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_RenderSmallClx);
BENCHMARK(BM_RenderLargeClx);

} // namespace
} // namespace devilution
