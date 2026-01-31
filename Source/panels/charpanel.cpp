#include "panels/charpanel.hpp"

#include <cstdint>

#include <algorithm>
#include <string>

#include <expected.hpp>
#include <fmt/format.h>
#include <function_ref.hpp>

#include "control/control.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/ui_panels.hpp"
#include "msg.h"
#include "player.h"
#include "tables/playerdat.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/display.h"
#include "utils/enum_traits.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"
#include "utils/screen_reader.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pChrButtons;

namespace {

enum class CharacterScreenField : uint8_t {
	NameAndClass,
	Level,
	Experience,
	NextLevel,
	Strength,
	Magic,
	Dexterity,
	Vitality,
	PointsToDistribute,
	Gold,
	ArmorClass,
	ChanceToHit,
	Damage,
	Life,
	Mana,
	ResistMagic,
	ResistFire,
	ResistLightning,
};

constexpr std::array<CharacterScreenField, 18> CharacterScreenFieldOrder = {
	CharacterScreenField::NameAndClass,
	CharacterScreenField::Level,
	CharacterScreenField::Experience,
	CharacterScreenField::NextLevel,
	CharacterScreenField::Strength,
	CharacterScreenField::Magic,
	CharacterScreenField::Dexterity,
	CharacterScreenField::Vitality,
	CharacterScreenField::PointsToDistribute,
	CharacterScreenField::Gold,
	CharacterScreenField::ArmorClass,
	CharacterScreenField::ChanceToHit,
	CharacterScreenField::Damage,
	CharacterScreenField::Life,
	CharacterScreenField::Mana,
	CharacterScreenField::ResistMagic,
	CharacterScreenField::ResistFire,
	CharacterScreenField::ResistLightning,
};

size_t SelectedCharacterScreenFieldIndex = 0;

struct StyledText {
	UiFlags style;
	std::string text;
	int spacing = 1;
};

struct PanelEntry {
	std::string label;
	Point position;
	int length;
	int labelLength;                                               // max label's length - used for line wrapping
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
	const int current = InspectPlayer->GetCurrentAttributeValue(attr);
	const int base = InspectPlayer->GetBaseAttributeValue(attr);
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
	const int mindam = InspectPlayer->_pIMinDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMinDam / 100 + damageMod;
	const int maxdam = InspectPlayer->_pIMaxDam + InspectPlayer->_pIBonusDam * InspectPlayer->_pIMaxDam / 100 + damageMod;
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

constexpr int LeftColumnLabelWidth = 76;
constexpr int RightColumnLabelWidth = 68;

// Indices in `panelEntries`.
constexpr unsigned AttributeHeaderEntryIndices[2] = { 5, 6 };
constexpr unsigned GoldHeaderEntryIndex = 16;

PanelEntry panelEntries[] = {
	{ "", { 9, 14 }, 150, 0,
	    []() { return StyledText { UiFlags::ColorWhite, InspectPlayer->_pName }; } },
	{ "", { 161, 14 }, 149, 0,
	    []() { return StyledText { UiFlags::ColorWhite, std::string(InspectPlayer->getClassName()) }; } },

	{ N_("Level"), { 57, 52 }, 57, 45,
	    []() { return StyledText { UiFlags::ColorWhite, StrCat(InspectPlayer->getCharacterLevel()) }; } },
	{ N_("Experience"), { TopRightLabelX, 52 }, 99, 91,
	    []() {
	        return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pExperience) };
	    } },
	{ N_("Next level"), { TopRightLabelX, 80 }, 99, 198,
	    []() {
	        if (InspectPlayer->isMaxCharacterLevel()) {
		        return StyledText { UiFlags::ColorWhitegold, std::string(_("None")) };
	        }
	        const uint32_t nextExperienceThreshold = InspectPlayer->getNextExperienceThreshold();
	        return StyledText { UiFlags::ColorWhite, FormatInteger(nextExperienceThreshold) };
	    } },

