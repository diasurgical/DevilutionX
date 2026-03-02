#pragma once

#include <cstdint>
#include <string_view>

namespace devilution {
/**
 * @brief Triggers a Lua event by name.
 * This is a minimal header for code that only needs to trigger events.
 */
struct Player;
struct Monster;

namespace lua {

void LoadModsComplete();
void GameStart();
void GameDrawComplete();

void ItemDataLoaded();
void UniqueItemDataLoaded();
void MonsterDataLoaded();
void UniqueMonsterDataLoaded();

void StoreOpened(std::string_view name);

void OnPlayerGainExperience(const Player &player, uint32_t experience);
void OnPlayerTakeDamage(const Player *player, int damage, int damageType);
void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType);

bool OnPlayerDeathDropGold(const Player *player);
bool OnPlayerDeathDropItem(const Player *player);
bool OnPlayerDeathDropEar(const Player *player);

} // namespace lua

} // namespace devilution
