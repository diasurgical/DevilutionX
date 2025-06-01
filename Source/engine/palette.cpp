/**
 * @file palette.cpp
 *
 * Implementation of functions for handling the engines color palette.
 */
#include "engine/palette.h"

#include <cstdint>

#include <fmt/core.h>

#include "engine/backbuffer_state.hpp"
#include "engine/demomode.h"
#include "engine/dx.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "utils/display.h"
#include "utils/palette_blending.hpp"
#include "utils/sdl_compat.h"

namespace devilution {

std::array<SDL_Color, 256> logical_palette;
std::array<SDL_Color, 256> system_palette;
std::array<SDL_Color, 256> orig_palette;

namespace {

/** Specifies whether the palette has max brightness. */
bool sgbFadedIn = true;

void LoadBrightness()
{
	int brightnessValue = *GetOptions().Graphics.brightness;
	brightnessValue = std::clamp(brightnessValue, 0, 100);
	GetOptions().Graphics.brightness.SetValue(brightnessValue - brightnessValue % 5);
}

/**
 * @brief Generate lookup table for transparency
 *
 * This is based of the same technique found in Quake2.
 *
 * To mimic 50% transparency we figure out what colors in the existing palette are the best match for the combination of any 2 colors.
 * We save this into a lookup table for use during rendering.
 *
 * @param palette The colors to operate on
 * @param skipFrom Do not use colors between this index and skipTo
 * @param skipTo Do not use colors between skipFrom and this index
 * @param toUpdate Only update the first n colors
 */
/**
 * @brief Cycle the given range of colors in the palette
 * @param from First color index of the range
 * @param to First color index of the range
 */
void CycleColors(int from, int to)
{
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
	std::rotate(system_palette.begin() + from, system_palette.begin() + to, system_palette.begin() + to + 1);

	for (auto &palette : paletteTransparencyLookup) {
		std::rotate(std::begin(palette) + from, std::begin(palette) + to, std::begin(palette) + to + 1);
	}

	std::rotate(&paletteTransparencyLookup[from][0], &paletteTransparencyLookup[to][0], &paletteTransparencyLookup[to + 1][0]);

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	UpdateTransparencyLookupBlack16(from, to);
#endif
}

} // namespace

void palette_update(int first, int ncolor)
{
	if (HeadlessMode)
		return;

	assert(Palette);
	if (SDLC_SetSurfaceAndPaletteColors(PalSurface, Palette.get(), system_palette.data(), first, ncolor) < 0) {
		ErrSdl();
	}
	pal_surface_palette_version++;
}

// Applies a tone mapping curve based on the brightness slider value.
// The brightness value is in the range [0, 100] where 0 is neutral (no change)
// and 100 produces maximum brightening.
void ApplyToneMapping(std::array<SDL_Color, 256> &dst,
    const std::array<SDL_Color, 256> &src,
    int n)
{
	// Get the brightness slider value (0 = neutral, 100 = max brightening)
	int brightnessSlider = *GetOptions().Graphics.brightness; // New brightness setting.

	// Maximum adjustment factor (tweak this constant to change the effect strength)
	const float maxAdjustment = 2.0f;
	// Compute the quadratic parameter:
	// When brightnessSlider==0, then a==0 (identity mapping)
	// When brightnessSlider==100, then a== -maxAdjustment (maximum brightening)
	float a = -(brightnessSlider / 100.0f) * maxAdjustment;

	// Precompute a lookup table for speed.
	uint8_t toneMap[256];
	for (int i = 0; i < 256; i++) {
		float x = i / 255.0f;
		// Our quadratic tone mapping: f(x) = a*x^2 + (1-a)*x.
		const float y = std::clamp(a * x * x + (1.0f - a) * x, 0.0f, 1.0f);
		toneMap[i] = static_cast<uint8_t>(y * 255.0f + 0.5f);
	}

	// Apply the lookup table to each color channel in the palette.
	for (int i = 0; i < n; i++) {
		dst[i].r = toneMap[src[i].r];
		dst[i].g = toneMap[src[i].g];
		dst[i].b = toneMap[src[i].b];
	}
	RedrawEverything();
}

void palette_init()
{
	LoadBrightness();
	system_palette = orig_palette;
	InitPalette();
}

void LoadPalette(const char *pszFileName, bool blend /*= true*/)
{
	assert(pszFileName);

	if (HeadlessMode)
		return;

	struct Color {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	std::array<Color, 256> palData;

	LoadFileInMem(pszFileName, palData);

	for (unsigned i = 0; i < palData.size(); i++) {
		orig_palette[i].r = palData[i].r;
		orig_palette[i].g = palData[i].g;
		orig_palette[i].b = palData[i].b;
#ifndef USE_SDL1
		orig_palette[i].a = SDL_ALPHA_OPAQUE;
#endif
	}

	if (blend) {
		if (leveltype == DTYPE_CAVES || leveltype == DTYPE_CRYPT) {
			GenerateBlendedLookupTable(orig_palette.data(), 1, 31);
		} else if (leveltype == DTYPE_NEST) {
			GenerateBlendedLookupTable(orig_palette.data(), 1, 15);
		} else {
			GenerateBlendedLookupTable(orig_palette.data(), -1, -1);
		}
	}
}

void LoadRndLvlPal(dungeon_type l)
{
	if (HeadlessMode)
		return;

	if (l == DTYPE_TOWN) {
		LoadPalette("levels\\towndata\\town.pal");
		return;
	}

	if (l == DTYPE_CRYPT) {
		LoadPalette("nlevels\\l5data\\l5base.pal");
		return;
	}

	int rv = RandomIntBetween(1, 4);
	char szFileName[27];
	if (l == DTYPE_NEST) {
		if (!*GetOptions().Graphics.alternateNestArt) {
			rv++;
		}
		*fmt::format_to(szFileName, R"(nlevels\l{0}data\l{0}base{1}.pal)", 6, rv) = '\0';
	} else {
		*fmt::format_to(szFileName, R"(levels\l{0}data\l{0}_{1}.pal)", static_cast<int>(l), rv) = '\0';
	}
	LoadPalette(szFileName);
}

void IncreaseBrightness()
{
	int brightnessValue = *GetOptions().Graphics.brightness;
	if (brightnessValue < 100) {
		int newBrightness = std::min(brightnessValue + 5, 100);
		GetOptions().Graphics.brightness.SetValue(newBrightness);
		ApplyToneMapping(system_palette, logical_palette, 256);
		palette_update();
	}
}

void DecreaseBrightness()
{
	int brightnessValue = *GetOptions().Graphics.brightness;
	if (brightnessValue > 0) {
		int newBrightness = std::max(brightnessValue - 5, 0);
		GetOptions().Graphics.brightness.SetValue(newBrightness);
		ApplyToneMapping(system_palette, logical_palette, 256);
		palette_update();
	}
}

int UpdateBrightness(int brightness)
{
	if (brightness >= 0) {
		GetOptions().Graphics.brightness.SetValue(brightness);
		ApplyToneMapping(system_palette, logical_palette, 256);
		palette_update();
	}

	return *GetOptions().Graphics.brightness;
}

void SetFadeLevel(int fadeval, bool updateHardwareCursor, const std::array<SDL_Color, 256> &srcPalette)
{
	if (HeadlessMode)
		return;

	for (int i = 0; i < 256; i++) {
		system_palette[i].r = (fadeval * srcPalette[i].r) / 256;
		system_palette[i].g = (fadeval * srcPalette[i].g) / 256;
		system_palette[i].b = (fadeval * srcPalette[i].b) / 256;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		system_palette[i].a = SDL_ALPHA_OPAQUE;
#endif
	}
	palette_update();
	if (updateHardwareCursor && IsHardwareCursor()) {
		ReinitializeHardwareCursor();
	}
}

void BlackPalette()
{
	// With fade level 0 updating the hardware cursor may be redundant
	// since everything is black. The caller should update the cursor
	// when needed instead.
	SetFadeLevel(0, /*updateHardwareCursor=*/false);
}

void PaletteFadeIn(int fr, const std::array<SDL_Color, 256> &srcPalette)
{
	if (HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	ApplyToneMapping(logical_palette, srcPalette, 256);

	if (fr > 0) {
		const uint32_t tc = SDL_GetTicks();
		fr *= 3;
		uint32_t prevFadeValue = 255;
		for (uint32_t i = 0; i < 256; i = fr * (SDL_GetTicks() - tc) / 50) {
			if (i != prevFadeValue) {
				// We can skip hardware cursor update for fade level 0 (everything is black).
				SetFadeLevel(i, /*updateHardwareCursor=*/i != 0u, logical_palette);
				prevFadeValue = i;
			}
			BltFast(nullptr, nullptr);
			RenderPresent();
		}
		SetFadeLevel(256);
	} else {
		SetFadeLevel(256);
		BltFast(nullptr, nullptr);
		RenderPresent();
	}

	logical_palette = srcPalette;

	sgbFadedIn = true;
}

void PaletteFadeOut(int fr, const std::array<SDL_Color, 256> &srcPalette)
{
	if (!sgbFadedIn || HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	if (fr > 0) {
		const uint32_t tc = SDL_GetTicks();
		fr *= 3;
		uint32_t prevFadeValue = 0;
		for (uint32_t i = 0; i < 256; i = fr * (SDL_GetTicks() - tc) / 50) {
			if (i != prevFadeValue) {
				SetFadeLevel(256 - i, /*updateHardwareCursor=*/true, srcPalette);
				prevFadeValue = i;
			}
			BltFast(nullptr, nullptr);
			RenderPresent();
		}
		SetFadeLevel(0, /*updateHardwareCursor=*/true, srcPalette);
	} else {
		SetFadeLevel(0, /*updateHardwareCursor=*/true, srcPalette);
		BltFast(nullptr, nullptr);
		RenderPresent();
	}

	sgbFadedIn = false;
}

void palette_update_caves()
{
	CycleColors(1, 31);
	palette_update(0, 31);
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
	palette_update(0, 31);
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
	palette_update(0, 15);
	delay = 0;
}

void palette_update_quest_palette(int n)
{
	int i = 32 - n;
	logical_palette[i] = orig_palette[i];
	ApplyToneMapping(system_palette, logical_palette, 32);
	palette_update(0, 31);
	UpdateBlendedLookupTableSingleColor(i, logical_palette.data(), /*skipFrom=*/1, /*skipTo=*/31);
}

} // namespace devilution
