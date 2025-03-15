#include "panels/charpanel.hpp"

#include <cstdint>

#include <algorithm>
#include <string>

#include <expected.hpp>
#include <fmt/format.h>
#include <function_ref.hpp>

#include "control.h"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/display.h"
#include "utils/enum_traits.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pChrButtons;

namespace {

struct StyledText {
	UiFlags style;
	std::string text;
	int spacing = 1;
};

struct PanelLabel {
	int labelWidth;
	int boxWidth;
};

struct PanelEntry {
	std::string label;
	Point position;
	PanelLabel width;                                              // max label's length - used for line wrapping
	std::optional<tl::function_ref<StyledText()>> statDisplayFunc; // function responsible for displaying stat
};

UiFlags GetBaseStatColor(CharacterAttribute attr)
{
	UiFlags style = UiFlags::ColorWhite;
	if (InspectPlayer->GetBaseAttributeValue(attr) == InspectPlayer->GetMaximumAttributeValue(attr))
		style = UiFlags::ColorWhitegold;
	return style;
}

UiFlags GetCurrentStatColor(CharacterAttribute attr)
{
	UiFlags style = UiFlags::ColorWhite;
	int current = InspectPlayer->GetCurrentAttributeValue(attr);
	int base = InspectPlayer->GetBaseAttributeValue(attr);
	if (current > base)
		style = UiFlags::ColorBlue;
	if (current < base)
		style = UiFlags::ColorRed;
	return style;
}

UiFlags GetValueColor(int value, bool flip = false)
{
	UiFlags style = UiFlags::ColorWhite;
	if (value > 0)
		style = (flip ? UiFlags::ColorRed : UiFlags::ColorBlue);
	if (value < 0)
		style = (flip ? UiFlags::ColorBlue : UiFlags::ColorRed);
	return style;
}

UiFlags GetMaxManaColor()
{
	if (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana))
		return UiFlags::ColorRed;
	return InspectPlayer->_pMaxMana > InspectPlayer->_pMaxManaBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

UiFlags GetMaxHealthColor()
{
	return InspectPlayer->_pMaxHP > InspectPlayer->_pMaxHPBase ? UiFlags::ColorBlue : UiFlags::ColorWhite;
}

std::pair<int, int> GetDamage()
{
	int damageMod = InspectPlayer->_pIBonusDamMod;
	if (InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && InspectPlayer->_pClass != HeroClass::Rogue) {
		damageMod += InspectPlayer->_pDamageMod / 2;
	} else {
		damageMod += InspectPlayer->_pDamageMod;
	}
	int mindam = InspectPlayer->_pIMinDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMinDam / 100 + damageMod;
	int maxdam = InspectPlayer->_pIMaxDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMaxDam / 100 + damageMod;
	return { mindam, maxdam };
}

StyledText GetResistInfo(int8_t resist)
{
	UiFlags style = UiFlags::ColorBlue;
	if (resist == 0)
		style = UiFlags::ColorWhite;
	else if (resist < 0)
		style = UiFlags::ColorRed;
	else if (resist >= MaxResistance)
		style = UiFlags::ColorWhitegold;

	return { style, StrCat(resist, "%") };
}

constexpr int LeftColumnLabelX = 88;
constexpr int TopRightLabelX = 211;
constexpr int RightColumnLabelX = 253;

// Indices in `panelEntries`.
constexpr unsigned AttributeHeaderEntryIndices[2] = { 5, 6 };
constexpr unsigned GoldHeaderEntryIndex = 16;

PanelLabel nameLabel = { 0, 150 };
PanelLabel classLabel = { 0, 149 };
PanelLabel levelLabel = { 45, 57 };
PanelLabel expLabel = { 91, 99 };
PanelLabel nextExpLabel = { 198, 99 };
PanelLabel attributeHeaderLabel = { 44, 0 };
PanelLabel goldHeaderLabel = { 98, 0 };
PanelLabel goldLabel = { 0, 99 };
PanelLabel wideLabel = { 68, 57 };
PanelLabel narrowLabel = { 76, 45 };
PanelLabel noTextNarrowLabel = { 0, 45 };

PanelEntry panelEntries[] = {
	{ "", { 9, 14 }, nameLabel,
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	{ "", { 161, 14 }, classLabel,
	    []() { return StyledText { UiFlags::ColorWhite, std::string(InspectPlayer->getClassName()) }; } },

	{ N_("Level"), { 57, 52 }, levelLabel,
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->getCharacterLevel()) }; } },
	{ N_("Experience"), { TopRightLabelX, 52 }, expLabel,
	    []() {
	        int spacing = ((InspectPlayer->_pExperience >= 1000000000) ? 0 : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pExperience), spacing };
	    } },
	{ N_("Next level"), { TopRightLabelX, 80 }, nextExpLabel,
	    []() {
	        if (InspectPlayer->isMaxCharacterLevel()) {
		        return StyledText { UiFlags::ColorWhitegold, std::string(_("None")) };
	        }
	        uint32_t nextExperienceThreshold = InspectPlayer->getNextExperienceThreshold();
	        int spacing = ((nextExperienceThreshold >= 1000000000) ? 0 : 1);
	        return StyledText { UiFlags::ColorWhite, FormatInteger(nextExperienceThreshold), spacing };
	    } },

	{ N_("Base"), { LeftColumnLabelX, /* set dynamically */ 0 }, attributeHeaderLabel, {} },
	{ N_("Now"), { 135, /* set dynamically */ 0 }, attributeHeaderLabel, {} },
	{ N_("Strength"), { LeftColumnLabelX, 135 }, narrowLabel,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pBaseStr) }; } },
	{ "", { 135, 135 }, noTextNarrowLabel,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pStrength) }; } },
	{ N_("Magic"), { LeftColumnLabelX, 163 }, narrowLabel,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pBaseMag) }; } },
	{ "", { 135, 163 }, noTextNarrowLabel,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pMagic) }; } },
	{ N_("Dexterity"), { LeftColumnLabelX, 191 }, narrowLabel, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pBaseDex) }; } },
	{ "", { 135, 191 }, noTextNarrowLabel,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pDexterity) }; } },
	{ N_("Vitality"), { LeftColumnLabelX, 219 }, narrowLabel, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pBaseVit) }; } },
	{ "", { 135, 219 }, noTextNarrowLabel,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pVitality) }; } },
	{ N_("Points to distribute"), { LeftColumnLabelX, 248 }, narrowLabel,
	    []() {
	        InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	        return StyledText { UiFlags::ColorRed, (InspectPlayer->_pStatPts > 0 ? StrCat(InspectPlayer->_pStatPts) : "") };
	    } },

	{ N_("Gold"), { TopRightLabelX, /* set dynamically */ 0 }, goldHeaderLabel, {} },
	{ "", { TopRightLabelX, 127 }, goldLabel,
	    []() { return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pGold) }; } },

	{ N_("Armor class"), { RightColumnLabelX, 163 }, wideLabel,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusAC), StrCat(InspectPlayer->GetArmor() + InspectPlayer->getCharacterLevel() * 2) }; } },
	{ N_("To hit"), { RightColumnLabelX, 191 }, wideLabel,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusToHit), StrCat(InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow ? InspectPlayer->GetRangedToHit() : InspectPlayer->GetMeleeToHit(), "%") }; } },
	{ N_("Damage"), { RightColumnLabelX, 219 }, wideLabel,
	    []() {
	        const auto [dmgMin, dmgMax] = GetDamage();
	        int spacing = ((dmgMin >= 100) ? -1 : 1);
	        return StyledText { GetValueColor(InspectPlayer->_pIBonusDam), StrCat(dmgMin, "-", dmgMax), spacing };
	    } },

	{ N_("Life"), { LeftColumnLabelX, 284 }, narrowLabel,
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; } },
	{ "", { 135, 284 }, noTextNarrowLabel,
	    []() { return StyledText { (InspectPlayer->_pHitPoints != InspectPlayer->_pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor()), StrCat(InspectPlayer->_pHitPoints >> 6) }; } },
	{ N_("Mana"), { LeftColumnLabelX, 312 }, narrowLabel,
	    []() { return StyledText { GetMaxManaColor(), StrCat(HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) ? 0 : InspectPlayer->_pMaxMana >> 6) }; } },
	{ "", { 135, 312 }, noTextNarrowLabel,
	    []() { return StyledText { (InspectPlayer->_pMana != InspectPlayer->_pMaxMana ? UiFlags::ColorRed : GetMaxManaColor()), StrCat((HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) || (InspectPlayer->_pMana >> 6) <= 0) ? 0 : InspectPlayer->_pMana >> 6) }; } },

	{ N_("Resist Magic"), { RightColumnLabelX, 256 }, wideLabel,
	    []() { return GetResistInfo(InspectPlayer->_pMagResist); } },
	{ N_("Resist Fire"), { RightColumnLabelX, 284 }, wideLabel,
	    []() { return GetResistInfo(InspectPlayer->_pFireResist); } },
	{ N_("Resist Lightning"), { RightColumnLabelX, 313 }, wideLabel,
	    []() { return GetResistInfo(InspectPlayer->_pLghtResist); } },
};

