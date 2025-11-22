/**
 * @file minitext.cpp
 *
 * Implementation of scrolling dialog text.
 */
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "playerdat.hpp"
#include "textdat.h"
#include "utils/language.h"
#include "utils/timer.hpp"

namespace devilution {

bool qtextflag;

namespace {

/** Vertical speed of the scrolling text in ms/px */
int qtextSpd;
/** Start time of scrolling */
uint32_t ScrollStart;
/** Graphics for the window border */
OptionalOwnedClxSpriteList pTextBoxCels;

/** Pixels for a line of text and the empty space under it. */
const int LineHeight = 38;

std::vector<std::string> TextLines;

void LoadText(std::string_view text)
{
	TextLines.clear();

	const std::string paragraphs = WordWrapString(text, 543, GameFont30);

	size_t previous = 0;
	while (true) {
		const size_t next = paragraphs.find('\n', previous);
		TextLines.emplace_back(paragraphs.substr(previous, next - previous));
		if (next == std::string::npos)
			break;
		previous = next + 1;
	}
}

/**
 * @brief Calculate the speed the current text should scroll to match the given audio
 * @param nSFX The index of the sound in the sgSFX table
 * @return ms/px
 */
uint32_t CalculateTextSpeed(SfxID nSFX)
{
	const auto numLines = static_cast<uint32_t>(TextLines.size());

#ifndef NOSOUND
	uint32_t sfxFrames = GetSFXLength(nSFX);
#else
	// Sound is disabled -- estimate length from the number of lines.
	uint32_t sfxFrames = numLines * 3000;
#endif
	assert(sfxFrames != 0);

	uint32_t textHeight = LineHeight * numLines;
	textHeight += LineHeight * 5; // adjust so when speaker is done two line are left
	assert(textHeight != 0);

	return sfxFrames / textHeight;
}

int CalculateTextPosition()
{
	const uint32_t currTime = GetMillisecondsSinceStartup();

	const int y = (currTime - ScrollStart) / qtextSpd - 260;

	const auto textHeight = static_cast<int>(LineHeight * TextLines.size());
	if (y >= textHeight)
		qtextflag = false;

	return y;
}

/**
 * @brief Draw the current text in the quest dialog window
 */
void DrawQTextContent(const Surface &out)
{
	const int y = CalculateTextPosition();

	const int sx = GetUIRectangle().position.x + 48;
	const int sy = 0 - (y % LineHeight);

	const unsigned int skipLines = y / LineHeight;

	for (int i = 0; i < 8; i++) {
		const unsigned int lineNumber = skipLines + i;
		if (lineNumber >= TextLines.size()) {
			continue;
		}

		const std::string &line = TextLines[lineNumber];
		if (line.empty()) {
			continue;
		}

		DrawString(out, line, { { sx, sy + i * LineHeight }, { 543, LineHeight } },
		    { .flags = UiFlags::FontSize30 | UiFlags::ColorGold });
	}
}

} // namespace

void FreeQuestText()
{
	pTextBoxCels = std::nullopt;
}

void InitQuestText()
{
	pTextBoxCels = LoadCel("data\\textbox", 591);
}

void InitQTextMsg(_speech_id m)
{
	SfxID sfxnr = Speeches[m].sfxnr;
	switch (sfxnr) {
	case SfxID::Warrior1:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::ChamberOfBoneLore);
		break;
	case SfxID::Warrior10:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::ValorLore);
		break;
	case SfxID::Warrior11:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::HallsOfTheBlindLore);
		break;
	case SfxID::Warrior12:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::WarlordOfBloodLore);
		break;
	case SfxID::Warrior54:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::InSpirituSanctum);
		break;
	case SfxID::Warrior55:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::PraedictumOtium);
		break;
	case SfxID::Warrior56:
		sfxnr = GetHeroSound(MyPlayer->_pClass, HeroSpeech::EfficioObitusUtInimicus);
		break;
	default:
		break;
	}
	if (Speeches[m].scrlltxt) {
		QuestLogIsOpen = false;
		LoadText(_(Speeches[m].txtstr));
		qtextflag = true;
		qtextSpd = CalculateTextSpeed(sfxnr);
		ScrollStart = GetMillisecondsSinceStartup();
	}
	PlaySFX(sfxnr);
}

void DrawQTextBack(const Surface &out)
{
	const Point uiPosition = GetUIRectangle().position;
	ClxDraw(out, uiPosition + Displacement { 24, 327 }, (*pTextBoxCels)[0]);
	DrawHalfTransparentRectTo(out, uiPosition.x + 27, uiPosition.y + 28, 585, 297);
}

void DrawQText(const Surface &out)
{
	DrawQTextBack(out);
	DrawQTextContent(out.subregionY(GetUIRectangle().position.y + 49, 260));
}

} // namespace devilution
