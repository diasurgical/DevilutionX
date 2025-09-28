/**
 * @file xpbar.cpp
 *
 * Adds XP bar QoL feature
 */
#include "xpbar.h"

#include <array>
#include <cstdint>

#include <fmt/core.h>

#include "control.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "options.h"
#include "playerdat.hpp"
#include "utils/format_int.hpp"
#include "utils/language.h"

namespace devilution {

namespace {

constexpr int BarWidth = 307;

using ColorGradient = std::array<Uint8, 12>;
constexpr ColorGradient GoldGradient = { 0xCF, 0xCE, 0xCD, 0xCC, 0xCB, 0xCA, 0xC9, 0xC8, 0xC7, 0xC6, 0xC5, 0xC4 };
constexpr ColorGradient SilverGradient = { 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6, 0xF5, 0xF4, 0xF3 };

constexpr int BackWidth = 313;
constexpr int BackHeight = 9;

OptionalOwnedClxSpriteList xpbarArt;

void DrawBar(const Surface &out, Point screenPosition, int width, const ColorGradient &gradient)
{
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 1 }, width, gradient[gradient.size() * 3 / 4 - 1]);
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 2 }, width, gradient[gradient.size() - 1]);
	UnsafeDrawHorizontalLine(out, screenPosition + Displacement { 0, 3 }, width, gradient[gradient.size() / 2 - 1]);
}

void DrawEndCap(const Surface &out, Point point, int idx, const ColorGradient &gradient)
{
	out.SetPixel({ point.x, point.y + 1 }, gradient[idx * 3 / 4]);
	out.SetPixel({ point.x, point.y + 2 }, gradient[idx]);
	out.SetPixel({ point.x, point.y + 3 }, gradient[idx / 2]);
}

} // namespace

void InitXPBar()
{
	if (*sgOptions.Gameplay.experienceBar) {
		xpbarArt = LoadClx("data\\xpbar.clx");
	}
}

void FreeXPBar()
{
	xpbarArt = std::nullopt;
}

void DrawXPBar(const Surface &out)
{
	if (!*sgOptions.Gameplay.experienceBar || talkflag)
		return;

	const Player &player = *MyPlayer;
	const Rectangle &mainPanel = GetMainPanel();

	const Point back = { mainPanel.position.x + mainPanel.size.width / 2 - 155, mainPanel.position.y + mainPanel.size.height - 11 };
	const Point position = back + Displacement { 3, 2 };

	RenderClxSprite(out, (*xpbarArt)[0], back);

	const int8_t charLevel = player._pLevel;

	if (charLevel == MaxCharacterLevel) {
		// Draw a nice golden bar for max level characters.
		DrawBar(out, position, BarWidth, GoldGradient);

		return;
	}

	const uint64_t prevXp = ExpLvlsTbl[charLevel - 1];
	if (player._pExperience < prevXp)
		return;

	uint64_t prevXpDelta1 = player._pExperience - prevXp;
	uint64_t prevXpDelta = ExpLvlsTbl[charLevel] - prevXp;
	uint64_t fullBar = BarWidth * prevXpDelta1 / prevXpDelta;

	// Figure out how much to fill the last pixel of the XP bar, to make it gradually appear with gained XP
	uint64_t onePx = prevXpDelta / BarWidth + 1;
	uint64_t lastFullPx = fullBar * prevXpDelta / BarWidth;

	const uint64_t fade = (prevXpDelta1 - lastFullPx) * (SilverGradient.size() - 1) / onePx;

	// Draw beginning of bar full brightness
	DrawBar(out, position, fullBar, SilverGradient);

	// End pixels appear gradually
	DrawEndCap(out, position + Displacement { static_cast<int>(fullBar), 0 }, fade, SilverGradient);
}

bool CheckXPBarInfo()
{
	if (!*sgOptions.Gameplay.experienceBar)
		return false;
	const Rectangle &mainPanel = GetMainPanel();

	const int backX = mainPanel.position.x + mainPanel.size.width / 2 - 155;
	const int backY = mainPanel.position.y + mainPanel.size.height - 11;

	if (MousePosition.x < backX || MousePosition.x >= backX + BackWidth || MousePosition.y < backY || MousePosition.y >= backY + BackHeight)
		return false;

	const Player &player = *MyPlayer;

	const int8_t charLevel = player._pLevel;

	AddPanelString(fmt::format(fmt::runtime(_("Level {:d}")), charLevel));

	if (charLevel == MaxCharacterLevel) {
		// Show a maximum level indicator for max level players.
		InfoColor = UiFlags::ColorWhitegold;

		AddPanelString(fmt::format(fmt::runtime(_("Experience: {:s}")), FormatInteger(ExpLvlsTbl[charLevel - 1])));
		AddPanelString(_("Maximum Level"));

		return true;
	}

	InfoColor = UiFlags::ColorWhite;

	AddPanelString(fmt::format(fmt::runtime(_("Experience: {:s}")), FormatInteger(player._pExperience)));
	AddPanelString(fmt::format(fmt::runtime(_("Next Level: {:s}")), FormatInteger(ExpLvlsTbl[charLevel])));
	AddPanelString(fmt::format(fmt::runtime(_("{:s} to Level {:d}")), FormatInteger(ExpLvlsTbl[charLevel] - player._pExperience), charLevel + 1));

	return true;
}

} // namespace devilution
