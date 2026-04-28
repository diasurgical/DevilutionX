#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Fixed size info box frame
 *
 * Used in stores, the quest log, the help window, and the unique item info window.
 */
extern OptionalOwnedClxSpriteList pSTextBoxCels;

/**
 * @brief Dynamic size info box frame and scrollbar graphics.
 *
 * Used in stores and `DrawDiabloMsg`.
 */
extern OptionalOwnedClxSpriteList pSTextSlidCels;

/**
 * @brief Height of the divider strip drawn between info box text lines.
 */
constexpr int TextBoxDividerHeight = 3;

/**
 * @brief Extracts the divider strip from an info box frame sprite.
 *
 * The divider drawn between text lines is a slice of the frame's own interior, so it used to
 * be copied straight out of the back buffer. The back buffer is not readable on every renderer,
 * so the slice is composited once when the frame is loaded instead.
 *
 * The result holds palette indices and is therefore unaffected by later palette changes.
 */
OwnedSurface MakeTextBoxDivider(ClxSprite frame);

/**
 * @brief The divider strip of the narrow (side panel width) info box frame.
 */
const Surface &GetSTextBoxDivider();

void InitInfoBoxGfx();
void FreeInfoBoxGfx();

} // namespace devilution
