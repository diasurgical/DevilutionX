/**
 * @file local_coop_assets.cpp
 *
 * Implementation of asset management for local co-op HUD.
 */
#include "controls/local_coop/local_coop_assets.hpp"

#include <algorithm>
#include <array>

#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/load_pcx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"

namespace devilution {
namespace LocalCoopAssets {

namespace {

// Local co-op HUD sprites
OptionalOwnedClxSpriteList HealthBox;
OptionalOwnedClxSpriteList Health;
OptionalOwnedClxSpriteList HealthBlue; // For mana shield
OptionalOwnedClxSpriteList BoxLeft;
OptionalOwnedClxSpriteList BoxMiddle;
OptionalOwnedClxSpriteList BoxRight;
OptionalOwnedClxSpriteList CharBg;
OptionalOwnedClxSpriteList BarSprite;       // Grayscale bar sprite (list_gry.pcx)
OptionalOwnedClxSpriteList BarSpriteRed;    // Red transformed bar sprite
OptionalOwnedClxSpriteList BarSpriteBlue;   // Blue transformed bar sprite
OptionalOwnedClxSpriteList BarSpriteYellow; // Yellow transformed bar sprite
bool AssetsLoaded = false;

} // namespace

void Init()
{
	if (AssetsLoaded)
		return;

	HealthBox = LoadOptionalClx("data\\healthbox.clx");
	Health = LoadOptionalClx("data\\health.clx");
	if (Health) {
		std::array<uint8_t, 256> healthBlueTrn = {};
		for (int i = 0; i < 256; i++)
			healthBlueTrn[i] = static_cast<uint8_t>(i);
		healthBlueTrn[234] = PAL16_BLUE + 5;
		healthBlueTrn[235] = PAL16_BLUE + 6;
		healthBlueTrn[236] = PAL16_BLUE + 7;
		HealthBlue = Health->clone();
		ClxApplyTrans(*HealthBlue, healthBlueTrn.data());
	}

	BoxLeft = LoadOptionalClx("data\\boxleftend.clx");
	BoxMiddle = LoadOptionalClx("data\\boxmiddle.clx");
	BoxRight = LoadOptionalClx("data\\boxrightend.clx");

	CharBg = LoadOptionalClx("data\\charbg.clx");
	BarSprite = LoadPcx("ui_art\\list_gry");
	if (BarSprite) {
		auto createColorTrn = [](uint8_t colorBase) {
			std::array<uint8_t, 256> trn = {};
			for (int i = 0; i < 256; i++) {
				trn[i] = static_cast<uint8_t>(i);
			}
			for (int i = 0; i < 16; i++) {
				const int targetShade = std::min(15, 4 + i);
				trn[PAL16_GRAY + i] = colorBase + targetShade;
			}
			for (int i = 230; i < 240; i++) {
				const int shade = i - 230;
				const int targetShade = std::min(15, 4 + shade);
				trn[i] = colorBase + targetShade;
			}
			return trn;
		};

		const std::array<uint8_t, 256> redTrn = createColorTrn(PAL16_RED);
		const std::array<uint8_t, 256> blueTrn = createColorTrn(PAL16_BLUE);
		const std::array<uint8_t, 256> yellowTrn = createColorTrn(PAL16_YELLOW);

		BarSpriteRed = BarSprite->clone();
		ClxApplyTrans(*BarSpriteRed, redTrn.data());

		BarSpriteBlue = BarSprite->clone();
		ClxApplyTrans(*BarSpriteBlue, blueTrn.data());

		BarSpriteYellow = BarSprite->clone();
		ClxApplyTrans(*BarSpriteYellow, yellowTrn.data());
	}

	AssetsLoaded = true;
}

void Free()
{
	BarSpriteYellow = std::nullopt;
	BarSpriteBlue = std::nullopt;
	BarSpriteRed = std::nullopt;
	BarSprite = std::nullopt;
	CharBg = std::nullopt;
	BoxRight = std::nullopt;
	BoxMiddle = std::nullopt;
	BoxLeft = std::nullopt;
	HealthBlue = std::nullopt;
	Health = std::nullopt;
	HealthBox = std::nullopt;
	AssetsLoaded = false;
}

bool IsLoaded()
{
	return AssetsLoaded;
}

const OptionalOwnedClxSpriteList &GetHealthBox()
{
	return HealthBox;
}

const OptionalOwnedClxSpriteList &GetHealth()
{
	return Health;
}

const OptionalOwnedClxSpriteList &GetHealthBlue()
{
	return HealthBlue;
}

const OptionalOwnedClxSpriteList &GetBoxLeft()
{
	return BoxLeft;
}

const OptionalOwnedClxSpriteList &GetBoxMiddle()
{
	return BoxMiddle;
}

const OptionalOwnedClxSpriteList &GetBoxRight()
{
	return BoxRight;
}

const OptionalOwnedClxSpriteList &GetCharBg()
{
	return CharBg;
}

const OptionalOwnedClxSpriteList &GetBarSprite()
{
	return BarSprite;
}

const OptionalOwnedClxSpriteList &GetBarSpriteRed()
{
	return BarSpriteRed;
}

const OptionalOwnedClxSpriteList &GetBarSpriteBlue()
{
	return BarSpriteBlue;
}

const OptionalOwnedClxSpriteList &GetBarSpriteYellow()
{
	return BarSpriteYellow;
}

} // namespace LocalCoopAssets
} // namespace devilution
