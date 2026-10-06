/**
 * @file levels/drlg_l4.h
 *
 * Interface of the hell level generation algorithms.
 */
#pragma once

#include <cstdint>
#include <expected>

#include "engine/world_tile.hpp"
#include "levels/gendung.h"

namespace devilution {

extern WorldTilePosition DiabloQuad1;
extern WorldTilePosition DiabloQuad2;
extern WorldTilePosition DiabloQuad3;
extern WorldTilePosition DiabloQuad4;

void CreateL4Dungeon(uint32_t rseed, lvl_entry entry);
void LoadPreL4Dungeon(const char *path);
std::expected<void, std::string> LoadL4Dungeon(const char *path, Point spawn);

} // namespace devilution
