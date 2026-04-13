#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

void DrawControllerModifierHints(const Surface &out);
void InitModifierHints();
void FreeModifierHints();

/** Get the hint box (golden border) sprite for skill/belt slots. Size is 39x39. */
OptionalOwnedClxSpriteList &GetHintBoxSprite();

/** Get the hint box background sprite. Size is 37x38. */
OptionalOwnedClxSpriteList &GetHintBoxBackgroundSprite();

} // namespace devilution