	{ N_("Base"), { LeftColumnLabelX, /* set dynamically */ 0 }, 0, 44, {} },
	{ N_("Now"), { 135, /* set dynamically */ 0 }, 0, 44, {} },
	{ N_("Strength"), { LeftColumnLabelX, 135 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pBaseStr) }; } },
	{ "", { 135, 135 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Strength), StrCat(InspectPlayer->_pStrength) }; } },
	{ N_("Magic"), { LeftColumnLabelX, 163 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetBaseStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pBaseMag) }; } },
	{ "", { 135, 163 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Magic), StrCat(InspectPlayer->_pMagic) }; } },
	{ N_("Dexterity"), { LeftColumnLabelX, 191 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pBaseDex) }; } },
	{ "", { 135, 191 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Dexterity), StrCat(InspectPlayer->_pDexterity) }; } },
	{ N_("Vitality"), { LeftColumnLabelX, 219 }, 45, LeftColumnLabelWidth, []() { return StyledText { GetBaseStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pBaseVit) }; } },
	{ "", { 135, 219 }, 45, 0,
	    []() { return StyledText { GetCurrentStatColor(CharacterAttribute::Vitality), StrCat(InspectPlayer->_pVitality) }; } },
	{ N_("Points to distribute"), { LeftColumnLabelX, 248 }, 45, LeftColumnLabelWidth,
	    []() {
	        InspectPlayer->_pStatPts = std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
	        return StyledText { UiFlags::ColorRed, (InspectPlayer->_pStatPts > 0 ? StrCat(InspectPlayer->_pStatPts) : "") };
	    } },

	{ N_("Gold"), { TopRightLabelX, /* set dynamically */ 0 }, 0, 98, {} },
	{ "", { TopRightLabelX, 127 }, 99, 0,
	    []() { return StyledText { UiFlags::ColorWhite, FormatInteger(InspectPlayer->_pGold) }; } },

	{ N_("Armor class"), { RightColumnLabelX, 163 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusAC), StrCat(InspectPlayer->GetArmor() + InspectPlayer->getCharacterLevel() * 2) }; } },
	{ N_("Chance To Hit"), { RightColumnLabelX, 191 }, 57, RightColumnLabelWidth,
	    []() { return StyledText { GetValueColor(InspectPlayer->_pIBonusToHit), StrCat(InspectPlayer->InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow ? InspectPlayer->GetRangedToHit() : InspectPlayer->GetMeleeToHit(), "%") }; } },
	{ N_("Damage"), { RightColumnLabelX, 219 }, 57, RightColumnLabelWidth,
	    []() {
	        const auto [dmgMin, dmgMax] = GetDamage();
	        return StyledText { GetValueColor(InspectPlayer->_pIBonusDam), StrCat(dmgMin, "-", dmgMax) };
	    } },

	{ N_("Life"), { LeftColumnLabelX, 284 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxHealthColor(), StrCat(InspectPlayer->_pMaxHP >> 6) }; } },
	{ "", { 135, 284 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pHitPoints != InspectPlayer->_pMaxHP ? UiFlags::ColorRed : GetMaxHealthColor()), StrCat(InspectPlayer->_pHitPoints >> 6) }; } },
	{ N_("Mana"), { LeftColumnLabelX, 312 }, 45, LeftColumnLabelWidth,
	    []() { return StyledText { GetMaxManaColor(), StrCat(HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) ? 0 : InspectPlayer->_pMaxMana >> 6) }; } },
	{ "", { 135, 312 }, 45, 0,
	    []() { return StyledText { (InspectPlayer->_pMana != InspectPlayer->_pMaxMana ? UiFlags::ColorRed : GetMaxManaColor()), StrCat((HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) || InspectPlayer->hasNoMana()) ? 0 : InspectPlayer->_pMana >> 6) }; } },

	{ N_("Resist magic"), { RightColumnLabelX, 256 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pMagResist); } },
	{ N_("Resist fire"), { RightColumnLabelX, 284 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pFireResist); } },
	{ N_("Resist lightning"), { RightColumnLabelX, 313 }, 57, RightColumnLabelWidth,
	    []() { return GetResistInfo(InspectPlayer->_pLghtResist); } },
};

OptionalOwnedClxSpriteList Panel;

constexpr int PanelFieldHeight = 24;
constexpr int PanelFieldPaddingTop = 3;
constexpr int PanelFieldPaddingBottom = 3;
constexpr int PanelFieldPaddingSide = 5;
constexpr int PanelFieldInnerHeight = PanelFieldHeight - PanelFieldPaddingTop - PanelFieldPaddingBottom;

[[nodiscard]] std::string GetEntryValue(const PanelEntry &entry)
{
	if (!entry.statDisplayFunc)
		return {};
	const StyledText tmp = (*entry.statDisplayFunc)();
	return tmp.text;
}

