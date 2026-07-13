/**
 * @file levels/drlg_l2.h
 *
 * Interface of the catacombs level generation algorithms.
 */
#pragma once

#include <cstdint>
#include <expected>

#include "levels/gendung.h"

namespace devilution {

void CreateL2Dungeon(uint32_t rseed, lvl_entry entry);
void LoadPreL2Dungeon(const char *path);
std::expected<void, std::string> LoadL2Dungeon(const char *path, Point spawn);

} // namespace devilution
