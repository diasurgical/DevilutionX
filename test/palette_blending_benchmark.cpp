#include "utils/palette_blending.hpp"

#include <array>
#include <cstdint>

#include <SDL.h>
#include <benchmark/benchmark.h>

#include "utils/palette_kd_tree.hpp"

namespace devilution {

std::array<SDL_Color, 256> logical_palette;

namespace {

void GeneratePalette(SDL_Color palette[256])
{
	for (unsigned j = 0; j < 4; ++j) {
		for (unsigned i = 0; i < 64; ++i) {
			palette[j * 64 + i].r = i * std::max(j, 1U);
			palette[j * 64 + i].g = i * j;
			palette[j * 64 + i].b = i * 2;
#ifndef USE_SDL1
			palette[j * 64 + i].a = SDL_ALPHA_OPAQUE;
#endif
		}
	}
}

void BM_GenerateBlendedLookupTable(benchmark::State &state)
{
	SDL_Color *palette = logical_palette.data();
	GeneratePalette(palette);
	for (auto _ : state) {
		GenerateBlendedLookupTable();
		int result = paletteTransparencyLookup[17][98];
		benchmark::DoNotOptimize(result);
	}
}

void BM_BuildTree(benchmark::State &state)
{
	SDL_Color *palette = logical_palette.data();
	GeneratePalette(palette);

	for (auto _ : state) {
		PaletteKdTree tree(palette, -1, -1);
		benchmark::DoNotOptimize(tree);
	}
}

void BM_FindNearestNeighbor(benchmark::State &state)
{
	SDL_Color *palette = logical_palette.data();
	GeneratePalette(palette);
	PaletteKdTree tree(palette, -1, -1);

	std::vector<std::array<uint8_t, 3>> queries;
	int64_t itemsProcessed = 0;
	for (auto _ : state) {
		for (int r = 0; r < 256; ++r) {
			for (int g = 0; g < 256; ++g) {
				for (int b = 0; b < 256; ++b) {
					uint8_t result = tree.findNearestNeighbor({ static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b) });
					benchmark::DoNotOptimize(result);
				}
			}
		}
		itemsProcessed += 256 * 256 * 256;
	}
	state.SetItemsProcessed(itemsProcessed);
}

BENCHMARK(BM_GenerateBlendedLookupTable);
BENCHMARK(BM_BuildTree);
BENCHMARK(BM_FindNearestNeighbor);

} // namespace
} // namespace devilution