[[nodiscard]] std::string_view GetBaseLabelForSpeech()
{
	return LanguageTranslate(panelEntries[AttributeHeaderEntryIndices[0]].label);
}

[[nodiscard]] std::string_view GetNowLabelForSpeech()
{
	return LanguageTranslate(panelEntries[AttributeHeaderEntryIndices[1]].label);
}

[[nodiscard]] std::optional<CharacterAttribute> AttributeForField(CharacterScreenField field)
{
	switch (field) {
	case CharacterScreenField::Strength:
		return CharacterAttribute::Strength;
	case CharacterScreenField::Magic:
		return CharacterAttribute::Magic;
	case CharacterScreenField::Dexterity:
		return CharacterAttribute::Dexterity;
	case CharacterScreenField::Vitality:
		return CharacterAttribute::Vitality;
	default:
		return std::nullopt;
	}
}

[[nodiscard]] std::string_view AttributeLabelForSpeech(CharacterAttribute attribute)
{
	switch (attribute) {
	case CharacterAttribute::Strength:
		return LanguageTranslate(panelEntries[7].label);
	case CharacterAttribute::Magic:
		return LanguageTranslate(panelEntries[9].label);
	case CharacterAttribute::Dexterity:
		return LanguageTranslate(panelEntries[11].label);
	case CharacterAttribute::Vitality:
		return LanguageTranslate(panelEntries[13].label);
	default:
		return {};
	}
}

[[nodiscard]] int PointsToDistributeForSpeech()
{
	if (InspectPlayer == nullptr)
		return 0;
	return std::min(CalcStatDiff(*InspectPlayer), InspectPlayer->_pStatPts);
}

[[nodiscard]] std::string GetCharacterScreenFieldText(CharacterScreenField field)
{
	if (InspectPlayer == nullptr)
		return {};

	switch (field) {
	case CharacterScreenField::NameAndClass: {
		const std::string name = GetEntryValue(panelEntries[0]);
		const std::string className = GetEntryValue(panelEntries[1]);
		if (name.empty())
			return className;
		if (className.empty())
			return name;
		return StrCat(name, ", ", className);
	}
	case CharacterScreenField::Level:
		return StrCat(LanguageTranslate(panelEntries[2].label), ": ", GetEntryValue(panelEntries[2]));
	case CharacterScreenField::Experience:
		return StrCat(LanguageTranslate(panelEntries[3].label), ": ", GetEntryValue(panelEntries[3]));
	case CharacterScreenField::NextLevel:
		return StrCat(LanguageTranslate(panelEntries[4].label), ": ", GetEntryValue(panelEntries[4]));
	case CharacterScreenField::Strength:
		return StrCat(LanguageTranslate(panelEntries[7].label), ": ", GetBaseLabelForSpeech(), " ", GetEntryValue(panelEntries[7]), ", ", GetNowLabelForSpeech(), " ", GetEntryValue(panelEntries[8]));
	case CharacterScreenField::Magic:
		return StrCat(LanguageTranslate(panelEntries[9].label), ": ", GetBaseLabelForSpeech(), " ", GetEntryValue(panelEntries[9]), ", ", GetNowLabelForSpeech(), " ", GetEntryValue(panelEntries[10]));
	case CharacterScreenField::Dexterity:
		return StrCat(LanguageTranslate(panelEntries[11].label), ": ", GetBaseLabelForSpeech(), " ", GetEntryValue(panelEntries[11]), ", ", GetNowLabelForSpeech(), " ", GetEntryValue(panelEntries[12]));
	case CharacterScreenField::Vitality:
		return StrCat(LanguageTranslate(panelEntries[13].label), ": ", GetBaseLabelForSpeech(), " ", GetEntryValue(panelEntries[13]), ", ", GetNowLabelForSpeech(), " ", GetEntryValue(panelEntries[14]));
	case CharacterScreenField::PointsToDistribute:
		return StrCat(LanguageTranslate(panelEntries[15].label), ": ", PointsToDistributeForSpeech());
	case CharacterScreenField::Gold:
		return StrCat(LanguageTranslate(panelEntries[16].label), ": ", GetEntryValue(panelEntries[17]));
	case CharacterScreenField::ArmorClass:
		return StrCat(LanguageTranslate(panelEntries[18].label), ": ", GetEntryValue(panelEntries[18]));
	case CharacterScreenField::ChanceToHit:
		return StrCat(LanguageTranslate(panelEntries[19].label), ": ", GetEntryValue(panelEntries[19]));
	case CharacterScreenField::Damage:
		return StrCat(LanguageTranslate(panelEntries[20].label), ": ", GetEntryValue(panelEntries[20]));
	case CharacterScreenField::Life: {
		const std::string maxValue = GetEntryValue(panelEntries[21]);
		const std::string currentValue = GetEntryValue(panelEntries[22]);
		return StrCat(LanguageTranslate(panelEntries[21].label), ": ", currentValue, "/", maxValue);
	}
	case CharacterScreenField::Mana: {
		const std::string maxValue = GetEntryValue(panelEntries[23]);
		const std::string currentValue = GetEntryValue(panelEntries[24]);
		return StrCat(LanguageTranslate(panelEntries[23].label), ": ", currentValue, "/", maxValue);
	}
	case CharacterScreenField::ResistMagic:
		return StrCat(LanguageTranslate(panelEntries[25].label), ": ", GetEntryValue(panelEntries[25]));
	case CharacterScreenField::ResistFire:
		return StrCat(LanguageTranslate(panelEntries[26].label), ": ", GetEntryValue(panelEntries[26]));
	case CharacterScreenField::ResistLightning:
		return StrCat(LanguageTranslate(panelEntries[27].label), ": ", GetEntryValue(panelEntries[27]));
	default:
		return {};
	}
}

