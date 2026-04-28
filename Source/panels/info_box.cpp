#include "panels/info_box.hpp"

#include <cassert>
#include <optional>

#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "utils/sdl_geometry.h"

namespace devilution {

namespace {

/** Horizontal inset of the frame's interior, i.e. the width of its left and right border. */
constexpr int DividerInsetX = 2;

std::optional<OwnedSurface> STextBoxDivider;

} // namespace

OptionalOwnedClxSpriteList pSTextBoxCels;
OptionalOwnedClxSpriteList pSTextSlidCels;

OwnedSurface MakeTextBoxDivider(ClxSprite frame)
{
	const int frameWidth = static_cast<int>(frame.width());
	const int frameHeight = static_cast<int>(frame.height());

	// Callers draw the frame with its bottom-left corner at y=327, putting its top edge at
	// y = 327 - frameHeight + 1. The divider row sits at y=25 on screen, so `frameHeight - 303`
	// within the sprite. That is row 0 for the stock 303px-high frames.
	const int dividerY = frameHeight - 303;
	const int dividerWidth = frameWidth - (DividerInsetX * 2);
	assert(dividerY >= 0 && dividerY + TextBoxDividerHeight <= frameHeight);
	assert(dividerWidth > 0);

	OwnedSurface frameSurface(frameWidth, frameHeight);
	FillRect(frameSurface, 0, 0, frameWidth, frameHeight, 0);
	ClxDraw(frameSurface, { 0, frameHeight - 1 }, frame);

	OwnedSurface divider(dividerWidth, TextBoxDividerHeight);
	divider.BlitFrom(frameSurface,
	    MakeSdlRect(DividerInsetX, dividerY, dividerWidth, TextBoxDividerHeight), { 0, 0 });
	return divider;
}

const Surface &GetSTextBoxDivider()
{
	return *STextBoxDivider;
}

void InitInfoBoxGfx()
{
	pSTextSlidCels = LoadCel("data\\textslid", 12);
	pSTextBoxCels = LoadCel("data\\textbox2", 271);
	STextBoxDivider = MakeTextBoxDivider((*pSTextBoxCels)[0]);
}

void FreeInfoBoxGfx()
{
	STextBoxDivider = std::nullopt;
	pSTextBoxCels = std::nullopt;
	pSTextSlidCels = std::nullopt;
}

} // namespace devilution
