#include "control_flasks.hpp"
#include "control.hpp"

#include "engine/render/renderer.h"
#include "engine/surface.hpp"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

std::optional<OwnedSurface> pLifeBuff;
std::optional<OwnedSurface> pManaBuff;

namespace {

Rectangle FlaskTopRect { { 11, 3 }, { 62, 13 } };
Rectangle FlaskBottomRect { { 0, 16 }, { 88, 69 } };

/**
 * Draws the dome of the flask that protrudes above the panel top line.
 * It draws a rectangle of fixed width 59 and height 'h' from the source buffer
 * into the target buffer.
 * @param out The target buffer.
 * @param celBuf Buffer of the flask cel.
 * @param targetPosition Target buffer coordinate.
 */
void DrawFlaskAbovePanel(const Surface &celBuf, Point targetPosition)
{
	GetRenderer().BlitSurfaceSkipColorIndexZero(celBuf, MakeSdlRect(0, 0, celBuf.w(), celBuf.h()), targetPosition);
}

/**
 * @brief Draws the part of the life/mana flasks protruding above the bottom panel
 * @see DrawFlaskLower()
 * @param out The display region to draw to
 * @param sourceBuffer A sprite representing the appropriate background/empty flask style
 * @param offset X coordinate offset for where the flask should be drawn
 * @param fillPer How full the flask is (a value from 0 to 81)
 */
void DrawFlaskUpper(const Surface &sourceBuffer, int offset, int fillPer)
{
	const Rectangle &rect = FlaskTopRect;
	const int emptyRows = std::clamp(81 - fillPer, 0, rect.size.height);
	const int filledRows = rect.size.height - emptyRows;

	// Draw the empty part of the flask
	DrawFlaskAbovePanel(
	    sourceBuffer.subregion(rect.position.x, rect.position.y, rect.size.width, rect.size.height),
	    GetMainPanel().position + Displacement { offset, -rect.size.height });

	// Draw the filled part of the flask over the empty part
	if (filledRows > 0) {
		DrawFlaskAbovePanel(
		    BottomBuffer->subregion(offset, rect.position.y + emptyRows, rect.size.width, filledRows),
		    GetMainPanel().position + Displacement { offset, -rect.size.height + emptyRows });
	}
}

/**
 * Draws a section of the empty flask cel on top of the panel to create the illusion
 * of the flask getting empty. This function takes a cel and draws a
 * horizontal stripe of height (max-min) onto the given buffer.
 * @param out Target buffer.
 * @param celBuf Buffer of the flask cel.
 * @param targetPosition Target buffer coordinate.
 */
void DrawFlaskOnPanel(const Surface &celBuf, Point targetPosition)
{
	GetRenderer().BlitSurface(celBuf, MakeSdlRect(0, 0, celBuf.w(), celBuf.h()), targetPosition);
}

/**
 * @brief Draws the part of the life/mana flasks inside the bottom panel
 * @see DrawFlaskUpper()
 * @param out The display region to draw to
 * @param sourceBuffer A sprite representing the appropriate background/empty flask style
 * @param offset X coordinate offset for where the flask should be drawn
 * @param fillPer How full the flask is (a value from 0 to 80)
 * @param drawFilledPortion Indicates whether to draw the filled portion of the flask
 */
void DrawFlaskLower(const Surface &sourceBuffer, int offset, int fillPer, bool drawFilledPortion)
{
	const Rectangle &rect = FlaskBottomRect;
	const int filledRows = std::clamp(fillPer, 0, rect.size.height);
	const int emptyRows = rect.size.height - filledRows;

	// Draw the empty part of the flask
	if (emptyRows > 0) {
		DrawFlaskOnPanel(
		    sourceBuffer.subregion(rect.position.x, rect.position.y, rect.size.width, emptyRows),
		    GetMainPanel().position + Displacement { offset, 0 });
	}

	// Draw the filled part of the flask
	if (drawFilledPortion && filledRows > 0) {
		DrawFlaskOnPanel(
		    BottomBuffer->subregion(offset, rect.position.y + emptyRows, rect.size.width, filledRows),
		    GetMainPanel().position + Displacement { offset, emptyRows });
	}
}

} // namespace

void DrawLifeFlaskUpper()
{
	constexpr int LifeFlaskUpperOffset = 107;
	DrawFlaskUpper(*pLifeBuff, LifeFlaskUpperOffset, MyPlayer->_pHPPer);
}

void DrawManaFlaskUpper()
{
	constexpr int ManaFlaskUpperOffset = 475;
	DrawFlaskUpper(*pManaBuff, ManaFlaskUpperOffset, MyPlayer->_pManaPer);
}

void DrawLifeFlaskLower(bool drawFilledPortion)
{
	constexpr int LifeFlaskLowerOffset = 96;
	DrawFlaskLower(*pLifeBuff, LifeFlaskLowerOffset, MyPlayer->_pHPPer, drawFilledPortion);
}

void DrawManaFlaskLower(bool drawFilledPortion)
{
	constexpr int ManaFlaskLowerOffset = 464;
	DrawFlaskLower(*pManaBuff, ManaFlaskLowerOffset, MyPlayer->_pManaPer, drawFilledPortion);
}

void DrawFlaskValues(Point pos, int currValue, int maxValue)
{
	const UiFlags color = (currValue > 0 ? (currValue == maxValue ? UiFlags::ColorGold : UiFlags::ColorWhite) : UiFlags::ColorRed);

	auto drawStringWithShadow = [color](std::string_view text, Point pos) {
		DrawString(text, { pos + Displacement { -1, -1 }, { gnScreenWidth - pos.x + 1, 0 } },
		    { .flags = UiFlags::ColorBlack | UiFlags::KerningFitSpacing, .spacing = 0 });
		DrawString(text, { pos, { gnScreenWidth - pos.x, 0 } },
		    { .flags = color | UiFlags::KerningFitSpacing, .spacing = 0 });
	};

	const std::string currText = StrCat(currValue);
	drawStringWithShadow(currText, pos - Displacement { GetLineWidth(currText, GameFont12) + 1, 0 });
	drawStringWithShadow("/", pos);
	drawStringWithShadow(StrCat(maxValue), pos + Displacement { GetLineWidth("/", GameFont12) + 1, 0 });
}

void UpdateLifeManaPercent()
{
	MyPlayer->UpdateManaPercentage();
	MyPlayer->UpdateHitPointPercentage();
}

} // namespace devilution
