#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "player.h"
#include "spelldat.h"

namespace devilution {

struct SpellListItem {
	Point location;
	SpellType type;
	SpellID id;
	bool isSelected;
};

/**
 * @brief draws the current right mouse button spell.
 * @param out screen buffer representing the main UI panel
 */
void DrawSpell(const Surface &out);
void DrawSpellList(const Surface &out);
std::vector<SpellListItem> GetSpellListItems();
void SetSpell();
void SetSpeedSpell(size_t slot);
bool IsValidSpeedSpell(size_t slot);
void ToggleSpell(size_t slot);
std::pair<std::string_view, int> GetRuneInfoStringAndCount(const devilution::Player &player, devilution::SpellID spellId);

/**
 * Draws the "Speed Book": the rows of known spells for quick-setting a spell that
 * show up when you click the spell slot at the control panel.
 */
void DoSpeedBook();

} // namespace devilution
