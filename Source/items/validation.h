/**
 * @file items/validation.h
 *
 * Interface of functions for validation of player and item data.
 */
#pragma once

#include <cstdint>

namespace devilution {

// Forward declared structs to avoid circular dependencies
struct Item;
struct TCmdPItem;
struct Player;

bool IsCreationFlagComboValid(uint16_t iCreateInfo);
bool IsTownItemValid(uint16_t iCreateInfo, const Player &player);
bool IsShopPriceValid(const Item &item);
bool IsUniqueMonsterItemValid(uint16_t iCreateInfo, uint32_t dwBuff);
bool IsDungeonItemValid(uint16_t iCreateInfo, uint32_t dwBuff);
bool IsItemValid(const Player &player, const Item &item);
bool IsItemDeltaValid(const TCmdPItem &itemDelta);

} // namespace devilution
