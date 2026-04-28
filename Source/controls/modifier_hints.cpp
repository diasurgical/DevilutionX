#include "controls/modifier_hints.h"

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "control/control.hpp"
#include "controls/controller_motion.h"
#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/renderer.h"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "utils/language.h"

namespace devilution {

namespace {

/** Vertical distance between text lines. */
constexpr int LineHeight = 25;

/** Horizontal margin of the hints circle from panel edge. */
constexpr int CircleMarginX = 16;

/** Distance between the panel top and the circle top. */
constexpr int CircleTop = 101;

/** Spell icon side size. */
constexpr int IconSize = 37;

constexpr int HintBoxSize = 39;
constexpr int HintBoxMargin = 5;

OptionalOwnedClxSpriteList hintBox;
OptionalOwnedClxSpriteList hintBoxBackground;
OptionalOwnedClxSpriteList hintIcons;

enum HintIcon : uint8_t {
	IconChar,
	IconInv,
	IconQuests,
	IconSpells,
	IconMap,
	IconMenu,
	IconNull
};

struct CircleMenuHint {
	CircleMenuHint(HintIcon top, HintIcon right, HintIcon bottom, HintIcon left)
	    : top(top)
	    , right(right)
	    , bottom(bottom)
	    , left(left)
	{
	}

	HintIcon top;
	HintIcon right;
	HintIcon bottom;
	HintIcon left;
};

/**
 * @brief Draws hint text for a four button layout with the top/left edge of the bounding box at the position given by origin.
 * @param out The output buffer to draw on.
 * @param hint Struct describing the icon to draw.
 * @param origin Top left corner of the layout (relative to the output buffer).
 */
void DrawCircleMenuHint(const CircleMenuHint &hint, const Point &origin)
{
	const Displacement backgroundDisplacement = { ((HintBoxSize - IconSize) / 2) + 1, ((HintBoxSize - IconSize) / 2) - 1 };
	const Point hintBoxPositions[4] = {
		origin + Displacement { 0, LineHeight - HintBoxSize },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight - (HintBoxSize * 2) - HintBoxMargin },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight + HintBoxMargin },
		origin + Displacement { (HintBoxSize * 2) + (HintBoxMargin * 2), LineHeight - HintBoxSize }
	};
	const Point iconPositions[4] = {
		hintBoxPositions[0] + backgroundDisplacement,
		hintBoxPositions[1] + backgroundDisplacement,
		hintBoxPositions[2] + backgroundDisplacement,
		hintBoxPositions[3] + backgroundDisplacement,
	};
	const uint8_t iconIndices[4] { hint.left, hint.top, hint.bottom, hint.right };

	for (int slot = 0; slot < 4; ++slot) {
		if (iconIndices[slot] == HintIcon::IconNull)
			continue;

		GetRenderer().RenderClx(iconPositions[slot], (*hintBoxBackground)[0]);
		GetRenderer().SetClipRegion(iconPositions[slot].x, iconPositions[slot].y, 37, 38);
		GetRenderer().RenderClx({ iconPositions[slot].x, iconPositions[slot].y }, (*hintIcons)[iconIndices[slot]]);
		GetRenderer().ClearClipRegion();
		GetRenderer().RenderClx(hintBoxPositions[slot], (*hintBox)[0]);
	}
}

/**
 * @brief Draws hint text for a four button layout with the top/left edge of the bounding box at the position given by origin plus the icon for the spell mapped to that entry.
 * @param out The output buffer to draw on.
 * @param origin Top left corner of the layout (relative to the output buffer).
 */
void DrawSpellsCircleMenuHint(const Point &origin)
{
	const Player &myPlayer = *MyPlayer;
	const Displacement spellIconDisplacement = { ((HintBoxSize - IconSize) / 2) + 1, HintBoxSize - ((HintBoxSize - IconSize) / 2) - 1 };
	const Point hintBoxPositions[4] = {
		origin + Displacement { 0, LineHeight - HintBoxSize },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight - (HintBoxSize * 2) - HintBoxMargin },
		origin + Displacement { HintBoxSize + HintBoxMargin, LineHeight + HintBoxMargin },
		origin + Displacement { (HintBoxSize * 2) + (HintBoxMargin * 2), LineHeight - HintBoxSize }
	};
	const Point spellIconPositions[4] = {
		hintBoxPositions[0] + spellIconDisplacement,
		hintBoxPositions[1] + spellIconDisplacement,
		hintBoxPositions[2] + spellIconDisplacement,
		hintBoxPositions[3] + spellIconDisplacement,
	};
	const uint64_t spells = myPlayer._pAblSpells | myPlayer._pMemSpells | myPlayer._pScrlSpells | myPlayer._pISpells;
	SpellID splId;
	SpellType splType;

	for (int slot = 0; slot < 4; ++slot) {
		splId = myPlayer._pSplHotKey[slot];

		if (IsValidSpell(splId) && (spells & GetSpellBitmask(splId)) != 0)
			splType = (leveltype == DTYPE_TOWN && !GetSpellData(splId).isAllowedInTown()) ? SpellType::Invalid : myPlayer._pSplTHotKey[slot];
		else {
			splType = SpellType::Invalid;
			splId = SpellID::Null;
		}

		SetSpellTrans(splType);
		DrawSmallSpellIcon(spellIconPositions[slot], splId);
		GetRenderer().RenderClx(hintBoxPositions[slot], (*hintBox)[0]);
	}
}

void DrawGamepadMenuNavigator()
{
	if (!PadMenuNavigatorActive || SimulatingMouseWithPadmapper)
		return;
	static const CircleMenuHint DPad(/*top=*/HintIcon::IconMenu, /*right=*/HintIcon::IconInv, /*bottom=*/HintIcon::IconMap, /*left=*/HintIcon::IconChar);
	static const CircleMenuHint Buttons(/*top=*/HintIcon::IconNull, /*right=*/HintIcon::IconNull, /*bottom=*/HintIcon::IconSpells, /*left=*/HintIcon::IconQuests);
	const Rectangle &mainPanel = GetMainPanel();
	DrawCircleMenuHint(DPad, { mainPanel.position.x + CircleMarginX, mainPanel.position.y - CircleTop });
	DrawCircleMenuHint(Buttons, { mainPanel.position.x + mainPanel.size.width - (HintBoxSize * 3) - CircleMarginX - (HintBoxMargin * 2), mainPanel.position.y - CircleTop });
}

void DrawGamepadHotspellMenu()
{
	if (!PadHotspellMenuActive || SimulatingMouseWithPadmapper)
		return;

	const Rectangle &mainPanel = GetMainPanel();
	DrawSpellsCircleMenuHint({ mainPanel.position.x + mainPanel.size.width - (HintBoxSize * 3) - CircleMarginX - (HintBoxMargin * 2), mainPanel.position.y - CircleTop });
}

} // namespace

void InitModifierHints()
{
	hintBox = LoadClx("data\\hintbox.clx");
	hintBoxBackground = LoadClx("data\\hintboxbackground.clx");
	hintIcons = LoadClx("data\\hinticons.clx");
}

void FreeModifierHints()
{
	hintIcons = std::nullopt;
	hintBoxBackground = std::nullopt;
	hintBox = std::nullopt;
}

void DrawControllerModifierHints()
{
	DrawGamepadMenuNavigator();
	DrawGamepadHotspellMenu();
}

} // namespace devilution