OptionalOwnedClxSpriteList Panel;

constexpr int PanelFieldHeight = 24;
constexpr int PanelFieldPaddingTop = 3;
constexpr int PanelFieldPaddingBottom = 3;
constexpr int PanelFieldInnerHeight = PanelFieldHeight - PanelFieldPaddingTop - PanelFieldPaddingBottom;

void DrawPanelField(const Surface &out, Point pos, int len, ClxSprite left, ClxSprite middle, ClxSprite right)
{
	RenderClxSprite(out, left, pos);
	pos.x += left.width();
	len -= left.width() + right.width();
	RenderClxSprite(out.subregion(pos.x, pos.y, len, middle.height()), middle, Point { 0, 0 });
	pos.x += len;
	RenderClxSprite(out, right, pos);
}

void DrawShadowString(const Surface &out, const PanelEntry &entry)
{
	if (entry.label.empty())
		return;

	constexpr int Spacing = 0;
	const std::string_view textStr = LanguageTranslate(entry.label);
	std::string_view text;
	std::string wrapped;
	if (entry.width.labelWidth > 0) {
		wrapped = WordWrapString(textStr, entry.width.labelWidth, GameFont12, Spacing);
		text = wrapped;
	} else {
		text = textStr;
	}

	UiFlags style = UiFlags::VerticalCenter;

	Point labelPosition = entry.position;

	if (entry.width.boxWidth == 0) {
		style |= UiFlags::AlignCenter;
	} else {
		style |= UiFlags::AlignRight;
		labelPosition += Displacement { -entry.width.labelWidth - (IsSmallFontTall() ? 2 : 3), 0 };
	}

	// If the text is less tall than the field, we center it vertically relative to the field.
	// Otherwise, we draw from the top of the field.
	const int textHeight = static_cast<int>((c_count(wrapped, '\n') + 1) * GetLineHeight(wrapped, GameFont12));
	const int labelHeight = std::max(PanelFieldHeight, textHeight);

	DrawString(out, text, { labelPosition + Displacement { -2, 2 }, { entry.width.labelWidth, labelHeight } },
	    { .flags = style | UiFlags::ColorBlack, .spacing = Spacing });
	DrawString(out, text, { labelPosition, { entry.width.labelWidth, labelHeight } },
	    { .flags = style | UiFlags::ColorWhite, .spacing = Spacing });
}

