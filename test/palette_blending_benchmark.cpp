#include "utils/palette_blending.hpp"

#include <SDL.h>
#include <benchmark/benchmark.h>

namespace devilution {
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
	SDL_Color palette[256];
	GeneratePalette(palette);
	for (auto _ : state) {
		GenerateBlendedLookupTable(palette, /*skipFrom=*/-1, /*skipTo=*/-1);
		int result = paletteTransparencyLookup[17][98];
		benchmark::DoNotOptimize(result);
	}
}

BENCHMARK(BM_GenerateBlendedLookupTable);

} // namespace
} // namespace devilution
