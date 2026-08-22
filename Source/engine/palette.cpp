/**
 * @file palette.cpp
 *
 * Implementation of functions for handling the engines color palette.
 */
#include "engine/palette.h"

#include <algorithm>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "appfat.h"
#include "engine/backbuffer_state.hpp"
#include "engine/demomode.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/render/renderer.h"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "utils/display.h"
#include "utils/palette_blending.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"
#include "utils/str_cat.hpp"

namespace devilution {

std::array<SDL_Color, 256> logical_palette;
std::array<SDL_Color, 256> system_palette;

namespace {

/** Specifies whether the palette has max brightness. */
bool sgbFadedIn = true;

void LoadBrightness()
{
	int brightnessValue = *GetOptions().Graphics.brightness;
	brightnessValue = std::clamp(brightnessValue, 0, 100);
	GetOptions().Graphics.brightness.SetValue(brightnessValue - (brightnessValue % 5));
}

/**
 * @brief Cycle the given range of colors in the palette
 * @param from First color index of the range
 * @param to First color index of the range
 */
void CycleColors(int from, int to)
{
	std::rotate(logical_palette.begin() + from, logical_palette.begin() + from + 1, logical_palette.begin() + to + 1);
	std::rotate(system_palette.begin() + from, system_palette.begin() + from + 1, system_palette.begin() + to + 1);

	for (auto &palette : paletteTransparencyLookup) {
		std::rotate(std::begin(palette) + from, std::begin(palette) + from + 1, std::begin(palette) + to + 1);
	}

	std::rotate(&paletteTransparencyLookup[from][0], &paletteTransparencyLookup[from + 1][0], &paletteTransparencyLookup[to + 1][0]);

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	UpdateTransparencyLookupBlack16(from, to);
#endif
}

/**
 * @brief Cycle the given range of colors in the palette in reverse direction
 * @param from First color index of the range
 * @param to Last color index of the range
 */
void CycleColorsReverse(int from, int to)
{
	std::rotate(logical_palette.begin() + from, logical_palette.begin() + to, logical_palette.begin() + to + 1);
	std::rotate(system_palette.begin() + from, system_palette.begin() + to, system_palette.begin() + to + 1);

	for (auto &palette : paletteTransparencyLookup) {
		std::rotate(std::begin(palette) + from, std::begin(palette) + to, std::begin(palette) + to + 1);
	}

	std::rotate(&paletteTransparencyLookup[from][0], &paletteTransparencyLookup[to][0], &paletteTransparencyLookup[to + 1][0]);

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	UpdateTransparencyLookupBlack16(from, to);
#endif
}

// When brightness==0, then a==0 (identity mapping)
// When brightness==100, then a==-MaxAdjustment (maximum brightening)
constexpr float CalculateToneMappingParameter(int brightness)
{
	// Maximum adjustment factor (tweak this constant to change the effect strength)
	constexpr float MaxAdjustment = 2.0F;
	return -(brightness / 100.0F) * MaxAdjustment;
}

constexpr uint8_t MapTone(float a, uint8_t color)
{
	const auto x = static_cast<float>(color / 255.0F);
	// Our quadratic tone mapping: f(x) = a*x^2 + (1-a)*x.
	const float y = std::clamp((a * x * x) + ((1.0F - a) * x), 0.0F, 1.0F);
	return static_cast<uint8_t>((y * 255.0F) + 0.5F);
}

void ApplyGlobalBrightnessSingleColor(SDL_Color &dst, const SDL_Color &src)
{
	const float a = CalculateToneMappingParameter(*GetOptions().Graphics.brightness);
	dst.r = MapTone(a, src.r);
	dst.g = MapTone(a, src.g);
	dst.b = MapTone(a, src.b);
}

} // namespace

void ApplyGlobalBrightness(SDL_Color *dst, const SDL_Color *src)
{
	// Get the brightness slider value (0 = neutral, 100 = max brightening)
	const int brightnessSlider = *GetOptions().Graphics.brightness;

	// Precompute a lookup table for speed.
	const float a = CalculateToneMappingParameter(brightnessSlider);
	uint8_t toneMap[256];
	for (int i = 0; i < 256; i++) {
		toneMap[i] = MapTone(a, i);
	}

	// Apply the lookup table to each color channel in the palette.
	for (int i = 0; i < 256; i++) {
		dst[i].r = toneMap[src[i].r];
		dst[i].g = toneMap[src[i].g];
		dst[i].b = toneMap[src[i].b];
	}
}

void ApplyFadeLevel(unsigned fadeval, SDL_Color *dst, const SDL_Color *src)
{
	for (int i = 0; i < 256; i++) {
		dst[i].r = (fadeval * src[i].r) / 256;
		dst[i].g = (fadeval * src[i].g) / 256;
		dst[i].b = (fadeval * src[i].b) / 256;
	}
}

// Applies a tone mapping curve based on the brightness slider value.
// The brightness value is in the range [0, 100] where 0 is neutral (no change)
// and 100 produces maximum brightening.
void UpdateSystemPalette(std::span<const SDL_Color, 256> src)
{
	ApplyGlobalBrightness(system_palette.data(), src.data());
	SystemPaletteUpdated();
	RedrawEverything();
}

void SystemPaletteUpdated(int first, int ncolor)
{
	if (HeadlessMode)
		return;

	GetRenderer().NotifyPaletteChanged(first, ncolor);
}

void palette_init()
{
	LoadBrightness();
}

void LoadPalette(const char *path)
{
	assert(path != nullptr);
	if (HeadlessMode) return;

	LogVerbose("Loading palette from {}", path);
	std::array<Color, 256> palData;
	LoadFileInMem(path, palData);
	for (unsigned i = 0; i < palData.size(); i++) {
		logical_palette[i] = palData[i].toSDL();
	}

	GetRenderer().ClearTextureCache();
}

void LoadPaletteAndInitBlending(const char *path)
{
	assert(path != nullptr);
	if (HeadlessMode) return;
	LoadPalette(path);
	if (leveltype == DTYPE_CAVES || leveltype == DTYPE_CRYPT) {
		GenerateBlendedLookupTable(logical_palette.data(), /*skipFrom=*/1, /*skipTo=*/31);
	} else if (leveltype == DTYPE_NEST) {
		GenerateBlendedLookupTable(logical_palette.data(), /*skipFrom=*/1, /*skipTo=*/15);
	} else {
		GenerateBlendedLookupTable(logical_palette.data());
	}
}

void LoadRndLvlPal(dungeon_type l)
{
	if (HeadlessMode)
		return;

	if (l == DTYPE_TOWN) {
		LoadPaletteAndInitBlending("levels\\towndata\\town.pal");
		return;
	}

	if (l == DTYPE_CRYPT) {
		LoadPaletteAndInitBlending("nlevels\\l5data\\l5base.pal");
		return;
	}

	int rv = RandomIntBetween(1, 4);
	char szFileName[27];
	if (l == DTYPE_NEST) {
		if (!*GetOptions().Graphics.alternateNestArt) {
			rv++;
		}
		*BufCopy(szFileName, R"(nlevels\l6data\l6base)", rv, ".pal") = '\0';
	} else {
		char nbuf[3];
		const char *end = BufCopy(nbuf, static_cast<int>(l));
		const std::string_view n = std::string_view(nbuf, end - nbuf);
		*BufCopy(szFileName, "levels\\l", n, "data\\l", n, "_", rv, ".pal") = '\0';
	}
	LoadPaletteAndInitBlending(szFileName);
}

void IncreaseBrightness()
{
	const int brightnessValue = *GetOptions().Graphics.brightness;
	if (brightnessValue < 100) {
		const int newBrightness = std::min(brightnessValue + 5, 100);
		GetOptions().Graphics.brightness.SetValue(newBrightness);
		UpdateSystemPalette(logical_palette);
	}
}

void DecreaseBrightness()
{
	const int brightnessValue = *GetOptions().Graphics.brightness;
	if (brightnessValue > 0) {
		const int newBrightness = std::max(brightnessValue - 5, 0);
		GetOptions().Graphics.brightness.SetValue(newBrightness);
		UpdateSystemPalette(logical_palette);
	}
}

int UpdateBrightness(int brightness)
{
	if (brightness >= 0) {
		GetOptions().Graphics.brightness.SetValue(brightness);
		UpdateSystemPalette(logical_palette);
	}

	return *GetOptions().Graphics.brightness;
}

void BlackPalette()
{
	for (SDL_Color &c : system_palette) {
		c.r = c.g = c.b = 0;
	}
	SystemPaletteUpdated();
}

void PaletteFadeIn(int fr, const std::array<SDL_Color, 256> &srcPalette, std::function<void()> redraw)
{
	if (HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	std::array<SDL_Color, 256> palette;

#ifndef USE_SDL1
	for (SDL_Color &color : palette) {
		color.a = SDL_ALPHA_OPAQUE;
	}
#endif

	ApplyGlobalBrightness(palette.data(), srcPalette.data());

	// Set the final palette so the scene is rendered at full brightness.
	// The renderer's overlay provides the black-to-visible transition.
	system_palette = palette;
	SystemPaletteUpdated();
	RedrawEverything();
	if (IsHardwareCursor()) ReinitializeHardwareCursor();

	GetRenderer().StartFadeIn(fr);
	while (GetRenderer().IsFading()) {
		if (redraw)
			redraw();
		GetRenderer().EndFrame();
	}

	sgbFadedIn = true;
}

void PaletteFadeOut(int fr, const std::array<SDL_Color, 256> &srcPalette, std::function<void()> redraw)
{
	if (!sgbFadedIn || HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	GetRenderer().StartFadeOut(fr);
	while (GetRenderer().IsFading()) {
		if (redraw)
			redraw();
		GetRenderer().EndFrame();
	}

	// Overlay is now fully opaque (BlackOutScreen equivalent).
	BlackPalette();
	if (IsHardwareCursor()) ReinitializeHardwareCursor();

	sgbFadedIn = false;
}

void palette_update_caves()
{
	CycleColors(1, 31);
	SystemPaletteUpdated(1, 31);
}

/**
 * @brief Cycle the lava every other frame, and glow every frame
 * Lava has 15 colors and the glow 16, so the full animation has 240 frames before it loops
 */
void palette_update_crypt()
{
	static bool delayLava = false;

	if (!delayLava) {
		CycleColorsReverse(1, 15);
		delayLava = false;
	}

	CycleColorsReverse(16, 31);
	SystemPaletteUpdated(1, 31);
	delayLava = !delayLava;
}

/**
 * @brief Cycle the pond waves and bubles colors every 3rd frame
 * Bubles have 8 colors and waves 7, so the full animation has 56 frames before it loops
 */
void palette_update_hive()
{
	static uint8_t delay = 0;

	if (delay != 2) {
		delay++;
		return;
	}

	CycleColorsReverse(1, 8);
	CycleColorsReverse(9, 15);
	SystemPaletteUpdated(1, 15);
	delay = 0;
}

void SetLogicalPaletteColor(unsigned i, const SDL_Color &color)
{
	logical_palette[i] = color;
	ApplyGlobalBrightnessSingleColor(system_palette[i], logical_palette[i]);
	SystemPaletteUpdated(i, 1);
	UpdateBlendedLookupTableSingleColor(logical_palette.data(), i);
}

void ApplySystemPaletteToSurface(SDL_Surface *surface)
{
#ifdef USE_SDL1
	// On SDL1, surfaces have their own palette in format->palette.
	// SDLC_SetSurfaceAndPaletteColors sets colors via SDL_SetPalette directly.
	if (!SDLC_SetSurfaceAndPaletteColors(surface, surface->format->palette, system_palette.data(), 0, 256)) {
		ErrSdl();
	}
#else
	SDLPaletteUniquePtr palette = SDLWrap::AllocPalette();
	SDL_SetPaletteColors(palette.get(), system_palette.data(), 0, 256);
	if (!SDLC_SetSurfacePalette(surface, palette.get())) {
		ErrSdl();
	}
#endif
}

} // namespace devilution