void DrawStatButtons(const Surface &out)
{
	if (InspectPlayer->_pStatPts > 0 && !IsInspectingPlayer()) {
		if (InspectPlayer->_pBaseStr < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 157 }), (*pChrButtons)[CharPanelButton[static_cast<size_t>(CharacterAttribute::Strength)] ? 2 : 1]);
		if (InspectPlayer->_pBaseMag < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Magic))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 185 }), (*pChrButtons)[CharPanelButton[static_cast<size_t>(CharacterAttribute::Magic)] ? 4 : 3]);
		if (InspectPlayer->_pBaseDex < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Dexterity))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 214 }), (*pChrButtons)[CharPanelButton[static_cast<size_t>(CharacterAttribute::Dexterity)] ? 6 : 5]);
		if (InspectPlayer->_pBaseVit < InspectPlayer->GetMaximumAttributeValue(CharacterAttribute::Vitality))
			ClxDraw(out, GetPanelPosition(UiPanels::Character, { 137, 242 }), (*pChrButtons)[CharPanelButton[static_cast<size_t>(CharacterAttribute::Vitality)] ? 8 : 7]);
	}
}

} // namespace

tl::expected<void, std::string> LoadCharPanel()
{
	ASSIGN_OR_RETURN(OptionalOwnedClxSpriteList background, LoadClxWithStatus("data\\charbg.clx"));
	OwnedSurface out((*background)[0].width(), (*background)[0].height());
	RenderClxSprite(out, (*background)[0], { 0, 0 });
	background = std::nullopt;

	{
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxLeft, LoadClxWithStatus("data\\boxleftend.clx"));
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxMiddle, LoadClxWithStatus("data\\boxmiddle.clx"));
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxRight, LoadClxWithStatus("data\\boxrightend.clx"));

		const bool isSmallFontTall = IsSmallFontTall();
		const int attributeHeadersY = isSmallFontTall ? 112 : 114;
		for (unsigned i : AttributeHeaderEntryIndices) {
			panelEntries[i].position.y = attributeHeadersY;
		}
		panelEntries[GoldHeaderEntryIndex].position.y = isSmallFontTall ? 105 : 106;

		for (auto &entry : panelEntries) {
			if (entry.statDisplayFunc) {
				DrawPanelField(out, entry.position, entry.width.boxWidth, boxLeft[0], boxMiddle[0], boxRight[0]);
			}
			DrawShadowString(out, entry);
		}
	}

	Panel = SurfaceToClx(out);
	return {};
}

void FreeCharPanel()
{
	Panel = std::nullopt;
}

void DrawChr(const Surface &out)
{
	Point pos = GetPanelPosition(UiPanels::Character, { 0, 0 });
	RenderClxSprite(out, (*Panel)[0], pos);
	for (auto &entry : panelEntries) {
		if (entry.statDisplayFunc) {
			StyledText tmp = (*entry.statDisplayFunc)();
			DrawString(
			    out,
			    tmp.text,
			    { entry.position + Displacement { pos.x, pos.y + PanelFieldPaddingTop }, { entry.width.boxWidth, PanelFieldInnerHeight } },
			    { .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter | tmp.style, .spacing = tmp.spacing });
		}
	}
	DrawStatButtons(out);
}

} // namespace devilution
