#pragma once

#include <optional>
#include <string>
#include <vector>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"
#include "tables/spelldat.h"

namespace devilution {

struct Player;

tl::expected<void, std::string> InitSpellBook();
void FreeSpellBook();
void CheckSBook();
void DrawSpellBook(const Surface &out);

std::vector<SpellID> GetSpellBookAvailableSpells(int tab, const Player &player);
std::optional<SpellID> GetSpellBookFirstAvailableSpell(int tab, const Player &player);
std::optional<SpellID> GetSpellBookAdjacentAvailableSpell(int tab, const Player &player, SpellID currentSpell, int delta);

} // namespace devilution
