/**
 * @file levels/drlg_l1.h
 *
 * Interface of the cathedral level generation algorithms.
 */
#pragma once

#include <cstdint>
#include <expected>

#include "engine/world_tile.hpp"
#include "levels/gendung.h"

namespace devilution {

void PlaceMiniSetRandom(const Miniset &miniset, int rndper);
WorldTilePosition SelectChamber();
void CreateL5Dungeon(uint32_t rseed, lvl_entry entry);
void LoadPreL1Dungeon(const char *path);
std::expected<void, std::string> LoadL1Dungeon(const char *path, Point spawn);

} // namespace devilution
