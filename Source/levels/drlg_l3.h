/**
 * @file levels/drlg_l3.h
 *
 * Interface of the caves level generation algorithms.
 */
#pragma once

#include <cstdint>
#include <expected>

#include "levels/gendung.h"

namespace devilution {

void CreateL3Dungeon(uint32_t rseed, lvl_entry entry);
void LoadPreL3Dungeon(const char *sFileName);
std::expected<void, std::string> LoadL3Dungeon(const char *sFileName, Point spawn);

} // namespace devilution