void SpeakCurrentCharacterScreenField()
{
	const CharacterScreenField field = CharacterScreenFieldOrder[SelectedCharacterScreenFieldIndex];
	const std::string text = GetCharacterScreenFieldText(field);
	if (!text.empty())
		SpeakText(text, true);
}

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
	if (entry.labelLength > 0) {
		wrapped = WordWrapString(textStr, entry.labelLength, GameFont12, Spacing);
		text = wrapped;
	} else {
		text = textStr;
	}

	UiFlags style = UiFlags::VerticalCenter;

	Point labelPosition = entry.position;

	if (entry.length == 0) {
		style |= UiFlags::AlignCenter;
	} else {
		style |= UiFlags::AlignRight;
		labelPosition += Displacement { -entry.labelLength - (IsSmallFontTall() ? 2 : 3), 0 };
	}

	// If the text is less tall than the field, we center it vertically relative to the field.
	// Otherwise, we draw from the top of the field.
	const int textHeight = static_cast<int>((c_count(wrapped, '\n') + 1) * GetLineHeight(wrapped, GameFont12));
	const int labelHeight = std::max(PanelFieldHeight, textHeight);

	DrawString(out, text, { labelPosition + Displacement { -2, 2 }, { entry.labelLength, labelHeight } },
	    { .flags = style | UiFlags::ColorBlack, .spacing = Spacing });
	DrawString(out, text, { labelPosition, { entry.labelLength, labelHeight } },
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
	const OwnedSurface out((*background)[0].width(), (*background)[0].height());
	RenderClxSprite(out, (*background)[0], { 0, 0 });
	background = std::nullopt;

	{
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxLeft, LoadClxWithStatus("data\\boxleftend.clx"));
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxMiddle, LoadClxWithStatus("data\\boxmiddle.clx"));
		ASSIGN_OR_RETURN(OwnedClxSpriteList boxRight, LoadClxWithStatus("data\\boxrightend.clx"));

		const bool isSmallFontTall = IsSmallFontTall();
		const int attributeHeadersY = isSmallFontTall ? 112 : 114;
		for (const unsigned i : AttributeHeaderEntryIndices) {
			panelEntries[i].position.y = attributeHeadersY;
		}
		panelEntries[GoldHeaderEntryIndex].position.y = isSmallFontTall ? 105 : 106;

		for (auto &entry : panelEntries) {
			if (entry.statDisplayFunc) {
				DrawPanelField(out, entry.position, entry.length, boxLeft[0], boxMiddle[0], boxRight[0]);
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
	const Point pos = GetPanelPosition(UiPanels::Character, { 0, 0 });
	RenderClxSprite(out, (*Panel)[0], pos);
	for (auto &entry : panelEntries) {
		if (entry.statDisplayFunc) {
			const StyledText tmp = (*entry.statDisplayFunc)();
			DrawString(
			    out,
			    tmp.text,
			    { entry.position + Displacement { pos.x + PanelFieldPaddingSide, pos.y + PanelFieldPaddingTop }, { entry.length - (PanelFieldPaddingSide * 2), PanelFieldInnerHeight } },
			    { .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter | UiFlags::VerticalCenter | tmp.style });
		}
	}
	DrawStatButtons(out);
}

void InitCharacterScreenSpeech()
{
	if (InspectPlayer == nullptr) {
		SpeakText(_("No player."), true);
		return;
	}

	SelectedCharacterScreenFieldIndex = 0;
	if (!IsInspectingPlayer() && PointsToDistributeForSpeech() > 0) {
		for (auto attribute : enum_values<CharacterAttribute>()) {
			if (InspectPlayer->GetBaseAttributeValue(attribute) >= InspectPlayer->GetMaximumAttributeValue(attribute))
				continue;
			switch (attribute) {
			case CharacterAttribute::Strength:
				SelectedCharacterScreenFieldIndex = 4;
				break;
			case CharacterAttribute::Magic:
				SelectedCharacterScreenFieldIndex = 5;
				break;
			case CharacterAttribute::Dexterity:
				SelectedCharacterScreenFieldIndex = 6;
				break;
			case CharacterAttribute::Vitality:
				SelectedCharacterScreenFieldIndex = 7;
				break;
			}
			break;
		}
	}

	SpeakCurrentCharacterScreenField();
}

void CharacterScreenMoveSelection(int delta)
{
	if (CharFlag == false)
		return;

	const int size = static_cast<int>(CharacterScreenFieldOrder.size());
	int idx = static_cast<int>(SelectedCharacterScreenFieldIndex);
	idx = (idx + delta) % size;
	if (idx < 0)
		idx += size;

	SelectedCharacterScreenFieldIndex = static_cast<size_t>(idx);
	SpeakCurrentCharacterScreenField();
}

void CharacterScreenActivateSelection(bool addAllStatPoints)
{
	if (!CharFlag)
		return;

	if (InspectPlayer == nullptr)
		return;

	const CharacterScreenField field = CharacterScreenFieldOrder[SelectedCharacterScreenFieldIndex];
	const std::optional<CharacterAttribute> attribute = AttributeForField(field);
	if (!attribute) {
		SpeakCurrentCharacterScreenField();
		return;
	}

	if (IsInspectingPlayer()) {
		SpeakText(_("Can't distribute stat points while inspecting players."), true);
		return;
	}

	Player &player = *InspectPlayer;
	const int pointsAvailable = PointsToDistributeForSpeech();
	if (pointsAvailable <= 0) {
		SpeakText(_("No stat points to distribute."), true);
		return;
	}

	const int baseValue = player.GetBaseAttributeValue(*attribute);
	const int maxValue = player.GetMaximumAttributeValue(*attribute);
	if (baseValue >= maxValue) {
		SpeakText(_("Stat is already at maximum."), true);
		return;
	}

	const int pointsToReachCap = maxValue - baseValue;
	const int pointsToAdd = addAllStatPoints ? std::min(pointsAvailable, pointsToReachCap) : 1;
	if (pointsToAdd <= 0)
		return;

	switch (*attribute) {
	case CharacterAttribute::Strength:
		NetSendCmdParam1(true, CMD_ADDSTR, pointsToAdd);
		player._pStatPts -= pointsToAdd;
		break;
	case CharacterAttribute::Magic:
		NetSendCmdParam1(true, CMD_ADDMAG, pointsToAdd);
		player._pStatPts -= pointsToAdd;
		break;
	case CharacterAttribute::Dexterity:
		NetSendCmdParam1(true, CMD_ADDDEX, pointsToAdd);
		player._pStatPts -= pointsToAdd;
		break;
	case CharacterAttribute::Vitality:
		NetSendCmdParam1(true, CMD_ADDVIT, pointsToAdd);
		player._pStatPts -= pointsToAdd;
		break;
	}

	const int currentValue = player.GetCurrentAttributeValue(*attribute);
	const std::string_view attributeLabel = AttributeLabelForSpeech(*attribute);
	const int remaining = PointsToDistributeForSpeech();

	std::string message;
	StrAppend(message, attributeLabel, ": ", GetBaseLabelForSpeech(), " ", baseValue + pointsToAdd, ", ", GetNowLabelForSpeech(), " ", currentValue + pointsToAdd);
	StrAppend(message, ". ", LanguageTranslate(panelEntries[15].label), ": ", remaining);
	SpeakText(message, true);
}

} // namespace devilution
