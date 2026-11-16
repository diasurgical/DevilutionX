#pragma once

#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution {

void DrawControllerModifierHints(const Surface &out);
void InitModifierHints();
void FreeModifierHints();

struct Player;

/** Size of the quick spell (spell hotkeys) hint drawn by DrawSpellHotkeysHint. */
Size GetSpellHotkeysHintSize();

/**
 * @brief Draw the four quick spell hotkeys of a player, as shown while holding the gamepad hotspell modifier.
 * @param topLeft Top-left corner of the hint.
 */
void DrawSpellHotkeysHint(const Surface &out, const Player &player, Point topLeft);

} // namespace devilution
